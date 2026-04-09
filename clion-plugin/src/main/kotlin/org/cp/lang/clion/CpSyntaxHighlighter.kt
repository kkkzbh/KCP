package org.cp.lang.clion

import com.intellij.lexer.Lexer
import com.intellij.openapi.editor.DefaultLanguageHighlighterColors
import com.intellij.openapi.editor.HighlighterColors
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.fileTypes.SyntaxHighlighter
import com.intellij.openapi.fileTypes.SyntaxHighlighterFactory
import com.intellij.psi.TokenType
import com.intellij.psi.tree.IElementType
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.VirtualFile

class CpSyntaxHighlighter : SyntaxHighlighter {
    override fun getHighlightingLexer(): Lexer = CpLexer()

    override fun getTokenHighlights(tokenType: IElementType?): Array<TextAttributesKey> = when {
        tokenType == null || tokenType == TokenType.WHITE_SPACE -> emptyArray()
        CpTypes.KEYWORD_TOKENS.contains(tokenType) -> KEYWORD_KEYS
        tokenType == CpTypes.IDENTIFIER -> IDENTIFIER_KEYS
        tokenType == CpTypes.INTEGER_LITERAL || tokenType == CpTypes.FLOAT_LITERAL -> NUMBER_KEYS
        CpTypes.STRING_TOKENS.contains(tokenType) -> STRING_KEYS
        CpTypes.COMMENT_TOKENS.contains(tokenType) -> COMMENT_KEYS
        CpTypes.SYMBOL_TOKENS.contains(tokenType) -> SYMBOL_KEYS
        tokenType == CpTypes.INVALID -> BAD_CHARACTER_KEYS
        else -> emptyArray()
    }

    companion object {
        val KEYWORD: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.KEYWORD", DefaultLanguageHighlighterColors.KEYWORD)
        val IDENTIFIER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.IDENTIFIER", DefaultLanguageHighlighterColors.IDENTIFIER)
        val NUMBER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.NUMBER", DefaultLanguageHighlighterColors.NUMBER)
        val STRING: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.STRING", DefaultLanguageHighlighterColors.STRING)
        val COMMENT: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.COMMENT", DefaultLanguageHighlighterColors.LINE_COMMENT)
        val SYMBOL: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.SYMBOL", DefaultLanguageHighlighterColors.OPERATION_SIGN)
        val BAD_CHARACTER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.BAD_CHARACTER", HighlighterColors.BAD_CHARACTER)

        private val KEYWORD_KEYS = arrayOf(KEYWORD)
        private val IDENTIFIER_KEYS = arrayOf(IDENTIFIER)
        private val NUMBER_KEYS = arrayOf(NUMBER)
        private val STRING_KEYS = arrayOf(STRING)
        private val COMMENT_KEYS = arrayOf(COMMENT)
        private val SYMBOL_KEYS = arrayOf(SYMBOL)
        private val BAD_CHARACTER_KEYS = arrayOf(BAD_CHARACTER)
    }
}

class CpSyntaxHighlighterFactory : SyntaxHighlighterFactory() {
    override fun getSyntaxHighlighter(project: Project?, virtualFile: VirtualFile?): SyntaxHighlighter =
        CpSyntaxHighlighter()
}
