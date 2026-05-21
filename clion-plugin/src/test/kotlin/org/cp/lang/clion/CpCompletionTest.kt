package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase

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
}
