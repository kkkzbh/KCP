package org.cp.lang.clion

import com.intellij.ide.structureView.StructureViewBuilder
import com.intellij.ide.structureView.StructureViewModel
import com.intellij.ide.structureView.StructureViewModelBase
import com.intellij.ide.structureView.StructureViewTreeElement
import com.intellij.ide.structureView.TreeBasedStructureViewBuilder
import com.intellij.ide.structureView.impl.common.PsiTreeElementBase
import com.intellij.lang.PsiStructureViewFactory
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.util.Key
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile

class CpStructureViewFactory : PsiStructureViewFactory {
    override fun getStructureViewBuilder(psiFile: PsiFile): StructureViewBuilder? {
        if (psiFile.language != CpLanguage) {
            return null
        }
        return object : TreeBasedStructureViewBuilder() {
            override fun createStructureViewModel(editor: Editor?): StructureViewModel =
                StructureViewModelBase(psiFile, editor, CpStructureViewElement(psiFile))
                    .withSuitableClasses(CpFile::class.java, CpPsiElement::class.java, CpNamedElement::class.java)
        }
    }
}

private class CpStructureViewElement(
    element: PsiElement,
) : PsiTreeElementBase<PsiElement>(element) {
    override fun getPresentableText(): String = element?.let { CpStructureEngine.label(it) } ?: "cp 文件"

    override fun getChildrenBase(): Collection<StructureViewTreeElement> {
        val current = element ?: return emptyList()
        return CpStructureEngine.children(current)
            .map { CpStructureViewElement(it) }
    }
}

object CpStructureEngine {
    private val rootTypes = setOf(
        CpElements.MODULE_HEADER,
        CpElements.IMPORT_DECLARATION,
        CpElements.STRUCT_DECLARATION,
        CpElements.ENUM_DECLARATION,
        CpElements.VARIANT_DECLARATION,
        CpElements.CONCEPT_DECLARATION,
        CpElements.IMPL_BLOCK,
        CpElements.TYPE_ALIAS,
        CpElements.FUNCTION,
    )

    private val nestedTypes = setOf(
        CpElements.STRUCT_FIELD,
        CpElements.ENUM_CASE,
        CpElements.VARIANT_CASE,
        CpElements.TYPE_ALIAS,
        CpElements.FUNCTION,
        CpElements.REQUIRES_CLAUSE,
    )

    fun labels(file: PsiFile): List<String> {
        val stamp = file.modificationStamp
        file.getUserData(labelsCacheKey)?.takeIf { it.modificationStamp == stamp }?.let {
            return it.labels
        }

        val labels = children(file).flatMap { flattenLabels(it) }
        file.putUserData(labelsCacheKey, CachedStructureLabels(stamp, labels))
        return labels
    }

    fun label(element: PsiElement): String {
        val stamp = element.structureModificationStamp()
        element.getUserData(labelCacheKey)?.takeIf { it.modificationStamp == stamp }?.let {
            return it.label
        }

        val label = computeLabel(element)
        element.putUserData(labelCacheKey, CachedStructureLabel(stamp, label))
        return label
    }

    private fun computeLabel(element: PsiElement): String =
        when (element.cpElementType()) {
            CpElements.MODULE_HEADER -> "module ${element.directChild(CpElements.MODULE_NAME)?.text.orEmpty()}"
            CpElements.IMPORT_DECLARATION -> "import ${element.directChild(CpElements.IMPORT_NAME)?.text.orEmpty()}"
            CpElements.STRUCT_DECLARATION -> "struct ${element.directChild(CpElements.TYPE_NAME)?.text.orEmpty()}"
            CpElements.ENUM_DECLARATION -> "enum ${element.directChild(CpElements.TYPE_NAME)?.text.orEmpty()}"
            CpElements.VARIANT_DECLARATION -> "variant ${element.directChild(CpElements.TYPE_NAME)?.text.orEmpty()}"
            CpElements.CONCEPT_DECLARATION -> "concept ${element.directChild(CpElements.TYPE_NAME)?.text.orEmpty()}"
            CpElements.IMPL_BLOCK -> "impl ${element.directChild(CpElements.TYPE_REFERENCE)?.text.orEmpty()}"
            CpElements.TYPE_ALIAS -> "type ${element.directChild(CpElements.TYPE_NAME)?.text.orEmpty()}"
            CpElements.FUNCTION -> element.directChild(CpElements.FUNCTION_NAME)?.text.orEmpty()
            CpElements.STRUCT_FIELD -> fieldLabel(element)
            CpElements.ENUM_CASE -> element.directChild(CpElements.ENUM_CASE_NAME)?.text.orEmpty()
            CpElements.VARIANT_CASE -> element.text.trim().removeSuffix(";")
            CpElements.REQUIRES_CLAUSE -> element.text
            else -> "cp 文件"
        }

    fun children(element: PsiElement): List<PsiElement> {
        val stamp = element.structureModificationStamp()
        element.getUserData(childrenCacheKey)?.takeIf { it.modificationStamp == stamp }?.let {
            return it.children
        }

        val wanted = if (element is PsiFile) rootTypes else nestedTypes
        val children = element.children.filter { it.cpElementType() in wanted }
        element.putUserData(childrenCacheKey, CachedStructureChildren(stamp, children))
        return children
    }

    private fun flattenLabels(element: PsiElement): List<String> =
        listOf(label(element)) + children(element).flatMap { flattenLabels(it) }

    private fun fieldLabel(element: PsiElement): String {
        val name = element.directChild(CpElements.FIELD_DECLARATION)?.text.orEmpty()
        val type = element.directChild(CpElements.TYPE_REFERENCE)?.text
        return if (type == null) name else "$name: $type"
    }

    private fun PsiElement.structureModificationStamp(): Long =
        containingFile?.modificationStamp ?: modificationStampFallback

    private val labelsCacheKey = Key.create<CachedStructureLabels>("org.cp.lang.clion.structure.labels")
    private val labelCacheKey = Key.create<CachedStructureLabel>("org.cp.lang.clion.structure.label")
    private val childrenCacheKey = Key.create<CachedStructureChildren>("org.cp.lang.clion.structure.children")
    private const val modificationStampFallback = -1L
}

private data class CachedStructureLabels(
    val modificationStamp: Long,
    val labels: List<String>,
)

private data class CachedStructureLabel(
    val modificationStamp: Long,
    val label: String,
)

private data class CachedStructureChildren(
    val modificationStamp: Long,
    val children: List<PsiElement>,
)
