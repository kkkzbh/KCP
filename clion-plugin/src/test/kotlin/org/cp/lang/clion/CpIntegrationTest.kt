package org.cp.lang.clion

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class CpIntegrationTest {
    @Test
    fun fileTypeMetadataMatchesPluginContract() {
        assertEquals("cp", CpLanguage.id)
        assertEquals("cp", CpFileType.INSTANCE.name)
        assertEquals("cp", CpFileType.INSTANCE.defaultExtension)
        assertEquals("cp source file", CpFileType.INSTANCE.description)
    }

    @Test
    fun externalAnnotatorReportsLexerErrors() {
        val request = CpExternalAnnotator.Request(
            filename = "broken.cp",
            text = "\"unterminated\n",
        )

        val diagnostics = CpExternalAnnotator().doAnnotate(request)
        assertTrue(
            diagnostics.any { it.code == "unterminated_string_literal" && it.message == "unterminated string literal" },
        )
    }

    @Test
    fun helperOffsetsAreNormalizedForUnicodeText() {
        val text = "// 导入方式\n\"unterminated\n"
        val diagnostic = CpHelperRunner.analyze("unicode.cp", text)
            .single { it.code == "unterminated_string_literal" }

        assertEquals("\"unterminated", text.substring(diagnostic.startOffset, diagnostic.endOffset))
        assertEquals(2, diagnostic.line)
        assertEquals(1, diagnostic.column)
    }
}
