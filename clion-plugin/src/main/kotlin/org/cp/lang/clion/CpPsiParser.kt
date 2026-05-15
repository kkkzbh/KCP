package org.cp.lang.clion

import com.intellij.lang.ASTNode
import com.intellij.lang.PsiBuilder
import com.intellij.lang.PsiParser
import com.intellij.psi.tree.IElementType

class CpPsiParser : PsiParser {
    override fun parse(root: IElementType, builder: PsiBuilder): ASTNode {
        CpBuilder(builder).parseTranslationUnit(root)
        return builder.treeBuilt
    }
}

private class CpBuilder(
    private val builder: PsiBuilder,
) {
    private var syntheticGreaterClosers = 0

    fun parseTranslationUnit(root: IElementType) {
        val file = builder.mark()
        if (at(CpTypes.KW_EXPORT) && lookAhead(1) == CpTypes.KW_MODULE) {
            parseModuleHeader()
        }
        while (at(CpTypes.KW_IMPORT)) {
            parseImportDeclaration()
        }
        while (!builder.eof()) {
            if (startsFunction()) {
                parseFunction()
            } else {
                builder.error("expected top-level function")
                advance()
                synchronizeTopLevel()
            }
        }
        file.done(root)
    }

    private fun parseModuleHeader() {
        val marker = builder.mark()
        expect(CpTypes.KW_EXPORT, "expected 'export'")
        expect(CpTypes.KW_MODULE, "expected 'module'")
        parseModuleName(CpElements.MODULE_NAME)
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.MODULE_HEADER)
    }

    private fun parseImportDeclaration() {
        val marker = builder.mark()
        expect(CpTypes.KW_IMPORT, "expected 'import'")
        parseModuleName(CpElements.IMPORT_NAME)
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.IMPORT_DECLARATION)
    }

    private fun parseModuleName(type: IElementType) {
        val marker = builder.mark()
        expectIdentifier("expected module name")
        while (consume(CpTypes.DOT)) {
            expectIdentifier("expected module name component")
        }
        marker.done(type)
    }

    private fun startsFunction(): Boolean =
        at(CpTypes.IDENTIFIER) || (at(CpTypes.KW_EXPORT) && lookAhead(1) == CpTypes.IDENTIFIER)

    private fun parseFunction() {
        val marker = builder.mark()
        consume(CpTypes.KW_EXPORT)
        markIdentifier(CpElements.FUNCTION_NAME, "expected function name")
        expect(CpTypes.L_PAREN, "expected '('")
        parseParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        parseBlock()
        marker.done(CpElements.FUNCTION)
    }

    private fun parseParameterList() {
        val marker = builder.mark()
        if (!at(CpTypes.R_PAREN)) {
            parseParameter()
            while (consume(CpTypes.COMMA)) {
                parseParameter()
            }
        }
        marker.done(CpElements.PARAMETER_LIST)
    }

    private fun parseParameter() {
        val marker = builder.mark()
        consume(CpTypes.KW_CONST)
        markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
        expect(CpTypes.COLON, "expected ':'")
        parseType()
        marker.done(CpElements.PARAMETER)
    }

    private fun parseType(): Boolean {
        if (!at(CpTypes.IDENTIFIER)) {
            builder.error("expected type")
            return false
        }

        val marker = builder.mark()
        markIdentifier(CpElements.TYPE_NAME, "expected type name")
        if (consume(CpTypes.LESS)) {
            parseTypeArgumentList()
            expectClosingAngle()
        }
        consume(CpTypes.KW_CONST)
        while (consume(CpTypes.STAR)) {
        }
        consume(CpTypes.AMP)
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseTypeArgumentList() {
        val marker = builder.mark()
        if (!at(CpTypes.GREATER) && !at(CpTypes.GREATER_GREATER)) {
            parseTypeArgument()
            while (consume(CpTypes.COMMA)) {
                parseTypeArgument()
            }
        }
        marker.done(CpElements.TYPE_ARGUMENT_LIST)
    }

    private fun parseTypeArgument() {
        val marker = builder.mark()
        when {
            at(CpTypes.INTEGER_LITERAL) -> advance()
            at(CpTypes.IDENTIFIER) -> parseType()
            else -> builder.error("expected type argument")
        }
        marker.done(CpElements.TYPE_ARGUMENT)
    }

    private fun expectClosingAngle(): Boolean {
        if (syntheticGreaterClosers > 0) {
            syntheticGreaterClosers -= 1
            return true
        }
        if (consume(CpTypes.GREATER)) {
            return true
        }
        if (consume(CpTypes.GREATER_GREATER)) {
            syntheticGreaterClosers += 1
            return true
        }
        builder.error("expected '>'")
        return false
    }

    private fun parseBlock(): Boolean {
        val marker = builder.mark()
        if (!expect(CpTypes.L_BRACE, "expected '{'")) {
            marker.drop()
            return false
        }
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            if (!parseStatement()) {
                advance()
                synchronizeStatement()
            }
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.BLOCK)
        return true
    }

    private fun parseStatement(): Boolean {
        return when {
            at(CpTypes.L_BRACE) -> parseBlock()
            at(CpTypes.KW_LET) || at(CpTypes.KW_CONST) -> {
                parseDeclarationStatement()
                true
            }
            at(CpTypes.KW_IF) -> {
                parseIfStatement()
                true
            }
            at(CpTypes.KW_WHILE) -> {
                parseWhileStatement()
                true
            }
            at(CpTypes.KW_DO) -> {
                parseDoWhileStatement()
                true
            }
            at(CpTypes.KW_FOR) -> {
                parseForStatement()
                true
            }
            at(CpTypes.KW_BREAK) -> {
                parseLoopJump(CpElements.BREAK_STATEMENT)
                true
            }
            at(CpTypes.KW_CONTINUE) -> {
                parseLoopJump(CpElements.CONTINUE_STATEMENT)
                true
            }
            at(CpTypes.KW_RETURN) -> {
                parseReturnStatement()
                true
            }
            startsExpression() -> {
                parseExpressionStatement()
                true
            }
            else -> {
                builder.error("expected statement")
                false
            }
        }
    }

    private fun parseDeclarationStatement() {
        val marker = builder.mark()
        advance()
        markIdentifier(CpElements.LOCAL_DECLARATION, "expected declaration name")
        if (consume(CpTypes.COLON)) {
            parseType()
        }
        expect(CpTypes.EQUAL, "expected '='")
        parseExpression()
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.DECLARATION_STATEMENT)
    }

    private fun parseIfStatement() {
        val marker = builder.mark()
        expect(CpTypes.KW_IF, "expected 'if'")
        parseParenthesizedExpression()
        parseBlock()
        if (consume(CpTypes.KW_ELSE)) {
            if (at(CpTypes.KW_IF)) {
                parseIfStatement()
            } else {
                parseBlock()
            }
        }
        marker.done(CpElements.IF_STATEMENT)
    }

    private fun parseWhileStatement() {
        val marker = builder.mark()
        expect(CpTypes.KW_WHILE, "expected 'while'")
        parseParenthesizedExpression()
        parseBlock()
        marker.done(CpElements.WHILE_STATEMENT)
    }

    private fun parseDoWhileStatement() {
        val marker = builder.mark()
        expect(CpTypes.KW_DO, "expected 'do'")
        parseBlock()
        expect(CpTypes.KW_WHILE, "expected 'while'")
        parseParenthesizedExpression()
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.DO_WHILE_STATEMENT)
    }

    private fun parseForStatement() {
        val marker = builder.mark()
        expect(CpTypes.KW_FOR, "expected 'for'")
        if (consume(CpTypes.COLON)) {
            markIdentifier(CpElements.LOOP_LABEL, "expected loop label")
        }
        expect(CpTypes.L_PAREN, "expected '('")
        if (at(CpTypes.KW_LET) || at(CpTypes.KW_CONST)) {
            advance()
        } else {
            builder.error("expected 'let' or 'const'")
        }
        markIdentifier(CpElements.LOCAL_DECLARATION, "expected for binding name")
        expect(CpTypes.COLON, "expected ':'")
        parseExpression()
        expect(CpTypes.R_PAREN, "expected ')'")
        parseBlock()
        marker.done(CpElements.FOR_STATEMENT)
    }

    private fun parseLoopJump(type: IElementType) {
        val marker = builder.mark()
        advance()
        if (at(CpTypes.IDENTIFIER)) {
            markIdentifier(CpElements.LOOP_LABEL, "expected loop label")
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(type)
    }

    private fun parseReturnStatement() {
        val marker = builder.mark()
        expect(CpTypes.KW_RETURN, "expected 'return'")
        if (!at(CpTypes.SEMICOLON)) {
            parseExpression()
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.RETURN_STATEMENT)
    }

    private fun parseExpressionStatement() {
        val marker = builder.mark()
        parseExpression()
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.EXPRESSION_STATEMENT)
    }

    private fun parseParenthesizedExpression() {
        expect(CpTypes.L_PAREN, "expected '('")
        parseExpression()
        expect(CpTypes.R_PAREN, "expected ')'")
    }

    private fun parseExpression(): PsiBuilder.Marker? = parseAssignment()

    private fun parseAssignment(): PsiBuilder.Marker? {
        val left = parseExpressionPratt(0) ?: return null
        if (!isAssignmentOperator(token())) {
            return left
        }
        val marker = left.precede()
        advance()
        parseAssignment()
        marker.done(CpElements.ASSIGNMENT_EXPRESSION)
        return marker
    }

    private fun parseExpressionPratt(minBindingPower: Int): PsiBuilder.Marker? {
        var left = parseUnary() ?: return null
        while (true) {
            val operator = binaryOperator(token()) ?: break
            if (operator.leftBindingPower < minBindingPower) {
                break
            }
            val marker = left.precede()
            advance()
            if (operator.kind == CpTypes.KW_AS) {
                parseType()
                marker.done(CpElements.CAST_EXPRESSION)
            } else {
                parseExpressionPratt(operator.rightBindingPower)
                marker.done(CpElements.BINARY_EXPRESSION)
            }
            left = marker
        }
        return left
    }

    private fun parseUnary(): PsiBuilder.Marker? {
        if (isPrefixOperator(token())) {
            val marker = builder.mark()
            advance()
            parseUnary()
            marker.done(CpElements.UNARY_EXPRESSION)
            return marker
        }
        return parsePostfix()
    }

    private fun parsePostfix(): PsiBuilder.Marker? {
        var operand = parsePrimary() ?: return null
        while (true) {
            if (at(CpTypes.L_PAREN)) {
                val marker = operand.precede()
                parseArgumentList()
                marker.done(CpElements.CALL_EXPRESSION)
                operand = marker
                continue
            }
            if (at(CpTypes.PLUS_PLUS) || at(CpTypes.MINUS_MINUS)) {
                val marker = operand.precede()
                advance()
                marker.done(CpElements.UNARY_EXPRESSION)
                operand = marker
                continue
            }
            break
        }
        return operand
    }

    private fun parseArgumentList() {
        val marker = builder.mark()
        expect(CpTypes.L_PAREN, "expected '('")
        if (!at(CpTypes.R_PAREN)) {
            parseExpression()
            while (consume(CpTypes.COMMA)) {
                parseExpression()
            }
        }
        expect(CpTypes.R_PAREN, "expected ')'")
        marker.done(CpElements.ARGUMENT_LIST)
    }

    private fun parsePrimary(): PsiBuilder.Marker? {
        return when {
            at(CpTypes.IDENTIFIER) -> {
                val marker = builder.mark()
                advance()
                marker.done(CpElements.NAME_EXPRESSION)
                marker
            }
            isLiteral(token()) -> {
                val marker = builder.mark()
                advance()
                marker.done(CpElements.LITERAL_EXPRESSION)
                marker
            }
            at(CpTypes.L_BRACKET) -> parseDelimitedExpressionList(CpTypes.L_BRACKET, CpTypes.R_BRACKET, CpElements.ARRAY_LITERAL)
            at(CpTypes.L_BRACE) -> parseDelimitedExpressionList(CpTypes.L_BRACE, CpTypes.R_BRACE, CpElements.SEQUENCE_LITERAL)
            at(CpTypes.L_PAREN) -> parseParenExpression()
            else -> {
                builder.error("expected expression")
                null
            }
        }
    }

    private fun parseDelimitedExpressionList(
        open: IElementType,
        close: IElementType,
        type: IElementType,
    ): PsiBuilder.Marker {
        val marker = builder.mark()
        expect(open, "expected delimiter")
        if (!at(close)) {
            parseExpression()
            while (consume(CpTypes.COMMA)) {
                parseExpression()
            }
        }
        expect(close, "expected delimiter")
        marker.done(type)
        return marker
    }

    private fun parseParenExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expect(CpTypes.L_PAREN, "expected '('")
        parseExpression()
        var tuple = false
        while (consume(CpTypes.COMMA)) {
            tuple = true
            parseExpression()
        }
        expect(CpTypes.R_PAREN, "expected ')'")
        marker.done(if (tuple) CpElements.TUPLE_LITERAL else CpElements.GROUPED_EXPRESSION)
        return marker
    }

    private fun startsExpression(): Boolean =
        at(CpTypes.IDENTIFIER) ||
            isLiteral(token()) ||
            at(CpTypes.L_PAREN) ||
            at(CpTypes.L_BRACKET) ||
            at(CpTypes.L_BRACE) ||
            isPrefixOperator(token())

    private fun markIdentifier(type: IElementType, message: String): Boolean {
        val marker = builder.mark()
        if (!expectIdentifier(message)) {
            marker.drop()
            return false
        }
        marker.done(type)
        return true
    }

    private fun expectIdentifier(message: String): Boolean = expect(CpTypes.IDENTIFIER, message)

    private fun expect(type: IElementType, message: String): Boolean {
        if (consume(type)) {
            return true
        }
        builder.error(message)
        return false
    }

    private fun consume(type: IElementType): Boolean {
        if (type == CpTypes.GREATER && syntheticGreaterClosers > 0) {
            syntheticGreaterClosers -= 1
            return true
        }
        if (!atRaw(type)) {
            return false
        }
        advance()
        return true
    }

    private fun at(type: IElementType): Boolean =
        (type == CpTypes.GREATER && syntheticGreaterClosers > 0) || atRaw(type)

    private fun atRaw(type: IElementType): Boolean = token() == type

    private fun token(): IElementType? = builder.tokenType

    private fun lookAhead(steps: Int): IElementType? = builder.lookAhead(steps)

    private fun advance() {
        if (!builder.eof()) {
            builder.advanceLexer()
        }
    }

    private fun synchronizeTopLevel() {
        while (!builder.eof() && !startsFunction() && !at(CpTypes.KW_IMPORT)) {
            if (consume(CpTypes.SEMICOLON)) {
                return
            }
            advance()
        }
    }

    private fun synchronizeStatement() {
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            if (consume(CpTypes.SEMICOLON)) {
                return
            }
            advance()
        }
    }

    private data class BinaryOperator(
        val kind: IElementType,
        val leftBindingPower: Int,
        val rightBindingPower: Int,
    )

    private companion object {
        private val BINARY_OPERATORS = listOf(
            BinaryOperator(CpTypes.KW_OR, 30, 31),
            BinaryOperator(CpTypes.KW_AND, 40, 41),
            BinaryOperator(CpTypes.PIPE, 50, 51),
            BinaryOperator(CpTypes.CARET, 60, 61),
            BinaryOperator(CpTypes.AMP, 70, 71),
            BinaryOperator(CpTypes.EQUAL_EQUAL, 80, 81),
            BinaryOperator(CpTypes.BANG_EQUAL, 80, 81),
            BinaryOperator(CpTypes.LESS, 90, 91),
            BinaryOperator(CpTypes.LESS_EQUAL, 90, 91),
            BinaryOperator(CpTypes.GREATER, 90, 91),
            BinaryOperator(CpTypes.GREATER_EQUAL, 90, 91),
            BinaryOperator(CpTypes.LESS_LESS, 100, 101),
            BinaryOperator(CpTypes.GREATER_GREATER, 100, 101),
            BinaryOperator(CpTypes.PLUS, 110, 111),
            BinaryOperator(CpTypes.MINUS, 110, 111),
            BinaryOperator(CpTypes.STAR, 120, 121),
            BinaryOperator(CpTypes.SLASH, 120, 121),
            BinaryOperator(CpTypes.PERCENT, 120, 121),
            BinaryOperator(CpTypes.KW_AS, 130, 131),
        )

        private val ASSIGNMENT_OPERATORS = setOf(
            CpTypes.EQUAL,
            CpTypes.PLUS_EQUAL,
            CpTypes.MINUS_EQUAL,
            CpTypes.STAR_EQUAL,
            CpTypes.SLASH_EQUAL,
            CpTypes.PERCENT_EQUAL,
            CpTypes.AMP_EQUAL,
            CpTypes.PIPE_EQUAL,
            CpTypes.CARET_EQUAL,
            CpTypes.LESS_LESS_EQUAL,
            CpTypes.GREATER_GREATER_EQUAL,
        )

        private val PREFIX_OPERATORS = setOf(
            CpTypes.PLUS,
            CpTypes.MINUS,
            CpTypes.KW_NOT,
            CpTypes.TILDE,
            CpTypes.AMP,
            CpTypes.STAR,
            CpTypes.PLUS_PLUS,
            CpTypes.MINUS_MINUS,
        )

        private val LITERALS = setOf(
            CpTypes.INTEGER_LITERAL,
            CpTypes.FLOAT_LITERAL,
            CpTypes.CHAR_LITERAL,
            CpTypes.STRING_LITERAL,
            CpTypes.KW_TRUE,
            CpTypes.KW_FALSE,
        )

        private fun binaryOperator(type: IElementType?): BinaryOperator? =
            BINARY_OPERATORS.firstOrNull { it.kind == type }

        private fun isAssignmentOperator(type: IElementType?): Boolean = type in ASSIGNMENT_OPERATORS

        private fun isPrefixOperator(type: IElementType?): Boolean = type in PREFIX_OPERATORS

        private fun isLiteral(type: IElementType?): Boolean = type in LITERALS
    }
}
