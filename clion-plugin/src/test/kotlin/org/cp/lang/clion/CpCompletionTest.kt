package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse

class CpCompletionTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
    }

    override fun tearDown() {
        try {
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testCompletesLocalsFunctionsAndTypes() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            helper(value: i32) -> i32
            {
                let total = value;
                return to<caret>;
            }
            """.trimIndent(),
        )

        val items = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }
        assertContainsElements(items, "total", "helper", "box", "builtin")
    }

    fun testCompletionUsesCachedSymbolIndexWithoutDroppingSymbols() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            helper(value: i32) -> i32
            {
                let total = value;
                return le<caret>;
            }
            """.trimIndent(),
        )

        val first = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }
        val second = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }

        assertContainsElements(first, "let", "total", "helper", "box", "builtin")
        assertEquals(first, second)
    }

    fun testCompletesMemberNames() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
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
                return item.<caret>;
            }
            """.trimIndent(),
        )

        val items = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }
        assertContainsElements(items, "value", "get")
    }

    fun testCompletesSymbolsFromCachedImportClosureAfterLocalEdit() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main() -> i32
            {
                return a<caret>;
            }
            """.trimIndent(),
        )
        val activePath = myFixture.file.virtualFile.path
        val importedPath = "$activePath.math.cp"
        CpSemanticCache.get(project).store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(
                    CpInspectionFile(activePath, myFixture.file.text),
                    CpInspectionFile(
                        importedPath,
                        """
                        export module math;

                        export struct number_box {
                            value: i32;
                        }

                        add(value: i32) -> i32
                        {
                            return value;
                        }
                        """.trimIndent(),
                    ),
                ),
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )

        myFixture.type("d")

        val items = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }
        assertContainsElements(items, "add", "number_box")
    }

    fun testCompletionCacheInvalidatesWhenImportedSnapshotChanges() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main() -> i32
            {
                return <caret>;
            }
            """.trimIndent(),
        )
        val activePath = myFixture.file.virtualFile.path
        val importedPath = "$activePath.math.cp"
        storeImportClosure(
            activePath,
            importedPath,
            """
            export module math;

            old_answer() -> i32
            {
                return 1;
            }
            """.trimIndent(),
        )
        val first = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }

        storeImportClosure(
            activePath,
            importedPath,
            """
            export module math;

            new_answer() -> i32
            {
                return 2;
            }
            """.trimIndent(),
        )
        val second = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }

        assertContainsElements(first, "old_answer")
        assertContainsElements(second, "new_answer")
        assertFalse("stale imported completion survived cache invalidation", "old_answer" in second)
    }

    fun testCompletesOperatorAffixes() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct counter {
                value: i32;
            }

            impl counter {
                operator <caret>
            }
            """.trimIndent(),
        )

        val items = CpCompletionEngine.items(myFixture.file, myFixture.caretOffset).map { it.name }
        assertContainsElements(items, "prefix", "postfix")
    }

    private fun storeImportClosure(activePath: String, importedPath: String, importedText: String) {
        CpSemanticCache.get(project).store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(
                    CpInspectionFile(activePath, myFixture.file.text),
                    CpInspectionFile(importedPath, importedText),
                ),
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )
    }
}
