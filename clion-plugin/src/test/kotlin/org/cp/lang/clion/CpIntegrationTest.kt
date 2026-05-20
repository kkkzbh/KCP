package org.cp.lang.clion

import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.testFramework.LightVirtualFile
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.file.Files
import java.nio.file.Path

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

    @Test
    fun inspectReportsSemanticNavigationTargets() {
        val text = """
            struct box {
                value: i32;
            }

            impl box {
                get(self const&) -> i32
                {
                    return self.value;
                }
            }

            main() -> i32
            {
                let item = box{ .value = 1 };
                return item.get();
            }
        """.trimIndent()

        val result = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "main.cp",
                files = listOf(CpInspectionFile("main.cp", text)),
            ),
        )

        val field = result.navigation.single {
            it.category == "field.reference" &&
                it.sourceStartOffset > text.indexOf("box{") &&
                text.substring(it.sourceStartOffset, it.sourceEndOffset) == "value" &&
                text.substring(it.targetStartOffset, it.targetEndOffset) == "value"
        }
        val method = result.navigation.single {
            it.category == "member.function.call" &&
                text.substring(it.sourceStartOffset, it.sourceEndOffset) == "get" &&
                text.substring(it.targetStartOffset, it.targetEndOffset) == "get"
        }

        assertEquals("main.cp", field.targetFile)
        assertEquals("main.cp", method.targetFile)
    }

    @Test
    fun labSourcesProduceAcceptedNativeSemanticHighlights() {
        val repoRoot = Path.of(System.getProperty("cp.repo.root") ?: error("cp.repo.root is not configured"))
        val labSources = cpSourcesUnder(repoRoot.resolve("lab"))
        val stdSources = cpSourcesUnder(repoRoot.resolve("std"))
        val projectFiles = (labSources + stdSources).map { path ->
            CpInspectionFile(path.toString(), Files.readString(path))
        }

        for (labSource in labSources) {
            val text = Files.readString(labSource)
            val result = CpHelperRunner.inspect(
                CpInspectionRequest(
                    activeFile = labSource.toString(),
                    files = projectFiles,
                ),
            )

            assertTrue("${repoRoot.relativize(labSource)} should be semantically accepted", result.accepted)
            assertTrue("${repoRoot.relativize(labSource)} should not produce active diagnostics", result.diagnostics.isEmpty())
            assertTrue("${repoRoot.relativize(labSource)} should produce semantic highlights", result.highlights.isNotEmpty())
            assertNoConflictingHighlights(repoRoot, labSource, text, result.highlights)

            if (repoRoot.relativize(labSource).toString() == "lab/lexer/main.cp") {
                assertHighlight(text, result.highlights, "read_source", "function.call")
                assertHighlight(text, result.highlights, "lex", "function.call")
                assertHighlight(text, result.highlights, "println", "function.call")
                assertHighlight(text, result.highlights, "size", "member.function.call")
                assertHighlight(text, result.highlights, "tokens", "field.reference")
                assertHighlight(text, result.highlights, "kw_int", "enum.case")
                assertHighlight(text, result.highlights, "eof", "enum.case")
            }
        }
    }

    private fun cpSourcesUnder(root: Path): List<Path> =
        Files.walk(root).use { stream ->
            stream
                .filter { Files.isRegularFile(it) && it.toString().endsWith(".cp") }
                .sorted()
                .toList()
        }

    private fun assertHighlight(text: String, highlights: List<CpHelperHighlight>, fragment: String, category: String) {
        assertTrue(
            "$fragment should be highlighted as $category",
            highlights.any { highlight ->
                highlight.category == category &&
                    highlight.endOffset <= text.length &&
                    text.substring(highlight.startOffset, highlight.endOffset) == fragment
            },
        )
    }

    private fun assertNoConflictingHighlights(
        repoRoot: Path,
        source: Path,
        text: String,
        highlights: List<CpHelperHighlight>,
    ) {
        val conflicts = highlights
            .groupBy { it.startOffset to it.endOffset }
            .filterValues { group -> group.map { it.category }.distinct().size > 1 }
        assertTrue(
            buildString {
                append(repoRoot.relativize(source))
                append(" should not assign conflicting highlight categories")
                for ((range, group) in conflicts) {
                    append('\n')
                    append(text.substring(range.first, range.second))
                    append(": ")
                    append(group.map { it.category }.distinct().joinToString(", "))
                }
            },
            conflicts.isEmpty(),
        )
    }

}
