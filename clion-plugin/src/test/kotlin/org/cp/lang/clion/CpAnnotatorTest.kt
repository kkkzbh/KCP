package org.cp.lang.clion

import com.intellij.codeInsight.daemon.impl.HighlightInfo
import com.intellij.lang.ExternalLanguageAnnotators
import com.intellij.lang.LanguageAnnotators
import com.intellij.lang.LanguageParserDefinitions
import com.intellij.lang.annotation.HighlightSeverity
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.psi.PsiFile
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

    fun testExternalAnnotatorDiagnosticsBecomeEditorErrors() {
        val externalAnnotator = CpExternalAnnotator()
        ExternalLanguageAnnotators.INSTANCE.addExplicitExtension(CpLanguage, externalAnnotator)
        try {
            val file = myFixture.configureByText(
                CpFileType.INSTANCE,
                """
                main() -> i32
                {
                    let value: i32 = 1;;
                    return value;
                }
                """.trimIndent(),
            )
            seedSemanticCache(file)

            val highlights = myFixture.doHighlighting()

            assertWarningCount(file.text, highlights, "empty_statement", 1)
        } finally {
            ExternalLanguageAnnotators.INSTANCE.removeExplicitExtension(CpLanguage, externalAnnotator)
        }
    }

    fun testExternalAnnotatorSemanticDiagnosticsBecomeEditorErrors() {
        val externalAnnotator = CpExternalAnnotator()
        ExternalLanguageAnnotators.INSTANCE.addExplicitExtension(CpLanguage, externalAnnotator)
        try {
            val file = myFixture.configureByText(
                CpFileType.INSTANCE,
                """
                main() -> i32
                {
                }
                """.trimIndent(),
            )
            seedSemanticCache(file)

            val highlights = myFixture.doHighlighting()

            assertHasError(file.text, highlights, "main", "missing_return")
        } finally {
            ExternalLanguageAnnotators.INSTANCE.removeExplicitExtension(CpLanguage, externalAnnotator)
        }
    }

    fun testExternalAnnotatorKeepsProjectedCachedDiagnosticsWhileRefreshPending() {
        val externalAnnotator = CpExternalAnnotator()
        ExternalLanguageAnnotators.INSTANCE.addExplicitExtension(CpLanguage, externalAnnotator)
        try {
            val file = myFixture.configureByText(
                CpFileType.INSTANCE,
                """
                main() -> i32
                {
                    let value: i32 = 1;;
                    return value;
                }<caret>
                """.trimIndent(),
            )
            seedSemanticCache(file)

            myFixture.type("\n")
            val highlights = myFixture.doHighlighting()

            assertWarningCount(file.text, highlights, "empty_statement", 1)
        } finally {
            ExternalLanguageAnnotators.INSTANCE.removeExplicitExtension(CpLanguage, externalAnnotator)
        }
    }

    fun testCachedDiagnosticsStayVisibleThroughSynchronousAnnotatorDuringTyping() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import std;

            main() -> i32
            {
                let a: i32 = 3;
                let e: i32 - 3;
                if(a > b) {
                    let q: i32 = 2;
                    let growing: i32 = 123<caret>;
                }
                return 0;
            }
            """.trimIndent(),
        )
        seedSemanticCache(file)

        myFixture.type("4")
        val highlights = myFixture.doHighlighting()

        assertHasError(file.text, highlights, "-", "expected_token")
    }

    fun testSemanticCacheStoreReportsOnlyPresentationChanges() {
        val file = myFixture.configureByText(CpFileType.INSTANCE, "let a = 2;\n")
        val activePath = file.virtualFile.path
        val cache = CpSemanticCache.get(project)
        val request = CpInspectionRequest(
            activeFile = activePath,
            files = listOf(CpInspectionFile(activePath, file.text)),
        )
        val result = CpInspectionResult(
            accepted = false,
            diagnostics = listOf(
                CpHelperDiagnostic(
                    stage = "parse",
                    code = "unexpected_token",
                    message = "unexpected token",
                    severity = "error",
                    startOffset = 0,
                    endOffset = 3,
                    line = 1,
                    column = 1,
                ),
            ),
            highlights = listOf(CpHelperHighlight("local.declaration", 4, 5)),
        )

        assertTrue(cache.store(request, result, modificationCount = 1).presentationChanged)
        assertFalse(cache.store(request, result.copy(accepted = true), modificationCount = 2).presentationChanged)
        assertTrue(
            cache.store(
                request,
                result.copy(diagnostics = result.diagnostics.map { it.copy(endOffset = 4) }),
                modificationCount = 3,
            ).presentationChanged,
        )
    }

    fun testCachedHelperDiagnosticsAreNotDuplicatedWhenExternalAnnotatorAlsoRuns() {
        val externalAnnotator = CpExternalAnnotator()
        ExternalLanguageAnnotators.INSTANCE.addExplicitExtension(CpLanguage, externalAnnotator)
        try {
            val file = myFixture.configureByText(
                CpFileType.INSTANCE,
                """
                main() -> i32
                {
                    let value: i32 = 1;;
                    return value;
                }
                """.trimIndent(),
            )
            seedSemanticCache(file)

            val highlights = myFixture.doHighlighting()

            assertWarningCount(file.text, highlights, "empty_statement", 1)
        } finally {
            ExternalLanguageAnnotators.INSTANCE.removeExplicitExtension(CpLanguage, externalAnnotator)
        }
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

    fun testNestedCallArgumentsDoNotStealOuterFunctionHighlight() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            log(message: str, count: i32)
            {
            }

            main()
            {
                let source = "abc";
                log(source.as_str(), source.size());
            }
            """.trimIndent(),
        )
        val highlights = myFixture.doHighlighting()

        assertHasHighlight(file.text, highlights, "log", CpSyntaxHighlighter.FUNCTION_CALL)
        assertHasHighlight(file.text, highlights, "as_str", CpSyntaxHighlighter.MEMBER_FUNCTION)
        assertHasHighlight(file.text, highlights, "size", CpSyntaxHighlighter.MEMBER_FUNCTION)
        assertNoHighlight(file.text, highlights, "as_str", CpSyntaxHighlighter.FUNCTION_CALL)
    }

    fun testFieldReferencesInsideCallArgumentsRemainFields() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            use(value: i32)
            {
            }

            main()
            {
                use(item.value);
            }
            """.trimIndent(),
        )
        val highlights = myFixture.doHighlighting()

        assertHasHighlight(file.text, highlights, "use", CpSyntaxHighlighter.FUNCTION_CALL)
        assertHasHighlight(file.text, highlights, "value", CpSyntaxHighlighter.FIELD)
    }

    fun testPsiAnnotationIndexInvalidatesAfterEdit() {
        val file = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() -> i32
            {
                let first<caret> = 1;
                return first;
            }
            """.trimIndent(),
        )
        CpFileSymbolIndex.get(file)

        myFixture.type("_value")
        val highlights = myFixture.doHighlighting()

        assertHasHighlight(file.text, highlights, "first_value", CpSyntaxHighlighter.LOCAL_VARIABLE)
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
                "f64" to CpSyntaxHighlighter.TYPE,
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

    private fun assertHasError(
        source: String,
        highlights: List<HighlightInfo>,
        text: String,
        code: String,
    ) {
        assertTrue(
            "$text should have $code error",
            highlights.any { info ->
                info.severity == HighlightSeverity.ERROR &&
                    info.description?.contains(code) == true &&
                    source.substring(info.startOffset, info.endOffset).contains(text)
            },
        )
    }

    private fun assertWarningCount(
        source: String,
        highlights: List<HighlightInfo>,
        code: String,
        expectedCount: Int,
    ) {
        val count = highlights.count { info ->
            info.severity == HighlightSeverity.WARNING &&
                info.description?.contains(code) == true &&
                source.substring(info.startOffset, info.endOffset) == ";"
        }
        assertEquals("$code warning count", expectedCount, count)
    }

    private fun seedSemanticCache(file: PsiFile) {
        val request = CpProjectSnapshotCollector.buildRequest(
            activePath = file.virtualFile.path,
            activeText = myFixture.editor.document.text,
            projectFiles = emptyList(),
        )
        CpSemanticCache.get(project).store(request, CpHelperRunner.inspect(request), cpModificationCount(project))
    }
}
