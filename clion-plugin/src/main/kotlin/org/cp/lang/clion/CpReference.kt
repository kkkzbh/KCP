package org.cp.lang.clion

import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiReferenceBase

class CpReference(
    element: PsiElement,
) : PsiReferenceBase<PsiElement>(element, TextRange(0, element.textLength)) {
    override fun getCanonicalText(): String = element.text

    override fun resolve(): PsiElement? =
        cpResolveDeclarationForReference(element)

    override fun isReferenceTo(element: PsiElement): Boolean =
        (resolve() ?: CpSemanticDeclarationResolver.resolveNow(this.element, null))?.let { resolved ->
            val target = element.cpNavigationTargetElement() ?: element
            resolved == target || resolved.manager.areElementsEquivalent(resolved, target)
        } == true

    override fun handleElementRename(newElementName: String): PsiElement {
        val current = element
        if (current is CpNamedElement) {
            return current.renameTo(newElementName)
        }
        return current
    }
}
