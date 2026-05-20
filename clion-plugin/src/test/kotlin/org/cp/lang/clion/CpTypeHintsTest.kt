package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue

class CpTypeHintsTest : BasePlatformTestCase() {
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

    fun testInfersLocalLiteralStructAndCallTypes() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            answer() -> i32
            {
                return 42;
            }

            main() -> i32
            {
                let count = 1;
                let ok = true;
                let item = box{ .value = count };
                let result = answer();
                return result;
            }
            """.trimIndent(),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("i32", hints["count"])
        assertEquals("bool", hints["ok"])
        assertEquals("box", hints["item"])
        assertEquals("i32", hints["result"])
    }

    fun testDoesNotShowHintForExplicitLocalType() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main()
            {
                let explicit: i32 = 1;
                let inferred = explicit;
            }
            """.trimIndent(),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).map { it.name }

        assertFalse("explicit" in hints)
        assertTrue("inferred" in hints)
    }
}
