package org.cp.lang.clion

import com.intellij.lang.documentation.DocumentationMarkup
import com.intellij.model.Pointer
import com.intellij.openapi.util.Key
import com.intellij.platform.backend.documentation.DocumentationResult
import com.intellij.platform.backend.documentation.DocumentationTarget
import com.intellij.platform.backend.documentation.DocumentationTargetProvider
import com.intellij.platform.backend.documentation.PsiDocumentationTargetProvider
import com.intellij.platform.backend.presentation.TargetPresentation
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.SmartPointerManager
import com.intellij.psi.SmartPsiElementPointer

class CpDocumentationProvider : DocumentationTargetProvider, PsiDocumentationTargetProvider {
    override fun documentationTargets(file: PsiFile, offset: Int): List<DocumentationTarget> {
        if (file.language != CpLanguage || file.textLength == 0) {
            return emptyList()
        }
        val resolvedOffset = offset.coerceIn(0, file.textLength - 1)
        val element = file.findElementAt(resolvedOffset) ?: return emptyList()
        return documentationTarget(element, element)?.let(::listOf).orEmpty()
    }

    override fun documentationTarget(element: PsiElement, originalElement: PsiElement?): DocumentationTarget? {
        if (element.language != CpLanguage) {
            return null
        }
        val target = CpDocumentationEngine.target(element, originalElement) ?: return null
        return CpPsiDocumentationTarget(SmartPointerManager.createPointer(target.element))
    }
}

object CpDocumentationEngine {
    fun quickInfo(element: PsiElement, originalElement: PsiElement? = null): String? =
        target(element, originalElement)?.documentation?.definition

    fun documentation(element: PsiElement, originalElement: PsiElement? = null): String? {
        val target = target(element, originalElement)?.documentation ?: return null
        val body = buildString {
            if (target.sections.isNotEmpty()) {
                append(DocumentationMarkup.SECTIONS_START)
                for (section in target.sections) {
                    append(DocumentationMarkup.SECTION_HEADER_START)
                    append(escape(section.title))
                    append(DocumentationMarkup.SECTION_SEPARATOR)
                    append(escape(section.content).replace("\n", "<br/>"))
                    append(DocumentationMarkup.SECTION_END)
                }
                append(DocumentationMarkup.SECTIONS_END)
            }
        }
        return DocumentationMarkup.DEFINITION_START +
            escape(target.definition) +
            DocumentationMarkup.DEFINITION_END +
            body
    }

    fun target(element: PsiElement, originalElement: PsiElement? = null): CpResolvedDocumentation? {
        val resolved = originalElement?.reference?.resolve() ?: originalElement?.parent?.reference?.resolve()
        val candidate = resolved ?: element.reference?.resolve() ?: element.parent?.reference?.resolve() ?: element
        val documentation = when (candidate.cpElementType()) {
            CpElements.FUNCTION_NAME -> candidate.parentByType(CpElements.FUNCTION)?.cachedDoc { functionDoc() }
            CpElements.TYPE_NAME -> candidate.typeDoc()
            CpElements.FIELD_DECLARATION -> candidate.parentByType(CpElements.STRUCT_FIELD)?.cachedDoc { fieldDoc() }
            CpElements.MEMBER_NAME -> candidate.reference?.resolve()?.parentByType(CpElements.STRUCT_FIELD)?.cachedDoc { fieldDoc() }
            CpElements.GENERIC_PARAMETER_NAME -> candidate.cachedDoc { CpDocTarget("generic $text", emptyList()) }
            else -> when (candidate.cpElementType()) {
                CpElements.FUNCTION -> candidate.cachedDoc { functionDoc() }
                CpElements.STRUCT_FIELD -> candidate.cachedDoc { fieldDoc() }
                else -> null
            }
        }
        return documentation?.let { CpResolvedDocumentation(candidate, it) }
    }

    private fun PsiElement.functionDoc(): CpDocTarget? {
        val name = directChild(CpElements.FUNCTION_NAME)?.text ?: return null
        val parameters = directChild(CpElements.PARAMETER_LIST)
            ?.directChildren(CpElements.PARAMETER)
            ?.joinToString(", ") { it.text }
            .orEmpty()
        val generic = directChild(CpElements.GENERIC_PARAMETER_LIST)?.text.orEmpty()
        val returnType = returnTypeText() ?: "void"
        val requires = directChild(CpElements.REQUIRES_CLAUSE)?.text
        val sections = buildList {
            if (requires != null) {
                add(CpDocSection("requires", requires))
            }
        }
        return CpDocTarget("$name$generic($parameters) -> $returnType", sections)
    }

    private fun PsiElement.typeDoc(): CpDocTarget? {
        val declaration = when (parent?.cpElementType()) {
            CpElements.STRUCT_DECLARATION,
            CpElements.ENUM_DECLARATION,
            CpElements.VARIANT_DECLARATION,
            CpElements.CONCEPT_DECLARATION,
            CpElements.TYPE_ALIAS -> parent
            else -> reference?.resolve()?.parent
        } ?: return null
        return when (declaration.cpElementType()) {
            CpElements.STRUCT_DECLARATION -> declaration.cachedDoc { structDoc() }
            CpElements.ENUM_DECLARATION -> declaration.cachedDoc { enumDoc() }
            CpElements.VARIANT_DECLARATION -> declaration.cachedDoc { variantDoc() }
            CpElements.CONCEPT_DECLARATION -> declaration.cachedDoc { conceptDoc() }
            CpElements.TYPE_ALIAS -> declaration.cachedDoc { typeAliasDoc() }
            else -> null
        }
    }

    private fun PsiElement.structDoc(): CpDocTarget {
        val name = directChild(CpElements.TYPE_NAME)?.text.orEmpty()
        val fields = directChildren(CpElements.STRUCT_FIELD)
            .mapNotNull { field ->
                val fieldName = field.directChild(CpElements.FIELD_DECLARATION)?.text ?: return@mapNotNull null
                val type = field.directChild(CpElements.TYPE_REFERENCE)?.text ?: return@mapNotNull null
                "$fieldName: $type"
            }
        return CpDocTarget("struct $name", listOf(CpDocSection("fields", fields.joinToString("\n"))))
    }

    private fun PsiElement.enumDoc(): CpDocTarget {
        val name = directChild(CpElements.TYPE_NAME)?.text.orEmpty()
        val cases = directChildren(CpElements.ENUM_CASE).mapNotNull { it.directChild(CpElements.ENUM_CASE_NAME)?.text }
        return CpDocTarget("enum $name", listOf(CpDocSection("cases", cases.joinToString("\n"))))
    }

    private fun PsiElement.variantDoc(): CpDocTarget {
        val name = directChild(CpElements.TYPE_NAME)?.text.orEmpty()
        val cases = directChildren(CpElements.VARIANT_CASE).map { it.text.trim().removeSuffix(";") }
        return CpDocTarget("variant $name", listOf(CpDocSection("cases", cases.joinToString("\n"))))
    }

    private fun PsiElement.conceptDoc(): CpDocTarget {
        val name = directChild(CpElements.TYPE_NAME)?.text.orEmpty()
        val associatedTypes = directChildren(CpElements.TYPE_ALIAS).mapNotNull { it.directChild(CpElements.TYPE_NAME)?.text }
        val functions = directChildren(CpElements.FUNCTION).mapNotNull { it.functionDoc()?.definition }
        val requires = directChildren(CpElements.REQUIRES_CLAUSE).map { it.text }
        val sections = buildList {
            if (associatedTypes.isNotEmpty()) {
                add(CpDocSection("associated types", associatedTypes.joinToString("\n")))
            }
            if (functions.isNotEmpty()) {
                add(CpDocSection("requirements", functions.joinToString("\n")))
            }
            if (requires.isNotEmpty()) {
                add(CpDocSection("requires", requires.joinToString("\n")))
            }
        }
        return CpDocTarget("concept $name", sections)
    }

    private fun PsiElement.typeAliasDoc(): CpDocTarget {
        val name = directChild(CpElements.TYPE_NAME)?.text.orEmpty()
        val target = directChild(CpElements.TYPE_REFERENCE)?.text ?: "opaque"
        return CpDocTarget("type $name = $target", emptyList())
    }

    private fun PsiElement.fieldDoc(): CpDocTarget? {
        val name = directChild(CpElements.FIELD_DECLARATION)?.text ?: return null
        val type = directChild(CpElements.TYPE_REFERENCE)?.text ?: return null
        return CpDocTarget("$name: $type", emptyList())
    }

    private fun PsiElement.returnTypeText(): String? {
        val parameterList = directChild(CpElements.PARAMETER_LIST) ?: return directChild(CpElements.TYPE_REFERENCE)?.text
        return children
            .dropWhile { it != parameterList }
            .drop(1)
            .firstOrNull { it.cpElementType() == CpElements.TYPE_REFERENCE }
            ?.text
    }

    private fun escape(text: String): String =
        text
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")

    private fun PsiElement.cachedDoc(build: PsiElement.() -> CpDocTarget?): CpDocTarget? {
        val stamp = containingFile?.modificationStamp ?: docModificationStampFallback
        getUserData(docCacheKey)?.takeIf { it.modificationStamp == stamp }?.let {
            return it.target
        }

        val target = build()
        putUserData(docCacheKey, CachedDocTarget(stamp, target))
        return target
    }

    private val docCacheKey = Key.create<CachedDocTarget>("org.cp.lang.clion.documentation.target")
    private const val docModificationStampFallback = -1L
}

data class CpDocTarget(
    val definition: String,
    val sections: List<CpDocSection>,
)

data class CpDocSection(
    val title: String,
    val content: String,
)

data class CpResolvedDocumentation(
    val element: PsiElement,
    val documentation: CpDocTarget,
)

private class CpPsiDocumentationTarget(
    private val elementPointer: SmartPsiElementPointer<PsiElement>,
) : DocumentationTarget {
    override fun createPointer(): Pointer<out DocumentationTarget> =
        Pointer.delegatingPointer(elementPointer) { element ->
            CpPsiDocumentationTarget(SmartPointerManager.createPointer(element))
        }

    override fun computePresentation(): TargetPresentation {
        val text = elementPointer.element?.let { CpDocumentationEngine.quickInfo(it) }.orEmpty()
        return TargetPresentation.builder(text).presentation()
    }

    override fun computeDocumentationHint(): String? =
        elementPointer.element?.let { CpDocumentationEngine.quickInfo(it) }

    override fun computeDocumentation(): DocumentationResult? =
        elementPointer.element
            ?.let { CpDocumentationEngine.documentation(it) }
            ?.let(DocumentationResult::documentation)
}

private data class CachedDocTarget(
    val modificationStamp: Long,
    val target: CpDocTarget?,
)
