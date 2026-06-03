package org.cp.lang.clion

import com.intellij.execution.ExecutionException
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.execution.configurations.RuntimeConfigurationError
import com.intellij.mock.MockProject
import com.intellij.openapi.Disposable
import com.intellij.testFramework.LightVirtualFile
import org.jdom.Element
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
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
        assertEquals("cp 源文件", CpFileType.INSTANCE.description)
    }

    @Test
    fun pluginRegistersBuildBeforeRunTaskOnCurrentExtensionPoint() {
        val pluginXml = Files.readString(pluginXmlPath())

        assertTrue(
            pluginXml.contains("""<stepsBeforeRunProvider implementation="org.cp.lang.clion.CpBuildBeforeRunTaskProvider" />"""),
        )
        assertTrue(
            pluginXml.contains("""<projectTaskRunner implementation="org.cp.lang.clion.CpProjectTaskRunner" id="CpProjectTaskRunner" />"""),
        )
        assertTrue(
            pluginXml.contains("""<buildConfigurationProvider id="CpBuildConfigurationProvider" implementation="org.cp.lang.clion.CpBuildConfigurationProvider" />"""),
        )
        assertTrue(pluginXml.contains("""<module name="intellij.cidr.execution" />"""))
        assertTrue(pluginXml.contains("""<module name="intellij.cidr.runner" />"""))
        assertTrue(pluginXml.contains("""<plugin id="com.intellij.nativeDebug" />"""))
        assertFalse(pluginXml.contains("<beforeRunTaskProvider "))
        assertFalse(pluginXml.contains("CpRunToolbarBuildAction"))
        assertFalse(pluginXml.contains("RunToolbarMainActionGroup"))
    }

    @Test
    fun fileTypeOverriderClaimsCpExtension() {
        val overrider = CpFileTypeOverrider()

        assertEquals(CpFileType.INSTANCE, overrider.getOverriddenFileType(LightVirtualFile("sample.cp")))
        assertNull(overrider.getOverriddenFileType(LightVirtualFile("sample.cpp")))
    }

    @Test
    fun helperReportsLexerErrors() {
        val diagnostics = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "broken.cp",
                files = listOf(
                    CpInspectionFile(
                        path = "broken.cp",
                        text = "\"unterminated\n",
                    ),
                ),
            ),
        ).diagnostics
        assertTrue(
            diagnostics.any { it.code == "unterminated_string_literal" && it.message == "unterminated string literal" },
        )
    }

    @Test
    fun helperReportsTargetSemanticFacts() {
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

        val mainDiagnostics = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "main.cp",
                files = listOf(
                    CpInspectionFile("math.cp", mathSource),
                    CpInspectionFile("main.cp", mainSource),
                ),
            ),
        ).diagnostics
        assertTrue(mainDiagnostics.any { it.code == "unexported_name" })
        assertTrue(mainDiagnostics.any { it.code == "missing_return" })

        val providerDiagnostics = CpHelperRunner.inspect(
            CpInspectionRequest(
                activeFile = "math.cp",
                files = listOf(
                    CpInspectionFile("math.cp", mathSource),
                    CpInspectionFile("main.cp", mainSource),
                ),
            ),
        ).diagnostics
        assertTrue(providerDiagnostics.any { it.code == "unexported_name" && it.startOffset == mathSource.indexOf("add") })
    }

    @Test
    fun externalAnnotatorOnlyPresentsCachedResults() {
        val result = CpInspectionResult(
            accepted = false,
            diagnostics = emptyList(),
            highlights = listOf(CpHelperHighlight("function.declaration", 0, 4)),
        )

        assertEquals(result, CpExternalAnnotator().doAnnotate(CpExternalAnnotator.Request(result)))
        assertNull(CpExternalAnnotator().doAnnotate(null))
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
    fun inspectionSummariesKeepDiagnosticsAndTruncateNavigation() {
        val result = CpInspectionResult(
            accepted = false,
            diagnostics = listOf(
                CpHelperDiagnostic(
                    stage = "parse",
                    code = "expected_token",
                    message = "expected token",
                    severity = "error",
                    startOffset = 1,
                    endOffset = 3,
                    line = 1,
                    column = 2,
                ),
                CpHelperDiagnostic(
                    stage = "semantic",
                    code = "unknown_name",
                    message = "unknown name",
                    severity = "error",
                    startOffset = 8,
                    endOffset = 12,
                    line = 2,
                    column = 1,
                ),
            ),
            highlights = emptyList(),
            navigation = (0 until 10).map { index ->
                CpHelperNavigation(
                    category = "reference$index",
                    sourceStartOffset = index,
                    sourceEndOffset = index + 1,
                    targetFile = "main.cp",
                    targetStartOffset = index + 2,
                    targetEndOffset = index + 3,
                )
            },
        )

        assertEquals("[expected_token@1:3,unknown_name@8:12]", result.diagnosticSummary())
        assertEquals(
            "[reference0@0:1->main.cp:2:3,reference1@1:2->main.cp:3:4," +
                "reference2@2:3->main.cp:4:5,reference3@3:4->main.cp:5:6," +
                "reference4@4:5->main.cp:6:7,reference5@5:6->main.cp:7:8," +
                "reference6@6:7->main.cp:8:9,reference7@7:8->main.cp:9:10,...]",
            result.navigationSummary(),
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
    fun helperAcceptsStdSortForArrayDirectAndUfcsCalls() {
        val repoRoot = Path.of(System.getProperty("cp.repo.root") ?: error("cp.repo.root is not configured"))
        val source = """
            import std;

            main() -> i32
            {
                let a: [i32; 7] = [5,1,4,3,2,1,2];
                let b: [i32; 7] = a;
                a.sort();
                sort(b);
                return 0;
            }
        """.trimIndent()
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = "/project/main.cp",
            activeText = source,
            importRoots = listOf(repoRoot.resolve("std").toString()),
            projectFiles = listOf(CpSnapshotSource("/project/main.cp", source)),
        )

        val result = CpHelperRunner.inspect(request)

        assertTrue(result.diagnostics.joinToString("\n") { "${it.code}: ${it.message}" }, result.diagnostics.isEmpty())
        assertTrue(result.accepted)
        assertHighlight(source, result.highlights, "sort", "member.function.call")
        assertHighlight(source, result.highlights, "sort", "function.call")
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
    fun runSourceClosureCacheInvalidatesWhenImportsChange() {
        val root = Files.createTempDirectory("cp-run-sources")
        try {
            val project = root.resolve("project")
            val main = project.resolve("main.cp")
            val math = project.resolve("math.cp")
            writeSource(main, "import math;\nmain() -> i32 { return add(); }\n")
            writeSource(math, "export module math;\nexport add() -> i32 { return 1; }\n")

            val first = CpRunPaths.resolveSourceClosure(main, listOf(project))
                .map { root.relativize(it).toString() }

            writeSource(main, "main() -> i32 { return 0; }\n")
            val second = CpRunPaths.resolveSourceClosure(main, listOf(project))
                .map { root.relativize(it).toString() }

            assertTrue(first.contains("project/main.cp"))
            assertTrue(first.contains("project/math.cp"))
            assertTrue(second.contains("project/main.cp"))
            assertFalse(second.contains("project/math.cp"))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runSourceClosureFailsWhenResolverCannotBuildRequest() {
        assertThrows(ExecutionException::class.java) {
            CpRunPaths.resolveSourceClosure(Path.of("/path/that/does/not/exist/main.cp"), emptyList())
        }
    }

    @Test
    fun buildCommandLineIncludesCompileOptionsBeforeSources() {
        val commandLine = buildCpBuildCommandLine(
            compiler = Path.of("/tool/cp"),
            sources = listOf(Path.of("/project/main.cp"), Path.of("/project/math.cp")),
            compileOptions = "--release --clang-arg -O0",
            executable = Path.of("/tmp/cp run/main"),
            workingDirectory = Path.of("/project"),
        )

        assertEquals("/tool/cp", commandLine.exePath)
        assertEquals(
            listOf("--release", "--clang-arg", "-O0", "/project/main.cp", "/project/math.cp", "-o", "/tmp/cp run/main"),
            commandLine.parametersList.list,
        )
    }

    @Test
    fun runCommandLineParsesProgramArgumentsAndRuntimeEnvironment() {
        val input = Files.createTempFile("cp-run-input", ".txt")
        try {
            val commandLine = buildCpRunCommandLine(
                executable = Path.of("/tmp/cp-run/main"),
                programArguments = """alpha "two words"""",
                workingDirectory = Path.of("/project"),
                envs = mapOf("CASE" to "1"),
                passParentEnvs = false,
                inputFile = input.toFile(),
                emulateTerminal = false,
            )

            assertEquals("/tmp/cp-run/main", commandLine.exePath)
            assertEquals(listOf("alpha", "two words"), commandLine.parametersList.list)
            assertEquals("1", commandLine.environment["CASE"])
            assertFalse(commandLine.isPassParentEnvironment)
            assertEquals(input.toFile(), commandLine.inputFile)
        } finally {
            Files.deleteIfExists(input)
        }
    }

    @Test
    fun runCommandLineCanRequestPtyTerminal() {
        val commandLine = buildCpRunCommandLine(
            executable = Path.of("/tmp/cp-run/main"),
            programArguments = "",
            workingDirectory = Path.of("/project"),
            emulateTerminal = true,
        )

        assertEquals("/tmp/cp-run/main", commandLine.exePath)
        assertTrue(commandLine is com.intellij.execution.configurations.PtyCommandLine)
    }

    @Test
    fun runConfigurationEditorRoundTripsPrimaryFields() {
        val configuration = newCpRunConfiguration()
        configuration.mainFile = "/project/main.cp"
        configuration.compilerPath = "/tool/cp"
        configuration.compileOptions = "--release"
        configuration.programArguments = "alpha beta"
        configuration.emulateTerminal = true
        @Suppress("UNCHECKED_CAST")
        val editor = configuration.configurationEditor as com.intellij.openapi.options.SettingsEditor<CpRunConfiguration>

        editor.resetFrom(configuration)
        editor.applyTo(configuration)

        assertEquals("/project/main.cp", configuration.mainFile)
        assertEquals("/tool/cp", configuration.compilerPath)
        assertEquals("--release", configuration.compileOptions)
        assertEquals("alpha beta", configuration.programArguments)
        assertTrue(configuration.emulateTerminal)
        assertNotNull(editor.component)
    }

    @Test
    fun runConfigurationValidationAcceptsExplicitCompilerAndWorkingDirectory() {
        val root = Files.createTempDirectory("cp-run-config")
        try {
            val source = root.resolve("main.cp")
            val compiler = root.resolve("cp")
            writeSource(source, "main() -> i32 { return 0; }\n")
            Files.writeString(compiler, "#!/usr/bin/env sh\nexit 0\n")
            compiler.toFile().setExecutable(true)

            val configuration = newCpRunConfiguration()
            configuration.mainFile = source.toString()
            configuration.compilerPath = compiler.toString()
            configuration.workingDirectory = root.toString()
            configuration.compileOptions = "--release"
            configuration.programArguments = """alpha "two words""""

            configuration.checkConfiguration()
            assertEquals(compiler.toAbsolutePath().normalize(), CpRunPaths.resolveCompiler(configuration.project, compiler.toString(), source))
            assertEquals(root.toAbsolutePath().normalize(), CpRunPaths.resolveWorkingDirectory(configuration.project, root.toString(), source))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runPathsDiscoverRuntimeAndStdlibBesideCompilerAndSource() {
        val root = Files.createTempDirectory("cp-run-paths")
        try {
            val project = root.resolve("project")
            val tool = root.resolve("tool/bin/cp")
            val runtime = root.resolve("tool/bin/libcp_runtime.a")
            val source = project.resolve("main.cp")
            val stdlib = project.resolve("std")
            writeSource(source, "main() -> i32 { return 0; }\n")
            Files.createDirectories(tool.parent)
            Files.writeString(tool, "#!/usr/bin/env sh\nexit 0\n")
            Files.writeString(runtime, "")
            Files.createDirectories(stdlib)
            tool.toFile().setExecutable(true)

            val configuration = newCpRunConfiguration()
            configuration.mainFile = source.toString()
            configuration.compilerPath = tool.toString()

            assertEquals(runtime, CpRunPaths.resolveRuntimeLibrary(tool))
            assertEquals(stdlib.toAbsolutePath().normalize(), CpRunPaths.resolveStdlibRoot(configuration.project, source, tool))
        } finally {
            Files.walk(root).use { stream ->
                stream.sorted(Comparator.reverseOrder()).forEach(Files::deleteIfExists)
            }
        }
    }

    @Test
    fun runConfigurationPersistsClionSingleFileFields() {
        val configuration = newCpRunConfiguration()
        configuration.mainFile = "/project/main.cp"
        configuration.compilerPath = "/tool/cp"
        configuration.compileOptions = "--release"
        configuration.workingDirectory = "/project"
        configuration.programArguments = "1 2"
        configuration.envs = mutableMapOf("A" to "B")
        configuration.passParentEnvs = false
        configuration.redirectInput = true
        configuration.redirectInputPath = "/project/input.txt"
        configuration.emulateTerminal = true

        val element = Element("configuration")
        configuration.writeExternal(element)
        val copy = newCpRunConfiguration()
        copy.readExternal(element)

        assertEquals("/project/main.cp", copy.mainFile)
        assertEquals("/tool/cp", copy.compilerPath)
        assertEquals("--release", copy.compileOptions)
        assertEquals("/project", copy.workingDirectory)
        assertEquals("1 2", copy.programArguments)
        assertEquals(mapOf("A" to "B"), copy.envs)
        assertFalse(copy.passParentEnvs)
        assertTrue(copy.redirectInput)
        assertEquals("/project/input.txt", copy.redirectInputPath)
        assertTrue(copy.emulateTerminal)
    }

    @Test
    fun runConfigurationDefaultBuildTaskIsAddedOnce() {
        val configuration = newCpRunConfiguration()

        configuration.ensureBuildBeforeRunTask()
        configuration.ensureBuildBeforeRunTask()

        assertEquals(1, configuration.beforeRunTasks.count { it.providerId == CpBuildBeforeRunTaskProvider.ID })
    }

    @Test
    fun runConfigurationRedirectInputOptionsAcceptNullablePath() {
        val configuration = newCpRunConfiguration()
        val options = configuration.inputRedirectOptions

        options.setRedirectInputPath("/project/input.txt")
        assertEquals("/project/input.txt", configuration.redirectInputPath)
        assertEquals("/project/input.txt", options.redirectInputPath)

        options.setRedirectInputPath(null)
        assertEquals("", configuration.redirectInputPath)
        assertNull(options.redirectInputPath)
    }

    @Test
    fun legacyRunConfigurationMigratesBuildTaskOnce() {
        val element = Element("configuration")
        val configuration = newCpRunConfiguration()

        configuration.readExternal(element)
        assertEquals(1, configuration.beforeRunTasks.count { it.providerId == CpBuildBeforeRunTaskProvider.ID })
        configuration.beforeRunTasks = emptyList()
        val migratedElement = Element("configuration")
        configuration.writeExternal(migratedElement)

        val copy = newCpRunConfiguration()
        copy.readExternal(migratedElement)
        assertEquals(0, copy.beforeRunTasks.count { it.providerId == CpBuildBeforeRunTaskProvider.ID })
    }

    @Test
    fun redirectInputValidationRequiresExistingFile() {
        val error = assertThrows(RuntimeConfigurationError::class.java) {
            resolveRedirectInputFile(true, "/path/that/does/not/exist/input.txt")
        }

        assertTrue(error.localizedMessage.orEmpty().contains("重定向输入文件不存在"))
    }

    @Test
    fun stableBuildOutputPathDoesNotUseProjectDirectory() {
        val project = Path.of("/project")
        val source = project.resolve("src/main.cp")
        val output = CpRunPaths.resolveBuildOutput(project, source)

        assertFalse(output.startsWith(project))
        assertTrue(output.toString().contains("cp-run"))
    }

    @Test
    fun sourceImportRootsDoNotClimbIntoParentWorkspace() {
        val workspace = Path.of("/home/user/code")
        val project = workspace.resolve("cp")
        val source = project.resolve("lab/parser/main.cp")
        val stdlib = project.resolve("std")

        val roots = cpSourceImportRoots(
            source = source,
            projectBasePath = project.toString(),
            stdlibRoot = stdlib,
        )

        assertEquals(
            listOf(
                source.parent.toAbsolutePath().normalize(),
                project.toAbsolutePath().normalize(),
                stdlib.toAbsolutePath().normalize(),
            ),
            roots,
        )
        assertFalse(roots.contains(workspace.toAbsolutePath().normalize()))
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
                assertHighlight(text, result.highlights, "kw_void", "enum.case")
                assertHighlight(text, result.highlights, "eof", "enum.case")
            }
        }
    }

    private fun pluginXmlPath(): Path =
        sequenceOf(
            Path.of("src/main/resources/META-INF/plugin.xml"),
            Path.of("clion-plugin/src/main/resources/META-INF/plugin.xml"),
        ).first { Files.isRegularFile(it) }

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

    private fun newCpRunConfiguration(): CpRunConfiguration {
        val project = MockProject(null, Disposable {})
        val factory = CpRunConfigurationType().configurationFactories.single()
        return CpRunConfiguration(project, factory, "main")
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
