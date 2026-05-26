package org.cp.lang.clion

import com.intellij.lexer.LexerBase
import com.intellij.psi.TokenType
import com.intellij.psi.tree.IElementType

class CpLexer : LexerBase() {
    private var buffer: CharSequence = ""
    private var startOffset: Int = 0
    private var endOffset: Int = 0
    private var tokenStart: Int = 0
    private var tokenEnd: Int = 0
    private var tokenType: IElementType? = null

    override fun start(buffer: CharSequence, startOffset: Int, endOffset: Int, initialState: Int) {
        this.buffer = buffer
        this.startOffset = startOffset
        this.endOffset = endOffset
        advanceFrom(startOffset)
    }

    override fun getState(): Int = 0

    override fun getTokenType(): IElementType? = tokenType

    override fun getTokenStart(): Int = tokenStart

    override fun getTokenEnd(): Int = tokenEnd

    override fun advance() {
        advanceFrom(tokenEnd)
    }

    override fun getBufferSequence(): CharSequence = buffer

    override fun getBufferEnd(): Int = endOffset

    private fun advanceFrom(offset: Int) {
        tokenStart = offset
        if (offset >= endOffset) {
            tokenType = null
            tokenEnd = offset
            return
        }

        val ch = buffer[offset]
        when {
            ch.isWhitespace() -> scanWhitespace(offset)
            ch == '/' && offset + 1 < endOffset && buffer[offset + 1] == '/' -> scanLineComment(offset)
            ch == '/' && offset + 1 < endOffset && buffer[offset + 1] == '*' -> scanBlockComment(offset)
            CpTrivia.isIdentifierStart(ch) -> scanIdentifier(offset)
            ch.isDigit() -> scanNumber(offset)
            ch == '"' -> scanString(offset)
            ch == '\'' -> scanChar(offset)
            else -> scanSymbolOrInvalid(offset)
        }
    }

    private fun scanWhitespace(offset: Int) {
        var index = offset + 1
        while (index < endOffset && buffer[index].isWhitespace()) {
            index += 1
        }
        tokenType = TokenType.WHITE_SPACE
        tokenEnd = index
    }

    private fun scanLineComment(offset: Int) {
        var index = offset + 2
        while (index < endOffset && buffer[index] != '\n') {
            index += 1
        }
        tokenType = CpTypes.LINE_COMMENT
        tokenEnd = index
    }

    private fun scanBlockComment(offset: Int) {
        var index = offset + 2
        while (index + 1 < endOffset) {
            if (buffer[index] == '*' && buffer[index + 1] == '/') {
                tokenType = CpTypes.BLOCK_COMMENT
                tokenEnd = index + 2
                return
            }
            index += 1
        }

        tokenType = CpTypes.INVALID
        tokenEnd = endOffset
    }

    private fun scanIdentifier(offset: Int) {
        var index = offset + 1
        while (index < endOffset && CpTrivia.isIdentifierContinue(buffer[index])) {
            index += 1
        }

        val text = buffer.subSequence(offset, index).toString()
        tokenType = CpTypes.keyword(text) ?: CpTypes.IDENTIFIER
        tokenEnd = index
    }

    private fun scanNumber(offset: Int) {
        var index = offset
        while (index < endOffset && buffer[index].isDigit()) {
            index += 1
        }

        var isFloat = false
        if (index + 1 < endOffset && buffer[index] == '.' && buffer[index + 1].isDigit()) {
            isFloat = true
            index += 1
            while (index < endOffset && buffer[index].isDigit()) {
                index += 1
            }
        }

        if (index < endOffset && (buffer[index] == 'e' || buffer[index] == 'E')) {
            var probe = index + 1
            if (probe < endOffset && (buffer[probe] == '+' || buffer[probe] == '-')) {
                probe += 1
            }
            if (probe < endOffset && buffer[probe].isDigit()) {
                isFloat = true
                index = probe + 1
                while (index < endOffset && buffer[index].isDigit()) {
                    index += 1
                }
            }
        }

        if (index < endOffset && CpTrivia.isIdentifierStart(buffer[index])) {
            while (index < endOffset && buffer[index] !in CpTrivia.recoveryDelimiters) {
                index += 1
            }
            tokenType = CpTypes.INVALID
            tokenEnd = index
            return
        }

        tokenType = if (isFloat) CpTypes.FLOAT_LITERAL else CpTypes.INTEGER_LITERAL
        tokenEnd = index
    }

    private fun scanString(offset: Int) {
        var index = offset + 1
        var hasInvalidEscape = false

        while (index < endOffset) {
            when (buffer[index]) {
                '"' -> {
                    tokenType = if (hasInvalidEscape) CpTypes.INVALID else CpTypes.STRING_LITERAL
                    tokenEnd = index + 1
                    return
                }

                '\n' -> {
                    tokenType = CpTypes.INVALID
                    tokenEnd = index
                    return
                }

                '\\' -> {
                    val result = consumeEscape(index)
                    index = result.nextIndex
                    if (result.unterminated) {
                        tokenType = CpTypes.INVALID
                        tokenEnd = index
                        return
                    }
                    if (result.invalid) {
                        hasInvalidEscape = true
                    }
                }

                else -> index += 1
            }
        }

        tokenType = CpTypes.INVALID
        tokenEnd = index
    }

    private fun scanChar(offset: Int) {
        var index = offset + 1
        var contentCount = 0
        var hasInvalidEscape = false

        while (index < endOffset) {
            when (buffer[index]) {
                '\'' -> {
                    tokenType = if (!hasInvalidEscape && contentCount == 1) {
                        CpTypes.CHAR_LITERAL
                    } else {
                        CpTypes.INVALID
                    }
                    tokenEnd = index + 1
                    return
                }

                '\n' -> {
                    tokenType = CpTypes.INVALID
                    tokenEnd = index
                    return
                }

                '\\' -> {
                    val result = consumeEscape(index)
                    index = result.nextIndex
                    if (result.unterminated) {
                        tokenType = CpTypes.INVALID
                        tokenEnd = index
                        return
                    }
                    if (result.invalid) {
                        hasInvalidEscape = true
                    }
                    contentCount += 1
                }

                else -> {
                    contentCount += 1
                    index += 1
                }
            }
        }

        tokenType = CpTypes.INVALID
        tokenEnd = index
    }

    private fun scanSymbolOrInvalid(offset: Int) {
        when (buffer[offset]) {
            '(' -> acceptSymbol(CpTypes.L_PAREN, offset, 1)
            ')' -> acceptSymbol(CpTypes.R_PAREN, offset, 1)
            '{' -> acceptSymbol(CpTypes.L_BRACE, offset, 1)
            '}' -> acceptSymbol(CpTypes.R_BRACE, offset, 1)
            '[' -> acceptSymbol(CpTypes.L_BRACKET, offset, 1)
            ']' -> acceptSymbol(CpTypes.R_BRACKET, offset, 1)
            ',' -> acceptSymbol(CpTypes.COMMA, offset, 1)
            ';' -> acceptSymbol(CpTypes.SEMICOLON, offset, 1)
            ':' -> if (matchesAt(offset, "::")) {
                acceptSymbol(CpTypes.COLON_COLON, offset, 2)
            } else {
                acceptSymbol(CpTypes.COLON, offset, 1)
            }
            '.' -> acceptSymbol(CpTypes.DOT, offset, 1)
            '+' -> when {
                matchesAt(offset, "++") -> acceptSymbol(CpTypes.PLUS_PLUS, offset, 2)
                matchesAt(offset, "+=") -> acceptSymbol(CpTypes.PLUS_EQUAL, offset, 2)
                else -> acceptSymbol(CpTypes.PLUS, offset, 1)
            }
            '-' -> when {
                matchesAt(offset, "->") -> acceptSymbol(CpTypes.ARROW, offset, 2)
                matchesAt(offset, "--") -> acceptSymbol(CpTypes.MINUS_MINUS, offset, 2)
                matchesAt(offset, "-=") -> acceptSymbol(CpTypes.MINUS_EQUAL, offset, 2)
                else -> acceptSymbol(CpTypes.MINUS, offset, 1)
            }
            '*' -> if (matchesAt(offset, "*=")) {
                acceptSymbol(CpTypes.STAR_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.STAR, offset, 1)
            }
            '/' -> if (matchesAt(offset, "/=")) {
                acceptSymbol(CpTypes.SLASH_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.SLASH, offset, 1)
            }
            '%' -> if (matchesAt(offset, "%=")) {
                acceptSymbol(CpTypes.PERCENT_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.PERCENT, offset, 1)
            }
            '=' -> if (matchesAt(offset, "==")) {
                acceptSymbol(CpTypes.EQUAL_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.EQUAL, offset, 1)
            }
            '!' -> if (matchesAt(offset, "!=")) {
                acceptSymbol(CpTypes.BANG_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.BANG, offset, 1)
            }
            '<' -> scanLess(offset)
            '>' -> scanGreater(offset)
            '&' -> if (matchesAt(offset, "&=")) {
                acceptSymbol(CpTypes.AMP_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.AMP, offset, 1)
            }
            '|' -> if (matchesAt(offset, "|=")) {
                acceptSymbol(CpTypes.PIPE_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.PIPE, offset, 1)
            }
            '^' -> if (matchesAt(offset, "^=")) {
                acceptSymbol(CpTypes.CARET_EQUAL, offset, 2)
            } else {
                acceptSymbol(CpTypes.CARET, offset, 1)
            }
            '~' -> acceptSymbol(CpTypes.TILDE, offset, 1)
            '?' -> acceptSymbol(CpTypes.QUESTION, offset, 1)
            else -> {
                tokenType = CpTypes.INVALID
                tokenEnd = offset + 1
            }
        }
    }

    private fun scanLess(offset: Int) {
        when {
            matchesAt(offset, "<=>") -> acceptSymbol(CpTypes.SPACESHIP, offset, 3)
            matchesAt(offset, "<<=") -> acceptSymbol(CpTypes.LESS_LESS_EQUAL, offset, 3)
            matchesAt(offset, "<<") -> acceptSymbol(CpTypes.LESS_LESS, offset, 2)
            matchesAt(offset, "<=") -> acceptSymbol(CpTypes.LESS_EQUAL, offset, 2)
            else -> acceptSymbol(CpTypes.LESS, offset, 1)
        }
    }

    private fun scanGreater(offset: Int) {
        when {
            matchesAt(offset, ">>=") -> acceptSymbol(CpTypes.GREATER_GREATER_EQUAL, offset, 3)
            matchesAt(offset, ">>") -> acceptSymbol(CpTypes.GREATER_GREATER, offset, 2)
            matchesAt(offset, ">=") -> acceptSymbol(CpTypes.GREATER_EQUAL, offset, 2)
            else -> acceptSymbol(CpTypes.GREATER, offset, 1)
        }
    }

    private fun acceptSymbol(type: IElementType, offset: Int, length: Int) {
        tokenType = type
        tokenEnd = offset + length
    }

    private fun matchesAt(offset: Int, spelling: String): Boolean {
        if (offset + spelling.length > endOffset) {
            return false
        }
        for (index in spelling.indices) {
            if (buffer[offset + index] != spelling[index]) {
                return false
            }
        }
        return true
    }

    private fun consumeEscape(offset: Int): EscapeResult {
        var index = offset + 1
        if (index >= endOffset || buffer[index] == '\n') {
            return EscapeResult(index, invalid = false, unterminated = true)
        }

        val current = buffer[index]
        if (CpTrivia.isSimpleEscape(current)) {
            return EscapeResult(index + 1, invalid = false, unterminated = false)
        }
        if (CpTrivia.isOctalDigit(current)) {
            index += 1
            repeat(2) {
                if (index < endOffset && CpTrivia.isOctalDigit(buffer[index])) {
                    index += 1
                }
            }
            return EscapeResult(index, invalid = false, unterminated = false)
        }
        if (current == 'x') {
            index += 1
            if (index >= endOffset || !CpTrivia.isHexDigit(buffer[index])) {
                return EscapeResult(index, invalid = true, unterminated = false)
            }
            while (index < endOffset && CpTrivia.isHexDigit(buffer[index])) {
                index += 1
            }
            return EscapeResult(index, invalid = false, unterminated = false)
        }

        return EscapeResult(index + 1, invalid = true, unterminated = false)
    }

    private data class EscapeResult(
        val nextIndex: Int,
        val invalid: Boolean,
        val unterminated: Boolean,
    )
}
