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
        CpTypes.NULL_LITERAL_TOKENS.contains(tokenType) -> NULL_KEYS
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
        val NULL_LITERAL: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.NULL", DefaultLanguageHighlighterColors.CONSTANT)
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
        val MEMBER_FUNCTION: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.MEMBER_FUNCTION", DefaultLanguageHighlighterColors.INSTANCE_METHOD)
        val ASSOCIATED_FUNCTION: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.ASSOCIATED_FUNCTION", DefaultLanguageHighlighterColors.STATIC_METHOD)
        val CONSTRUCTOR: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.CONSTRUCTOR", DefaultLanguageHighlighterColors.FUNCTION_DECLARATION)
        val DESTRUCTOR: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.DESTRUCTOR", DefaultLanguageHighlighterColors.FUNCTION_DECLARATION)
        val BUILTIN_FUNCTION: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.BUILTIN_FUNCTION", DefaultLanguageHighlighterColors.STATIC_METHOD)
        val FUNCTION_TYPE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.FUNCTION_TYPE", DefaultLanguageHighlighterColors.KEYWORD)
        val PARAMETER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.PARAMETER", DefaultLanguageHighlighterColors.PARAMETER)
        val LOCAL_VARIABLE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LOCAL_VARIABLE", DefaultLanguageHighlighterColors.LOCAL_VARIABLE)
        val LOCAL_CONSTANT: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LOCAL_CONSTANT", DefaultLanguageHighlighterColors.CONSTANT)
        val LAMBDA_CAPTURE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LAMBDA_CAPTURE", DefaultLanguageHighlighterColors.LOCAL_VARIABLE)
        val TYPE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.TYPE", DefaultLanguageHighlighterColors.CLASS_NAME)
        val TYPE_PARAMETER: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.TYPE_PARAMETER", DefaultLanguageHighlighterColors.CLASS_NAME)
        val TYPE_ALIAS: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.TYPE_ALIAS", DefaultLanguageHighlighterColors.CLASS_NAME)
        val ASSOCIATED_TYPE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.ASSOCIATED_TYPE", DefaultLanguageHighlighterColors.STATIC_FIELD)
        val CONCEPT: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.CONCEPT", DefaultLanguageHighlighterColors.INTERFACE_NAME)
        val MODULE_NAME: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.MODULE_NAME", DefaultLanguageHighlighterColors.CLASS_NAME)
        val LABEL: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.LABEL", DefaultLanguageHighlighterColors.LABEL)
        val FIELD: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.FIELD", DefaultLanguageHighlighterColors.INSTANCE_FIELD)
        val VARIANT_CASE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.VARIANT_CASE", DefaultLanguageHighlighterColors.CONSTANT)
        val ENUM_CASE: TextAttributesKey =
            TextAttributesKey.createTextAttributesKey("CP.ENUM_CASE", DefaultLanguageHighlighterColors.CONSTANT)

        private val CONTROL_KEYWORD_KEYS = arrayOf(CONTROL_KEYWORD)
        private val DECLARATION_KEYWORD_KEYS = arrayOf(DECLARATION_KEYWORD)
        private val MODULE_KEYWORD_KEYS = arrayOf(MODULE_KEYWORD)
        private val BOOLEAN_KEYS = arrayOf(BOOLEAN)
        private val NULL_KEYS = arrayOf(NULL_LITERAL)
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
