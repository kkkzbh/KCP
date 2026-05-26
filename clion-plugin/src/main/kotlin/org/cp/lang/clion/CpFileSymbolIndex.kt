package org.cp.lang.clion

import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.util.Key
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile

internal data class CpSymbol(
    val name: String,
    val element: PsiElement,
    val offset: Int,
)

internal data class CpFunctionSymbol(
    val name: String,
    val element: PsiElement,
    val offset: Int,
    val returnType: String,
    val receiverType: String?,
)

internal data class CpFieldSymbol(
    val name: String,
    val element: PsiElement,
    val offset: Int,
    val type: String,
    val ownerType: String?,
)

internal data class CpFunctionScopeSymbols(
    val parameters: Set<String>,
    val locals: Set<String>,
)

internal data class CpFileSymbols(
    val modules: List<CpSymbol>,
    val imports: List<CpSymbol>,
    val parameters: List<CpSymbol>,
    val locals: List<CpSymbol>,
    val functions: List<CpFunctionSymbol>,
    val typeDeclarations: List<CpSymbol>,
    val structFields: List<CpFieldSymbol>,
    val variantCases: List<CpSymbol>,
    val enumCases: List<CpSymbol>,
    val declarationStatements: List<PsiElement>,
    val referenceElements: List<PsiElement>,
    val functionScopes: Map<PsiElement, CpFunctionScopeSymbols>,
    val annotationKeys: Map<PsiElement, TextAttributesKey>,
)

internal object CpFileSymbolIndex {
    fun get(file: PsiFile): CpFileSymbols {
        val modificationStamp = file.modificationStamp
        file.getUserData(cacheKey)?.takeIf { it.modificationStamp == modificationStamp }?.let {
            return it.symbols
        }

        return CpSymbolCollector().collect(file).also { symbols ->
            file.putUserData(cacheKey, CpCachedFileSymbols(modificationStamp, symbols))
        }
    }

    private val cacheKey = Key.create<CpCachedFileSymbols>("org.cp.lang.clion.fileSymbolIndex")
}

private data class CpCachedFileSymbols(
    val modificationStamp: Long,
    val symbols: CpFileSymbols,
)

private class CpSymbolCollector {
    private val modules = mutableListOf<CpSymbol>()
    private val imports = mutableListOf<CpSymbol>()
    private val parameters = mutableListOf<CpSymbol>()
    private val locals = mutableListOf<CpSymbol>()
    private val functions = mutableListOf<CpFunctionSymbol>()
    private val typeDeclarations = mutableListOf<CpSymbol>()
    private val structFields = mutableListOf<CpFieldSymbol>()
    private val variantCases = mutableListOf<CpSymbol>()
    private val enumCases = mutableListOf<CpSymbol>()
    private val declarationStatements = mutableListOf<PsiElement>()
    private val referenceElements = mutableListOf<PsiElement>()
    private val mutableFunctionScopes = linkedMapOf<PsiElement, MutableFunctionScopeSymbols>()
    private val annotationKeys = linkedMapOf<PsiElement, TextAttributesKey>()
    private val nameReferencesByFunction = linkedMapOf<PsiElement, MutableList<PsiElement>>()

    fun collect(file: PsiFile): CpFileSymbols {
        visit(file, currentFunction = null, currentImplReceiver = null, currentStruct = null)
        collectScopedNameAnnotations()
        return CpFileSymbols(
            modules = modules,
            imports = imports,
            parameters = parameters,
            locals = locals,
            functions = functions,
            typeDeclarations = typeDeclarations,
            structFields = structFields,
            variantCases = variantCases,
            enumCases = enumCases,
            declarationStatements = declarationStatements,
            referenceElements = referenceElements,
            functionScopes = mutableFunctionScopes.mapValues { (_, scope) ->
                CpFunctionScopeSymbols(
                    parameters = scope.parameters,
                    locals = scope.locals,
                )
            },
            annotationKeys = annotationKeys,
        )
    }

    private fun visit(element: PsiElement, currentFunction: PsiElement?, currentImplReceiver: String?, currentStruct: String?) {
        val type = element.cpElementType()
        val nextImplReceiver = if (type == CpElements.IMPL_BLOCK) {
            element.directChild(CpElements.TYPE_REFERENCE)?.typeName()?.baseType()
        } else {
            currentImplReceiver
        }
        val nextStruct = if (type == CpElements.STRUCT_DECLARATION) {
            element.directChild(CpElements.TYPE_NAME)?.text
        } else {
            currentStruct
        }
        val nextFunction = if (type == CpElements.FUNCTION) element else currentFunction

        collectElement(element, currentFunction, nextImplReceiver, nextStruct)

        for (child in element.children) {
            visit(
                element = child,
                currentFunction = nextFunction,
                currentImplReceiver = nextImplReceiver,
                currentStruct = nextStruct,
            )
        }
    }

    private fun collectElement(element: PsiElement, currentFunction: PsiElement?, currentImplReceiver: String?, currentStruct: String?) {
        when (element.cpElementType()) {
            CpElements.FUNCTION_NAME -> annotate(element, CpSyntaxHighlighter.FUNCTION_DECLARATION)
            CpElements.PARAMETER_NAME -> {
                annotate(element, CpSyntaxHighlighter.PARAMETER)
                parameters += element.symbol()
                currentFunction?.scope()?.parameters?.add(element.text)
            }
            CpElements.MATCH_BINDING -> annotate(element, CpSyntaxHighlighter.PARAMETER)
            CpElements.LOCAL_DECLARATION -> {
                annotate(element, CpSyntaxHighlighter.LOCAL_VARIABLE)
                locals += element.symbol()
                currentFunction?.scope()?.locals?.add(element.text)
            }
            CpElements.TYPE_NAME -> {
                annotate(element, CpSyntaxHighlighter.TYPE)
                collectTypeName(element)
            }
            CpElements.GENERIC_PARAMETER_NAME -> annotate(element, CpSyntaxHighlighter.TYPE_PARAMETER)
            CpElements.FIELD_DECLARATION -> {
                annotate(element, CpSyntaxHighlighter.FIELD)
                collectField(element, currentStruct)
            }
            CpElements.MEMBER_NAME -> {
                collectMemberNameAnnotation(element)
                referenceElements += element
            }
            CpElements.VARIANT_CASE_NAME -> {
                annotate(element, CpSyntaxHighlighter.VARIANT_CASE)
                variantCases += element.symbol()
                referenceElements += element
            }
            CpElements.ENUM_CASE_NAME -> {
                annotate(element, CpSyntaxHighlighter.ENUM_CASE)
                enumCases += element.symbol()
            }
            CpElements.MODULE_NAME -> {
                annotate(element, CpSyntaxHighlighter.MODULE_NAME)
                modules += element.symbol()
            }
            CpElements.IMPORT_NAME -> {
                annotate(element, CpSyntaxHighlighter.MODULE_NAME)
                imports += element.symbol()
                referenceElements += element
            }
            CpElements.FUNCTION -> collectFunction(element, currentImplReceiver)
            CpElements.DECLARATION_STATEMENT -> declarationStatements += element
            CpElements.LOOP_LABEL -> {
                annotate(element, CpSyntaxHighlighter.LABEL)
                referenceElements += element
            }
            CpElements.CALL_EXPRESSION -> collectCallAnnotation(element)
            CpElements.NAME_EXPRESSION -> {
                currentFunction?.let { function ->
                    if (element.parent?.cpElementType() != CpElements.CALL_EXPRESSION) {
                        nameReferencesByFunction.getOrPut(function) { mutableListOf() } += element
                    }
                }
                referenceElements += element
            }
            in CpNavigationKinds.referenceTypes -> referenceElements += element
        }
    }

    private fun collectFunction(element: PsiElement, currentImplReceiver: String?) {
        val name = element.directChild(CpElements.FUNCTION_NAME) ?: return
        functions += CpFunctionSymbol(
            name = name.text,
            element = name,
            offset = name.textRange.startOffset,
            returnType = element.returnTypeText() ?: "void",
            receiverType = currentImplReceiver,
        )
    }

    private fun collectTypeName(element: PsiElement) {
        if (element.parent?.cpElementType() !in typeDeclarationParents) {
            referenceElements += element
            return
        }
        if (element.parent?.directChild(CpElements.TYPE_NAME) == element) {
            typeDeclarations += element.symbol()
        }
    }

    private fun collectField(element: PsiElement, currentStruct: String?) {
        referenceElements += element
        if (element.parent?.cpElementType() != CpElements.STRUCT_FIELD) {
            return
        }
        val fieldType = element.parent?.directChild(CpElements.TYPE_REFERENCE)?.text ?: return
        structFields += CpFieldSymbol(
            name = element.text,
            element = element,
            offset = element.textRange.startOffset,
            type = fieldType,
            ownerType = currentStruct?.baseType(),
        )
    }

    private fun collectCallAnnotation(element: PsiElement) {
        val callee = element.children.firstOrNull {
            it.cpElementType() == CpElements.NAME_EXPRESSION ||
                it.cpElementType() == CpElements.MEMBER_EXPRESSION ||
                it.cpElementType() == CpElements.ASSOCIATED_NAME_EXPRESSION
        } ?: return

        val member = callee.directChild(CpElements.MEMBER_NAME)
        if (member != null) {
            annotate(member, CpSyntaxHighlighter.MEMBER_FUNCTION)
            return
        }

        val associated = callee.directChild(CpElements.ASSOCIATED_NAME)
        if (associated != null) {
            annotate(associated, CpSyntaxHighlighter.ASSOCIATED_FUNCTION)
            return
        }

        if (callee.cpElementType() == CpElements.NAME_EXPRESSION) {
            annotate(callee, CpSyntaxHighlighter.FUNCTION_CALL)
        }
    }

    private fun collectMemberNameAnnotation(element: PsiElement) {
        val memberExpression = element.parent?.takeIf { it.cpElementType() == CpElements.MEMBER_EXPRESSION }
        if (memberExpression?.parent?.cpElementType() == CpElements.CALL_EXPRESSION) {
            return
        }
        annotate(element, CpSyntaxHighlighter.FIELD)
    }

    private fun collectScopedNameAnnotations() {
        for ((function, references) in nameReferencesByFunction) {
            val names = mutableFunctionScopes[function] ?: continue
            for (reference in references) {
                val key = when (reference.text) {
                    in names.parameters -> CpSyntaxHighlighter.PARAMETER
                    in names.locals -> CpSyntaxHighlighter.LOCAL_VARIABLE
                    else -> continue
                }
                annotate(reference, key)
            }
        }
    }

    private fun annotate(element: PsiElement, key: TextAttributesKey) {
        annotationKeys[element] = key
    }

    private fun PsiElement.scope(): MutableFunctionScopeSymbols =
        mutableFunctionScopes.getOrPut(this) { MutableFunctionScopeSymbols() }

    private fun PsiElement.symbol(): CpSymbol =
        CpSymbol(
            name = text,
            element = this,
            offset = textRange.startOffset,
        )

    private fun PsiElement.returnTypeText(): String? {
        val parameterList = directChild(CpElements.PARAMETER_LIST) ?: return directChild(CpElements.TYPE_REFERENCE)?.text
        return children
            .dropWhile { it != parameterList }
            .drop(1)
            .firstOrNull { it.cpElementType() == CpElements.TYPE_REFERENCE }
            ?.text
    }

    private fun PsiElement.typeName(): String? =
        firstDescendant(CpElements.TYPE_NAME)?.text

    private fun String.baseType(): String =
        takeWhile { it.isLetterOrDigit() || it == '_' }

    private data class MutableFunctionScopeSymbols(
        val parameters: MutableSet<String> = linkedSetOf(),
        val locals: MutableSet<String> = linkedSetOf(),
    )

    private companion object {
        private val typeDeclarationParents = setOf(
            CpElements.STRUCT_DECLARATION,
            CpElements.ENUM_DECLARATION,
            CpElements.VARIANT_DECLARATION,
            CpElements.CONCEPT_DECLARATION,
            CpElements.TYPE_ALIAS,
        )
    }
}
