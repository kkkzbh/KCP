package org.cp.lang.clion

import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiReferenceBase

class CpReference(
    element: PsiElement,
) : PsiReferenceBase<PsiElement>(element, TextRange(0, element.textLength)) {
    override fun getCanonicalText(): String = element.text

    override fun resolve(): PsiElement? =
        CpSemanticDeclarationResolver.resolve(element, null)

    override fun isReferenceTo(element: PsiElement): Boolean {
        val resolved = resolve()
            ?: if (isExplicitReferenceCheck()) CpSemanticDeclarationResolver.resolveNow(this.element, null) else null
        return resolved?.let {
            val target = element.cpNavigationTargetElement() ?: element
            it == target || it.manager.areElementsEquivalent(it, target)
        } == true
    }

    override fun handleElementRename(newElementName: String): PsiElement {
        val current = element
        if (current is CpNamedElement) {
            return current.renameTo(newElementName)
        }
        return current
    }

    private fun isExplicitReferenceCheck(): Boolean =
        Thread.currentThread().stackTrace.any { frame ->
            frame.className.startsWith("com.intellij.psi.search.") ||
                frame.className.startsWith("com.intellij.find.findUsages.") ||
                frame.className.startsWith("com.intellij.refactoring.")
        }
}
