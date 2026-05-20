package org.cp.lang.clion

import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.testFramework.LightVirtualFile
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CpIntegrationTest {
    @Test
    fun fileTypeMetadataMatchesPluginContract() {
        assertEquals("cp", CpLanguage.id)
        assertEquals("cp", CpFileType.INSTANCE.name)
        assertEquals("cp", CpFileType.INSTANCE.defaultExtension)
        assertEquals("cp source file", CpFileType.INSTANCE.description)
    }

    @Test
    fun fileTypeOverriderClaimsCpExtension() {
        val overrider = CpFileTypeOverrider()

        assertEquals(CpFileType.INSTANCE, overrider.getOverriddenFileType(LightVirtualFile("sample.cp")))
        assertNull(overrider.getOverriddenFileType(LightVirtualFile("sample.cpp")))
    }

    @Test
    fun externalAnnotatorReportsLexerErrors() {
        val request = CpExternalAnnotator.Request(
            inspection = CpInspectionRequest(
                activeFile = "broken.cp",
                files = listOf(
                    CpInspectionFile(
                        path = "broken.cp",
                        text = "\"unterminated\n",
                    ),
                ),
            ),
        )

        val diagnostics = CpExternalAnnotator().doAnnotate(request).diagnostics
        assertTrue(
            diagnostics.any { it.code == "unterminated_string_literal" && it.message == "unterminated string literal" },
        )
    }

    @Test
    fun diagnosticPresentationKeepsNativeStageAndCode() {
        val diagnostic = CpHelperDiagnostic(
            stage = "semantic",
            code = "unknown_module",
            message = "unknown module 'missing'",
            severity = "error",
            startOffset = 0,
            endOffset = 7,
            line = 1,
            column = 1,
        )

        assertEquals(HighlightSeverity.ERROR, CpExternalAnnotator.diagnosticSeverity(diagnostic.severity))
        assertEquals("unknown module 'missing' [semantic:unknown_module]", CpExternalAnnotator.diagnosticMessage(diagnostic))
        assertEquals(HighlightSeverity.WARNING, CpExternalAnnotator.diagnosticSeverity("warning"))
        assertEquals(HighlightSeverity.INFORMATION, CpExternalAnnotator.diagnosticSeverity("info"))
    }

    @Test
    fun inspectOffsetsAreNormalizedForUnicodeText() {
        val text = "// 导入方式\n\"unterminated\n"
        val result = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "unicode.cp",
                files = listOf(CpInspectionFile("unicode.cp", text)),
            ),
        )
        val diagnostic = result.diagnostics
            .single { it.code == "unterminated_string_literal" }

        assertEquals("\"unterminated", text.substring(diagnostic.startOffset, diagnostic.endOffset))
        assertEquals(2, diagnostic.line)
        assertEquals(1, diagnostic.column)
    }

    @Test
    fun projectSnapshotUsesActiveDocumentText() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = "import math;\nmain() { return value(); }\n",
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "main() { return 1; }\n"),
                CpSnapshotSource("/project/math.cp", "export module math;\nvalue() -> i32 { return 2; }\n"),
                CpSnapshotSource("/project/unused.cp", "export module unused;\n"),
            ),
        )

        assertEquals("/project/main.cp", request.activeFile)
        assertEquals("import math;\nmain() { return value(); }\n", request.files.single { it.path == "/project/main.cp" }.text)
        assertEquals(2, request.files.size)
        assertFalse(request.files.any { it.path == "/project/unused.cp" })
    }

    @Test
    fun projectSnapshotFollowsSelectedTargetClosure() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/lib.cp",
            activeText = "export module lib;\nvalue() -> i32 { return 3; }\n",
            targetPath = "/project/main.cp",
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "import lib;\nmain() -> i32 { return value(); }\n"),
                CpSnapshotSource("/project/lib.cp", "export module lib;\nvalue() -> i32 { return 1; }\n"),
                CpSnapshotSource("/project/other.cp", "export module other;\n"),
            ),
        )

        assertEquals("/project/lib.cp", request.activeFile)
        assertEquals(2, request.files.size)
        assertEquals("export module lib;\nvalue() -> i32 { return 3; }\n", request.files.single { it.path == "/project/lib.cp" }.text)
        assertFalse(request.files.any { it.path == "/project/other.cp" })
    }

    @Test
    fun projectSnapshotResolvesStdlibModulePrefixFromImportRoot() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = "import std.span;\nmain() -> i32 { return 0; }\n",
            targetPath = "/project/main.cp",
            importRoots = listOf("/stdlib"),
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "import std.span;\nmain() -> i32 { return 0; }\n"),
                CpSnapshotSource("/stdlib/span.cp", "export module std.span;\nimport std.iter;\nimport std.detail.runtime;\n"),
                CpSnapshotSource("/stdlib/iter.cp", "export module std.iter;\n"),
                CpSnapshotSource("/stdlib/detail/runtime.cp", "export module std.detail.runtime;\n"),
                CpSnapshotSource("/stdlib/ranges.cp", "export module std.ranges;\n"),
            ),
        )

        assertTrue(request.files.any { it.path == "/stdlib/span.cp" })
        assertTrue(request.files.any { it.path == "/stdlib/iter.cp" })
        assertTrue(request.files.any { it.path == "/stdlib/detail/runtime.cp" })
        assertFalse(request.files.any { it.path == "/stdlib/ranges.cp" })
    }

    @Test
    fun inspectReportsSemanticDiagnosticsAndHighlights() {
        val result = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "main.cp",
                files = listOf(
                    CpInspectionFile(
                        path = "main.cp",
                        text = "import missing;\nmain() { return; }\n",
                    ),
                ),
            ),
        )

        assertTrue(result.diagnostics.any { it.stage == "semantic" && it.code == "unknown_module" })
        assertTrue(result.highlights.any { it.category == "import.name" })
        assertTrue(result.highlights.any { it.category == "function.declaration" })
    }
}
