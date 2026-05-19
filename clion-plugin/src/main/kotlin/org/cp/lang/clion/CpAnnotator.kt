package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.Annotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.psi.PsiElement

class CpAnnotator : Annotator {
    override fun annotate(element: PsiElement, holder: AnnotationHolder) {
        when (element.cpElementType()) {
            CpElements.FUNCTION_NAME -> annotate(element, holder, CpSyntaxHighlighter.FUNCTION_DECLARATION)
            CpElements.PARAMETER_NAME -> annotate(element, holder, CpSyntaxHighlighter.PARAMETER)
            CpElements.MATCH_BINDING -> annotate(element, holder, CpSyntaxHighlighter.PARAMETER)
            CpElements.LOCAL_DECLARATION -> annotate(element, holder, CpSyntaxHighlighter.LOCAL_VARIABLE)
            CpElements.TYPE_NAME -> annotate(element, holder, CpSyntaxHighlighter.TYPE)
            CpElements.GENERIC_PARAMETER_NAME -> annotate(element, holder, CpSyntaxHighlighter.TYPE_PARAMETER)
            CpElements.FIELD_DECLARATION -> annotate(element, holder, CpSyntaxHighlighter.FIELD)
            CpElements.MEMBER_NAME -> annotateMemberName(element, holder)
            CpElements.VARIANT_CASE_NAME -> annotate(element, holder, CpSyntaxHighlighter.VARIANT_CASE)
            CpElements.MODULE_NAME, CpElements.IMPORT_NAME -> annotate(element, holder, CpSyntaxHighlighter.MODULE_NAME)
            CpElements.LOOP_LABEL -> annotate(element, holder, CpSyntaxHighlighter.LABEL)
            CpElements.CALL_EXPRESSION -> annotateCall(element, holder)
            CpElements.NAME_EXPRESSION -> annotateNameReference(element, holder)
        }
    }

    private fun annotateCall(element: PsiElement, holder: AnnotationHolder) {
        val member = element.descendants(CpElements.MEMBER_NAME).lastOrNull()
        if (member != null) {
            annotate(member, holder, CpSyntaxHighlighter.MEMBER_FUNCTION)
            return
        }

        val associated = element.descendants(CpElements.ASSOCIATED_NAME).lastOrNull()
        if (associated != null) {
            annotate(associated, holder, CpSyntaxHighlighter.ASSOCIATED_FUNCTION)
            return
        }

        val callee = element.firstDescendant(CpElements.NAME_EXPRESSION) ?: return
        annotate(callee, holder, CpSyntaxHighlighter.FUNCTION_CALL)
    }

    private fun annotateMemberName(element: PsiElement, holder: AnnotationHolder) {
        if (element.parentByType(CpElements.CALL_EXPRESSION) != null) {
            return
        }
        annotate(element, holder, CpSyntaxHighlighter.FIELD)
    }

    private fun annotateNameReference(element: PsiElement, holder: AnnotationHolder) {
        if (element.parent?.cpElementType() == CpElements.CALL_EXPRESSION) {
            return
        }
        val function = element.parentByType(CpElements.FUNCTION) ?: return
        val name = element.text
        val key = when {
            function.descendants(CpElements.PARAMETER_NAME).any { it.text == name } -> CpSyntaxHighlighter.PARAMETER
            function.descendants(CpElements.LOCAL_DECLARATION).any { it.text == name } -> CpSyntaxHighlighter.LOCAL_VARIABLE
            else -> return
        }
        annotate(element, holder, key)
    }

    private fun annotate(element: PsiElement, holder: AnnotationHolder, key: TextAttributesKey) {
        if (element.textRange.isEmpty) {
            return
        }
        holder.newSilentAnnotation(HighlightSeverity.INFORMATION)
            .range(element.textRange)
            .textAttributes(key)
            .create()
    }
}
