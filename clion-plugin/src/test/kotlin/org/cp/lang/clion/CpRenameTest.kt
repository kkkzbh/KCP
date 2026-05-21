package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue

class CpRenameTest : BasePlatformTestCase() {
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

    fun testRenameLocalDeclarationUpdatesReferences() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() -> i32
            {
                let <caret>sum: i32 = 1;
                return sum + sum;
            }
            """.trimIndent(),
        )

        myFixture.renameElementAtCaret("total")

        assertTrue(myFixture.file.text, myFixture.file.text.contains("let total: i32 = 1;"))
        assertTrue(myFixture.file.text, myFixture.file.text.contains("return total + total;"))
        assertFalse(myFixture.file.text.contains("sum"))
    }

    fun testRenameFunctionUpdatesCallReference() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            add(value: i32) -> i32
            {
                return value + 1;
            }

            main() -> i32
            {
                return <caret>add(1);
            }
            """.trimIndent(),
        )

        CpSemanticCache.get(project).computeNow(myFixture.file, myFixture.editor.document.text)
        myFixture.renameElementAtCaret("inc")

        assertTrue(myFixture.file.text, myFixture.file.text.contains("inc(value: i32) -> i32"))
        assertTrue(myFixture.file.text, myFixture.file.text.contains("return inc(1);"))
        assertFalse(myFixture.file.text.contains("add"))
    }
}
