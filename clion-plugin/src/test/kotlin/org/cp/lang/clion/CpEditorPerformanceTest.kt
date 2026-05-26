package org.cp.lang.clion

import com.intellij.codeInsight.completion.CompletionContributor
import com.intellij.codeInsight.completion.CompletionContributorEP
import com.intellij.codeInsight.lookup.LookupManager
import com.intellij.ide.plugins.PluginManagerCore
import com.intellij.lang.LanguageParserDefinitions
import com.intellij.openapi.actionSystem.IdeActions
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileTypes.FileTypeManager
import com.intellij.psi.TokenType
import com.intellij.testFramework.IndexingTestUtil
import com.intellij.testFramework.fixtures.BasePlatformTestCase
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
}
