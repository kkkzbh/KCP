package org.cp.lang.clion

import com.intellij.codeInsight.hints.declarative.HintFontSize
import com.intellij.codeInsight.hints.declarative.HintFormat
import com.intellij.codeInsight.hints.declarative.HintMarginPadding
import com.intellij.codeInsight.hints.declarative.InlayHintsCollector
import com.intellij.codeInsight.hints.declarative.InlayHintsProvider
import com.intellij.codeInsight.hints.declarative.InlayTreeSink
import com.intellij.codeInsight.hints.declarative.InlineInlayPosition
import com.intellij.codeInsight.hints.declarative.SharedBypassCollector
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.util.Key
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile

class CpTypeHintsProvider : InlayHintsProvider {
    override fun createCollector(file: PsiFile, editor: Editor): InlayHintsCollector? {
        if (file.language != CpLanguage) {
            return null
        }
        val hints = CpTypeHintEngine.items(file).associateBy { it.offset }
        return object : SharedBypassCollector {
            private val hintFormat = HintFormat.default
                .withFontSize(HintFontSize.ABitSmallerThanInEditor)
                .withHorizontalMargin(HintMarginPadding.MarginAndSmallerPadding)

            override fun collectFromElement(element: PsiElement, sink: InlayTreeSink) {
                if (element.cpElementType() != CpElements.LOCAL_DECLARATION) {
                    return
                }
                val hint = hints[element.textRange.endOffset] ?: return
                sink.addPresentation(
                    InlineInlayPosition(hint.offset, true),
                    hintFormat = hintFormat,
                ) {
                    text(": ${hint.type}")
                }
            }
        }
    }
}

data class CpTypeHint(
    val offset: Int,
    val name: String,
    val type: String,
)

object CpTypeHintEngine {
    private val expressionTypes = setOf(
        CpElements.NAME_EXPRESSION,
        CpElements.LITERAL_EXPRESSION,
        CpElements.UNARY_EXPRESSION,
        CpElements.BINARY_EXPRESSION,
        CpElements.CALL_EXPRESSION,
        CpElements.MEMBER_EXPRESSION,
        CpElements.INDEX_EXPRESSION,
        CpElements.CAST_EXPRESSION,
        CpElements.ARRAY_LITERAL,
        CpElements.TUPLE_LITERAL,
        CpElements.GROUPED_EXPRESSION,
        CpElements.STRUCT_INIT_EXPRESSION,
        CpElements.MATCH_EXPRESSION,
        CpElements.LAMBDA_EXPRESSION,
        CpElements.BLOCK_EXPRESSION,
    )

    fun items(file: PsiFile): List<CpTypeHint> =
        CpTypeHintIndex.get(file)

    private fun compute(file: PsiFile, snapshot: CpSymbolScopeSnapshot): List<CpTypeHint> {
        val context = CpTypeContext(snapshot)
        val hints = mutableListOf<CpTypeHint>()
        for (declaration in CpFileSymbolIndex.get(file).declarationStatements) {
            val name = declaration.directChild(CpElements.LOCAL_DECLARATION) ?: continue
            val explicitType = declaration.directTypeReference()
            val inferred = explicitType?.text ?: context.infer(declaration.directExpression())
            if (inferred == null) {
                continue
            }
            context.locals[name.text] = inferred
            if (explicitType == null) {
                hints += CpTypeHint(name.textRange.endOffset, name.text, inferred)
            }
        }
        return hints
    }

    private fun PsiElement.directTypeReference(): PsiElement? =
        children.firstOrNull { it.cpElementType() == CpElements.TYPE_REFERENCE }

    private fun PsiElement.directExpression(): PsiElement? =
        children.firstOrNull { it.cpElementType() in expressionTypes }

    private class CpTypeContext(snapshot: CpSymbolScopeSnapshot) {
        val locals = linkedMapOf<String, String>()
        private val functions = mutableMapOf<String, String>()
        private val memberFunctions = mutableMapOf<String, MutableMap<String, String>>()
        private val structFields = mutableMapOf<String, MutableMap<String, String>>()

        init {
            for (entry in snapshot.entries) {
                collectSymbols(entry.symbols)
            }
        }

        fun infer(expression: PsiElement?): String? {
            expression ?: return null
            return when (expression.cpElementType()) {
                CpElements.LITERAL_EXPRESSION -> inferLiteral(expression.text)
                CpElements.NAME_EXPRESSION -> locals[expression.text] ?: inferKeywordLiteral(expression.text)
                CpElements.GROUPED_EXPRESSION -> expression.firstExpression()?.let(::infer)
                CpElements.TUPLE_LITERAL -> inferTuple(expression)
                CpElements.ARRAY_LITERAL -> inferArray(expression)
                CpElements.STRUCT_INIT_EXPRESSION -> expression.firstDescendant(CpElements.TYPE_NAME)?.text
                CpElements.CALL_EXPRESSION -> inferCall(expression)
                CpElements.MEMBER_EXPRESSION -> inferMemberExpression(expression)
                CpElements.CAST_EXPRESSION -> expression.directChild(CpElements.TYPE_REFERENCE)?.text
                CpElements.UNARY_EXPRESSION -> infer(expression.firstExpression())
                CpElements.BINARY_EXPRESSION -> inferBinary(expression)
                CpElements.LAMBDA_EXPRESSION -> expression.lambdaType()
                else -> null
            }
        }

        private fun inferCall(expression: PsiElement): String? {
            val callee = expression.children.firstOrNull {
                it.cpElementType() == CpElements.NAME_EXPRESSION ||
                    it.cpElementType() == CpElements.MEMBER_EXPRESSION
            } ?: return null
            return when (callee.cpElementType()) {
                CpElements.NAME_EXPRESSION ->
                    if (callee.text == "builtin") {
                        expression.argumentExpressions().firstOrNull()?.let(::infer)
                    } else {
                        enclosingImplType(expression)?.let { receiver ->
                            memberFunctions[receiver.baseType()]?.get(callee.text)
                        } ?: functions[callee.text]
                    }
                CpElements.MEMBER_EXPRESSION -> inferMemberCall(callee)
                else -> null
            }
        }

        private fun inferMemberCall(expression: PsiElement): String? {
            val member = expression.directChild(CpElements.MEMBER_NAME)?.text ?: return null
            val receiver = expression.firstExpression()?.let(::infer)?.baseType() ?: return null
            return memberFunctions[receiver]?.get(member) ?: functions[member]
        }

        private fun inferMemberExpression(expression: PsiElement): String? {
            val member = expression.directChild(CpElements.MEMBER_NAME)?.text ?: return null
            val receiver = expression.firstExpression()?.let(::infer)?.baseType() ?: return null
            return structFields[receiver]?.get(member)
        }

        private fun inferTuple(expression: PsiElement): String {
            val elementTypes = expression.children
                .filter { it.cpElementType() in expressionTypes }
                .map { infer(it) ?: "unknown" }
            return "(${elementTypes.joinToString(", ")})"
        }

        private fun inferArray(expression: PsiElement): String? {
            val elementTypes = expression.children
                .filter { it.cpElementType() in expressionTypes }
                .mapNotNull(::infer)
            if (elementTypes.isEmpty()) {
                return null
            }
            val elementType = elementTypes.reduce { current, next -> if (current == next) current else "unknown" }
            return "[$elementType; ${elementTypes.size}]"
        }

        private fun inferBinary(expression: PsiElement): String? {
            val operatorText = expression.text
            if (listOf("==", "!=", "<=", ">=", "<", ">", "&&", "||", " and ", " or ").any { it in operatorText }) {
                return "bool"
            }
            val operands = expression.children.filter { it.cpElementType() in expressionTypes }.mapNotNull(::infer)
            if ("f64" in operands) {
                return "f64"
            }
            if ("i32" in operands) {
                return "i32"
            }
            return operands.firstOrNull()
        }

        private fun PsiElement.firstExpression(): PsiElement? =
            children.firstOrNull { it.cpElementType() in expressionTypes }

        private fun PsiElement.argumentExpressions(): List<PsiElement> =
            (directChild(CpElements.ARGUMENT_LIST)?.children ?: emptyArray())
                .filter { it.cpElementType() in expressionTypes }

        private fun PsiElement.lambdaType(): String {
            val parameterTypes = directChild(CpElements.PARAMETER_LIST)
                ?.directChildren(CpElements.PARAMETER)
                ?.map { it.directChild(CpElements.TYPE_REFERENCE)?.text ?: "unknown" }
                .orEmpty()
            val returnType = returnTypeText() ?: "unknown"
            return "f(${parameterTypes.joinToString(", ")}) -> $returnType"
        }

        private fun collectSymbols(symbols: CpFileSymbols) {
            for (function in symbols.functions) {
                if (function.receiverType != null || function.element.parent?.parentByType(CpElements.CONCEPT_DECLARATION) != null) {
                    continue
                }
                functions.putIfAbsent(function.name, function.returnType)
            }
            for (function in symbols.functions) {
                val receiver = function.receiverType ?: continue
                val receiverFunctions = memberFunctions.getOrPut(receiver) { mutableMapOf() }
                receiverFunctions.putIfAbsent(function.name, function.returnType)
            }
            for (field in symbols.structFields) {
                val owner = field.ownerType ?: continue
                val fields = structFields.getOrPut(owner) { mutableMapOf() }
                fields.putIfAbsent(field.name, field.type)
            }
        }

        private fun enclosingImplType(element: PsiElement): String? =
            element.parentByType(CpElements.IMPL_BLOCK)
                ?.directChild(CpElements.TYPE_REFERENCE)
                ?.typeName()
    }

    private fun PsiElement.returnTypeText(): String? {
        val parameterList = directChild(CpElements.PARAMETER_LIST) ?: return directChild(CpElements.TYPE_REFERENCE)?.text
        return children
            .dropWhile { it != parameterList }
            .drop(1)
            .firstOrNull { it.cpElementType() == CpElements.TYPE_REFERENCE }
            ?.text
    }

    private fun inferLiteral(text: String): String? =
        when {
            inferKeywordLiteral(text) != null -> inferKeywordLiteral(text)
            text.startsWith('"') -> "str"
            text.startsWith('\'') -> "char"
            text.contains('.') -> "f64"
            text.matches(Regex("""\d+""")) -> "i32"
            else -> null
        }

    private fun inferKeywordLiteral(text: String): String? =
        when (text) {
            "true", "false" -> "bool"
            "nullptr" -> "nullptr"
            else -> null
        }

    private fun PsiElement.typeName(): String? =
        firstDescendant(CpElements.TYPE_NAME)?.text

    private fun String.baseType(): String =
        takeWhile { it.isLetterOrDigit() || it == '_' }

    private object CpTypeHintIndex {
        fun get(file: PsiFile): List<CpTypeHint> {
            val snapshot = CpSymbolScope.snapshot(file)
            val signature = snapshot.entries.map { it.stamp }
            file.getUserData(cacheKey)?.takeIf { it.signature == signature }?.let {
                return it.hints
            }

            val hints = compute(file, snapshot)
            file.putUserData(cacheKey, CachedTypeHints(signature, hints))
            return hints
        }

        private val cacheKey = Key.create<CachedTypeHints>("org.cp.lang.clion.typeHintIndex")
    }
}

private data class CachedTypeHints(
    val signature: List<CpSymbolScopeStamp>,
    val hints: List<CpTypeHint>,
)
