package org.cp.lang.clion

import com.intellij.codeInsight.daemon.impl.HighlightInfo
import com.intellij.lang.LanguageAnnotators
import com.intellij.lang.LanguageParserDefinitions
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import java.nio.file.Files
import java.nio.file.Path

class CpAnnotatorTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()
    private val annotator = CpAnnotator()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        LanguageAnnotators.INSTANCE.addExplicitExtension(CpLanguage, annotator)
    }

    override fun tearDown() {
        try {
            LanguageAnnotators.INSTANCE.removeExplicitExtension(CpLanguage, annotator)
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testSemanticHighlightsComeFromPsiAnnotator() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module math;
            import std.io;

            export add(x: i32, y: i32) -> i32
            {
                let sum: i32 = add(x, y);
                return sum;
            }
            """.trimIndent(),
        )
        val highlights = myFixture.doHighlighting()

        assertHasHighlight(file.text, highlights, "math", CpSyntaxHighlighter.MODULE_NAME)
        assertHasHighlight(file.text, highlights, "std.io", CpSyntaxHighlighter.MODULE_NAME)
        assertHasHighlight(file.text, highlights, "add", CpSyntaxHighlighter.FUNCTION_DECLARATION)
        assertHasHighlight(file.text, highlights, "add", CpSyntaxHighlighter.FUNCTION_CALL)
        assertHasHighlight(file.text, highlights, "x", CpSyntaxHighlighter.PARAMETER)
        assertHasHighlight(file.text, highlights, "sum", CpSyntaxHighlighter.LOCAL_VARIABLE)
        assertHasHighlight(file.text, highlights, "i32", CpSyntaxHighlighter.TYPE)
    }

    fun testMemberCallsAndMatchPatternsUseSpecificHighlights() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            variant event {
                key(char);
                quit;
            }

            score(value: event) -> i32
            {
                let item = value;
                return match value {
                    .key(code) => item.value_or(code),
                    .quit => 0,
                };
            }
            """.trimIndent(),
        )
        val highlights = myFixture.doHighlighting()

        assertHasHighlight(file.text, highlights, "key", CpSyntaxHighlighter.VARIANT_CASE)
        assertHasHighlight(file.text, highlights, "quit", CpSyntaxHighlighter.VARIANT_CASE)
        assertHasHighlight(file.text, highlights, "code", CpSyntaxHighlighter.PARAMETER)
        assertHasHighlight(file.text, highlights, "value_or", CpSyntaxHighlighter.MEMBER_FUNCTION)
        assertNoHighlight(file.text, highlights, "item", CpSyntaxHighlighter.FUNCTION_CALL)
    }

    fun testSemanticHighlightsRealExampleFiles() {
        val repoRoot = Path.of(System.getProperty("cp.repo.root") ?: error("cp.repo.root is not configured"))
        val cases = mapOf(
            "design/examples/flow/main.cp" to listOf(
                "sum_non_negative" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "countdown" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "contains_target" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "total" to CpSyntaxHighlighter.LOCAL_VARIABLE,
                "values" to CpSyntaxHighlighter.PARAMETER,
                "i32" to CpSyntaxHighlighter.TYPE,
            ),
            "design/examples/basics/main.cp" to listOf(
                "main" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "answer" to CpSyntaxHighlighter.LOCAL_VARIABLE,
                "rounded" to CpSyntaxHighlighter.LOCAL_VARIABLE,
                "i32" to CpSyntaxHighlighter.TYPE,
            ),
            "design/examples/modules/math.cp" to listOf(
                "add" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "clamp_min" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "value" to CpSyntaxHighlighter.PARAMETER,
                "i32" to CpSyntaxHighlighter.TYPE,
            ),
            "design/examples/types/main.cp" to listOf(
                "main" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
                "count" to CpSyntaxHighlighter.LOCAL_VARIABLE,
                "array" to CpSyntaxHighlighter.TYPE,
                "i32" to CpSyntaxHighlighter.TYPE,
            ),
        )

        for ((relativePath, expectedHighlights) in cases) {
            val text = Files.readString(repoRoot.resolve(relativePath))
            val file = myFixture.configureByText(CpFileType.INSTANCE, text)
            val highlights = myFixture.doHighlighting()
            for ((textFragment, key) in expectedHighlights) {
                assertHasHighlight(file.text, highlights, textFragment, key)
            }
        }
    }

    private fun assertHasHighlight(
        source: String,
        highlights: List<HighlightInfo>,
        text: String,
        key: TextAttributesKey,
    ) {
        assertTrue(
            "$text should have $key",
            highlights.any { info ->
                info.forcedTextAttributesKey == key &&
                    source.substring(info.startOffset, info.endOffset) == text
            },
        )
    }

    private fun assertNoHighlight(
        source: String,
        highlights: List<HighlightInfo>,
        text: String,
        key: TextAttributesKey,
    ) {
        assertFalse(
            "$text should not have $key",
            highlights.any { info ->
                info.forcedTextAttributesKey == key &&
                    source.substring(info.startOffset, info.endOffset) == text
            },
        )
    }
}
