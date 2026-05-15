package org.cp.lang.clion

import com.intellij.psi.PsiElement
import com.intellij.psi.tree.IElementType

fun PsiElement.cpElementType(): IElementType? = node?.elementType

fun PsiElement.firstDescendant(type: IElementType): PsiElement? {
    for (child in children) {
        if (child.cpElementType() == type) {
            return child
        }
        val nested = child.firstDescendant(type)
        if (nested != null) {
            return nested
        }
    }
    return null
}

fun PsiElement.descendants(type: IElementType): List<PsiElement> {
    val result = mutableListOf<PsiElement>()
    collectDescendants(type, result)
    return result
}

private fun PsiElement.collectDescendants(type: IElementType, result: MutableList<PsiElement>) {
    for (child in children) {
        if (child.cpElementType() == type) {
            result += child
        }
        child.collectDescendants(type, result)
    }
}

fun PsiElement.parentByType(type: IElementType): PsiElement? {
    var current = parent
    while (current != null) {
        if (current.cpElementType() == type) {
            return current
        }
        current = current.parent
    }
    return null
}
