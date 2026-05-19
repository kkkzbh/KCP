package org.cp.lang.clion

import org.junit.Assert.assertArrayEquals
import org.junit.Test

class CpSyntaxHighlighterTest {
    private val highlighter = CpSyntaxHighlighter()

    @Test
    fun highlightCategoriesMapToExpectedKeys() {
        assertArrayEquals(
            arrayOf(CpSyntaxHighlighter.DECLARATION_KEYWORD),
            highlighter.getTokenHighlights(CpTypes.KW_LET),
        )
        assertArrayEquals(
            arrayOf(CpSyntaxHighlighter.DECLARATION_KEYWORD),
            highlighter.getTokenHighlights(CpTypes.KW_OPERATOR),
        )
        assertArrayEquals(
            arrayOf(CpSyntaxHighlighter.CONTROL_KEYWORD),
            highlighter.getTokenHighlights(CpTypes.KW_RETURN),
        )
        assertArrayEquals(
            arrayOf(CpSyntaxHighlighter.MODULE_KEYWORD),
            highlighter.getTokenHighlights(CpTypes.KW_IMPORT),
        )
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.BOOLEAN), highlighter.getTokenHighlights(CpTypes.KW_TRUE))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.IDENTIFIER), highlighter.getTokenHighlights(CpTypes.IDENTIFIER))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.NUMBER), highlighter.getTokenHighlights(CpTypes.INTEGER_LITERAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.STRING), highlighter.getTokenHighlights(CpTypes.STRING_LITERAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.CHARACTER), highlighter.getTokenHighlights(CpTypes.CHAR_LITERAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.LINE_COMMENT), highlighter.getTokenHighlights(CpTypes.LINE_COMMENT))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.BLOCK_COMMENT), highlighter.getTokenHighlights(CpTypes.BLOCK_COMMENT))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.OPERATOR), highlighter.getTokenHighlights(CpTypes.PLUS_EQUAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.PUNCTUATION), highlighter.getTokenHighlights(CpTypes.SEMICOLON))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.BAD_CHARACTER), highlighter.getTokenHighlights(CpTypes.INVALID))
    }
}
