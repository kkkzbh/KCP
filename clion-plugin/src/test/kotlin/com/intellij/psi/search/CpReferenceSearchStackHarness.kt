package com.intellij.psi.search

import com.intellij.psi.PsiElement
import com.intellij.psi.PsiReference

object CpReferenceSearchStackHarness {
    fun isReferenceTo(reference: PsiReference, target: PsiElement): Boolean =
        reference.isReferenceTo(target)
}
