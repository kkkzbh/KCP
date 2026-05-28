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
        psiElement.cpUsageSourceElement() is CpNamedElement

    override fun getHelpId(psiElement: PsiElement): String? = null

    override fun getType(element: PsiElement): String =
        when (element.cpUsageSourceElement()?.cpElementType()) {
            CpElements.FUNCTION_NAME -> "函数"
            CpElements.PARAMETER_NAME -> "参数"
            CpElements.LOCAL_DECLARATION -> "局部变量"
            CpElements.TYPE_NAME -> "类型"
            CpElements.FIELD_DECLARATION -> "字段"
            CpElements.MODULE_NAME -> "模块"
            else -> "符号"
        }

    override fun getDescriptiveName(element: PsiElement): String =
        element.cpUsageSourceElement()?.text ?: element.text

    override fun getNodeText(element: PsiElement, useFullName: Boolean): String =
        element.cpUsageSourceElement()?.text ?: element.text
}

class CpFindUsagesHandlerFactory : FindUsagesHandlerFactory() {
    override fun canFindUsages(element: PsiElement): Boolean =
        element.cpUsageSourceElement() is CpNamedElement

    override fun createFindUsagesHandler(element: PsiElement, forHighlightUsages: Boolean): FindUsagesHandler? =
        element.cpResolvedUsageTargetElement()?.let { CpFindUsagesHandler(it) }
}

private class CpFindUsagesHandler(element: PsiElement) : FindUsagesHandler(element)

private fun PsiElement.cpUsageSourceElement(): PsiElement? =
    cpUsageDeclarationElement()
        ?: cpNavigationElement()?.takeIf { it.cpElementType() in CpNavigationKinds.referenceTypes }

private fun PsiElement.cpResolvedUsageTargetElement(): PsiElement? =
    cpUsageDeclarationElement()
        ?: cpNavigationElement()?.let { CpSemanticDeclarationResolver.resolveNow(it, null) }
            ?.cpUsageDeclarationElement()

private fun PsiElement.cpUsageDeclarationElement(): PsiElement? =
    cpNavigationTargetElement()
        ?.takeIf { it.cpElementType() in CpNavigationKinds.declarationTypes && it is CpNamedElement }
        ?.takeIf { it.isUsageDeclarationElement() }

private fun PsiElement.isUsageDeclarationElement(): Boolean =
    when (cpElementType()) {
        CpElements.TYPE_NAME -> isCpTypeDeclarationName()
        CpElements.FIELD_DECLARATION ->
            parent?.cpElementType() == CpElements.STRUCT_FIELD
        CpElements.VARIANT_CASE_NAME ->
            parent?.cpElementType() == CpElements.VARIANT_CASE
        CpElements.LOOP_LABEL ->
            parent?.cpElementType() == CpElements.FOR_STATEMENT ||
                parent?.cpElementType() == CpElements.TEMPLATE_FOR_STATEMENT
        else -> true
    }
