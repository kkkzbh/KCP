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
        CpTypes.CONTROL_KEYWORD_TOKENS.contains(tokenType) -> CONTROL_KEYWORD_KEYS
        CpTypes.DECLARATION_KEYWORD_TOKENS.contains(tokenType) -> DECLARATION_KEYWORD_KEYS
        CpTypes.MODULE_KEYWORD_TOKENS.contains(tokenType) -> MODULE_KEYWORD_KEYS
        CpTypes.BOOLEAN_LITERAL_TOKENS.contains(tokenType) -> BOOLEAN_KEYS
        tokenType == CpTypes.KW_AS || tokenType == CpTypes.KW_AND ||
            tokenType == CpTypes.KW_OR || tokenType == CpTypes.KW_NOT -> OPERATOR_KEYS
        tokenType == CpTypes.IDENTIFIER -> IDENTIFIER_KEYS
        tokenType == CpTypes.INTEGER_LITERAL || tokenType == CpTypes.FLOAT_LITERAL -> NUMBER_KEYS
        tokenType == CpTypes.STRING_LITERAL -> STRING_KEYS
        tokenType == CpTypes.CHAR_LITERAL -> CHARACTER_KEYS
        tokenType == CpTypes.LINE_COMMENT -> LINE_COMMENT_KEYS
        tokenType == CpTypes.BLOCK_COMMENT -> BLOCK_COMMENT_KEYS
        CpTypes.OPERATOR_TOKENS.contains(tokenType) -> OPERATOR_KEYS
        CpTypes.PUNCTUATION_TOKENS.contains(tokenType) -> PUNCTUATION_KEYS
        tokenType == CpTypes.INVALID -> BAD_CHARACTER_KEYS
        else -> emptyArray()
    }

    companion object {
        val CONTROL_KEYWORD: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.CONTROL_KEYWORD", DefaultLanguageHighlighterColors.KEYWORD)
        val DECLARATION_KEYWORD: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.DECLARATION_KEYWORD", DefaultLanguageHighlighterColors.KEYWORD)
        val MODULE_KEYWORD: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.MODULE_KEYWORD", DefaultLanguageHighlighterColors.KEYWORD)
        val BOOLEAN: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.BOOLEAN", DefaultLanguageHighlighterColors.KEYWORD)
        val IDENTIFIER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.IDENTIFIER", DefaultLanguageHighlighterColors.IDENTIFIER)
        val NUMBER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.NUMBER", DefaultLanguageHighlighterColors.NUMBER)
        val STRING: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.STRING", DefaultLanguageHighlighterColors.STRING)
        val CHARACTER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.CHARACTER", DefaultLanguageHighlighterColors.STRING)
        val LINE_COMMENT: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.COMMENT", DefaultLanguageHighlighterColors.LINE_COMMENT)
        val BLOCK_COMMENT: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.BLOCK_COMMENT", DefaultLanguageHighlighterColors.BLOCK_COMMENT)
        val OPERATOR: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.OPERATOR", DefaultLanguageHighlighterColors.OPERATION_SIGN)
        val PUNCTUATION: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.PUNCTUATION", DefaultLanguageHighlighterColors.COMMA)
        val BAD_CHARACTER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.BAD_CHARACTER", HighlighterColors.BAD_CHARACTER)

        val FUNCTION_DECLARATION: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey(
                "CP.FUNCTION_DECLARATION",
                DefaultLanguageHighlighterColors.FUNCTION_DECLARATION,
            )
        val FUNCTION_CALL: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.FUNCTION_CALL", DefaultLanguageHighlighterColors.FUNCTION_CALL)
        val PARAMETER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.PARAMETER", DefaultLanguageHighlighterColors.PARAMETER)
        val LOCAL_VARIABLE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LOCAL_VARIABLE", DefaultLanguageHighlighterColors.LOCAL_VARIABLE)
        val TYPE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.TYPE", DefaultLanguageHighlighterColors.CLASS_NAME)
        val MODULE_NAME: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.MODULE_NAME", DefaultLanguageHighlighterColors.STATIC_FIELD)
        val LABEL: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LABEL", DefaultLanguageHighlighterColors.LABEL)

        private val CONTROL_KEYWORD_KEYS = arrayOf(CONTROL_KEYWORD)
        private val DECLARATION_KEYWORD_KEYS = arrayOf(DECLARATION_KEYWORD)
        private val MODULE_KEYWORD_KEYS = arrayOf(MODULE_KEYWORD)
        private val BOOLEAN_KEYS = arrayOf(BOOLEAN)
        private val IDENTIFIER_KEYS = arrayOf(IDENTIFIER)
        private val NUMBER_KEYS = arrayOf(NUMBER)
        private val STRING_KEYS = arrayOf(STRING)
        private val CHARACTER_KEYS = arrayOf(CHARACTER)
        private val LINE_COMMENT_KEYS = arrayOf(LINE_COMMENT)
        private val BLOCK_COMMENT_KEYS = arrayOf(BLOCK_COMMENT)
        private val OPERATOR_KEYS = arrayOf(OPERATOR)
        private val PUNCTUATION_KEYS = arrayOf(PUNCTUATION)
        private val BAD_CHARACTER_KEYS = arrayOf(BAD_CHARACTER)
    }
}

class CpSyntaxHighlighterFactory : SyntaxHighlighterFactory() {
    override fun getSyntaxHighlighter(project: Project?, virtualFile: VirtualFile?): SyntaxHighlighter =
        CpSyntaxHighlighter()
}
