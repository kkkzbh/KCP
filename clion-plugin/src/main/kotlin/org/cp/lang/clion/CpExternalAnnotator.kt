package org.cp.lang.clion

import com.intellij.lang.annotation.AnnotationHolder
import com.intellij.lang.annotation.ExternalAnnotator
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.project.DumbAware
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

class CpExternalAnnotator : ExternalAnnotator<CpExternalAnnotator.Request, CpInspectionResult>(), DumbAware {
    data class Request(
        val inspection: CpInspectionRequest,
        val project: Project? = null,
        val modificationCount: Long = -1,
    )

    override fun collectInformation(file: PsiFile): Request? = buildRequest(file, file.text)

    override fun collectInformation(file: PsiFile, editor: Editor, hasErrors: Boolean): Request? =
        buildRequest(file, editor.document.text)

    override fun doAnnotate(collectedInfo: Request?): CpInspectionResult =
        collectedInfo?.let { request ->
            CpHelperRunner.inspectOrNull(request.inspection)?.also { result ->
                request.project?.let { project ->
                    CpSemanticCache.get(project).store(request.inspection, result, request.modificationCount)
                }
            } ?: emptyInspectionResult()
        } ?: CpInspectionResult(
            accepted = false,
            diagnostics = emptyList(),
            highlights = emptyList(),
        )

    override fun apply(file: PsiFile, annotationResult: CpInspectionResult?, holder: AnnotationHolder) {
        if (annotationResult == null) {
            return
        }

        val textLength = file.textLength
        for (highlight in annotationResult.highlights) {
            val key = highlightKey(highlight.category) ?: continue
            val start = highlight.startOffset.coerceIn(0, textLength)
            val end = highlight.endOffset.coerceIn(start, textLength)
            if (end == start) {
                continue
            }
            holder.newSilentAnnotation(HighlightSeverity.INFORMATION)
                .range(TextRange(start, end))
                .textAttributes(key)
                .create()
        }
    }

    private fun buildRequest(file: PsiFile, text: String): Request? {
        return CpProjectSnapshotCollector.collect(file, text)?.let {
            Request(
                inspection = it,
                project = file.project,
                modificationCount = cpModificationCount(file.project),
            )
        }
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
        "lambda.capture.reference" -> CpSyntaxHighlighter.LAMBDA_CAPTURE
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
