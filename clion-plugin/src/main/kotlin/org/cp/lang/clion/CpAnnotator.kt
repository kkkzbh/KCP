package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.Annotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.project.DumbAware
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile

class CpAnnotator : Annotator, DumbAware {
    override fun annotate(element: PsiElement, holder: AnnotationHolder) {
        annotateDiagnostics(element, holder)

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
            CpElements.ENUM_CASE_NAME -> annotate(element, holder, CpSyntaxHighlighter.ENUM_CASE)
            CpElements.MODULE_NAME, CpElements.IMPORT_NAME -> annotate(element, holder, CpSyntaxHighlighter.MODULE_NAME)
            CpElements.LOOP_LABEL -> annotate(element, holder, CpSyntaxHighlighter.LABEL)
            CpElements.CALL_EXPRESSION -> annotateCall(element, holder)
            CpElements.NAME_EXPRESSION -> annotateNameReference(element, holder)
        }
    }

    private fun annotateDiagnostics(element: PsiElement, holder: AnnotationHolder) {
        val file = element.containingFile ?: element as? PsiFile ?: return
        if (file.language != CpLanguage || CpInjectedFragmentPolicy.isCpMarkdownCodeFence(file)) {
            return
        }

        val cache = CpSemanticCache.get(file.project)
        val cached = cache.current(file)
        if (cached == null) {
            CpDiagnosticsTrace.info("annotator-cache-miss:${file.cpDiagnosticPath()}") {
                "cp annotator cache miss file=${file.cpDiagnosticPath()} language=${file.language.id} " +
                    "fileType=${file.fileType.name}"
            }
            cache.requestRefresh(file, file.currentEditorText())
            return
        }

        CpDiagnosticsTrace.info("annotator-cache-hit:${file.cpDiagnosticPath()}:${cached.result.diagnosticSummary()}") {
            "cp annotator cache hit file=${file.cpDiagnosticPath()} accepted=${cached.result.accepted} " +
                "diagnostics=${cached.result.diagnosticSummary()} highlights=${cached.result.highlights.size} " +
                "files=${cached.request.files.map { it.path }}"
        }

        val textLength = file.textLength
        for (diagnostic in cached.result.diagnostics) {
            val start = diagnostic.startOffset.coerceIn(0, textLength)
            val end = diagnostic.endOffset.coerceIn(start, textLength).let { boundedEnd ->
                when {
                    boundedEnd > start -> boundedEnd
                    start < textLength -> start + 1
                    else -> continue
                }
            }
            val range = TextRange(start, end)
            if (!element.isDiagnosticAnchor(range)) {
                continue
            }

            holder.newAnnotation(
                CpExternalAnnotator.diagnosticSeverity(diagnostic.severity),
                CpExternalAnnotator.diagnosticMessage(diagnostic),
            )
                .range(range)
                .create()
        }
    }

    private fun PsiFile.currentEditorText(): String =
        virtualFile?.let { FileDocumentManager.getInstance().getDocument(it)?.text } ?: text

    private fun PsiElement.isDiagnosticAnchor(range: TextRange): Boolean =
        textRange.startOffset <= range.startOffset &&
            textRange.endOffset >= range.endOffset &&
            children.none { child ->
                !child.textRange.isEmpty &&
                    child.textRange.startOffset <= range.startOffset &&
                    child.textRange.endOffset >= range.endOffset
            }

    private fun annotateCall(element: PsiElement, holder: AnnotationHolder) {
        val callee = element.children.firstOrNull {
            it.cpElementType() == CpElements.NAME_EXPRESSION ||
                it.cpElementType() == CpElements.MEMBER_EXPRESSION ||
                it.cpElementType() == CpElements.ASSOCIATED_NAME_EXPRESSION
        } ?: return

        val member = callee.directChild(CpElements.MEMBER_NAME)
        if (member != null) {
            annotate(member, holder, CpSyntaxHighlighter.MEMBER_FUNCTION)
            return
        }

        val associated = callee.directChild(CpElements.ASSOCIATED_NAME)
        if (associated != null) {
            annotate(associated, holder, CpSyntaxHighlighter.ASSOCIATED_FUNCTION)
            return
        }

        if (callee.cpElementType() == CpElements.NAME_EXPRESSION) {
            annotate(callee, holder, CpSyntaxHighlighter.FUNCTION_CALL)
        }
    }

    private fun annotateMemberName(element: PsiElement, holder: AnnotationHolder) {
        val memberExpression = element.parent?.takeIf { it.cpElementType() == CpElements.MEMBER_EXPRESSION }
        if (memberExpression?.parent?.cpElementType() == CpElements.CALL_EXPRESSION) {
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

private fun PsiFile.cpDiagnosticPath(): String =
    virtualFile?.path ?: name
