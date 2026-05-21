package org.cp.lang.clion

import com.intellij.psi.PsiElement
import com.intellij.psi.PsiReference
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.util.Pass
import com.intellij.refactoring.listeners.RefactoringElementListener
import com.intellij.refactoring.rename.RenamePsiElementProcessor
import com.intellij.usageView.UsageInfo

class CpRenameProcessor : RenamePsiElementProcessor() {
    override fun canProcessElement(element: PsiElement): Boolean =
        element is CpNamedElement && element.cpElementType() in CpNavigationKinds.declarationTypes

    @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
    override fun substituteElementToRename(element: PsiElement, editor: Editor?): PsiElement {
        val source = element.cpNavigationElement() ?: return element
        return CpSemanticDeclarationResolver.resolveNow(source, editor)
            ?: source.cpNavigationTargetElement()
            ?: element
    }

    override fun substituteElementToRename(element: PsiElement, editor: Editor, renameCallback: Pass<in PsiElement>) {
        renameCallback.pass(substituteElementToRename(element, editor))
    }

    @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
    override fun findReferences(element: PsiElement, searchInComments: Boolean): Collection<PsiReference> {
        val declaration = element.cpRenameTargetElement() ?: return emptyList()
        val file = declaration.containingFile ?: return emptyList()
        return CpNavigationKinds.referenceTypes
            .flatMap { file.descendants(it) }
            .mapNotNull { it.reference }
            .filter { it.isReferenceTo(declaration) }
    }

    override fun renameElement(
        element: PsiElement,
        newName: String,
        usages: Array<UsageInfo>,
        listener: RefactoringElementListener?,
    ) {
        val declaration = element.cpRenameTargetElement() ?: element
        for (usage in usages) {
            val reference = usage.reference ?: continue
            if (reference.element == declaration) {
                continue
            }
            reference.handleElementRename(newName)
        }

        if (declaration is CpNamedElement) {
            declaration.renameTo(newName)
        }
        listener?.elementRenamed(declaration)
    }
}

private fun PsiElement.cpRenameTargetElement(): PsiElement? =
    cpNavigationTargetElement()
        ?: cpNavigationElement()?.let { CpSemanticDeclarationResolver.resolveNow(it, null) }
