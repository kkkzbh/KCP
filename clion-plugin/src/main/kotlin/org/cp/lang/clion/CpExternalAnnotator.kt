package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.ExternalAnnotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.project.DumbAware
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

class CpExternalAnnotator : ExternalAnnotator<CpExternalAnnotator.Request, CpInspectionResult?>(), DumbAware {
    data class Request(
        val result: CpInspectionResult,
    )

    private data class HighlightAnnotationKey(
        val category: String,
        val startOffset: Int,
        val endOffset: Int,
    )

    override fun collectInformation(file: PsiFile): Request? = buildRequest(file)

    override fun collectInformation(file: PsiFile, editor: Editor, hasErrors: Boolean): Request? =
        buildRequest(file)

    override fun doAnnotate(collectedInfo: Request?): CpInspectionResult? =
        collectedInfo?.result

    override fun apply(file: PsiFile, annotationResult: CpInspectionResult?, holder: AnnotationHolder) {
        if (annotationResult == null) {
            return
        }

        val textLength = file.textLength
        val capturesByRange = annotationResult.captures.associateBy {
            it.referenceStartOffset to it.referenceEndOffset
        }
        for (highlight in annotationResult.highlights.distinctBy(::highlightAnnotationKey)) {
            val key = highlightKey(highlight.category) ?: continue
            val start = highlight.startOffset.coerceIn(0, textLength)
            val end = highlight.endOffset.coerceIn(start, textLength)
            if (end == start) {
                continue
            }
            val capture = capturesByRange[start to end]
            val builder = if (capture == null) {
                holder.newSilentAnnotation(HighlightSeverity.INFORMATION)
            } else {
                holder.newAnnotation(HighlightSeverity.INFORMATION, captureMessage(capture))
            }
            builder.range(TextRange(start, end))
                .textAttributes(key)
                .create()
        }
    }

    private fun highlightAnnotationKey(highlight: CpHelperHighlight): HighlightAnnotationKey =
        HighlightAnnotationKey(
            category = highlight.category,
            startOffset = highlight.startOffset,
            endOffset = highlight.endOffset,
        )

    private fun buildRequest(file: PsiFile): Request? {
        if (file.language != CpLanguage) {
            return null
        }
        return CpSemanticCache.get(file.project).presentation(file)?.let(::Request)
    }

    private fun captureMessage(capture: CpHelperCapture): String {
        val escape = if (capture.escaped) {
            ", escaping ${capture.reason}"
        } else {
            ", non-escaping"
        }
        val mutation = if (capture.mutated) {
            ", mutable"
        } else {
            ", read-only"
        }
        return "capture ${capture.name}: ${capture.mode}$escape$mutation"
    }

    private fun highlightKey(category: String): TextAttributesKey? = when (category) {
        "function.declaration" -> CpSyntaxHighlighter.FUNCTION_DECLARATION
        "function.call" -> CpSyntaxHighlighter.FUNCTION_CALL
        "function.reference" -> CpSyntaxHighlighter.FUNCTION_CALL
        "constructor.declaration" -> CpSyntaxHighlighter.CONSTRUCTOR
        "destructor.declaration" -> CpSyntaxHighlighter.DESTRUCTOR
        "member.function.call", "member.function.reference" -> CpSyntaxHighlighter.MEMBER_FUNCTION
        "member.function.declaration" -> CpSyntaxHighlighter.MEMBER_FUNCTION
        "associated.function.call", "associated.function.reference" -> CpSyntaxHighlighter.ASSOCIATED_FUNCTION
        "associated.function.declaration" -> CpSyntaxHighlighter.ASSOCIATED_FUNCTION
        "builtin.function.call" -> CpSyntaxHighlighter.BUILTIN_FUNCTION
        "function.type" -> CpSyntaxHighlighter.FUNCTION_TYPE
        "parameter.declaration", "parameter.reference" -> CpSyntaxHighlighter.PARAMETER
        "parameter.pack.reference" -> CpSyntaxHighlighter.PARAMETER
        "pattern.binding" -> CpSyntaxHighlighter.PARAMETER
        "local.declaration", "local.reference" -> CpSyntaxHighlighter.LOCAL_VARIABLE
        "constant.declaration", "constant.reference" -> CpSyntaxHighlighter.LOCAL_CONSTANT
        "lambda.capture.reference",
        "lambda.capture.const_ref",
        "lambda.capture.ref",
        "lambda.capture.copy",
        "lambda.capture.owned_mut_copy" -> CpSyntaxHighlighter.LAMBDA_CAPTURE
        "type" -> CpSyntaxHighlighter.TYPE
        "type.parameter", "type.parameter.pack", "self.type" -> CpSyntaxHighlighter.TYPE_PARAMETER
        "type.alias.declaration", "opaque.type.declaration" -> CpSyntaxHighlighter.TYPE_ALIAS
        "associated.type.declaration", "associated.type.requirement", "associated.type.reference" ->
            CpSyntaxHighlighter.ASSOCIATED_TYPE
        "concept.declaration", "concept.reference" -> CpSyntaxHighlighter.CONCEPT
        "concept.function.requirement" -> CpSyntaxHighlighter.FUNCTION_DECLARATION
        "module.name", "import.name" -> CpSyntaxHighlighter.MODULE_NAME
        "lambda.marker", "decltype" -> CpSyntaxHighlighter.DECLARATION_KEYWORD
        "loop.label", "loop.label.reference" -> CpSyntaxHighlighter.LABEL
        "field.declaration", "field.reference" -> CpSyntaxHighlighter.FIELD
        "variant.case" -> CpSyntaxHighlighter.VARIANT_CASE
        "enum.case" -> CpSyntaxHighlighter.ENUM_CASE
        "literal", "number.literal" -> CpSyntaxHighlighter.NUMBER
        "boolean.literal" -> CpSyntaxHighlighter.BOOLEAN
        "null.literal" -> CpSyntaxHighlighter.NULL_LITERAL
        "string.literal" -> CpSyntaxHighlighter.STRING
        "character.literal" -> CpSyntaxHighlighter.CHARACTER
        "operator" -> CpSyntaxHighlighter.OPERATOR
        else -> null
    }

    companion object {
        internal fun emptyInspectionResult(): CpInspectionResult =
            CpInspectionResult(
                accepted = false,
                diagnostics = emptyList(),
                highlights = emptyList(),
            )

        internal fun diagnosticSeverity(severity: String): HighlightSeverity = when (severity.lowercase()) {
            "warning" -> HighlightSeverity.WARNING
            "information", "info" -> HighlightSeverity.INFORMATION
            else -> HighlightSeverity.ERROR
        }

        internal fun diagnosticMessage(diagnostic: CpHelperDiagnostic): String {
            val details = listOf(diagnostic.stage, diagnostic.code)
                .filter { it.isNotBlank() }
                .joinToString(":")
            return if (details.isBlank()) {
                diagnostic.message
            } else {
                "${diagnostic.message} [$details]"
            }
        }
    }
}
