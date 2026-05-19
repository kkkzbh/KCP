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
        while (at(CpTypes.KW_IMPORT) || (at(CpTypes.KW_EXPORT) && lookAhead(1) == CpTypes.KW_IMPORT)) {
            parseImportDeclaration()
        }
        while (!builder.eof()) {
            if (parseTopLevelItem()) {
                continue
            }
            builder.error("expected top-level function")
            advance()
            synchronizeTopLevel()
        }
        file.done(root)
    }

    private fun parseTopLevelItem(): Boolean {
        val exported = consume(CpTypes.KW_EXPORT)
        return when {
            at(CpTypes.KW_IMPORT) -> {
                parseImportDeclaration(exportAlreadyConsumed = exported)
                true
            }
            at(CpTypes.KW_STRUCT) -> {
                parseStructDeclaration()
                true
            }
            contextual("variant") -> {
                parseVariantDeclaration()
                true
            }
            at(CpTypes.KW_CONCEPT) -> {
                parseConceptDeclaration()
                true
            }
            at(CpTypes.KW_IMPL) && !exported -> {
                parseImplBlock()
                true
            }
            at(CpTypes.IDENTIFIER) -> {
                parseFunction(CpElements.FUNCTION)
                true
            }
            else -> {
                if (exported) {
                    builder.error("expected exported item")
                }
                false
            }
        }
    }

    private fun parseModuleHeader() {
        val marker = builder.mark()
        expect(CpTypes.KW_EXPORT, "expected 'export'")
        expect(CpTypes.KW_MODULE, "expected 'module'")
        parseModuleName(CpElements.MODULE_NAME)
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.MODULE_HEADER)
    }

    private fun parseImportDeclaration(exportAlreadyConsumed: Boolean = false) {
        val marker = builder.mark()
        if (!exportAlreadyConsumed) {
            consume(CpTypes.KW_EXPORT)
        }
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

    private fun parseStructDeclaration() {
        val marker = builder.mark()
        expect(CpTypes.KW_STRUCT, "expected 'struct'")
        markIdentifier(CpElements.TYPE_NAME, "expected struct name")
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
        if (!expect(CpTypes.L_BRACE, "expected '{'")) {
            marker.done(CpElements.STRUCT_DECLARATION)
            return
        }
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            val offset = builder.currentOffset
            parseStructField()
            recoverIfStalled(offset)
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.STRUCT_DECLARATION)
    }

    private fun parseStructField() {
        val marker = builder.mark()
        markIdentifier(CpElements.FIELD_DECLARATION, "expected field name")
        expect(CpTypes.COLON, "expected ':'")
        parseType()
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.STRUCT_FIELD)
    }

    private fun parseVariantDeclaration() {
        val marker = builder.mark()
        expectContextual("variant")
        markIdentifier(CpElements.TYPE_NAME, "expected variant name")
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
        if (!expect(CpTypes.L_BRACE, "expected '{'")) {
            marker.done(CpElements.VARIANT_DECLARATION)
            return
        }
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            val offset = builder.currentOffset
            parseVariantCase()
            recoverIfStalled(offset)
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.VARIANT_DECLARATION)
    }

    private fun parseVariantCase() {
        val marker = builder.mark()
        markIdentifier(CpElements.VARIANT_CASE_NAME, "expected variant case name")
        if (consume(CpTypes.L_PAREN)) {
            if (!at(CpTypes.R_PAREN)) {
                parseType()
                while (consume(CpTypes.COMMA)) {
                    parseType()
                }
            }
            expect(CpTypes.R_PAREN, "expected ')'")
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.VARIANT_CASE)
    }

    private fun parseConceptDeclaration() {
        val marker = builder.mark()
        expect(CpTypes.KW_CONCEPT, "expected 'concept'")
        markIdentifier(CpElements.TYPE_NAME, "expected concept name")
        if (!expect(CpTypes.L_BRACE, "expected '{'")) {
            marker.done(CpElements.CONCEPT_DECLARATION)
            return
        }
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            val offset = builder.currentOffset
            when {
                contextual("requires") -> parseRequiresClauseUntil(CpTypes.SEMICOLON)
                contextual("type") -> parseTypeAlias(CpElements.TYPE_ALIAS)
                else -> parseFunctionRequirement()
            }
            recoverIfStalled(offset)
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.CONCEPT_DECLARATION)
    }

    private fun parseFunctionRequirement() {
        val marker = builder.mark()
        markIdentifier(CpElements.FUNCTION_NAME, "expected function name")
        expect(CpTypes.L_PAREN, "expected '('")
        parseParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        if (at(CpTypes.SEMICOLON)) {
            advance()
        } else {
            parseBlock()
        }
        marker.done(CpElements.FUNCTION)
    }

    private fun parseImplBlock() {
        val marker = builder.mark()
        expect(CpTypes.KW_IMPL, "expected 'impl'")
        parseType()
        if (consume(CpTypes.KW_FOR)) {
            parseType()
        }
        if (contextual("requires")) {
            parseRequiresClauseUntil(CpTypes.L_BRACE)
        }
        expect(CpTypes.L_BRACE, "expected '{'")
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            when {
                contextual("type") -> parseTypeAlias(CpElements.TYPE_ALIAS)
                at(CpTypes.TILDE) -> parseDestructor()
                at(CpTypes.IDENTIFIER) -> parseFunction(CpElements.FUNCTION)
                else -> {
                    builder.error("expected impl item")
                    advance()
                    synchronizeTopLevel()
                }
            }
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.IMPL_BLOCK)
    }

    private fun parseDestructor() {
        val marker = builder.mark()
        expect(CpTypes.TILDE, "expected '~'")
        markIdentifier(CpElements.FUNCTION_NAME, "expected destructor name")
        expect(CpTypes.L_PAREN, "expected '('")
        expect(CpTypes.R_PAREN, "expected ')'")
        parseBlock()
        marker.done(CpElements.FUNCTION)
    }

    private fun parseFunction(type: IElementType) {
        val marker = builder.mark()
        markIdentifier(CpElements.FUNCTION_NAME, "expected function name")
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
        expect(CpTypes.L_PAREN, "expected '('")
        parseParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.EQUAL)) {
            expectContextual("default")
            expect(CpTypes.SEMICOLON, "expected ';'")
            marker.done(type)
            return
        }
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        if (contextual("requires")) {
            parseRequiresClauseUntil(CpTypes.L_BRACE)
        }
        parseBlock()
        marker.done(type)
    }

    private fun parseGenericParameterList() {
        val marker = builder.mark()
        expect(CpTypes.LESS, "expected '<'")
        if (!at(CpTypes.GREATER) && !at(CpTypes.GREATER_GREATER)) {
            parseGenericParameter()
            while (consume(CpTypes.COMMA)) {
                parseGenericParameter()
            }
        }
        expectClosingAngle()
        marker.done(CpElements.GENERIC_PARAMETER_LIST)
    }

    private fun parseGenericParameter() {
        val marker = builder.mark()
        markIdentifier(CpElements.GENERIC_PARAMETER_NAME, "expected generic parameter")
        consumeEllipsis()
        if (consume(CpTypes.COLON)) {
            expectIdentifier("expected concept bound")
            while (consume(CpTypes.KW_AND)) {
                expectIdentifier("expected concept bound")
            }
        }
        marker.done(CpElements.GENERIC_PARAMETER)
    }

    private fun parseRequiresClauseUntil(end: IElementType) {
        val marker = builder.mark()
        expectContextual("requires")
        var depth = 0
        while (!builder.eof()) {
            val current = token()
            if (depth == 0 && current == end) {
                break
            }
            if (current == CpTypes.L_PAREN || current == CpTypes.L_BRACE || current == CpTypes.L_BRACKET) {
                depth += 1
            } else if (current == CpTypes.R_PAREN || current == CpTypes.R_BRACE || current == CpTypes.R_BRACKET) {
                if (depth == 0) {
                    break
                }
                depth -= 1
            }
            advance()
        }
        if (end == CpTypes.SEMICOLON) {
            expect(CpTypes.SEMICOLON, "expected ';'")
        }
        marker.done(CpElements.REQUIRES_CLAUSE)
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
        if (at(CpTypes.IDENTIFIER) && builder.tokenText == "self" && lookAhead(1) != CpTypes.COLON) {
            markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
            if (consume(CpTypes.KW_CONST)) {
                expect(CpTypes.AMP, "expected '&'")
            } else {
                consume(CpTypes.AMP)
            }
            marker.done(CpElements.PARAMETER)
            return
        }
        markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
        expect(CpTypes.COLON, "expected ':'")
        parseType()
        consumeEllipsis()
        marker.done(CpElements.PARAMETER)
    }

    private fun parseType(): Boolean {
        if (looksLikeFunctionType()) {
            return parseFunctionType()
        }
        if (contextual("decltype") && lookAhead(1) == CpTypes.L_PAREN) {
            return parseDecltype()
        }
        if (!at(CpTypes.IDENTIFIER)) {
            builder.error("expected type")
            return false
        }

        val marker = builder.mark()
        markIdentifier(CpElements.TYPE_NAME, "expected type name")
        if (at(CpTypes.LESS)) {
            parseTypeArgumentList()
        }
        while (consume(CpTypes.COLON_COLON)) {
            markIdentifier(CpElements.TYPE_NAME, "expected associated type name")
        }
        consume(CpTypes.KW_CONST)
        while (consume(CpTypes.STAR)) {
        }
        consume(CpTypes.AMP)
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseFunctionType(): Boolean {
        val marker = builder.mark()
        expectIdentifier("expected function type marker")
        consume(CpTypes.STAR)
        expect(CpTypes.L_PAREN, "expected '('")
        if (!at(CpTypes.R_PAREN)) {
            parseFunctionTypeParameter()
            while (consume(CpTypes.COMMA)) {
                parseFunctionTypeParameter()
            }
        }
        expect(CpTypes.R_PAREN, "expected ')'")
        expect(CpTypes.ARROW, "expected '->'")
        parseType()
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseFunctionTypeParameter() {
        val marker = builder.mark()
        if (at(CpTypes.IDENTIFIER) && lookAhead(1) == CpTypes.COLON) {
            markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
            expect(CpTypes.COLON, "expected ':'")
        }
        parseType()
        marker.done(CpElements.PARAMETER)
    }

    private fun parseDecltype(): Boolean {
        val marker = builder.mark()
        expectIdentifier("expected decltype")
        expect(CpTypes.L_PAREN, "expected '('")
        parseExpression()
        expect(CpTypes.R_PAREN, "expected ')'")
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseTypeArgumentList() {
        val marker = builder.mark()
        expect(CpTypes.LESS, "expected '<'")
        if (!at(CpTypes.GREATER) && !at(CpTypes.GREATER_GREATER)) {
            parseTypeArgument()
            while (consume(CpTypes.COMMA)) {
                parseTypeArgument()
            }
        }
        expectClosingAngle()
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
            contextual("template") && lookAhead(1) == CpTypes.KW_FOR -> {
                parseTemplateForStatement()
                true
            }
            contextual("type") && lookAhead(1) == CpTypes.IDENTIFIER && lookAhead(2) == CpTypes.EQUAL -> {
                parseTypeAlias(CpElements.TYPE_ALIAS_STATEMENT)
                true
            }
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

    private fun parseTypeAlias(type: IElementType) {
        val marker = builder.mark()
        expectContextual("type")
        markIdentifier(CpElements.TYPE_NAME, "expected type alias name")
        if (consume(CpTypes.EQUAL)) {
            parseType()
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(type)
    }

    private fun parseDeclarationStatement() {
        val marker = builder.mark()
        advance()
        consumeContextual("ref")
        if (consume(CpTypes.L_PAREN)) {
            markIdentifier(CpElements.LOCAL_DECLARATION, "expected declaration binding name")
            while (consume(CpTypes.COMMA)) {
                markIdentifier(CpElements.LOCAL_DECLARATION, "expected declaration binding name")
            }
            expect(CpTypes.R_PAREN, "expected ')'")
        } else {
            markIdentifier(CpElements.LOCAL_DECLARATION, "expected declaration name")
        }
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

    private fun parseTemplateForStatement() {
        val marker = builder.mark()
        expectContextual("template")
        expect(CpTypes.KW_FOR, "expected 'for'")
        expect(CpTypes.L_PAREN, "expected '('")
        when {
            at(CpTypes.KW_LET) || at(CpTypes.KW_CONST) -> advance()
            consumeContextual("type") -> Unit
            else -> builder.error("expected template binding kind")
        }
        markIdentifier(CpElements.LOCAL_DECLARATION, "expected template binding name")
        expect(CpTypes.COLON, "expected ':'")
        expectIdentifier("expected pack name")
        consumeEllipsis()
        expect(CpTypes.R_PAREN, "expected ')'")
        parseBlock()
        marker.done(CpElements.TEMPLATE_FOR_STATEMENT)
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
            if (looksLikeTypeArgumentSuffixBefore(CpTypes.COLON_COLON)) {
                val marker = operand.precede()
                parseTypeArgumentList()
                expect(CpTypes.COLON_COLON, "expected '::'")
                markIdentifier(CpElements.ASSOCIATED_NAME, "expected associated name")
                marker.done(CpElements.ASSOCIATED_NAME_EXPRESSION)
                operand = marker
                continue
            }
            if (looksLikeGenericCallSuffix()) {
                val marker = operand.precede()
                parseTypeArgumentList()
                parseArgumentList()
                marker.done(CpElements.CALL_EXPRESSION)
                operand = marker
                continue
            }
            if (consume(CpTypes.COLON_COLON)) {
                val marker = operand.precede()
                markIdentifier(CpElements.ASSOCIATED_NAME, "expected associated name")
                marker.done(CpElements.ASSOCIATED_NAME_EXPRESSION)
                operand = marker
                continue
            }
            if (consume(CpTypes.DOT)) {
                val marker = operand.precede()
                markIdentifier(CpElements.MEMBER_NAME, "expected member name")
                marker.done(CpElements.MEMBER_EXPRESSION)
                operand = marker
                continue
            }
            if (at(CpTypes.L_PAREN)) {
                val marker = operand.precede()
                parseArgumentList()
                marker.done(CpElements.CALL_EXPRESSION)
                operand = marker
                continue
            }
            if (consume(CpTypes.L_BRACKET)) {
                val marker = operand.precede()
                parseExpression()
                expect(CpTypes.R_BRACKET, "expected ']'")
                marker.done(CpElements.INDEX_EXPRESSION)
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
            contextual("match") -> parseMatchExpression()
            looksLikeLambdaExpression() -> parseLambdaExpression()
            looksLikeTypeInitializer() -> parseStructInitExpression()
            looksLikeAssociatedNameExpression() -> parseAssociatedNameExpression()
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
            at(CpTypes.L_BRACKET) -> parseDelimitedExpressionList(
                CpTypes.L_BRACKET,
                CpTypes.R_BRACKET,
                CpElements.ARRAY_LITERAL,
            )
            at(CpTypes.L_PAREN) -> parseParenExpression()
            at(CpTypes.L_BRACE) -> parseBlockExpression()
            else -> {
                builder.error("expected expression")
                null
            }
        }
    }

    private fun parseAssociatedNameExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        parseType()
        expect(CpTypes.COLON_COLON, "expected '::'")
        markIdentifier(CpElements.ASSOCIATED_NAME, "expected associated name")
        marker.done(CpElements.ASSOCIATED_NAME_EXPRESSION)
        return marker
    }

    private fun parseStructInitExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        parseType()
        parseStructInitializerList()
        marker.done(CpElements.STRUCT_INIT_EXPRESSION)
        return marker
    }

    private fun parseStructInitializerList() {
        expect(CpTypes.L_BRACE, "expected '{'")
        if (!at(CpTypes.R_BRACE)) {
            parseStructInitializer()
            while (consume(CpTypes.COMMA) && !at(CpTypes.R_BRACE)) {
                parseStructInitializer()
            }
        }
        expect(CpTypes.R_BRACE, "expected '}'")
    }

    private fun parseStructInitializer() {
        val marker = builder.mark()
        if (consume(CpTypes.DOT)) {
            markIdentifier(CpElements.FIELD_DECLARATION, "expected field name")
            expect(CpTypes.EQUAL, "expected '='")
        }
        parseExpression()
        marker.done(CpElements.STRUCT_INITIALIZER)
    }

    private fun parseMatchExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expectContextual("match")
        if (at(CpTypes.IDENTIFIER) && lookAhead(1) == CpTypes.L_BRACE) {
            val value = builder.mark()
            advance()
            value.done(CpElements.NAME_EXPRESSION)
        } else {
            parseExpression()
        }
        expect(CpTypes.L_BRACE, "expected '{'")
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            parseMatchArm()
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.MATCH_EXPRESSION)
        return marker
    }

    private fun parseMatchArm() {
        val marker = builder.mark()
        parseMatchPattern()
        expect(CpTypes.EQUAL, "expected '='")
        expect(CpTypes.GREATER, "expected '>'")
        parseExpression()
        expect(CpTypes.COMMA, "expected ','")
        marker.done(CpElements.MATCH_ARM)
    }

    private fun parseMatchPattern() {
        val marker = builder.mark()
        if (at(CpTypes.IDENTIFIER) && builder.tokenText == "_") {
            advance()
        } else {
            expect(CpTypes.DOT, "expected '.'")
            markIdentifier(CpElements.VARIANT_CASE_NAME, "expected variant case")
            if (consume(CpTypes.L_PAREN)) {
                markIdentifier(CpElements.MATCH_BINDING, "expected pattern binding")
                while (consume(CpTypes.COMMA)) {
                    markIdentifier(CpElements.MATCH_BINDING, "expected pattern binding")
                }
                expect(CpTypes.R_PAREN, "expected ')'")
            }
        }
        marker.done(CpElements.MATCH_PATTERN)
    }

    private fun parseLambdaExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expectContextual("f")
        expect(CpTypes.L_PAREN, "expected '('")
        parseLambdaParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        if (consume(CpTypes.EQUAL)) {
            expect(CpTypes.GREATER, "expected '>'")
            parseExpression()
        } else {
            parseBlockExpression()
        }
        marker.done(CpElements.LAMBDA_EXPRESSION)
        return marker
    }

    private fun parseLambdaParameterList() {
        if (at(CpTypes.R_PAREN)) {
            return
        }
        parseLambdaParameter()
        while (consume(CpTypes.COMMA)) {
            parseLambdaParameter()
        }
    }

    private fun parseLambdaParameter() {
        val marker = builder.mark()
        markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
        if (consume(CpTypes.COLON)) {
            parseType()
        }
        marker.done(CpElements.PARAMETER)
    }

    private fun parseBlockExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expect(CpTypes.L_BRACE, "expected '{'")
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            if (startsExpression()) {
                val expression = parseExpression()
                if (consume(CpTypes.SEMICOLON)) {
                    val statement = expression?.precede() ?: builder.mark()
                    statement.done(CpElements.EXPRESSION_STATEMENT)
                    continue
                }
                if (at(CpTypes.R_BRACE)) {
                    break
                }
                builder.error("expected ';'")
                synchronizeStatement()
                continue
            }
            if (!parseStatement()) {
                advance()
                synchronizeStatement()
            }
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.BLOCK_EXPRESSION)
        return marker
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
            contextual("match") ||
            isLiteral(token()) ||
            at(CpTypes.L_PAREN) ||
            at(CpTypes.L_BRACKET) ||
            at(CpTypes.L_BRACE) ||
            isPrefixOperator(token())

    private fun looksLikeFunctionType(): Boolean =
        contextual("f") && (lookAhead(1) == CpTypes.L_PAREN || (lookAhead(1) == CpTypes.STAR && lookAhead(2) == CpTypes.L_PAREN))

    private fun looksLikeLambdaExpression(): Boolean {
        if (!contextual("f") || lookAhead(1) != CpTypes.L_PAREN) {
            return false
        }

        var depth = 0
        var index = 1
        while (true) {
            when (lookAhead(index) ?: return false) {
                CpTypes.L_PAREN -> depth += 1
                CpTypes.R_PAREN -> depth -= 1
            }
            index += 1
            if (depth == 0) {
                val next = lookAhead(index)
                return next == CpTypes.ARROW || next == CpTypes.EQUAL || next == CpTypes.L_BRACE
            }
            if (depth < 0) {
                return false
            }
        }
    }

    private fun looksLikeAssociatedNameExpression(): Boolean =
        scanTypeSuffixFor(CpTypes.COLON_COLON)

    private fun looksLikeTypeInitializer(): Boolean =
        scanTypeSuffixFor(CpTypes.L_BRACE)

    private fun looksLikeGenericCallSuffix(): Boolean =
        looksLikeTypeArgumentSuffixBefore(CpTypes.L_PAREN)

    private fun looksLikeTypeArgumentSuffixBefore(nextType: IElementType): Boolean {
        if (!at(CpTypes.LESS)) {
            return false
        }
        var depth = 0
        var index = 0
        while (true) {
            val kind = lookAhead(index) ?: return false
            when (kind) {
                CpTypes.LESS -> depth += 1
                CpTypes.GREATER -> depth -= 1
                CpTypes.GREATER_GREATER -> depth -= 2
                CpTypes.GREATER_GREATER_EQUAL -> return false
            }
            index += 1
            if (depth == 0) {
                return lookAhead(index) == nextType
            }
            if (depth < 0) {
                return false
            }
        }
    }

    private fun scanTypeSuffixFor(nextType: IElementType): Boolean {
        if (!at(CpTypes.IDENTIFIER)) {
            return false
        }
        var index = 1
        if (lookAhead(index) == CpTypes.LESS) {
            var depth = 0
            while (true) {
                val kind = lookAhead(index) ?: return false
                when (kind) {
                    CpTypes.LESS -> depth += 1
                    CpTypes.GREATER -> depth -= 1
                    CpTypes.GREATER_GREATER -> depth -= 2
                    CpTypes.GREATER_GREATER_EQUAL -> return false
                }
                index += 1
                if (depth == 0) {
                    break
                }
                if (depth < 0) {
                    return false
                }
            }
        }
        while (lookAhead(index) == CpTypes.COLON_COLON && lookAhead(index + 1) == CpTypes.IDENTIFIER) {
            index += 2
            if (lookAhead(index) == CpTypes.LESS) {
                return false
            }
        }
        return lookAhead(index) == nextType
    }

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

    private fun expectContextual(text: String): Boolean {
        if (contextual(text)) {
            advance()
            return true
        }
        builder.error("expected '$text'")
        return false
    }

    private fun consumeContextual(text: String): Boolean {
        if (!contextual(text)) {
            return false
        }
        advance()
        return true
    }

    private fun contextual(text: String): Boolean =
        at(CpTypes.IDENTIFIER) && builder.tokenText == text

    private fun consumeEllipsis(): Boolean {
        if (lookAhead(0) == CpTypes.DOT && lookAhead(1) == CpTypes.DOT && lookAhead(2) == CpTypes.DOT) {
            advance()
            advance()
            advance()
            return true
        }
        return false
    }

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
        while (!builder.eof() && !startsTopLevelItem() && !at(CpTypes.KW_IMPORT)) {
            if (consume(CpTypes.SEMICOLON)) {
                return
            }
            advance()
        }
    }

    private fun startsTopLevelItem(): Boolean =
        at(CpTypes.IDENTIFIER) ||
            at(CpTypes.KW_STRUCT) ||
            contextual("variant") ||
            at(CpTypes.KW_CONCEPT) ||
            at(CpTypes.KW_IMPL) ||
            at(CpTypes.KW_EXPORT)

    private fun synchronizeStatement() {
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            if (consume(CpTypes.SEMICOLON)) {
                return
            }
            advance()
        }
    }

    private fun recoverIfStalled(offset: Int) {
        if (!builder.eof() && builder.currentOffset == offset) {
            builder.error("expected item")
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
