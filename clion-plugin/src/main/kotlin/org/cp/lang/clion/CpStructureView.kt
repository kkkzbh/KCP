package org.cp.lang.clion

import com.intellij.ide.structureView.StructureViewBuilder
import com.intellij.ide.structureView.StructureViewModel
import com.intellij.ide.structureView.StructureViewModelBase
import com.intellij.ide.structureView.StructureViewTreeElement
import com.intellij.ide.structureView.TreeBasedStructureViewBuilder
import com.intellij.ide.structureView.impl.common.PsiTreeElementBase
import com.intellij.lang.PsiStructureViewFactory
import com.intellij.openapi.editor.Editor
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
                    .withSuitableClasses(CpFile::class.java, CpPsiElement::class.java)
        }
    }
}

private class CpStructureViewElement(
    element: PsiElement,
) : PsiTreeElementBase<PsiElement>(element) {
    override fun getPresentableText(): String = when (element?.cpElementType()) {
        CpElements.MODULE_HEADER -> "module ${element?.firstDescendant(CpElements.MODULE_NAME)?.text.orEmpty()}"
        CpElements.IMPORT_DECLARATION -> "import ${element?.firstDescendant(CpElements.IMPORT_NAME)?.text.orEmpty()}"
        CpElements.FUNCTION -> element?.firstDescendant(CpElements.FUNCTION_NAME)?.text.orEmpty()
        else -> "cp file"
    }

    override fun getChildrenBase(): Collection<StructureViewTreeElement> {
        val current = element ?: return emptyList()
        return current.children
            .filter { child ->
                child.cpElementType() == CpElements.MODULE_HEADER ||
                    child.cpElementType() == CpElements.IMPORT_DECLARATION ||
                    child.cpElementType() == CpElements.FUNCTION
            }
            .map { CpStructureViewElement(it) }
    }
}
