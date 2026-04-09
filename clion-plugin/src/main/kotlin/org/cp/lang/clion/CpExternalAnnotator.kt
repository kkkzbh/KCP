package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.ExternalAnnotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

class CpExternalAnnotator : ExternalAnnotator<CpExternalAnnotator.Request, List<CpHelperDiagnostic>>() {
    data class Request(
        val filename: String,
        val text: String,
    )

    override fun collectInformation(file: PsiFile): Request? = buildRequest(file, file.text)

    override fun collectInformation(file: PsiFile, editor: Editor, hasErrors: Boolean): Request? =
        buildRequest(file, editor.document.text)

    override fun doAnnotate(collectedInfo: Request?): List<CpHelperDiagnostic> =
        collectedInfo?.let { CpHelperRunner.analyze(it.filename, it.text) }.orEmpty()

    override fun apply(file: PsiFile, annotationResult: List<CpHelperDiagnostic>?, holder: AnnotationHolder) {
        if (annotationResult.isNullOrEmpty()) {
            return
        }

        val textLength = file.textLength
        for (diagnostic in annotationResult) {
            val start = diagnostic.startOffset.coerceIn(0, textLength)
            val end = diagnostic.endOffset.coerceIn(start, textLength).let { boundedEnd ->
                when {
                    boundedEnd > start -> boundedEnd
                    start < textLength -> start + 1
                    else -> continue
                }
            }

            val severity = if (diagnostic.severity == "warning") {
                HighlightSeverity.WARNING
            } else {
                HighlightSeverity.ERROR
            }

            holder.newAnnotation(severity, diagnostic.message)
                .range(TextRange(start, end))
                .create()
        }
    }

    private fun buildRequest(file: PsiFile, text: String): Request? {
        if (file.language != CpLanguage) {
            return null
        }

        return Request(
            filename = file.virtualFile?.name ?: file.name,
            text = text,
        )
    }
}
