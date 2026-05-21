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
import java.util.Comparator

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
    fun externalAnnotatorReportsTargetSemanticFacts() {
        val mainSource = """
            import math.x;

            main() -> i32
            {
                add(1, 2);
            }
        """.trimIndent()
        val mathSource = """
            export module math.x;

            add(x: i32, y: i32) -> i32
            {
                return x + y;
            }
        """.trimIndent()

        val mainDiagnostics = CpExternalAnnotator().doAnnotate(
            CpExternalAnnotator.Request(
                inspection = CpInspectionRequest(
                    activeFile = "main.cp",
                    files = listOf(
                        CpInspectionFile("math.cp", mathSource),
                        CpInspectionFile("main.cp", mainSource),
                    ),
                ),
            ),
        ).diagnostics
        assertTrue(mainDiagnostics.any { it.code == "unexported_name" })
        assertTrue(mainDiagnostics.any { it.code == "missing_return" })

        val providerDiagnostics = CpExternalAnnotator().doAnnotate(
            CpExternalAnnotator.Request(
                inspection = CpInspectionRequest(
                    activeFile = "math.cp",
                    files = listOf(
                        CpInspectionFile("math.cp", mathSource),
                        CpInspectionFile("main.cp", mainSource),
                    ),
                ),
            ),
        ).diagnostics
        assertTrue(providerDiagnostics.any { it.code == "unexported_name" && it.startOffset == mathSource.indexOf("add") })
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
    fun projectSnapshotAlsoFollowsActiveFileClosureWhenSelectedTargetDiffers() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/parser/parser.cp",
            activeText = "export module parser;\nimport parser.state;\nparse() { let state = parser{}; }\n",
            targetPath = "/project/lexer/main.cp",
            importRoots = listOf("/project"),
            projectFiles = listOf(
                CpSnapshotSource("/project/lexer/main.cp", "import lexer;\nmain() -> i32 { return 0; }\n"),
                CpSnapshotSource("/project/lexer/lexer.cp", "export module lexer;\n"),
                CpSnapshotSource("/project/parser/parser.cp", "export module parser;\n"),
                CpSnapshotSource("/project/parser/state.cp", "export module parser.state;\nexport struct parser {}\n"),
                CpSnapshotSource("/project/unused.cp", "export module unused;\n"),
            ),
        )

        assertEquals("/project/parser/parser.cp", request.activeFile)
        assertTrue(request.files.any { it.path == "/project/lexer/main.cp" })
        assertTrue(request.files.any { it.path == "/project/lexer/lexer.cp" })
        assertTrue(request.files.any { it.path == "/project/parser/parser.cp" })
        assertTrue(request.files.any { it.path == "/project/parser/state.cp" })
        assertFalse(request.files.any { it.path == "/project/unused.cp" })
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
    fun projectSnapshotExpandsReexportedStdSubmodules() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = "import std;\nmain() -> i32 { print(\"ok\"); return 0; }\n",
            targetPath = "/project/main.cp",
            importRoots = listOf("/stdlib"),
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "import std;\nmain() -> i32 { print(\"ok\"); return 0; }\n"),
                CpSnapshotSource("/stdlib/std.cp", "export module std;\nexport import std.io;\n"),
                CpSnapshotSource("/stdlib/io.cp", "export module std.io;\nexport print(value: str) -> unit {}\n"),
            ),
        )

        assertTrue(request.files.any { it.path == "/stdlib/std.cp" })
        assertTrue(request.files.any { it.path == "/stdlib/io.cp" })
    }

    @Test
    fun projectSnapshotResolvesSiblingAggregateModuleByDeclaration() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = "import math.x;\nmain() -> i32 { return add(1, 2); }\n",
            targetPath = "/project/main.cp",
            importRoots = listOf("/project"),
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "import math.x;\nmain() -> i32 { return add(1, 2); }\n"),
                CpSnapshotSource("/project/math.cp", "export module math.x;\nexport add(x: i32, y: i32) -> i32 { return x + y; }\n"),
                CpSnapshotSource("/project/x.cp", "export module x;\n"),
            ),
        )

        assertTrue(request.files.any { it.path == "/project/math.cp" })
        assertFalse(request.files.any { it.path == "/project/x.cp" })
    }

    @Test
    fun helperResolvesPrintThroughStdReexportClosure() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = "import std;\nmain() -> i32 { print(\"ok\"); return 0; }\n",
            targetPath = "/project/main.cp",
            importRoots = listOf("/stdlib"),
            projectFiles = listOf(
                CpSnapshotSource("/project/main.cp", "import std;\nmain() -> i32 { print(\"ok\"); return 0; }\n"),
                CpSnapshotSource("/stdlib/std.cp", "export module std;\nexport import std.io;\n"),
                CpSnapshotSource("/stdlib/io.cp", "export module std.io;\nexport import std.io.format;\n"),
                CpSnapshotSource("/stdlib/io/format.cp", "export module std.io.format;\nexport print(fmt: str) -> unit {}\n"),
            ),
        )

        val result = CpHelperRunner.inspect(request)

        assertFalse(result.diagnostics.any { it.code == "unknown_name" && it.message.contains("'print'") })
        assertFalse(result.diagnostics.any { it.code == "not_callable" })
    }

    @Test
    fun projectSnapshotReadsDependencyClosureFromFilesystemWithoutIndexedProjectFiles() {
        val root = Files.createTempDirectory("cp-project-snapshot")
        try {
            val parser = root.resolve("parser/lr/parser.cp")
            val table = root.resolve("parser/lr/table.cp")
            val state = root.resolve("parser/lr/state.cp")
            writeSource(
                parser,
                """
                export module parser.lr.parser;
                import parser.lr.table;
                import parser.lr.state;
                export parse() -> parser_trace { return parse_with_trace(build_parser_tables()); }
                """.trimIndent(),
            )
            writeSource(
                table,
                """
                export module parser.lr.table;
                export struct parser_tables {}
                export build_parser_tables() -> parser_tables { return parser_tables{}; }
                """.trimIndent(),
            )
            writeSource(
                state,
                """
                export module parser.lr.state;
                import parser.lr.table;
                export struct parser_trace {}
                export parse_with_trace(tables: parser_tables const&) -> parser_trace { return parser_trace{}; }
                """.trimIndent(),
            )

            val request = CpProjectSnapshotCollector.buildRequest(
                activePath = parser.toString(),
                activeText = Files.readString(parser),
                projectFiles = emptyList(),
            )

            assertTrue(request.files.any { it.path == parser.toString() })
            assertTrue(request.files.any { it.path == table.toString() })
            assertTrue(request.files.any { it.path == state.toString() })
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun projectSnapshotResolvesImportsByExportedModuleDeclaration() {
        val root = Files.createTempDirectory("cp-project-snapshot")
        try {
            val main = root.resolve("main.cp")
            val table = root.resolve("modules/generated_table.cp")
            writeSource(
                main,
                """
                import parser.lr.table;
                main() -> i32 { return build_parser_tables(); }
                """.trimIndent(),
            )
            writeSource(
                table,
                """
                export module parser.lr.table;
                export build_parser_tables() -> i32 { return 1; }
                """.trimIndent(),
            )

            val request = CpProjectSnapshotCollector.buildRequest(
                activePath = main.toString(),
                activeText = Files.readString(main),
                projectFiles = emptyList(),
            )

            assertTrue(request.files.any { it.path == main.toString() })
            assertTrue(request.files.any { it.path == table.toString() })
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runSourceClosureLeavesStdlibImportsForCompiler() {
        val root = Files.createTempDirectory("cp-run-sources")
        try {
            val project = root.resolve("project")
            val stdlib = root.resolve("stdlib")
            writeSource(project.resolve("main.cp"), "import std;\nimport parser;\nmain() -> i32 { return parse(); }\n")
            writeSource(project.resolve("parser.cp"), "export module parser;\nimport std.collections.vector;\nparse() -> i32 { return 1; }\n")
            writeSource(stdlib.resolve("std.cp"), "export module std;\n")
            writeSource(stdlib.resolve("collections/vector.cp"), "export module std.collections.vector;\n")

            val sources = CpRunPaths.resolveSourceClosure(project.resolve("main.cp"), listOf(project, stdlib))
            val relativeSources = sources.map { root.relativize(it).toString() }

            assertTrue(relativeSources.contains("project/main.cp"))
            assertTrue(relativeSources.contains("project/parser.cp"))
            assertFalse(relativeSources.any { it.startsWith("stdlib/") })
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runSourceClosureResolvesSiblingAggregateModuleByDeclaration() {
        val root = Files.createTempDirectory("cp-run-sources")
        try {
            val project = root.resolve("project")
            writeSource(project.resolve("main.cp"), "import math.x;\nmain() -> i32 { return add(1, 2); }\n")
            writeSource(project.resolve("math.cp"), "export module math.x;\nexport add(x: i32, y: i32) -> i32 { return x + y; }\n")

            val sources = CpRunPaths.resolveSourceClosure(project.resolve("main.cp"), listOf(project))
            val relativeSources = sources.map { root.relativize(it).toString() }

            assertTrue(relativeSources.contains("project/main.cp"))
            assertTrue(relativeSources.contains("project/math.cp"))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runSourceClosureResolvesNestedFolderAggregateModules() {
        val root = Files.createTempDirectory("cp-run-sources")
        try {
            val project = root.resolve("project")
            writeSource(project.resolve("main.cp"), "import parser.lr;\nmain() -> i32 { return parse(); }\n")
            writeSource(project.resolve("parser/lr/parser.cp"), "export module parser.lr;\nparse() -> i32 { return 0; }\n")

            val sources = CpRunPaths.resolveSourceClosure(project.resolve("main.cp"), listOf(project))
            val relativeSources = sources.map { root.relativize(it).toString() }

            assertTrue(relativeSources.contains("project/parser/lr/parser.cp"))
            assertTrue(relativeSources.contains("project/main.cp"))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runSourceClosureResolvesImportsByExportedModuleDeclaration() {
        val root = Files.createTempDirectory("cp-run-sources")
        try {
            val project = root.resolve("project")
            writeSource(project.resolve("main.cp"), "import parser.lr.table;\nmain() -> i32 { return build_parser_tables(); }\n")
            writeSource(
                project.resolve("generated/table_impl.cp"),
                """
                export module parser.lr.table;
                export build_parser_tables() -> i32 { return 1; }
                """.trimIndent(),
            )

            val sources = CpRunPaths.resolveSourceClosure(project.resolve("main.cp"), listOf(project))
            val relativeSources = sources.map { root.relativize(it).toString() }

            assertTrue(relativeSources.contains("project/generated/table_impl.cp"))
            assertTrue(relativeSources.contains("project/main.cp"))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun projectSnapshotResolvesAggregateSiblingModuleDirectories() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/lab/preprocessor/preprocessor.cp",
            activeText = """
                export module preprocessor;
                import source;
                import diagnostic;
            """.trimIndent(),
            importRoots = listOf("/project/lab"),
            projectFiles = listOf(
                CpSnapshotSource(
                    "/project/lab/preprocessor/preprocessor.cp",
                    "export module preprocessor;\nimport source;\nimport diagnostic;\n",
                ),
                CpSnapshotSource("/project/lab/source/source.cp", "export module source;\n"),
                CpSnapshotSource("/project/lab/diagnostic/diagnostic.cp", "export module diagnostic;\n"),
                CpSnapshotSource("/project/lab/lexer/lexer.cp", "export module lexer;\n"),
            ),
        )

        assertTrue(request.files.any { it.path == "/project/lab/source/source.cp" })
        assertTrue(request.files.any { it.path == "/project/lab/diagnostic/diagnostic.cp" })
        assertFalse(request.files.any { it.path == "/project/lab/lexer/lexer.cp" })
    }

    @Test
    fun projectSnapshotResolvesNestedFolderAggregateModules() {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/lab/parser/lr/main.cp",
            activeText = """
                import parser.lr;
                main() -> i32 { return parse(); }
            """.trimIndent(),
            importRoots = listOf("/project/lab"),
            projectFiles = listOf(
                CpSnapshotSource(
                    "/project/lab/parser/lr/main.cp",
                    "import parser.lr;\nmain() -> i32 { return parse(); }\n",
                ),
                CpSnapshotSource("/project/lab/parser/lr/parser.cp", "export module parser.lr;\nparse() -> i32 { return 0; }\n"),
                CpSnapshotSource("/project/lab/parser/lr/lr.cp", "export module parser.lr;\nparse() -> i32 { return 1; }\n"),
            ),
        )

        assertTrue(request.files.any { it.path == "/project/lab/parser/lr/parser.cp" })
        assertFalse(request.files.any { it.path == "/project/lab/parser/lr/lr.cp" })
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

    private fun writeSource(path: Path, text: String) {
        Files.createDirectories(path.parent)
        Files.writeString(path, text)
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
