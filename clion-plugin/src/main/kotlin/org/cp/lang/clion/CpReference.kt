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
            ?: if (isExplicitReferenceTargetQuery()) cpResolveDeclarationForReference(element) else null

    override fun isReferenceTo(element: PsiElement): Boolean {
        val resolved = resolve()
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

    private fun isExplicitReferenceTargetQuery(): Boolean {
        val stack = Thread.currentThread().stackTrace
        if (stack.any { it.className.startsWith("com.intellij.psi.search.") }) {
            return false
        }
        return stack.any { frame ->
            frame.className == "com.intellij.find.actions.SearchTargetVariantsDataRuleKt" ||
                frame.className == "com.intellij.codeInsight.TargetElementUtil"
        }
    }
}
