package org.cp.lang.clion

import com.intellij.codeInsight.hints.ChangeListener
import com.intellij.codeInsight.hints.ImmediateConfigurable
import com.intellij.codeInsight.hints.InlayHintsCollector
import com.intellij.codeInsight.hints.InlayHintsProvider
import com.intellij.codeInsight.hints.InlayHintsSink
import com.intellij.codeInsight.hints.NoSettings
import com.intellij.codeInsight.hints.SettingsKey
import com.intellij.codeInsight.hints.presentation.PresentationFactory
import com.intellij.lang.Language
import com.intellij.openapi.editor.Editor
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.tree.IElementType
import javax.swing.JComponent
import javax.swing.JPanel

class CpTypeHintsProvider : InlayHintsProvider<NoSettings> {
    override val name: String = "cp inferred types"

    override val key: SettingsKey<NoSettings> = SettingsKey("cp.type.hints")

    override val previewText: String =
        """
        main()
        {
            let value = answer();
        }
        """.trimIndent()

    override fun getCollectorFor(file: PsiFile, editor: Editor, settings: NoSettings, sink: InlayHintsSink): InlayHintsCollector? {
        if (file.language != CpLanguage) {
            return null
        }
        val hints = CpTypeHintEngine.items(file).associateBy { it.offset }
        val factory = PresentationFactory(editor)
        return object : InlayHintsCollector {
            override fun collect(element: PsiElement, editor: Editor, sink: InlayHintsSink): Boolean {
                val hint = hints[element.textRange.endOffset] ?: return true
                sink.addInlineElement(hint.offset, true, factory.smallText(": ${hint.type}"), false)
                return true
            }
        }
    }

    override fun createSettings(): NoSettings = NoSettings()

    override fun createConfigurable(settings: NoSettings): ImmediateConfigurable =
        object : ImmediateConfigurable {
            override fun createComponent(listener: ChangeListener): JComponent = JPanel()
        }

    override fun isLanguageSupported(language: Language): Boolean = language == CpLanguage
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

    fun items(file: PsiFile): List<CpTypeHint> {
        val context = CpTypeContext(file)
        val hints = mutableListOf<CpTypeHint>()
        for (declaration in file.descendants(CpElements.DECLARATION_STATEMENT)) {
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

    private class CpTypeContext(file: PsiFile) {
        val locals = linkedMapOf<String, String>()
        private val functions = file.descendants(CpElements.FUNCTION)
            .mapNotNull { function ->
                val name = function.directChild(CpElements.FUNCTION_NAME)?.text ?: return@mapNotNull null
                val returnType = function.returnTypeText() ?: "void"
                name to returnType
            }
            .toMap()

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
                CpElements.CAST_EXPRESSION -> expression.directChild(CpElements.TYPE_REFERENCE)?.text
                CpElements.UNARY_EXPRESSION -> infer(expression.firstExpression())
                CpElements.BINARY_EXPRESSION -> inferBinary(expression)
                CpElements.LAMBDA_EXPRESSION -> expression.lambdaType()
                else -> null
            }
        }

        private fun inferCall(expression: PsiElement): String? {
            val callee = expression.directChild(CpElements.NAME_EXPRESSION)?.text ?: return null
            return functions[callee]
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
            if (listOf("==", "!=", "<=", ">=", "<", ">", "&&", "||").any { it in operatorText }) {
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

        private fun PsiElement.lambdaType(): String {
            val parameterTypes = directChild(CpElements.PARAMETER_LIST)
                ?.directChildren(CpElements.PARAMETER)
                ?.map { it.directChild(CpElements.TYPE_REFERENCE)?.text ?: "unknown" }
                .orEmpty()
            val returnType = returnTypeText() ?: "unknown"
            return "f(${parameterTypes.joinToString(", ")}) -> $returnType"
        }
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
}
