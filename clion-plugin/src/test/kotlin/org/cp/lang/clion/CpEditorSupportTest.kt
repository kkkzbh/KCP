package org.cp.lang.clion

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
