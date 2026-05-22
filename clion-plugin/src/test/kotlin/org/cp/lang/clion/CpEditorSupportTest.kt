package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.codeInsight.editorActions.TypedHandlerDelegate
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class CpEditorSupportTest {
    @Test
    fun smartBackspaceDeletesToPreviousIndentStop() {
        val text = "main() {\n       "
        val deletion = CpEditorIndent.smartBackspaceDeletion(text, text.length)

        assertEquals(13..15, deletion)
    }

    @Test
    fun smartBackspaceIgnoresNonIndentText() {
        val text = "main() {\n    let"

        assertNull(CpEditorIndent.smartBackspaceDeletion(text, text.length))
    }

    @Test
    fun enterAfterOpenBraceIndentsBlankLine() {
        val text = "main() {\n"

        val edit = CpEditorIndent.openBraceIndentEdit(text, text.length)

        assertEquals("main() {\n    ", applyEdit(text, edit))
        assertEquals("main() {\n    ".length, edit?.caretOffset)
    }

    @Test
    fun enterBetweenPairedBracesSplitsClosingBraceLine() {
        val text = "main() {\n}"
        val caretOffset = "main() {\n".length

        val edit = CpEditorIndent.openBraceIndentEdit(text, caretOffset)

        assertEquals(
            "main() {\n    \n}",
            applyEdit(text, edit),
        )
        assertEquals("main() {\n    ".length, edit?.caretOffset)
    }

    private fun applyEdit(text: String, edit: CpEditorIndent.OpenBraceIndentEdit?): String {
        if (edit == null) {
            return text
        }

        val builder = StringBuilder(text)
        builder.replace(edit.replaceStartOffset, edit.replaceEndOffset, edit.replacement)
        edit.insertOffset?.let { builder.insert(it, edit.insertedText) }
        return builder.toString()
    }
}

class CpQuoteTypedHandlerTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        TypedHandlerDelegate.EP_NAME.point.registerExtension(CpQuoteTypedHandler(), testRootDisposable)
    }

    override fun tearDown() {
        try {
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testDoubleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let text = <caret>; }")

        myFixture.type("\"")

        myFixture.checkResult("main() { let text = \"<caret>\"; }")
    }

    fun testSingleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let ch = <caret>; }")

        myFixture.type("'")

        myFixture.checkResult("main() { let ch = '<caret>'; }")
    }

    fun testSmartSingleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let ch = <caret>; }")

        myFixture.type("‘")

        myFixture.checkResult("main() { let ch = ‘<caret>’; }")
    }

    fun testTypingQuoteBeforeClosingQuoteSkipsOverIt() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let text = \"<caret>\"; }")

        myFixture.type("\"")

        myFixture.checkResult("main() { let text = \"\"<caret>; }")
    }

    @Test
    fun quotePairLogicIgnoresEscapedPosition() {
        assertNull(CpQuotePairs.typingEdit("\\", 1, '"'))
    }
}
