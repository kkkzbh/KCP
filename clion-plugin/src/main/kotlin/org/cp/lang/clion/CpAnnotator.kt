package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.Annotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.project.DumbAware
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile

class CpAnnotator : Annotator, DumbAware {
    private data class DiagnosticAnnotationKey(
        val severity: String,
        val stage: String,
        val code: String,
        val message: String,
        val startOffset: Int,
        val endOffset: Int,
    )

    override fun annotate(element: PsiElement, holder: AnnotationHolder) {
        if (element is PsiFile) {
            annotateCachedDiagnostics(element, holder)
            return
        }
        if (element.textRange.isEmpty) {
            return
        }
        val key = element.containingFile?.let { CpFileSymbolIndex.get(it).annotationKeys[element] } ?: return
        holder.newSilentAnnotation(HighlightSeverity.INFORMATION)
            .range(element.textRange)
            .textAttributes(key)
            .create()
    }

    private fun annotateCachedDiagnostics(file: PsiFile, holder: AnnotationHolder) {
        if (file.language != CpLanguage) {
            return
        }
        val result = CpSemanticCache.get(file.project).presentation(file) ?: return
        val textLength = file.textLength
        for (diagnostic in result.diagnostics.distinctBy(::diagnosticAnnotationKey)) {
            val range = diagnosticRange(diagnostic, textLength) ?: continue
            holder.newAnnotation(
                CpExternalAnnotator.diagnosticSeverity(diagnostic.severity),
                CpExternalAnnotator.diagnosticMessage(diagnostic),
            )
                .range(range)
                .create()
        }
    }

    private fun diagnosticAnnotationKey(diagnostic: CpHelperDiagnostic): DiagnosticAnnotationKey =
        DiagnosticAnnotationKey(
            severity = diagnostic.severity,
            stage = diagnostic.stage,
            code = diagnostic.code,
            message = diagnostic.message,
            startOffset = diagnostic.startOffset,
            endOffset = diagnostic.endOffset,
        )

    private fun diagnosticRange(diagnostic: CpHelperDiagnostic, textLength: Int): TextRange? {
        val start = diagnostic.startOffset.coerceIn(0, textLength)
        val end = diagnostic.endOffset.coerceIn(start, textLength).let { boundedEnd ->
            when {
                boundedEnd > start -> boundedEnd
                start < textLength -> start + 1
                else -> return null
            }
        }
        return TextRange(start, end)
    }
}
