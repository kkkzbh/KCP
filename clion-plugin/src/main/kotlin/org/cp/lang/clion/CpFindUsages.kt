package org.cp.lang.clion

import com.intellij.lang.cacheBuilder.DefaultWordsScanner
import com.intellij.lang.cacheBuilder.WordsScanner
import com.intellij.lang.findUsages.FindUsagesProvider
import com.intellij.psi.PsiElement
import com.intellij.psi.tree.TokenSet

class CpFindUsagesProvider : FindUsagesProvider {
    override fun getWordsScanner(): WordsScanner =
        DefaultWordsScanner(
            CpLexer(),
            TokenSet.create(CpTypes.IDENTIFIER),
            CpTypes.COMMENT_TOKENS,
            TokenSet.create(CpTypes.STRING_LITERAL, CpTypes.CHAR_LITERAL),
        )

    override fun canFindUsagesFor(psiElement: PsiElement): Boolean =
        psiElement is CpNamedElement

    override fun getHelpId(psiElement: PsiElement): String? = null

    override fun getType(element: PsiElement): String =
        when (element.cpElementType()) {
            CpElements.FUNCTION_NAME -> "function"
            CpElements.PARAMETER_NAME -> "parameter"
            CpElements.LOCAL_DECLARATION -> "local"
            CpElements.TYPE_NAME -> "type"
            CpElements.FIELD_DECLARATION -> "field"
            CpElements.MODULE_NAME -> "module"
            else -> "symbol"
        }

    override fun getDescriptiveName(element: PsiElement): String = element.text

    override fun getNodeText(element: PsiElement, useFullName: Boolean): String = element.text
}
