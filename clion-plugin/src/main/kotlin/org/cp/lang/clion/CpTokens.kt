package org.cp.lang.clion

import com.intellij.psi.TokenType
import com.intellij.psi.tree.IElementType
import com.intellij.psi.tree.TokenSet

class CpTokenType(debugName: String) : IElementType(debugName, CpLanguage)

object CpTypes {
    private fun token(name: String): CpTokenType = CpTokenType(name)

    @JvmField val INVALID = token("invalid")
    @JvmField val IDENTIFIER = token("identifier")
    @JvmField val INTEGER_LITERAL = token("integer_literal")
    @JvmField val FLOAT_LITERAL = token("float_literal")
    @JvmField val CHAR_LITERAL = token("char_literal")
    @JvmField val STRING_LITERAL = token("string_literal")
    @JvmField val LINE_COMMENT = token("line_comment")
    @JvmField val BLOCK_COMMENT = token("block_comment")

    @JvmField val KW_LET = token("kw_let")
    @JvmField val KW_CONST = token("kw_const")
    @JvmField val KW_IF = token("kw_if")
    @JvmField val KW_ELSE = token("kw_else")
    @JvmField val KW_WHILE = token("kw_while")
    @JvmField val KW_DO = token("kw_do")
    @JvmField val KW_FOR = token("kw_for")
    @JvmField val KW_BREAK = token("kw_break")
    @JvmField val KW_CONTINUE = token("kw_continue")
    @JvmField val KW_RETURN = token("kw_return")
    @JvmField val KW_IMPORT = token("kw_import")
    @JvmField val KW_EXPORT = token("kw_export")
    @JvmField val KW_MODULE = token("kw_module")
    @JvmField val KW_STRUCT = token("kw_struct")
    @JvmField val KW_ENUM = token("kw_enum")
    @JvmField val KW_IMPL = token("kw_impl")
    @JvmField val KW_CONCEPT = token("kw_concept")
    @JvmField val KW_OPERATOR = token("kw_operator")
    @JvmField val KW_AS = token("kw_as")
    @JvmField val KW_TRUE = token("kw_true")
    @JvmField val KW_FALSE = token("kw_false")
    @JvmField val KW_NULLPTR = token("kw_nullptr")
    @JvmField val KW_AND = token("kw_and")
    @JvmField val KW_OR = token("kw_or")
    @JvmField val KW_NOT = token("kw_not")
    @JvmField val KW_REF = token("kw_ref")
    @JvmField val KW_MOVE = token("kw_move")
    @JvmField val KW_LIKE = token("kw_like")
    @JvmField val KW_FORWARD = token("kw_forward")
    @JvmField val KW_NEW = token("kw_new")
    @JvmField val KW_DELETE = token("kw_delete")

    @JvmField val L_PAREN = token("l_paren")
    @JvmField val R_PAREN = token("r_paren")
    @JvmField val L_BRACE = token("l_brace")
    @JvmField val R_BRACE = token("r_brace")
    @JvmField val L_BRACKET = token("l_bracket")
    @JvmField val R_BRACKET = token("r_bracket")
    @JvmField val COMMA = token("comma")
    @JvmField val SEMICOLON = token("semicolon")
    @JvmField val COLON = token("colon")
    @JvmField val COLON_COLON = token("colon_colon")
    @JvmField val DOT = token("dot")
    @JvmField val ARROW = token("arrow")
    @JvmField val PLUS = token("plus")
    @JvmField val PLUS_EQUAL = token("plus_equal")
    @JvmField val MINUS = token("minus")
    @JvmField val MINUS_EQUAL = token("minus_equal")
    @JvmField val STAR = token("star")
    @JvmField val STAR_EQUAL = token("star_equal")
    @JvmField val SLASH = token("slash")
    @JvmField val SLASH_EQUAL = token("slash_equal")
    @JvmField val PERCENT = token("percent")
    @JvmField val PERCENT_EQUAL = token("percent_equal")
    @JvmField val EQUAL = token("equal")
    @JvmField val EQUAL_EQUAL = token("equal_equal")
    @JvmField val BANG_EQUAL = token("bang_equal")
    @JvmField val LESS = token("less")
    @JvmField val LESS_EQUAL = token("less_equal")
    @JvmField val SPACESHIP = token("spaceship")
    @JvmField val GREATER = token("greater")
    @JvmField val GREATER_EQUAL = token("greater_equal")
    @JvmField val AMP = token("amp")
    @JvmField val AMP_EQUAL = token("amp_equal")
    @JvmField val PIPE = token("pipe")
    @JvmField val PIPE_EQUAL = token("pipe_equal")
    @JvmField val CARET = token("caret")
    @JvmField val CARET_EQUAL = token("caret_equal")
    @JvmField val BANG = token("bang")
    @JvmField val TILDE = token("tilde")
    @JvmField val LESS_LESS = token("less_less")
    @JvmField val LESS_LESS_EQUAL = token("less_less_equal")
    @JvmField val GREATER_GREATER = token("greater_greater")
    @JvmField val GREATER_GREATER_EQUAL = token("greater_greater_equal")
    @JvmField val PLUS_PLUS = token("plus_plus")
    @JvmField val MINUS_MINUS = token("minus_minus")
    @JvmField val QUESTION = token("question")

    @JvmField
    val KEYWORD_TOKENS: TokenSet = TokenSet.create(
        KW_LET, KW_CONST, KW_IF, KW_ELSE, KW_WHILE, KW_DO, KW_FOR, KW_BREAK, KW_CONTINUE, KW_RETURN,
        KW_IMPORT, KW_EXPORT, KW_MODULE, KW_STRUCT, KW_ENUM, KW_IMPL, KW_CONCEPT, KW_OPERATOR,
        KW_AS, KW_TRUE, KW_FALSE, KW_NULLPTR, KW_AND, KW_OR, KW_NOT, KW_REF, KW_MOVE, KW_LIKE,
        KW_FORWARD, KW_NEW, KW_DELETE,
    )

    @JvmField
    val CONTROL_KEYWORD_TOKENS: TokenSet = TokenSet.create(
        KW_IF, KW_ELSE, KW_WHILE, KW_DO, KW_FOR, KW_BREAK, KW_CONTINUE, KW_RETURN,
    )

    @JvmField
    val DECLARATION_KEYWORD_TOKENS: TokenSet = TokenSet.create(
        KW_LET, KW_CONST, KW_STRUCT, KW_ENUM, KW_IMPL, KW_CONCEPT, KW_OPERATOR,
    )

    @JvmField
    val MODULE_KEYWORD_TOKENS: TokenSet = TokenSet.create(KW_IMPORT, KW_EXPORT, KW_MODULE)

    @JvmField
    val BOOLEAN_LITERAL_TOKENS: TokenSet = TokenSet.create(KW_TRUE, KW_FALSE)

    @JvmField
    val NULL_LITERAL_TOKENS: TokenSet = TokenSet.create(KW_NULLPTR)

    @JvmField
    val COMMENT_TOKENS: TokenSet = TokenSet.create(LINE_COMMENT, BLOCK_COMMENT)

    @JvmField
    val STRING_TOKENS: TokenSet = TokenSet.create(STRING_LITERAL, CHAR_LITERAL)

    @JvmField
    val SYMBOL_TOKENS: TokenSet = TokenSet.create(
        L_PAREN, R_PAREN, L_BRACE, R_BRACE, L_BRACKET, R_BRACKET, COMMA, SEMICOLON, COLON,
        COLON_COLON, DOT, ARROW, PLUS, PLUS_EQUAL, MINUS, MINUS_EQUAL, STAR, STAR_EQUAL, SLASH,
        SLASH_EQUAL, PERCENT, PERCENT_EQUAL, EQUAL, EQUAL_EQUAL, BANG_EQUAL, LESS, LESS_EQUAL,
        SPACESHIP, GREATER, GREATER_EQUAL, AMP, AMP_EQUAL, PIPE, PIPE_EQUAL, CARET, CARET_EQUAL,
        TILDE, LESS_LESS, LESS_LESS_EQUAL, GREATER_GREATER, GREATER_GREATER_EQUAL, PLUS_PLUS,
        MINUS_MINUS, QUESTION, BANG,
    )

    @JvmField
    val OPERATOR_TOKENS: TokenSet = TokenSet.create(
        ARROW, PLUS, PLUS_EQUAL, MINUS, MINUS_EQUAL, STAR, STAR_EQUAL, SLASH, SLASH_EQUAL,
        PERCENT, PERCENT_EQUAL, EQUAL, EQUAL_EQUAL, BANG_EQUAL, LESS, LESS_EQUAL, GREATER,
        GREATER_EQUAL, SPACESHIP, AMP, AMP_EQUAL, PIPE, PIPE_EQUAL, CARET, CARET_EQUAL, BANG, TILDE,
        LESS_LESS, LESS_LESS_EQUAL, GREATER_GREATER, GREATER_GREATER_EQUAL, PLUS_PLUS,
        MINUS_MINUS, QUESTION, KW_AS, KW_AND, KW_OR, KW_NOT, KW_REF, KW_MOVE, KW_FORWARD, KW_NEW, KW_DELETE,
    )

    @JvmField
    val PUNCTUATION_TOKENS: TokenSet = TokenSet.create(
        L_PAREN, R_PAREN, L_BRACE, R_BRACE, L_BRACKET, R_BRACKET, COMMA, SEMICOLON,
        COLON, COLON_COLON, DOT,
    )

    private val keywordMap: Map<String, IElementType> = mapOf(
        "let" to KW_LET,
        "const" to KW_CONST,
        "if" to KW_IF,
        "else" to KW_ELSE,
        "while" to KW_WHILE,
        "do" to KW_DO,
        "for" to KW_FOR,
        "break" to KW_BREAK,
        "continue" to KW_CONTINUE,
        "return" to KW_RETURN,
        "import" to KW_IMPORT,
        "export" to KW_EXPORT,
        "module" to KW_MODULE,
        "struct" to KW_STRUCT,
        "enum" to KW_ENUM,
        "impl" to KW_IMPL,
        "concept" to KW_CONCEPT,
        "operator" to KW_OPERATOR,
        "as" to KW_AS,
        "true" to KW_TRUE,
        "false" to KW_FALSE,
        "nullptr" to KW_NULLPTR,
        "and" to KW_AND,
        "or" to KW_OR,
        "not" to KW_NOT,
        "ref" to KW_REF,
        "move" to KW_MOVE,
        "like" to KW_LIKE,
        "forward" to KW_FORWARD,
        "new" to KW_NEW,
        "delete" to KW_DELETE,
    )

    val punctuators: List<Pair<String, IElementType>> = listOf(
        "<=>" to SPACESHIP,
        "<<=" to LESS_LESS_EQUAL,
        ">>=" to GREATER_GREATER_EQUAL,
        "::" to COLON_COLON,
        "->" to ARROW,
        "==" to EQUAL_EQUAL,
        "!=" to BANG_EQUAL,
        "<=" to LESS_EQUAL,
        ">=" to GREATER_EQUAL,
        "<<" to LESS_LESS,
        ">>" to GREATER_GREATER,
        "++" to PLUS_PLUS,
        "--" to MINUS_MINUS,
        "+=" to PLUS_EQUAL,
        "-=" to MINUS_EQUAL,
        "*=" to STAR_EQUAL,
        "/=" to SLASH_EQUAL,
        "%=" to PERCENT_EQUAL,
        "&=" to AMP_EQUAL,
        "|=" to PIPE_EQUAL,
        "^=" to CARET_EQUAL,
        "(" to L_PAREN,
        ")" to R_PAREN,
        "{" to L_BRACE,
        "}" to R_BRACE,
        "[" to L_BRACKET,
        "]" to R_BRACKET,
        "," to COMMA,
        ";" to SEMICOLON,
        ":" to COLON,
        "." to DOT,
        "+" to PLUS,
        "-" to MINUS,
        "*" to STAR,
        "/" to SLASH,
        "%" to PERCENT,
        "=" to EQUAL,
        "<" to LESS,
        ">" to GREATER,
        "&" to AMP,
        "|" to PIPE,
        "^" to CARET,
        "!" to BANG,
        "~" to TILDE,
        "?" to QUESTION,
    )

    fun keyword(text: String): IElementType? = keywordMap[text]
}

object CpTrivia {
    val recoveryDelimiters: Set<Char> = " \t\r\n(){}[],:;.+-*/%<>=&|^~!?\"'".toSet()

    fun isIdentifierStart(ch: Char): Boolean = ch == '_' || ch in 'a'..'z' || ch in 'A'..'Z'

    fun isIdentifierContinue(ch: Char): Boolean = isIdentifierStart(ch) || ch in '0'..'9'

    fun isSimpleEscape(ch: Char): Boolean = "\\'\"?abfnrtv".contains(ch)

    fun isOctalDigit(ch: Char): Boolean = ch in '0'..'7'

    fun isHexDigit(ch: Char): Boolean = ch in '0'..'9' || ch in 'a'..'f' || ch in 'A'..'F'
}

fun IElementType.debugName(): String = toString()
