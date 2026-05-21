export module parser.statement;

import std;
import source;
import lexer.token;
import diagnostic;
import parser.ast;
import parser.state;
import parser.expression;

impl parser {
    parse_statement(self&) -> optional<stmt_id>
    {
        trace_enter("Stmt");
        if(check(token_kind::l_brace)) {
            trace_select("Stmt", "Stmt -> Block");
            let result = parse_block();
            trace_exit("Stmt", result.has_value());
            return result;
        }
        if(check(token_kind::kw_int)) {
            trace_select("Stmt", "Stmt -> VarDecl");
            let result = parse_var_decl();
            trace_exit("Stmt", result.has_value());
            return result;
        }
        if(check(token_kind::identifier)) {
            trace_select("Stmt", "Stmt -> IdentifierStmt");
            let result = parse_identifier_statement();
            trace_exit("Stmt", result.has_value());
            return result;
        }
        if(check(token_kind::kw_if)) {
            trace_select("Stmt", "Stmt -> IfStmt");
            let result = parse_if();
            trace_exit("Stmt", result.has_value());
            return result;
        }
        if(check(token_kind::kw_while)) {
            trace_select("Stmt", "Stmt -> WhileStmt");
            let result = parse_while();
            trace_exit("Stmt", result.has_value());
            return result;
        }
        if(check(token_kind::kw_return)) {
            trace_select("Stmt", "Stmt -> ReturnStmt");
            let result = parse_return();
            trace_exit("Stmt", result.has_value());
            return result;
        }

        trace_error("Stmt", "expected statement");
        report_current(diagnostic_kind::expected_statement);
        trace_exit("Stmt", false);
        return optional<stmt_id>::none;
    }

    parse_block(self&) -> optional<stmt_id>
    {
        trace_enter("Block");
        trace_select("Block", "Block -> \"{\" StmtList \"}\"");
        let open = expect(token_kind::l_brace);
        if(not open.has_value()) {
            trace_exit("Block", false);
            return optional<stmt_id>::none;
        }
        let statements = vector<stmt_id>{};
        while(not check(token_kind::r_brace) and not check(token_kind::eof)) {
            trace_select("StmtList", "StmtList -> Stmt StmtList");
            let statement = parse_statement();
            if(statement.has_value()) {
                statements.push_back(*statement);
            } else {
                synchronize_statement();
            }
        }
        trace_select("StmtList", "StmtList -> epsilon");
        let close = expect(token_kind::r_brace);
        if(not close.has_value()) {
            trace_exit("Block", false);
            return optional<stmt_id>::none;
        }
        let syntax = block_statement{
            .full_span = combine((*open).span, (*close).span),
            .statements = move statements
        };
        let result = arena.add_stmt(stmt_syntax::block(syntax));
        trace_exit("Block", true);
        return optional<stmt_id>::some(result);
    }

    parse_var_decl(self&) -> optional<stmt_id>
    {
        trace_enter("VarDecl");
        trace_select("VarDecl", "VarDecl -> \"int\" VarDeclarator VarDeclaratorTail \";\"");
        let start = consume();
        trace_match(start);
        let declarations = vector<var_decl_item>{};
        while(true) {
            let declaration = parse_var_decl_item();
            if(not declaration.has_value()) {
                trace_exit("VarDecl", false);
                return optional<stmt_id>::none;
            }
            declarations.push_back(*declaration);
            if(not check(token_kind::comma)) {
                trace_select("VarDeclaratorTail", "VarDeclaratorTail -> epsilon");
                break;
            }
            trace_select("VarDeclaratorTail", "VarDeclaratorTail -> \",\" VarDeclarator VarDeclaratorTail");
            let comma = consume();
            trace_match(comma);
        }

        let semicolon = expect(token_kind::semicolon);
        if(not semicolon.has_value()) {
            trace_exit("VarDecl", false);
            return optional<stmt_id>::none;
        }
        let syntax = var_decl_statement{
            .full_span = combine(start.span, (*semicolon).span),
            .declarations = move declarations
        };
        let result = arena.add_stmt(stmt_syntax::var_decl(syntax));
        trace_exit("VarDecl", true);
        return optional<stmt_id>::some(result);
    }

    parse_var_decl_item(self&) -> optional<var_decl_item>
    {
        trace_enter("VarDeclarator");
        trace_select("VarDeclarator", "VarDeclarator -> identifier VarInitOpt");
        let name = expect_identifier();
        if(not name.has_value()) {
            trace_exit("VarDeclarator", false);
            return optional<var_decl_item>::none;
        }
        let initializer = optional<expr_id>::none;
        let end = (*name).span;
        if(check(token_kind::equal)) {
            trace_select("VarInitOpt", "VarInitOpt -> \"=\" Expr");
            let equal = consume();
            trace_match(equal);
            let value = parse_expression();
            if(not value.has_value()) {
                trace_exit("VarDeclarator", false);
                return optional<var_decl_item>::none;
            }
            initializer = optional<expr_id>::some(*value);
            end = arena.expr_span(*value);
        } else {
            trace_select("VarInitOpt", "VarInitOpt -> epsilon");
        }

        let syntax = var_decl_item{
            .full_span = combine((*name).span, end),
            .name = (*name).span,
            .initializer = initializer
        };
        trace_exit("VarDeclarator", true);
        return optional<var_decl_item>::some(syntax);
    }

    parse_identifier_statement(self&) -> optional<stmt_id>
    {
        trace_enter("IdentifierStmt");
        trace_select("IdentifierStmt", "IdentifierStmt -> identifier IdentifierStmtTail");
        let name = consume();
        trace_match(name);
        if(check(token_kind::equal)) {
            trace_select("IdentifierStmtTail", "IdentifierStmtTail -> \"=\" Expr \";\"");
            let equal = consume();
            trace_match(equal);
            let value = parse_expression();
            let semicolon = expect(token_kind::semicolon);
            if(not value.has_value() or not semicolon.has_value()) {
                trace_exit("IdentifierStmt", false);
                return optional<stmt_id>::none;
            }
            let syntax = assign_statement{
                .full_span = combine(name.span, (*semicolon).span),
                .name = name.span,
                .value = *value
            };
            let result = arena.add_stmt(stmt_syntax::assign(syntax));
            trace_exit("IdentifierStmt", true);
            return optional<stmt_id>::some(result);
        }
        if(check(token_kind::l_paren)) {
            trace_select("IdentifierStmtTail", "IdentifierStmtTail -> \"(\" ArgListOpt \")\" \";\"");
            let open = consume();
            trace_match(open);
            let arguments = parse_argument_list();
            let close = expect(token_kind::r_paren);
            let semicolon = expect(token_kind::semicolon);
            if(not close.has_value() or not semicolon.has_value()) {
                trace_exit("IdentifierStmt", false);
                return optional<stmt_id>::none;
            }
            let syntax = call_statement{
                .full_span = combine(name.span, (*semicolon).span),
                .callee = name.span,
                .arguments = move arguments
            };
            let result = arena.add_stmt(stmt_syntax::call(syntax));
            trace_exit("IdentifierStmt", true);
            return optional<stmt_id>::some(result);
        }
        trace_error("IdentifierStmtTail", "expected assignment or call");
        report_current(diagnostic_kind::unexpected_token);
        trace_exit("IdentifierStmt", false);
        return optional<stmt_id>::none;
    }

    parse_if(self&) -> optional<stmt_id>
    {
        trace_enter("IfStmt");
        trace_select("IfStmt", "IfStmt -> \"if\" \"(\" Expr \")\" Block ElseOpt");
        let start = consume();
        trace_match(start);
        let open = expect(token_kind::l_paren);
        let condition = parse_expression();
        let close = expect(token_kind::r_paren);
        let then_branch = parse_block();
        if(
            not open.has_value()
            or not condition.has_value()
            or not close.has_value()
            or not then_branch.has_value()
        ) {
            trace_exit("IfStmt", false);
            return optional<stmt_id>::none;
        }

        let else_branch = optional<stmt_id>::none;
        let end = arena.stmt_span(*then_branch);
        if(check(token_kind::kw_else)) {
            trace_select("ElseOpt", "ElseOpt -> \"else\" Block");
            let else_token = consume();
            trace_match(else_token);
            let parsed_else = parse_block();
            if(not parsed_else.has_value()) {
                trace_exit("IfStmt", false);
                return optional<stmt_id>::none;
            }
            else_branch = optional<stmt_id>::some(*parsed_else);
            end = arena.stmt_span(*parsed_else);
        } else {
            trace_select("ElseOpt", "ElseOpt -> epsilon");
        }

        let syntax = if_statement{
            .full_span = combine(start.span, end),
            .condition = *condition,
            .then_branch = *then_branch,
            .else_branch = else_branch
        };
        let result = arena.add_stmt(stmt_syntax::if_stmt(syntax));
        trace_exit("IfStmt", true);
        return optional<stmt_id>::some(result);
    }

    parse_while(self&) -> optional<stmt_id>
    {
        trace_enter("WhileStmt");
        trace_select("WhileStmt", "WhileStmt -> \"while\" \"(\" Expr \")\" Block");
        let start = consume();
        trace_match(start);
        let open = expect(token_kind::l_paren);
        let condition = parse_expression();
        let close = expect(token_kind::r_paren);
        let body = parse_block();
        if(
            not open.has_value()
            or not condition.has_value()
            or not close.has_value()
            or not body.has_value()
        ) {
            trace_exit("WhileStmt", false);
            return optional<stmt_id>::none;
        }
        let syntax = while_statement{
            .full_span = combine(start.span, arena.stmt_span(*body)),
            .condition = *condition,
            .body = *body
        };
        let result = arena.add_stmt(stmt_syntax::while_stmt(syntax));
        trace_exit("WhileStmt", true);
        return optional<stmt_id>::some(result);
    }

    parse_return(self&) -> optional<stmt_id>
    {
        trace_enter("ReturnStmt");
        trace_select("ReturnStmt", "ReturnStmt -> \"return\" ReturnValueOpt \";\"");
        let start = consume();
        trace_match(start);
        let value = optional<expr_id>::none;
        if(not check(token_kind::semicolon)) {
            trace_select("ReturnValueOpt", "ReturnValueOpt -> Expr");
            let expression = parse_expression();
            if(not expression.has_value()) {
                trace_exit("ReturnStmt", false);
                return optional<stmt_id>::none;
            }
            value = optional<expr_id>::some(*expression);
        } else {
            trace_select("ReturnValueOpt", "ReturnValueOpt -> epsilon");
        }
        let semicolon = expect(token_kind::semicolon);
        if(not semicolon.has_value()) {
            trace_exit("ReturnStmt", false);
            return optional<stmt_id>::none;
        }
        let syntax = return_statement{
            .full_span = combine(start.span, (*semicolon).span),
            .value = value
        };
        let result = arena.add_stmt(stmt_syntax::return_stmt(syntax));
        trace_exit("ReturnStmt", true);
        return optional<stmt_id>::some(result);
    }
}
