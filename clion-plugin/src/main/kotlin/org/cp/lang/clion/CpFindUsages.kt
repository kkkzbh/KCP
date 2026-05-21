package org.cp.lang.clion

import com.intellij.find.findUsages.FindUsagesHandler
import com.intellij.find.findUsages.FindUsagesHandlerFactory
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
        psiElement.cpUsageTargetElement() is CpNamedElement

    override fun getHelpId(psiElement: PsiElement): String? = null

    override fun getType(element: PsiElement): String =
        when (element.cpUsageTargetElement()?.cpElementType()) {
            CpElements.FUNCTION_NAME -> "function"
            CpElements.PARAMETER_NAME -> "parameter"
            CpElements.LOCAL_DECLARATION -> "local"
            CpElements.TYPE_NAME -> "type"
            CpElements.FIELD_DECLARATION -> "field"
            CpElements.MODULE_NAME -> "module"
            else -> "symbol"
        }

    override fun getDescriptiveName(element: PsiElement): String =
        element.cpUsageTargetElement()?.text ?: element.text

    override fun getNodeText(element: PsiElement, useFullName: Boolean): String =
        element.cpUsageTargetElement()?.text ?: element.text
}

class CpFindUsagesHandlerFactory : FindUsagesHandlerFactory() {
    override fun canFindUsages(element: PsiElement): Boolean =
        element.cpUsageTargetElement() is CpNamedElement

    override fun createFindUsagesHandler(element: PsiElement, forHighlightUsages: Boolean): FindUsagesHandler? =
        element.cpUsageTargetElement()?.let { CpFindUsagesHandler(it) }
}

private class CpFindUsagesHandler(element: PsiElement) : FindUsagesHandler(element)

private fun PsiElement.cpUsageTargetElement(): PsiElement? =
    cpNavigationTargetElement()?.takeIf { it.cpElementType() in CpNavigationKinds.declarationTypes }
