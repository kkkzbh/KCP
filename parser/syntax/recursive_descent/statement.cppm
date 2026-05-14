module parser.syntax.recursive_descent:statement;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import :context;
import :type;
import :expression;

struct statement_parser
{
    explicit statement_parser(parser_context& context) :
        context(context) {}

    auto parse_statement() -> std::optional<stmt_id>
    {
        switch(context.peek().kind) {
            case token_kind::l_brace:
                return parse_block_statement();
            case token_kind::kw_let:
            case token_kind::kw_const:
                return parse_declaration_statement();
            case token_kind::kw_if:
                return parse_if_statement();
            case token_kind::kw_while:
                return parse_while_statement();
            case token_kind::kw_do:
                return parse_do_while_statement();
            case token_kind::kw_for:
                return parse_for_statement();
            case token_kind::kw_break:
                return parse_break_continue_statement(true);
            case token_kind::kw_continue:
                return parse_break_continue_statement(false);
            case token_kind::kw_return:
                return parse_return_statement();
            default:
                if(not expression_parser{ context }.starts(context.peek().kind)) {
                    context.report_current(
                        parser_diagnostic_code::expected_statement,
                        std::format(
                            "expected statement, got {}",
                            context.token_name(context.peek().kind)
                        )
                    );
                    return std::nullopt;
                }
                return parse_expression_statement();
        }
    }

    auto parse_expected_expression() -> std::optional<expr_id>
    {
        auto expressions = expression_parser{ context };
        if(not expressions.expect_start()) {
            return std::nullopt;
        }
        return expressions.parse_expression();
    }

    auto parse_expected_type() -> std::optional<type_id>
    {
        auto types = type_parser{ context };
        if(not types.expect_start()) {
            return std::nullopt;
        }
        return types.parse_type();
    }

    auto parse_block_statement() -> std::optional<stmt_id>
    {
        auto open = context.expect(token_kind::l_brace);
        if(not open) {
            return std::nullopt;
        }

        auto block = block_statement_syntax{};
        while(not context.check_any({ token_kind::r_brace, token_kind::eof })) {
            auto statement = parse_statement();
            if(not statement) {
                context.synchronize_statement();
                continue;
            }

            block.statements.emplace_back(*statement);
        }

        auto close = context.expect(token_kind::r_brace);
        if(not close) {
            return std::nullopt;
        }

        block.full_span = combine_spans(open->span, close->span);
        return context.arena.add(statement_syntax{ std::move(block) });
    }

    auto parse_declaration_statement() -> std::optional<stmt_id>
    {
        auto start = context.consume();
        auto name = context.expect_identifier("declaration name");
        std::optional<type_id> type{};
        if(context.check(token_kind::colon)) {
            context.consume();
            type = parse_expected_type();
            if(not type) {
                return std::nullopt;
            }
        }
        auto equal = context.expect(token_kind::equal);
        if(not name or not equal) {
            return std::nullopt;
        }

        auto initializer = parse_expected_expression();
        if(not initializer) {
            return std::nullopt;
        }
        auto semicolon = context.expect(token_kind::semicolon);
        if(not semicolon) {
            return std::nullopt;
        }

        auto statement = declaration_statement_syntax {
            .full_span = combine_spans(start.span, semicolon->span),
            .is_const = start.kind == token_kind::kw_const,
            .name = name->span,
            .declared_type = type,
            .initializer = *initializer,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_if_statement() -> std::optional<stmt_id>
    {
        auto start = context.expect(token_kind::kw_if);
        auto l_paren = context.expect(token_kind::l_paren);
        auto condition = parse_expected_expression();
        auto r_paren = context.expect(token_kind::r_paren);
        auto then_block = parse_block_statement();
        if (
            not start or not l_paren or not condition or not r_paren
            or not then_block
        ) {
            return std::nullopt;
        }

        std::optional<stmt_id> else_branch{};
        source_span end = context.arena.span(*then_block);
        if(context.check(token_kind::kw_else)) {
            context.consume();
            if(context.check(token_kind::kw_if)) {
                auto else_if = parse_if_statement();
                if(not else_if) {
                    return std::nullopt;
                }
                end = context.arena.span(*else_if);
                else_branch = *else_if;
            } else {
                auto else_block = parse_block_statement();
                if(not else_block) {
                    return std::nullopt;
                }
                end = context.arena.span(*else_block);
                else_branch = *else_block;
            }
        }

        auto statement = if_statement_syntax {
            .full_span = combine_spans(start->span, end),
            .condition = *condition,
            .then_branch = *then_block,
            .else_branch = else_branch,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_while_statement() -> std::optional<stmt_id>
    {
        auto start = context.expect(token_kind::kw_while);
        auto l_paren = context.expect(token_kind::l_paren);
        auto condition = parse_expected_expression();
        auto r_paren = context.expect(token_kind::r_paren);
        auto body = parse_block_statement();
        if (
            not start or not l_paren or not condition or not r_paren
            or not body
        ) {
            return std::nullopt;
        }

        auto statement = while_statement_syntax {
            .full_span = combine_spans(start->span, context.arena.span(*body)),
            .condition = *condition,
            .body = *body,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_do_while_statement() -> std::optional<stmt_id>
    {
        auto start = context.expect(token_kind::kw_do);
        auto body = parse_block_statement();
        auto while_kw = context.expect(token_kind::kw_while);
        auto l_paren = context.expect(token_kind::l_paren);
        auto condition = parse_expected_expression();
        auto r_paren = context.expect(token_kind::r_paren);
        auto semicolon = context.expect(token_kind::semicolon);
        if (
            not start or not body or not while_kw or not l_paren
            or not condition or not r_paren or not semicolon
        ) {
            return std::nullopt;
        }

        auto statement = do_while_statement_syntax {
            .full_span = combine_spans(start->span, semicolon->span),
            .body = *body,
            .condition = *condition,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_for_statement() -> std::optional<stmt_id>
    {
        auto start = context.expect(token_kind::kw_for);
        if(not start) {
            return std::nullopt;
        }

        std::optional<source_span> label{};
        if(context.check(token_kind::colon)) {
            context.consume();
            auto label_token = context.expect_identifier("loop label");
            if(not label_token) {
                return std::nullopt;
            }
            label = label_token->span;
        }

        auto l_paren = context.expect(token_kind::l_paren);
        if(not l_paren) {
            return std::nullopt;
        }

        if(not context.check_any({ token_kind::kw_let, token_kind::kw_const })) {
            context.report_current(
                parser_diagnostic_code::expected_token,
                "expected 'let' or 'const' in for binding"
            );
            return std::nullopt;
        }
        auto binding_kw = context.consume();
        auto binding_name = context.expect_identifier("for binding name");
        auto colon = context.expect(token_kind::colon);
        auto range = parse_expected_expression();
        auto r_paren = context.expect(token_kind::r_paren);
        auto body = parse_block_statement();
        if (
            not binding_name or not colon or not range or not r_paren
            or not body
        ) {
            return std::nullopt;
        }

        auto statement = for_statement_syntax {
            .full_span = combine_spans(start->span, context.arena.span(*body)),
            .is_const = binding_kw.kind == token_kind::kw_const,
            .name = binding_name->span,
            .label = label,
            .range = *range,
            .body = *body,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_break_continue_statement(bool is_break) -> std::optional<stmt_id>
    {
        auto start = context.consume();
        std::optional<source_span> label{};
        if(context.check(token_kind::identifier)) {
            label = context.consume().span;
        }
        auto semicolon = context.expect(token_kind::semicolon);
        if(not semicolon) {
            return std::nullopt;
        }

        auto span = combine_spans(start.span, semicolon->span);
        if(is_break) {
            return context.arena.add(statement_syntax {
                break_statement_syntax {
                    .full_span = span,
                    .label = label,
                }
            });
        }

        return context.arena.add(statement_syntax {
            continue_statement_syntax {
                .full_span = span,
                .label = label,
            }
        });
    }

    auto parse_return_statement() -> std::optional<stmt_id>
    {
        auto start = context.expect(token_kind::kw_return);
        if(not start) {
            return std::nullopt;
        }

        std::optional<expr_id> value{};
        auto expressions = expression_parser{ context };
        if(expressions.starts(context.peek().kind)) {
            value = expressions.parse_expression();
            if(not value) {
                return std::nullopt;
            }
        } else if(not context.check(token_kind::semicolon)) {
            expressions.expect_start();
            return std::nullopt;
        }

        auto semicolon = context.expect(token_kind::semicolon);
        if(not semicolon) {
            return std::nullopt;
        }

        auto statement = return_statement_syntax {
            .full_span = combine_spans(start->span, semicolon->span),
            .value = value,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    auto parse_expression_statement() -> std::optional<stmt_id>
    {
        auto expression = parse_expected_expression();
        if(not expression) {
            return std::nullopt;
        }
        auto semicolon = context.expect(token_kind::semicolon);
        if(not semicolon) {
            return std::nullopt;
        }

        auto statement = expression_statement_syntax {
            .full_span = combine_spans(context.arena.span(*expression), semicolon->span),
            .expression = *expression,
        };
        return context.arena.add(statement_syntax{ std::move(statement) });
    }

    parser_context& context;
};
