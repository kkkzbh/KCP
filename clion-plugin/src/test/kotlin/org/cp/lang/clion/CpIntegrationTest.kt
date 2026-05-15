package org.cp.lang.clion

import com.intellij.testFramework.LightVirtualFile
import org.junit.Assert.assertEquals
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
            activeText = "main() { return 2; }\n",
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "main() { return 1; }\n"),
                CpSnapshotSource("/project/math.cp", "export module math;\n"),
            ),
        )

        assertEquals("/project/main.cp", request.activeFile)
        assertEquals("main() { return 2; }\n", request.files.single { it.path == "/project/main.cp" }.text)
        assertEquals(2, request.files.size)
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
