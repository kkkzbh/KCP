package org.cp.lang.clion

import com.intellij.codeInsight.completion.CompletionContributor
import com.intellij.codeInsight.completion.CompletionContributorEP
import com.intellij.codeInsight.lookup.LookupManager
import com.intellij.ide.plugins.PluginManagerCore
import com.intellij.lang.LanguageParserDefinitions
import com.intellij.openapi.actionSystem.IdeActions
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileTypes.FileTypeManager
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.search.searches.ReferencesSearch
import com.intellij.psi.TokenType
import com.intellij.psi.tree.IElementType
import com.intellij.testFramework.ExtensionTestUtil
import com.intellij.testFramework.IndexingTestUtil
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import java.nio.file.Path
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import kotlin.system.measureNanoTime
import org.junit.Assert.assertTrue

class CpEditorPerformanceTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        val pluginDescriptor = PluginManagerCore.getPlugin(PluginManagerCore.CORE_ID)
            ?: error("platform plugin descriptor is unavailable")
        CompletionContributor.EP.point.registerExtension(
            CompletionContributorEP(
                CpLanguage.id,
                CpCompletionContributor::class.java.name,
                pluginDescriptor,
            ),
            testRootDisposable,
        )
        ExtensionTestUtil.addExtensions(ReferencesSearch.EP_NAME, listOf(CpReferencesSearch()), testRootDisposable)
        ApplicationManager.getApplication().runWriteAction {
            FileTypeManager.getInstance().associateExtension(CpFileType.INSTANCE, "cp")
        }
    }

    override fun tearDown() {
        try {
            ApplicationManager.getApplication().runWriteAction {
                FileTypeManager.getInstance().removeAssociatedExtension(CpFileType.INSTANCE, "cp")
            }
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testSymbolHeavyRelexingStaysWithinTypingBudget() {
        val text = symbolHeavyText(repetitions = 1_200)
        repeat(3) {
            lexTokenCount(text)
        }

        val runs = 8
        val elapsedMillis = measureMillis {
            repeat(runs) {
                lexTokenCount(text)
            }
        }
        val averageMillis = elapsedMillis.toDouble() / runs.toDouble()
        println("CP_EDITOR_PERF lexer_relex_avg_ms=$averageMillis text_chars=${text.length} runs=$runs")

        assertTrue(
            "symbol-heavy lexer pass averaged ${"%.2f".format(averageMillis)}ms; expected < 25ms",
            averageMillis < 25.0,
        )
    }

    fun testFixtureTypingStaysWithinInteractiveBudget() {
        myFixture.configureByText(CpFileType.INSTANCE, editingText(functionCount = 320))

        val chars = 80
        val elapsedMillis = measureMillis {
            repeat(chars) {
                myFixture.type("x")
            }
        }
        val averageMillis = elapsedMillis.toDouble() / chars.toDouble()
        println("CP_EDITOR_PERF fixture_typing_avg_ms=$averageMillis chars=$chars text_chars=${myFixture.editor.document.textLength}")

        assertTrue(
            "fixture typing averaged ${"%.2f".format(averageMillis)}ms per char; expected < 15ms",
            averageMillis < 15.0,
        )
    }

    fun testIncompleteLessThanTypingStaysWithinInteractiveBudget() {
        myFixture.configureByText(CpFileType.INSTANCE, incompleteLessThanText(functionCount = 1_200))

        val chars = 24
        val elapsedMillis = measureMillis {
            repeat(chars) {
                myFixture.type("x")
            }
        }
        val averageMillis = elapsedMillis.toDouble() / chars.toDouble()
        println(
            "CP_EDITOR_PERF incomplete_less_than_typing_avg_ms=$averageMillis " +
                "chars=$chars text_chars=${myFixture.editor.document.textLength}",
        )

        assertTrue(
            "incomplete less-than typing averaged ${"%.2f".format(averageMillis)}ms per char; expected < 20ms",
            averageMillis < 20.0,
        )
    }

    fun testTopLevelLetRepairTypingStaysWithinInteractiveBudget() {
        myFixture.configureByText(CpFileType.INSTANCE, "<caret>")
        myFixture.doHighlighting()
        IndexingTestUtil.waitUntilIndexesAreReady(project)

        val elapsedMillis = measureMillis {
            myFixture.type("let a = 2;\n")
            myFixture.type("let b = 2;\n")
            myFixture.type(";et c = 2")
            myFixture.performEditorAction(IdeActions.ACTION_EDITOR_BACKSPACE)
            myFixture.type("let c = 2")
        }
        val chars = "let a = 2;\nlet b = 2;\n;et c = 2let c = 2".length + 1
        val averageMillis = elapsedMillis.toDouble() / chars.toDouble()
        println("CP_EDITOR_PERF top_level_let_repair_typing_avg_ms=$averageMillis chars=$chars")

        assertTrue(
            "top-level let repair typing averaged ${"%.2f".format(averageMillis)}ms per char; expected < 20ms",
            averageMillis < 20.0,
        )
    }

    fun testTopLevelLetRepairCompletionStaysWithinCachedImportBudget() {
        myFixture.configureByText(CpFileType.INSTANCE, "<caret>")
        seedCompletionImportClosure(importCount = 40, functionCount = 20)
        CpCompletionEngine.items(myFixture.file, myFixture.caretOffset)

        var chars = 0
        val elapsedMillis = measureMillis {
            chars += typeWithCompletion("let a = 2;\n")
            chars += typeWithCompletion("let b = 2;\n")
            chars += typeWithCompletion(";et c = 2")
            myFixture.performEditorAction(IdeActions.ACTION_EDITOR_BACKSPACE)
            CpCompletionEngine.items(myFixture.file, myFixture.caretOffset)
            chars += 1
            chars += typeWithCompletion("let c = 2")
        }
        val averageMillis = elapsedMillis.toDouble() / chars.toDouble()
        println("CP_EDITOR_PERF top_level_let_repair_completion_avg_ms=$averageMillis chars=$chars")

        assertTrue(
            "top-level let repair completion averaged ${"%.2f".format(averageMillis)}ms per char; expected < 25ms",
            averageMillis < 25.0,
        )
    }

    fun testTopLevelLetRepairPlatformCompletionStaysWithinInteractiveBudget() {
        myFixture.configureByText(CpFileType.INSTANCE, "<caret>")
        seedCompletionImportClosure(importCount = 40, functionCount = 20)
        myFixture.type("le")
        assertTrue("platform completion should include let", "let" in platformCompletionItems())

        myFixture.configureByText(CpFileType.INSTANCE, "<caret>")
        seedCompletionImportClosure(importCount = 40, functionCount = 20)
        var invocations = 0
        val elapsedMillis = measureMillis {
            invocations += typeWithPlatformCompletion("let a = 2;\n")
            invocations += typeWithPlatformCompletion("let b = 2;\n")
            invocations += typeWithPlatformCompletion(";et c = 2")
            myFixture.performEditorAction(IdeActions.ACTION_EDITOR_BACKSPACE)
            invocations += typeWithPlatformCompletion("let c = 2")
        }
        val averageMillis = elapsedMillis.toDouble() / invocations.toDouble()
        println("CP_EDITOR_PERF top_level_let_repair_platform_completion_avg_ms=$averageMillis invocations=$invocations")

        assertTrue(
            "top-level let repair platform completion averaged ${"%.2f".format(averageMillis)}ms; expected < 120ms",
            averageMillis < 120.0,
        )
    }

    fun testLabProductionGotoUsesFastTypeNavigationWithoutHelper() {
        val file = addLabProjectFile("lab/parser/grammar.cp")
        myFixture.configureFromExistingVirtualFile(file.virtualFile)
        val productionOffset = myFixture.file.text.indexOf("vector<production>{}") + "vector<".length
        val source = myFixture.file.findElementAt(productionOffset)?.cpNavigationElement()
        assertNotNull(source)

        CpFastNavigationStats.reset()
        CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
        val target = cpResolveDeclarationForAction(source!!, myFixture.editor)

        assertNotNull(target)
        assertEquals(CpElements.TYPE_NAME, target!!.cpElementType())
        assertEquals("production", target.text)
        assertEquals(CpElements.STRUCT_DECLARATION, target.parent?.cpElementType())
        assertTrue("fast resolver should handle local type navigation", CpFastNavigationStats.snapshot().hits >= 1)
        assertEquals(0, CpSemanticAnalysisService.get(project).stats().helperInspectCalls)
    }

    fun testLabProductionFindUsagesFiltersNonTypeCandidatesWithoutHelper() {
        val grammar = addLabProjectFile("lab/parser/grammar.cp")
        addLabProjectFile("lab/parser/state.cp")
        addLabProjectFile("lab/parser/table.cp")
        myFixture.configureFromExistingVirtualFile(grammar.virtualFile)
        val target = myFixture.file.descendants(CpElements.TYPE_NAME)
            .single { it.text == "production" && it.isCpTypeDeclarationName() }
        val baselineTextCandidates = listOf(
            "lab/parser/grammar.cp",
            "lab/parser/state.cp",
            "lab/parser/table.cp",
        ).count { labSource(it).contains("production") }

        assertEquals(3, baselineTextCandidates)
        CpReferenceSearchStats.reset()
        CpFastNavigationStats.reset()
        CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
        val references = ReferencesSearch.search(target).findAll()
            .distinctBy { it.element.containingFile.virtualFile.path to it.element.textRange }
        val stats = CpReferenceSearchStats.snapshot()

        assertTrue("real grammar should expose production type references", references.size >= 2)
        assertTrue(
            "production type usages should come from grammar.cp only",
            references.all { it.element.containingFile.virtualFile.name == "grammar.cp" },
        )
        assertEquals(3, stats.textCandidateFiles)
        assertTrue("semantic candidate files should be filtered to <= 1", stats.semanticCandidateFiles <= 1)
        assertTrue("fast references should be emitted without semantic inspect", stats.fastReferences >= 2)
        assertEquals(0, CpSemanticAnalysisService.get(project).stats().helperInspectCalls)
    }

    fun testRealProjectNavigationMatrixCoversImportsTypesAndFunctionsWithoutHelper() {
        val files = addRealCpProjectSources()
        val cases = listOf(
            LabNavigationCase(
                activePath = "lab/parser/parser.cp",
                context = "import parser.table;",
                sourceText = "parser.table",
                targetPath = "lab/parser/table.cp",
                targetText = "parser.table",
                targetType = CpElements.MODULE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/ir/main.cp",
                context = "import semantic.result;",
                sourceText = "semantic.result",
                targetPath = "lab/semantic/result.cp",
                targetText = "semantic.result",
                targetType = CpElements.MODULE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/lexer/state.cp",
                context = "import lexer.keyword;",
                sourceText = "lexer.keyword",
                targetPath = "lab/lexer/keyword.cp",
                targetText = "lexer.keyword",
                targetType = CpElements.MODULE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/parser.cp",
                context = "ast: ast_arena;",
                sourceText = "ast_arena",
                targetPath = "lab/parser/ast/arena.cp",
                targetText = "ast_arena",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/grammar.cp",
                context = "template if(read_type<decltype(value)> == grammar_symbol)",
                sourceText = "grammar_symbol",
                targetPath = "lab/parser/symbol.cp",
                targetText = "grammar_symbol",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/grammar.cp",
                context = "else template if(read_type<decltype(value)> == token_kind)",
                sourceText = "token_kind",
                targetPath = "lab/lexer/token.cp",
                targetText = "token_kind",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/table.cp",
                context = ": grammar)",
                sourceText = "grammar",
                targetPath = "lab/parser/grammar.cp",
                targetText = "grammar",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/preprocessor/preprocessor.cp",
                context = ": source_file const&;",
                sourceText = "source_file",
                targetPath = "lab/source/source.cp",
                targetText = "source_file",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/lexer/state.cp",
                context = "diagnostics: diagnostic_collector;",
                sourceText = "diagnostic_collector",
                targetPath = "lab/diagnostic/collector.cp",
                targetText = "diagnostic_collector",
                targetType = CpElements.TYPE_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/parser.cp",
                context = "const static tables = build_parser_tables();",
                sourceText = "build_parser_tables",
                targetPath = "lab/parser/table.cp",
                targetText = "build_parser_tables",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/table.cp",
                context = "builder{make_minic_grammar()}",
                sourceText = "make_minic_grammar",
                targetPath = "lab/parser/grammar.cp",
                targetText = "make_minic_grammar",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/table.cp",
                context = "let target_items = go(states[state_index], symbol);",
                sourceText = "go",
                targetPath = "lab/parser/table.cp",
                targetText = "go",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/main.cp",
                context = "= lex(preprocessed);",
                sourceText = "lex",
                targetPath = "lab/lexer/lexer.cp",
                targetText = "lex",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/ir/main.cp",
                context = "let semantics = analyze_semantics(file, parsed);",
                sourceText = "analyze_semantics",
                targetPath = "lab/semantic/program.cp",
                targetText = "analyze_semantics",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "lab/parser/grammar.cp",
                context = "symbols.push_back(grammar_symbol_of(value));",
                sourceText = "grammar_symbol_of",
                targetPath = "lab/parser/grammar.cp",
                targetText = "grammar_symbol_of",
                targetType = CpElements.FUNCTION_NAME,
            ),
            LabNavigationCase(
                activePath = "std/collections/detail/btree_storage.cp",
                context = "keys: storage [K; 39];",
                sourceText = "K",
                targetPath = "std/collections/detail/btree_storage.cp",
                targetText = "K",
                targetType = CpElements.GENERIC_PARAMETER_NAME,
            ),
            LabNavigationCase(
                activePath = "std/collections/detail/btree_storage.cp",
                context = "values: storage [V; 39];",
                sourceText = "V",
                targetPath = "std/collections/detail/btree_storage.cp",
                targetText = "V",
                targetType = CpElements.GENERIC_PARAMETER_NAME,
            ),
        )

        for (case in cases) {
            myFixture.configureFromExistingVirtualFile(files.getValue(case.activePath).virtualFile)
            val source = sourceElement(case)
            CpFastNavigationStats.reset()
            CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
            var target: PsiElement? = null
            val elapsedMillis = measureMillis {
                target = cpResolveDeclarationForAction(source, myFixture.editor)
            }
            val stats = CpSemanticAnalysisService.get(project).stats()
            println(
                "CP_EDITOR_PERF lab_navigation_fast active=${case.activePath} " +
                    "source=${case.sourceText} target=${case.targetPath} elapsed_ms=$elapsedMillis " +
                    "helper_calls=${stats.helperInspectCalls}",
            )

            assertNotNull("${case.activePath}:${case.sourceText} should resolve", target)
            assertEquals(case.targetType, target!!.cpElementType())
            assertEquals(case.targetText, target!!.text)
            assertTrue(
                "${case.sourceText} should target ${case.targetPath}, got ${target!!.containingFile.virtualFile.path}",
                target!!.containingFile.virtualFile.path.endsWith(case.targetPath),
            )
            assertTrue("${case.sourceText} should use fast navigation", CpFastNavigationStats.snapshot().hits >= 1)
            assertEquals("${case.sourceText} should not call helper", 0, stats.helperInspectCalls)
        }
    }

    fun testRealLabBuildParserTablesFindUsagesFromReferenceIsFastAndNonEmpty() {
        val files = addRealCpProjectSources()
        myFixture.configureFromExistingVirtualFile(files.getValue("lab/parser/parser.cp").virtualFile)
        val source = sourceElement(
            LabNavigationCase(
                activePath = "lab/parser/parser.cp",
                context = "const static tables = build_parser_tables();",
                sourceText = "build_parser_tables",
                targetPath = "lab/parser/table.cp",
                targetText = "build_parser_tables",
                targetType = CpElements.FUNCTION_NAME,
            ),
        )
        myFixture.editor.caretModel.moveToOffset(source.textRange.startOffset)

        CpReferenceSearchStats.reset()
        CpFastNavigationStats.reset()
        CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
        assertTrue(CpFindUsagesProvider().canFindUsagesFor(source))
        val handler = CpFindUsagesHandlerFactory().createFindUsagesHandler(source, false)
        assertNotNull(handler)
        assertEquals(CpElements.FUNCTION_NAME, handler!!.psiElement.cpElementType())
        assertEquals("build_parser_tables", handler.psiElement.text)
        assertTrue(handler.psiElement.containingFile.virtualFile.path.endsWith("lab/parser/table.cp"))

        val references = ReferencesSearch.search(handler.psiElement).findAll()
            .distinctBy { it.element.containingFile.virtualFile.path to it.element.textRange }
        val referencePaths = references.map { it.element.containingFile.virtualFile.path }
        val stats = CpReferenceSearchStats.snapshot()

        assertEquals(2, references.size)
        assertTrue(referencePaths.any { it.endsWith("lab/parser/parser.cp") })
        assertTrue(referencePaths.any { it.endsWith("lab/parser/main.cp") })
        assertEquals(4, stats.textCandidateFiles)
        assertEquals(0, stats.semanticCandidateFiles)
        assertEquals(2, stats.fastReferences)
        assertEquals(0, CpSemanticAnalysisService.get(project).stats().helperInspectCalls)
    }

    fun testRealLabMakeMinicGrammarFindUsagesFromReferenceIsFastAndNonEmpty() {
        val files = addRealCpProjectSources()
        myFixture.configureFromExistingVirtualFile(files.getValue("lab/parser/table.cp").virtualFile)
        val source = sourceElement(
            LabNavigationCase(
                activePath = "lab/parser/table.cp",
                context = "builder{make_minic_grammar()}",
                sourceText = "make_minic_grammar",
                targetPath = "lab/parser/grammar.cp",
                targetText = "make_minic_grammar",
                targetType = CpElements.FUNCTION_NAME,
            ),
        )
        myFixture.editor.caretModel.moveToOffset(source.textRange.startOffset)

        CpReferenceSearchStats.reset()
        CpFastNavigationStats.reset()
        CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
        assertTrue(CpFindUsagesProvider().canFindUsagesFor(source))
        val handler = CpFindUsagesHandlerFactory().createFindUsagesHandler(source, false)
        assertNotNull(handler)
        assertEquals(CpElements.FUNCTION_NAME, handler!!.psiElement.cpElementType())
        assertEquals("make_minic_grammar", handler.psiElement.text)
        assertTrue(handler.psiElement.containingFile.virtualFile.path.endsWith("lab/parser/grammar.cp"))

        val references = ReferencesSearch.search(handler.psiElement).findAll()
            .distinctBy { it.element.containingFile.virtualFile.path to it.element.textRange }
        val referencePaths = references.map { it.element.containingFile.virtualFile.path }
        val stats = CpReferenceSearchStats.snapshot()

        assertEquals(1, references.size)
        assertTrue(referencePaths.single().endsWith("lab/parser/table.cp"))
        assertEquals(2, stats.textCandidateFiles)
        assertEquals(0, stats.semanticCandidateFiles)
        assertEquals(1, stats.fastReferences)
        assertEquals(0, CpSemanticAnalysisService.get(project).stats().helperInspectCalls)
    }

    fun testNavigationFallsBackToSemanticHelperWhenFastPathDoesNotCoverReference() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            impl box {
                get(self const&) -> i32
                {
                    return value;
                }
            }

            main() -> i32
            {
                let item = box{ .value = 1 };
                return item.<caret>get();
            }
            """.trimIndent(),
        )
        val source = myFixture.file.findElementAt(myFixture.caretOffset)?.cpNavigationElement()
        assertNotNull(source)

        CpFastNavigationStats.reset()
        CpSemanticAnalysisService.get(project).resetStats(clearCache = true)
        val target = cpResolveDeclarationForAction(source!!, myFixture.editor)

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("get", target.text)
        assertEquals(0, CpFastNavigationStats.snapshot().hits)
        assertTrue("member navigation should still use semantic helper fallback", CpSemanticAnalysisService.get(project).stats().helperInspectCalls >= 1)
    }

    private fun lexTokenCount(text: String): Int {
        val lexer = CpLexer()
        lexer.start(text)

        var count = 0
        while (true) {
            val tokenType = lexer.tokenType ?: break
            if (tokenType != TokenType.WHITE_SPACE) {
                count += 1
            }
            lexer.advance()
        }
        return count
    }

    private fun symbolHeavyText(repetitions: Int): String {
        val chunk = CpTypes.punctuators.joinToString(" ") { it.first }
        return buildString(capacity = repetitions * (chunk.length + 1)) {
            repeat(repetitions) {
                append(chunk)
                append('\n')
            }
        }
    }

    private fun editingText(functionCount: Int): String =
        buildString {
            appendLine("main() -> i32")
            appendLine("{")
            appendLine("    // typing <caret>")
            appendLine("    let total = 0;")
            repeat(functionCount) { index ->
                appendLine("    let value_$index = ((total + $index) * 3) - ($index / 2);")
            }
            appendLine("    return total;")
            appendLine("}")
        }

    private fun incompleteLessThanText(functionCount: Int): String =
        buildString {
            appendLine("main() -> i32")
            appendLine("{")
            appendLine("    let pending = value < <caret>;")
            appendLine("    return pending;")
            appendLine("}")
            repeat(functionCount) { index ->
                appendLine()
                appendLine("helper_$index(value: i32) -> i32")
                appendLine("{")
                appendLine("    let doubled = value + $index;")
                appendLine("    return doubled;")
                appendLine("}")
            }
        }

    private fun typeWithCompletion(text: String): Int {
        for (char in text) {
            myFixture.type(char.toString())
            CpCompletionEngine.items(myFixture.file, myFixture.caretOffset)
        }
        return text.length
    }

    private fun typeWithPlatformCompletion(text: String): Int {
        var invocations = 0
        for (char in text) {
            myFixture.type(char.toString())
            if (char.isLetterOrDigit() || char == '_') {
                platformCompletionItems()
                invocations += 1
            }
        }
        return invocations
    }

    private fun platformCompletionItems(): List<String> =
        myFixture.completeBasic()
            .orEmpty()
            .map { it.lookupString }
            .also {
                LookupManager.getInstance(project).activeLookup?.hideLookup(true)
            }

    private fun seedCompletionImportClosure(importCount: Int, functionCount: Int) {
        val activePath = myFixture.file.virtualFile.path
        val files = mutableListOf(CpInspectionFile(activePath, myFixture.file.text))
        repeat(importCount) { importIndex ->
            files += CpInspectionFile(
                path = "$activePath.import_$importIndex.cp",
                text = completionImportText(importIndex, functionCount),
            )
        }
        CpSemanticCache.get(project).store(
            CpInspectionRequest(
                activeFile = activePath,
                files = files,
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )
    }

    private fun completionImportText(importIndex: Int, functionCount: Int): String =
        buildString {
            appendLine("export module imported_$importIndex;")
            appendLine("struct imported_box_$importIndex {")
            appendLine("    value: i32;")
            appendLine("}")
            repeat(functionCount) { functionIndex ->
                appendLine()
                appendLine("imported_${importIndex}_helper_$functionIndex(value: i32) -> i32")
                appendLine("{")
                appendLine("    return value;")
                appendLine("}")
            }
        }

    private fun measureMillis(block: () -> Unit): Long =
        measureNanoTime(block) / 1_000_000

    private fun addLabProjectFile(relativePath: String): com.intellij.psi.PsiFile =
        myFixture.addFileToProject(relativePath, labSource(relativePath))

    private fun addRealCpProjectSources(): Map<String, PsiFile> {
        val root = repoRoot()
        return listOf("lab", "std")
            .flatMap { sourceRoot ->
                root.resolve(sourceRoot).toFile()
                    .walkTopDown()
                    .filter { file -> file.isFile && file.extension == "cp" }
                    .map { file ->
                        val relativePath = root.relativize(file.toPath()).toString()
                        relativePath to myFixture.addFileToProject(relativePath, file.readText())
                    }
                    .toList()
            }
            .toMap()
    }

    private fun sourceElement(case: LabNavigationCase): PsiElement {
        val contextOffset = myFixture.file.text.indexOf(case.context)
        assertTrue("${case.activePath} should contain context ${case.context}", contextOffset >= 0)
        val sourceOffsetInContext = case.context.indexOf(case.sourceText)
        assertTrue("${case.context} should contain ${case.sourceText}", sourceOffsetInContext >= 0)
        val sourceOffset = contextOffset + sourceOffsetInContext
        val leaf = myFixture.file.findElementAt(sourceOffset)
        val source = leaf?.cpNavigationElement()
        assertNotNull(
            "${case.activePath}:${case.sourceText} should be a navigation element; " +
                "leaf=${leaf?.text}:${leaf?.cpElementType()} chain=${leaf.parentChain()}",
            source,
        )
        assertEquals(case.sourceText, source!!.text)
        return source
    }

    private fun PsiElement?.parentChain(): String =
        generateSequence(this) { element -> element.parent }
            .take(8)
            .joinToString(" -> ") { element -> "${element.text.take(24)}:${element.cpElementType()}" }

    private fun labSource(relativePath: String): String =
        repoRoot().resolve(relativePath).toFile().readText()

    private fun repoRoot(): Path {
        var current = Path.of(System.getProperty("user.dir")).toAbsolutePath()
        while (current.parent != null) {
            if (current.resolve("lab/parser/grammar.cp").toFile().isFile) {
                return current
            }
            current = current.parent
        }
        error("failed to locate cp repo root from ${System.getProperty("user.dir")}")
    }

    private data class LabNavigationCase(
        val activePath: String,
        val context: String,
        val sourceText: String,
        val targetPath: String,
        val targetText: String,
        val targetType: IElementType,
    )
}
