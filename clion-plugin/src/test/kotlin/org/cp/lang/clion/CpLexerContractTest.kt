package org.cp.lang.clion

import com.intellij.psi.TokenType
import com.intellij.psi.tree.IElementType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import java.nio.file.Files
import java.nio.file.Path

class CpLexerContractTest {
    @Test
    fun lexerMatchesHelperOnRepresentativeSamples() {
        val repoRoot = repoRoot()
        val cases = listOf(
            repoRoot.resolve("design/examples/basics/main.cp"),
            repoRoot.resolve("design/examples/flow/main.cp"),
            repoRoot.resolve("design/examples/operators/main.cp"),
            repoRoot.resolve("design/examples/types/main.cp"),
            repoRoot.resolve("test/lexer/cases/valid/keywords/all_keywords.lex"),
            repoRoot.resolve("test/lexer/cases/valid/operators/all_punctuators.lex"),
            repoRoot.resolve("test/lexer/cases/valid/trivia/block_comment.lex"),
            repoRoot.resolve("test/lexer/cases/valid/trivia/line_comment.lex"),
            repoRoot.resolve("test/lexer/cases/invalid/misc/invalid_character.lex"),
            repoRoot.resolve("test/lexer/cases/invalid/numbers/invalid_suffix.lex"),
            repoRoot.resolve("test/lexer/cases/invalid/strings/unknown_escape.lex"),
        )

        for (path in cases) {
            compareCase(path)
        }
    }

    @Test
    fun lexerRecognizesForwardKeyword() {
        val token = tokenizeWithPlugin("forward value;").first()

        assertEquals("kw_forward", token.kind)
        assertEquals("forward", token.lexeme)
    }

    @Test
    fun lexerScansPunctuatorsWithoutSlicingRemainingBuffer() {
        val text = CpTypes.punctuators.joinToString(" ") { it.first }
        val tokenTypes = tokenizeTypesWithPlugin(NoSliceCharSequence(text))

        assertEquals(CpTypes.punctuators.map { it.second }, tokenTypes)
    }

    private fun compareCase(path: Path) {
        val text = Files.readString(path)
        val pluginTokens = tokenizeWithPlugin(text)
        val helperTokens = CpHelperRunner.tokens(path.fileName.toString(), text)
            .filter { it.kind != "eof" }

        assertFalse("helper token stream should not be empty for $path", helperTokens.isEmpty())
        assertEquals("token count should match for $path", helperTokens.size, pluginTokens.size)

        helperTokens.zip(pluginTokens).forEachIndexed { index, (helper, plugin) ->
            assertEquals("token kind mismatch at $path#$index", helper.kind, plugin.kind)
            assertEquals("token lexeme mismatch at $path#$index", helper.lexeme, plugin.lexeme)
            assertEquals("token start mismatch at $path#$index", helper.startOffset, plugin.startOffset)
            assertEquals("token end mismatch at $path#$index", helper.endOffset, plugin.endOffset)
        }
    }

    private fun tokenizeWithPlugin(text: String): List<PluginToken> {
        val lexer = CpLexer()
        lexer.start(text)

        val result = mutableListOf<PluginToken>()
        while (true) {
            val tokenType = lexer.tokenType ?: break
            val start = lexer.tokenStart
            val end = lexer.tokenEnd
            if (tokenType != TokenType.WHITE_SPACE &&
                tokenType != CpTypes.LINE_COMMENT &&
                tokenType != CpTypes.BLOCK_COMMENT
            ) {
                result += PluginToken(
                    kind = tokenType.debugName(),
                    lexeme = text.substring(start, end),
                    startOffset = start,
                    endOffset = end,
                )
            }
            lexer.advance()
        }
        return result
    }

    private fun tokenizeTypesWithPlugin(text: CharSequence): List<IElementType> {
        val lexer = CpLexer()
        lexer.start(text, 0, text.length, 0)

        val result = mutableListOf<IElementType>()
        while (true) {
            val tokenType = lexer.tokenType ?: break
            if (tokenType != TokenType.WHITE_SPACE) {
                result += tokenType
            }
            lexer.advance()
        }
        return result
    }

    private fun repoRoot(): Path =
        Path.of(System.getProperty("cp.repo.root") ?: error("cp.repo.root is not configured"))

    private data class PluginToken(
        val kind: String,
        val lexeme: String,
        val startOffset: Int,
        val endOffset: Int,
    )

    private class NoSliceCharSequence(
        private val text: String,
    ) : CharSequence {
        override val length: Int
            get() = text.length

        override fun get(index: Int): Char =
            text[index]

        override fun subSequence(startIndex: Int, endIndex: Int): CharSequence =
            error("symbol lexing must not slice the remaining input")

        override fun toString(): String =
            text
    }
}
