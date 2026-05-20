package org.cp.lang.clion

import com.intellij.psi.PsiElement
import com.intellij.psi.PsiReference
import com.intellij.refactoring.listeners.RefactoringElementListener
import com.intellij.refactoring.rename.RenamePsiElementProcessor
import com.intellij.usageView.UsageInfo

class CpRenameProcessor : RenamePsiElementProcessor() {
    override fun canProcessElement(element: PsiElement): Boolean =
        element is CpNamedElement && element.cpElementType() in CpDeclarationResolver.declarationTypes

    override fun findReferences(element: PsiElement, searchInComments: Boolean): Collection<PsiReference> {
        val file = element.containingFile ?: return emptyList()
        return CpDeclarationResolver.referenceTypes
            .flatMap { file.descendants(it) }
            .mapNotNull { it.reference }
            .filter { it.isReferenceTo(element) }
    }

    override fun renameElement(
        element: PsiElement,
        newName: String,
        usages: Array<UsageInfo>,
        listener: RefactoringElementListener?,
    ) {
        for (usage in usages) {
            val reference = usage.reference ?: continue
            if (reference.element == element) {
                continue
            }
            reference.handleElementRename(newName)
        }

        if (element is CpNamedElement) {
            element.renameTo(newName)
        }
        listener?.elementRenamed(element)
    }
}
