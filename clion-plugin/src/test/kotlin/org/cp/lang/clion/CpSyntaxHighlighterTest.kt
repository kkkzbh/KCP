package org.cp.lang.clion

import org.junit.Assert.assertArrayEquals
import org.junit.Test

class CpSyntaxHighlighterTest {
    private val highlighter = CpSyntaxHighlighter()

    @Test
    fun highlightCategoriesMapToExpectedKeys() {
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.KEYWORD), highlighter.getTokenHighlights(CpTypes.KW_LET))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.IDENTIFIER), highlighter.getTokenHighlights(CpTypes.IDENTIFIER))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.NUMBER), highlighter.getTokenHighlights(CpTypes.INTEGER_LITERAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.STRING), highlighter.getTokenHighlights(CpTypes.STRING_LITERAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.COMMENT), highlighter.getTokenHighlights(CpTypes.LINE_COMMENT))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.SYMBOL), highlighter.getTokenHighlights(CpTypes.PLUS_EQUAL))
        assertArrayEquals(arrayOf(CpSyntaxHighlighter.BAD_CHARACTER), highlighter.getTokenHighlights(CpTypes.INVALID))
    }
}
