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
            if (consume(CpTypes.SEMICOLON)) {
                continue
            }
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
            at(CpTypes.KW_ENUM) -> {
                parseEnumDeclaration()
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
            contextual("type") -> {
                parseTypeAlias(CpElements.TYPE_ALIAS)
                true
            }
            at(CpTypes.KW_IMPL) && !exported -> {
                parseImplBlock()
                true
            }
            at(CpTypes.KW_OPERATOR) -> {
                parseOperatorFunction(CpElements.FUNCTION)
                true
            }
            contextual("extern") -> {
                parseExternFunction(CpElements.FUNCTION)
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
        if (consume(CpTypes.EQUAL)) {
            parseExpression()
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.STRUCT_FIELD)
    }

    private fun parseEnumDeclaration() {
        val marker = builder.mark()
        expect(CpTypes.KW_ENUM, "expected 'enum'")
        markIdentifier(CpElements.TYPE_NAME, "expected enum name")
        expect(CpTypes.COLON, "expected ':'")
        parseType()
        if (!expect(CpTypes.L_BRACE, "expected '{'")) {
            marker.done(CpElements.ENUM_DECLARATION)
            return
        }
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            val offset = builder.currentOffset
            parseEnumCase()
            recoverIfStalled(offset)
        }
        expect(CpTypes.R_BRACE, "expected '}'")
        marker.done(CpElements.ENUM_DECLARATION)
    }

    private fun parseEnumCase() {
        val marker = builder.mark()
        markIdentifier(CpElements.ENUM_CASE_NAME, "expected enum case name")
        expect(CpTypes.EQUAL, "expected '='")
        parseExpression()
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(CpElements.ENUM_CASE)
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
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
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
        if (at(CpTypes.KW_OPERATOR)) {
            parseOperatorFunctionName()
        } else {
            markIdentifier(CpElements.FUNCTION_NAME, "expected function name")
        }
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
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
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
                at(CpTypes.KW_OPERATOR) -> parseOperatorFunction(CpElements.FUNCTION)
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
        parseFunctionTail(marker, type)
    }

    private fun parseExternFunction(type: IElementType) {
        val marker = builder.mark()
        expectContextual("extern")
        if (at(CpTypes.STRING_LITERAL)) {
            advance()
        } else {
            expect(CpTypes.STRING_LITERAL, "expected extern ABI string")
        }
        markIdentifier(CpElements.FUNCTION_NAME, "expected function name")
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
        expect(CpTypes.L_PAREN, "expected '('")
        parseParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(type)
    }

    private fun parseOperatorFunction(type: IElementType) {
        val marker = builder.mark()
        parseOperatorFunctionName()
        parseFunctionTail(marker, type)
    }

    private fun parseOperatorFunctionName() {
        val marker = builder.mark()
        expect(CpTypes.KW_OPERATOR, "expected 'operator'")
        parseOverloadOperatorName()
        marker.done(CpElements.FUNCTION_NAME)
    }

    private fun parseOverloadOperatorName() {
        if (consume(CpTypes.L_BRACKET)) {
            expect(CpTypes.R_BRACKET, "expected ']'")
            return
        }
        if (consume(CpTypes.L_PAREN)) {
            expect(CpTypes.R_PAREN, "expected ')'")
            return
        }
        if (consumeContextual("prefix") || consumeContextual("postfix")) {
            parseIncrementOrDecrementOverloadOperatorName()
            return
        }
        if (at(CpTypes.PLUS_PLUS) || at(CpTypes.MINUS_MINUS)) {
            builder.error("expected 'prefix' or 'postfix' before '++' or '--'")
            advance()
            return
        }
        val current = token()
        if (current != null && current in OVERLOAD_OPERATOR_TOKENS) {
            advance()
            return
        }
        builder.error("expected overloadable operator")
        if (!at(CpTypes.L_PAREN) && !builder.eof()) {
            advance()
        }
    }

    private fun parseIncrementOrDecrementOverloadOperatorName() {
        if (consume(CpTypes.PLUS_PLUS) || consume(CpTypes.MINUS_MINUS)) {
            return
        }
        builder.error("expected '++' or '--'")
        if (!at(CpTypes.L_PAREN) && !builder.eof()) {
            advance()
        }
    }

    private fun parseFunctionTail(marker: PsiBuilder.Marker, type: IElementType) {
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
        expect(CpTypes.L_PAREN, "expected '('")
        parseParameterList()
        expect(CpTypes.R_PAREN, "expected ')'")
        if (consume(CpTypes.ARROW)) {
            parseType()
        }
        if (consume(CpTypes.EQUAL)) {
            if (!consume(CpTypes.KW_DELETE)) {
                expectContextual("default")
            }
            expect(CpTypes.SEMICOLON, "expected ';'")
            marker.done(type)
            return
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
            parseConceptId()
            while (consume(CpTypes.KW_AND)) {
                parseRepeatedGenericConstraintTarget()
                parseConceptId()
            }
        }
        if (consume(CpTypes.EQUAL)) {
            parseTypeArgument()
        }
        marker.done(CpElements.GENERIC_PARAMETER)
    }

    private fun parseRepeatedGenericConstraintTarget() {
        if (lookAhead(0) != CpTypes.IDENTIFIER || lookAhead(1) != CpTypes.COLON) {
            return
        }
        markIdentifier(CpElements.TYPE_NAME, "expected generic parameter")
        expect(CpTypes.COLON, "expected ':'")
    }

    private fun parseConceptId() {
        markIdentifier(CpElements.TYPE_NAME, "expected concept bound")
        if (at(CpTypes.LESS)) {
            parseTypeArgumentList()
        }
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
            if (consumeSelfReferenceQualifier()) {
                expect(CpTypes.AMP, "expected '&'")
            } else {
                consume(CpTypes.AMP)
            }
            marker.done(CpElements.PARAMETER)
            return
        }
        markIdentifier(CpElements.PARAMETER_NAME, "expected parameter name")
        var hasExplicitType = false
        if (consume(CpTypes.COLON)) {
            hasExplicitType = true
            parseType()
            consumeEllipsis()
        } else if (consumeInferredReferenceQualifier()) {
            expect(CpTypes.AMP, "expected '&'")
        } else {
            consume(CpTypes.AMP)
        }
        if (!hasExplicitType && consume(CpTypes.COLON)) {
            builder.error("inferred parameter suffix cannot be combined with an explicit type")
            parseType()
        }
        if (consume(CpTypes.EQUAL)) {
            parseExpression()
        }
        marker.done(CpElements.PARAMETER)
    }

    private fun parseType(): Boolean {
        if (looksLikeFunctionType()) {
            return parseFunctionType()
        }
        if (contextual("decltype") && lookAhead(1) == CpTypes.L_PAREN) {
            return parseDecltype()
        }
        if (contextual("storage") && startsTypeAt(1)) {
            return parseStorageType()
        }
        if (at(CpTypes.BANG)) {
            val marker = builder.mark()
            advance()
            parseTypeSuffix()
            marker.done(CpElements.TYPE_REFERENCE)
            return true
        }
        if (at(CpTypes.L_BRACKET)) {
            return parseArrayType()
        }
        if (at(CpTypes.L_PAREN)) {
            return parseParenType()
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
        if (syntheticGreaterClosers == 0) {
            while (consume(CpTypes.COLON_COLON)) {
                markIdentifier(CpElements.TYPE_NAME, "expected associated type name")
            }
            parseTypeSuffix()
        }
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseStorageType(): Boolean {
        val marker = builder.mark()
        expectIdentifier("expected storage")
        if (at(CpTypes.L_BRACKET)) {
            parseStorageArrayType()
        } else {
            parseType()
        }
        parseTypeSuffix()
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseStorageArrayType() {
        expect(CpTypes.L_BRACKET, "expected '['")
        parseType()
        expect(CpTypes.SEMICOLON, "expected ';'")
        parseTypeLengthArgument()
        expect(CpTypes.R_BRACKET, "expected ']'")
    }

    private fun parseArrayType(): Boolean {
        val marker = builder.mark()
        expect(CpTypes.L_BRACKET, "expected '['")
        parseType()
        expect(CpTypes.SEMICOLON, "expected ';'")
        parseTypeLengthArgument()
        expect(CpTypes.R_BRACKET, "expected ']'")
        parseTypeSuffix()
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseParenType(): Boolean {
        val marker = builder.mark()
        expect(CpTypes.L_PAREN, "expected '('")
        parseType()
        while (consume(CpTypes.COMMA)) {
            if (at(CpTypes.R_PAREN)) {
                break
            }
            parseType()
        }
        expect(CpTypes.R_PAREN, "expected ')'")
        parseTypeSuffix()
        marker.done(CpElements.TYPE_REFERENCE)
        return true
    }

    private fun parseTypeSuffix() {
        consumeTypeTargetQualifier()
        while (consume(CpTypes.STAR)) {
        }
        if (!consume(CpTypes.AMP) && consumeMoveOrForward()) {
            expect(CpTypes.AMP, "expected '&'")
        }
    }

    private fun consumeSelfReferenceQualifier(): Boolean =
        consume(CpTypes.KW_CONST) || consume(CpTypes.KW_LIKE) || consumeMoveOrForward()

    private fun consumeInferredReferenceQualifier(): Boolean =
        consume(CpTypes.KW_CONST) || consumeMoveOrForward()

    private fun consumeTypeTargetQualifier(): Boolean =
        consume(CpTypes.KW_CONST) || consume(CpTypes.KW_LIKE) || consume(CpTypes.KW_FORWARD)

    private fun consumeMoveOrForward(): Boolean =
        consume(CpTypes.KW_MOVE) || consume(CpTypes.KW_FORWARD)

    private fun parseTypeLengthArgument() {
        val marker = builder.mark()
        when {
            at(CpTypes.INTEGER_LITERAL) -> advance()
            at(CpTypes.IDENTIFIER) -> advance()
            else -> builder.error("expected array length")
        }
        marker.done(CpElements.TYPE_ARGUMENT)
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
            startsType() -> {
                parseType()
                consumeEllipsis()
            }
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
            contextual("template") && lookAhead(1) == CpTypes.KW_IF -> {
                parseTemplateIfStatement()
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
            at(CpTypes.SEMICOLON) -> {
                advance()
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
            consumeContextual("opaque")
            parseType()
        }
        expect(CpTypes.SEMICOLON, "expected ';'")
        marker.done(type)
    }

    private fun parseDeclarationStatement() {
        val marker = builder.mark()
        advance()
        consumeStaticDeclarationModifier()
        consume(CpTypes.KW_REF)
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

    private fun consumeStaticDeclarationModifier() {
        if (contextual("static") && lookAhead(1) in STATIC_DECLARATION_FOLLOWERS) {
            advance()
        }
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
        consume(CpTypes.KW_REF)
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
            at(CpTypes.KW_LET) || at(CpTypes.KW_CONST) -> {
                advance()
                consume(CpTypes.KW_REF)
            }
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

    private fun parseTemplateIfStatement() {
        val marker = builder.mark()
        expectContextual("template")
        parseTemplateIfBranch()
        while (consume(CpTypes.KW_ELSE)) {
            if (contextual("template") && lookAhead(1) == CpTypes.KW_IF) {
                expectContextual("template")
                parseTemplateIfBranch()
            } else {
                parseBlock()
                break
            }
        }
        marker.done(CpElements.TEMPLATE_IF_STATEMENT)
    }

    private fun parseTemplateIfBranch() {
        expect(CpTypes.KW_IF, "expected 'if'")
        expect(CpTypes.L_PAREN, "expected '('")
        parseTemplateIfCondition()
        expect(CpTypes.R_PAREN, "expected ')'")
        parseBlock()
    }

    private fun parseTemplateIfCondition() {
        parseTemplateIfOrCondition()
    }

    private fun parseTemplateIfOrCondition() {
        parseTemplateIfAndCondition()
        while (consume(CpTypes.KW_OR)) {
            parseTemplateIfAndCondition()
        }
    }

    private fun parseTemplateIfAndCondition() {
        parseTemplateIfUnaryCondition()
        while (consume(CpTypes.KW_AND)) {
            parseTemplateIfUnaryCondition()
        }
    }

    private fun parseTemplateIfUnaryCondition() {
        if (consume(CpTypes.KW_NOT)) {
            parseTemplateIfUnaryCondition()
            return
        }
        parseTemplateIfPrimaryCondition()
    }

    private fun parseTemplateIfPrimaryCondition() {
        when {
            looksLikeTemplateIfTypeCondition() -> {
                parseType()
                if (consume(CpTypes.COLON)) {
                    parseConceptId()
                } else {
                    expect(CpTypes.EQUAL_EQUAL, "expected '=='")
                    parseType()
                }
            }
            consume(CpTypes.L_PAREN) -> {
                parseTemplateIfCondition()
                expect(CpTypes.R_PAREN, "expected ')'")
            }
            startsExpression() -> parseExpressionPratt(50)
            else -> builder.error("expected template if condition")
        }
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
                markMemberName()
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
            at(CpTypes.KW_NEW) -> parseNewExpression()
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

    private fun parseNewExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expect(CpTypes.KW_NEW, "expected 'new'")
        parseType()
        parseStructInitializerList()
        marker.done(CpElements.STRUCT_INIT_EXPRESSION)
        return marker
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
            val offset = builder.currentOffset
            parseMatchArm()
            recoverMatchArmIfStalled(offset)
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

    private fun recoverMatchArmIfStalled(offset: Int) {
        if (!builder.eof() && builder.currentOffset == offset) {
            builder.error("expected match arm")
            advance()
        }
    }

    private fun parseLambdaExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expectContextual("f")
        if (at(CpTypes.LESS)) {
            parseGenericParameterList()
        }
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
            consumeEllipsis()
        }
        if (consume(CpTypes.EQUAL)) {
            parseExpression()
        }
        marker.done(CpElements.PARAMETER)
    }

    private fun parseBlockExpression(): PsiBuilder.Marker {
        val marker = builder.mark()
        expect(CpTypes.L_BRACE, "expected '{'")
        while (!builder.eof() && !at(CpTypes.R_BRACE)) {
            when {
                contextual("template") && lookAhead(1) == CpTypes.KW_FOR -> {
                    parseTemplateForStatement()
                    continue
                }
                contextual("template") && lookAhead(1) == CpTypes.KW_IF -> {
                    parseTemplateIfStatement()
                    continue
                }
                contextual("type") && lookAhead(1) == CpTypes.IDENTIFIER && lookAhead(2) == CpTypes.EQUAL -> {
                    parseTypeAlias(CpElements.TYPE_ALIAS_STATEMENT)
                    continue
                }
            }
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
            at(CpTypes.KW_NEW) ||
            at(CpTypes.L_PAREN) ||
            at(CpTypes.L_BRACKET) ||
            at(CpTypes.L_BRACE) ||
            isPrefixOperator(token())

    private fun startsType(): Boolean =
        startsTypeAt(0)

    private fun startsTypeAt(index: Int): Boolean =
        lookAhead(index) == CpTypes.IDENTIFIER ||
            lookAhead(index) == CpTypes.BANG ||
            lookAhead(index) == CpTypes.L_BRACKET ||
            lookAhead(index) == CpTypes.L_PAREN

    private fun looksLikeFunctionType(): Boolean =
        contextual("f") && (lookAhead(1) == CpTypes.L_PAREN || (lookAhead(1) == CpTypes.STAR && lookAhead(2) == CpTypes.L_PAREN))

    private fun looksLikeLambdaExpression(): Boolean {
        if (!contextual("f")) {
            return false
        }

        var start = 1
        if (lookAhead(start) == CpTypes.LESS) {
            start = scanAngleArgumentListEnd(start) ?: return false
        }
        if (lookAhead(start) != CpTypes.L_PAREN) {
            return false
        }
        var depth = 0
        var index = start
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

    private fun looksLikeTemplateIfTypeCondition(): Boolean =
        scanTypeSuffixFor(CpTypes.EQUAL_EQUAL) || scanTypeSuffixFor(CpTypes.COLON)

    private fun looksLikeGenericCallSuffix(): Boolean =
        looksLikeTypeArgumentSuffixBefore(CpTypes.L_PAREN)

    private fun looksLikeTypeArgumentSuffixBefore(nextType: IElementType): Boolean {
        if (!at(CpTypes.LESS)) {
            return false
        }
        val index = scanAngleArgumentListEnd(0) ?: return false
        return lookAhead(index) == nextType
    }

    private fun scanTypeSuffixFor(nextType: IElementType): Boolean =
        scanTypeEnd()?.let { index -> lookAhead(index) == nextType } == true

    private fun scanTypeEnd(start: Int = 0, allowAssociatedNames: Boolean = true): Int? {
        var index = scanTypeHeadEnd(start, allowAssociatedNames) ?: return null
        if (
            lookAhead(index) == CpTypes.KW_CONST ||
            lookAhead(index) == CpTypes.KW_LIKE ||
            lookAhead(index) == CpTypes.KW_FORWARD
        ) {
            index += 1
        }
        while (lookAhead(index) == CpTypes.STAR) {
            index += 1
        }
        if (lookAhead(index) == CpTypes.AMP) {
            index += 1
        } else if (
            (lookAhead(index) == CpTypes.KW_MOVE || lookAhead(index) == CpTypes.KW_FORWARD) &&
            lookAhead(index + 1) == CpTypes.AMP
        ) {
            index += 2
        }
        return index
    }

    private fun scanTypeHeadEnd(start: Int, allowAssociatedNames: Boolean): Int? {
        var index = start
        when {
            contextualAt("decltype", index) && lookAhead(index + 1) == CpTypes.L_PAREN -> {
                index = scanBalancedEnd(index + 1, CpTypes.L_PAREN, CpTypes.R_PAREN) ?: return null
            }
            contextualAt("f", index) &&
                (lookAhead(index + 1) == CpTypes.L_PAREN || (lookAhead(index + 1) == CpTypes.STAR && lookAhead(index + 2) == CpTypes.L_PAREN)) -> {
                index += 1
                if (lookAhead(index) == CpTypes.STAR) {
                    index += 1
                }
                index = scanBalancedEnd(index, CpTypes.L_PAREN, CpTypes.R_PAREN) ?: return null
                if (lookAhead(index) != CpTypes.ARROW) {
                    return null
                }
                index = scanTypeEnd(index + 1) ?: return null
            }
            contextualAt("storage", index) && startsTypeAt(index + 1) -> {
                index = scanTypeEnd(index + 1) ?: return null
            }
            lookAhead(index) == CpTypes.BANG -> {
                index += 1
            }
            lookAhead(index) == CpTypes.L_BRACKET -> {
                index += 1
                index = scanTypeEnd(index) ?: return null
                if (lookAhead(index) != CpTypes.SEMICOLON) {
                    return null
                }
                index += 1
                if (lookAhead(index) != CpTypes.INTEGER_LITERAL && lookAhead(index) != CpTypes.IDENTIFIER) {
                    return null
                }
                index += 1
                if (lookAhead(index) != CpTypes.R_BRACKET) {
                    return null
                }
                index += 1
            }
            lookAhead(index) == CpTypes.L_PAREN -> {
                index += 1
                index = scanTypeEnd(index) ?: return null
                while (lookAhead(index) == CpTypes.COMMA) {
                    index += 1
                    if (lookAhead(index) == CpTypes.R_PAREN) {
                        break
                    }
                    index = scanTypeEnd(index) ?: return null
                }
                if (lookAhead(index) != CpTypes.R_PAREN) {
                    return null
                }
                index += 1
            }
            lookAhead(index) == CpTypes.IDENTIFIER -> {
                index += 1
                if (lookAhead(index) == CpTypes.LESS) {
                    index = scanAngleArgumentListEnd(index) ?: return null
                }
                while (allowAssociatedNames && lookAhead(index) == CpTypes.COLON_COLON && lookAhead(index + 1) == CpTypes.IDENTIFIER) {
                    index += 2
                    if (lookAhead(index) == CpTypes.LESS) {
                        return null
                    }
                }
            }
            else -> return null
        }
        return index
    }

    private fun scanBalancedEnd(start: Int, open: IElementType, close: IElementType): Int? {
        if (lookAhead(start) != open) {
            return null
        }
        var depth = 1
        var index = start + 1
        while (depth > 0) {
            when (lookAhead(index) ?: return null) {
                open -> depth += 1
                close -> depth -= 1
            }
            index += 1
        }
        return index
    }

    private fun scanAngleArgumentListEnd(start: Int): Int? {
        if (lookAhead(start) != CpTypes.LESS) {
            return null
        }

        var angleDepth = 0
        var parenDepth = 0
        var bracketDepth = 0
        var index = start
        while (true) {
            val kind = lookAhead(index) ?: return null
            when (kind) {
                CpTypes.LESS -> angleDepth += 1
                CpTypes.GREATER -> {
                    angleDepth -= 1
                    if (angleDepth == 0) {
                        return index + 1
                    }
                    if (angleDepth < 0) {
                        return null
                    }
                }
                CpTypes.GREATER_GREATER -> {
                    angleDepth -= 2
                    if (angleDepth == 0) {
                        return index + 1
                    }
                    if (angleDepth < 0) {
                        return null
                    }
                }
                CpTypes.GREATER_GREATER_EQUAL -> return null
                CpTypes.L_PAREN -> parenDepth += 1
                CpTypes.R_PAREN -> {
                    if (parenDepth == 0) {
                        return null
                    }
                    parenDepth -= 1
                }
                CpTypes.L_BRACKET -> bracketDepth += 1
                CpTypes.R_BRACKET -> {
                    if (bracketDepth == 0) {
                        return null
                    }
                    bracketDepth -= 1
                }
                CpTypes.L_BRACE, CpTypes.R_BRACE -> return null
                CpTypes.SEMICOLON -> if (bracketDepth == 0) {
                    return null
                }
                in ANGLE_ARGUMENT_SCAN_STOPS -> if (parenDepth == 0 && bracketDepth == 0) {
                    return null
                }
            }
            index += 1
        }
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

    private fun markMemberName(): Boolean {
        val marker = builder.mark()
        if (at(CpTypes.IDENTIFIER) || at(CpTypes.INTEGER_LITERAL)) {
            advance()
            marker.done(CpElements.MEMBER_NAME)
            return true
        }
        builder.error("expected member name")
        marker.drop()
        return false
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

    private fun contextualAt(text: String, index: Int): Boolean =
        index == 0 && contextual(text)

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
            at(CpTypes.KW_ENUM) ||
            contextual("variant") ||
            at(CpTypes.KW_CONCEPT) ||
            at(CpTypes.KW_IMPL) ||
            at(CpTypes.KW_OPERATOR) ||
            contextual("type") ||
            contextual("extern") ||
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
            BinaryOperator(CpTypes.SPACESHIP, 90, 91),
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
            CpTypes.KW_CONST,
            CpTypes.KW_REF,
            CpTypes.KW_MOVE,
            CpTypes.KW_FORWARD,
            CpTypes.KW_DELETE,
            CpTypes.PLUS_PLUS,
            CpTypes.MINUS_MINUS,
        )

        private val STATIC_DECLARATION_FOLLOWERS = setOf(
            CpTypes.KW_REF,
            CpTypes.IDENTIFIER,
            CpTypes.L_PAREN,
        )

        private val LITERALS = setOf(
            CpTypes.INTEGER_LITERAL,
            CpTypes.FLOAT_LITERAL,
            CpTypes.CHAR_LITERAL,
            CpTypes.STRING_LITERAL,
            CpTypes.KW_TRUE,
            CpTypes.KW_FALSE,
            CpTypes.KW_NULLPTR,
        )

        private val OVERLOAD_OPERATOR_TOKENS = setOf(
            CpTypes.PLUS,
            CpTypes.MINUS,
            CpTypes.STAR,
            CpTypes.SLASH,
            CpTypes.PERCENT,
            CpTypes.AMP,
            CpTypes.PIPE,
            CpTypes.CARET,
            CpTypes.LESS_LESS,
            CpTypes.GREATER_GREATER,
            CpTypes.TILDE,
            CpTypes.EQUAL_EQUAL,
            CpTypes.BANG_EQUAL,
            CpTypes.LESS,
            CpTypes.LESS_EQUAL,
            CpTypes.SPACESHIP,
            CpTypes.GREATER,
            CpTypes.GREATER_EQUAL,
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

        private val ANGLE_ARGUMENT_SCAN_STOPS = setOf(
            CpTypes.PLUS,
            CpTypes.MINUS,
            CpTypes.SLASH,
            CpTypes.PERCENT,
            CpTypes.EQUAL,
            CpTypes.EQUAL_EQUAL,
            CpTypes.BANG_EQUAL,
            CpTypes.LESS_EQUAL,
            CpTypes.GREATER_EQUAL,
            CpTypes.LESS_LESS,
            CpTypes.LESS_LESS_EQUAL,
            CpTypes.AMP_EQUAL,
            CpTypes.PIPE,
            CpTypes.PIPE_EQUAL,
            CpTypes.CARET,
            CpTypes.CARET_EQUAL,
            CpTypes.KW_AND,
            CpTypes.KW_OR,
            CpTypes.KW_AS,
        )

        private fun binaryOperator(type: IElementType?): BinaryOperator? =
            BINARY_OPERATORS.firstOrNull { it.kind == type }

        private fun isAssignmentOperator(type: IElementType?): Boolean = type in ASSIGNMENT_OPERATORS

        private fun isPrefixOperator(type: IElementType?): Boolean = type in PREFIX_OPERATORS

        private fun isLiteral(type: IElementType?): Boolean = type in LITERALS
    }
}
