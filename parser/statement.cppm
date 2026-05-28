module parser:statement;

import std;
import source;
import lexer;
import parser.ast;
import diagnostic;
import :state;

auto parser::parse_statement() -> std::optional<stmt_id>
{
    if(check_contextual("template")) {
        return parse_template_statement();
    }

    if (
        check_contextual("type")
        and peek(1uz).kind == token_kind::identifier
        and peek(2uz).kind == token_kind::equal
    ) {
        return parse_type_alias_statement();
    }

    switch(peek().kind) {
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
            if(not starts_expression(peek().kind)) {
                report_current(
                    diagnostic_kind::expected_statement,
                    std::format(
                        "expected statement, got {}",
                        token_name(peek().kind)
                    )
                );
                return std::nullopt;
            }
            return parse_expression_statement();
    }
}

auto parser::parse_expected_expression() -> std::optional<expr_id>
{
    if(not expect_expression_start()) {
        return std::nullopt;
    }
    return parse_expression();
}

auto parser::parse_expected_type() -> std::optional<type_id>
{
    if(not expect_type_start()) {
        return std::nullopt;
    }
    return parse_type();
}

auto parser::parse_block_statement() -> std::optional<stmt_id>
{
    auto open = expect(token_kind::l_brace);
    if(not open) {
        return std::nullopt;
    }

    auto block = block_statement_syntax{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        if(consume_empty_statement()) {
            continue;
        }

        auto statement = parse_statement();
        if(not statement) {
            synchronize_statement();
            continue;
        }

        block.statements.emplace_back(*statement);
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    block.full_span = combine_spans(open->span, close->span);
    return arena.add(statement_syntax{ std::move(block) });
}

auto parser::parse_declaration_statement() -> std::optional<stmt_id>
{
    auto start = consume();
    auto is_static = false;
    if (
        check_contextual("static")
        and (peek(1uz).kind == token_kind::kw_ref or peek(1uz).kind == token_kind::identifier or peek(1uz).kind == token_kind::l_paren)
    ) {
        consume();
        is_static = true;
    }

    auto is_ref = false;
    if(check(token_kind::kw_ref)) {
        consume();
        is_ref = true;
    }

    auto binding_names = std::vector<source_span>{};
    auto name = std::optional<token>{};
    if(check(token_kind::l_paren)) {
        consume();
        auto first = expect_identifier("declaration binding name");
        if(not first) {
            return std::nullopt;
        }
        name = *first;
        binding_names.emplace_back(first->span);
        while(check(token_kind::comma)) {
            consume();
            auto next = expect_identifier("declaration binding name");
            if(not next) {
                return std::nullopt;
            }
            binding_names.emplace_back(next->span);
        }
        auto close = expect(token_kind::r_paren);
        if(not close) {
            return std::nullopt;
        }
    } else {
        name = expect_identifier("declaration name");
    }

    std::optional<type_id> type{};
    if(check(token_kind::colon)) {
        consume();
        type = parse_expected_type();
        if(not type) {
            return std::nullopt;
        }
    }
    auto equal = expect(token_kind::equal);
    if(not name or not equal) {
        return std::nullopt;
    }

    auto initializer = parse_expected_expression();
    if(not initializer) {
        return std::nullopt;
    }
    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    auto statement = declaration_statement_syntax {
        .full_span = combine_spans(start.span, semicolon->span),
        .is_const = start.kind == token_kind::kw_const,
        .is_static = is_static,
        .is_ref = is_ref,
        .name = name->span,
        .binding_names = std::move(binding_names),
        .declared_type = type,
        .initializer = *initializer,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_type_alias_statement() -> std::optional<stmt_id>
{
    auto start = consume();
    auto name = expect_identifier("type alias name");
    auto equal = expect(token_kind::equal);
    if(not name or not equal) {
        return std::nullopt;
    }

    auto type = parse_expected_type();
    if(not type) {
        return std::nullopt;
    }

    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    auto statement = type_alias_statement_syntax {
        .full_span = combine_spans(start.span, semicolon->span),
        .name = name->span,
        .type = *type,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_if_statement() -> std::optional<stmt_id>
{
    auto start = expect(token_kind::kw_if);
    auto l_paren = expect(token_kind::l_paren);
    auto condition = parse_expected_expression();
    auto r_paren = expect(token_kind::r_paren);
    auto then_block = parse_block_statement();
    if (
        not start or not l_paren or not condition or not r_paren
        or not then_block
    ) {
        return std::nullopt;
    }

    std::optional<stmt_id> else_branch{};
    source_span end = arena.span(*then_block);
    if(check(token_kind::kw_else)) {
        consume();
        if(check(token_kind::kw_if)) {
            auto else_if = parse_if_statement();
            if(not else_if) {
                return std::nullopt;
            }
            end = arena.span(*else_if);
            else_branch = *else_if;
        } else {
            auto else_block = parse_block_statement();
            if(not else_block) {
                return std::nullopt;
            }
            end = arena.span(*else_block);
            else_branch = *else_block;
        }
    }

    auto statement = if_statement_syntax {
        .full_span = combine_spans(start->span, end),
        .condition = *condition,
        .then_branch = *then_block,
        .else_branch = else_branch,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_while_statement() -> std::optional<stmt_id>
{
    auto start = expect(token_kind::kw_while);
    auto l_paren = expect(token_kind::l_paren);
    auto condition = parse_expected_expression();
    auto r_paren = expect(token_kind::r_paren);
    auto body = parse_block_statement();
    if (
        not start or not l_paren or not condition or not r_paren
        or not body
    ) {
        return std::nullopt;
    }

    auto statement = while_statement_syntax {
        .full_span = combine_spans(start->span, arena.span(*body)),
        .condition = *condition,
        .body = *body,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_do_while_statement() -> std::optional<stmt_id>
{
    auto start = expect(token_kind::kw_do);
    auto body = parse_block_statement();
    auto while_kw = expect(token_kind::kw_while);
    auto l_paren = expect(token_kind::l_paren);
    auto condition = parse_expected_expression();
    auto r_paren = expect(token_kind::r_paren);
    auto semicolon = expect(token_kind::semicolon);
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
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_for_statement() -> std::optional<stmt_id>
{
    auto start = expect(token_kind::kw_for);
    if(not start) {
        return std::nullopt;
    }

    std::optional<source_span> label{};
    if(check(token_kind::colon)) {
        consume();
        auto label_token = expect_identifier("loop label");
        if(not label_token) {
            return std::nullopt;
        }
        label = label_token->span;
    }

    auto l_paren = expect(token_kind::l_paren);
    if(not l_paren) {
        return std::nullopt;
    }

    if(not check_any({ token_kind::kw_let, token_kind::kw_const })) {
        report_current(
            diagnostic_kind::expected_token,
            "expected 'let' or 'const' optionally followed by 'ref' in for binding"
        );
        return std::nullopt;
    }
    auto binding_kw = consume();
    auto is_ref = false;
    if(check(token_kind::kw_ref)) {
        consume();
        is_ref = true;
    }
    auto binding_name = expect_identifier("for binding name");
    auto colon = expect(token_kind::colon);
    auto range = parse_expected_expression();
    auto r_paren = expect(token_kind::r_paren);
    auto body = parse_block_statement();
    if (
        not binding_name or not colon or not range or not r_paren
        or not body
    ) {
        return std::nullopt;
    }

    auto statement = for_statement_syntax {
        .full_span = combine_spans(start->span, arena.span(*body)),
        .is_const = binding_kw.kind == token_kind::kw_const,
        .is_ref = is_ref,
        .name = binding_name->span,
        .label = label,
        .range = *range,
        .body = *body,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_template_statement() -> std::optional<stmt_id>
{
    if(peek(1uz).kind == token_kind::kw_for) {
        return parse_template_for_statement();
    }
    if(peek(1uz).kind == token_kind::kw_if) {
        return parse_template_if_statement();
    }

    report_current(
        diagnostic_kind::expected_token,
        "expected 'for' or 'if' after 'template'"
    );
    return std::nullopt;
}

auto parser::parse_template_for_statement() -> std::optional<stmt_id>
{
    auto start = expect_identifier("template");
    auto for_kw = expect(token_kind::kw_for);
    auto l_paren = expect(token_kind::l_paren);
    if(not start or not for_kw or not l_paren) {
        return std::nullopt;
    }

    auto binding_kind = template_for_binding_kind::let_binding;
    auto is_ref = false;
    if(check(token_kind::kw_let)) {
        consume();
        if(check(token_kind::kw_ref)) {
            consume();
            is_ref = true;
        }
    } else if(check(token_kind::kw_const)) {
        consume();
        binding_kind = template_for_binding_kind::const_binding;
        if(check(token_kind::kw_ref)) {
            consume();
            is_ref = true;
        }
    } else if(check_contextual("type")) {
        consume();
        binding_kind = template_for_binding_kind::type_binding;
    } else {
        report_current(
            diagnostic_kind::expected_token,
            "expected 'let'/'const' optionally followed by 'ref', or 'type' in template for binding"
        );
        return std::nullopt;
    }

    auto name = expect_identifier("template for binding name");
    auto colon = expect(token_kind::colon);
    auto pack_name = expect_identifier("template for pack name");
    auto ellipsis = consume_ellipsis();
    auto r_paren = expect(token_kind::r_paren);
    auto body = parse_block_statement();
    if(not name or not colon or not pack_name or not ellipsis or not r_paren or not body) {
        if(not ellipsis) {
            report_current(diagnostic_kind::expected_token, "expected '...' after template for pack name");
        }
        return std::nullopt;
    }

    return arena.add(statement_syntax {
        template_for_statement_syntax {
            .full_span = combine_spans(start->span, arena.span(*body)),
            .binding_kind = binding_kind,
            .is_ref = is_ref,
            .name = name->span,
            .pack_name = pack_name->span,
            .body = *body,
        }
    });
}

auto parser::parse_template_if_statement() -> std::optional<stmt_id>
{
    auto start = expect_identifier("template");
    if(not start) {
        return std::nullopt;
    }

    auto conditions = std::vector<template_if_condition_syntax>{};
    auto branches = std::vector<template_if_branch_syntax>{};
    auto first = parse_template_if_branch(conditions, start->span);
    if(not first) {
        return std::nullopt;
    }
    auto end = arena.span(first->body);
    branches.emplace_back(std::move(*first));

    auto else_branch = std::optional<stmt_id>{};
    while(check(token_kind::kw_else)) {
        auto else_kw = consume();
        if(check_contextual("template")) {
            auto template_kw = expect_identifier("template");
            if(not template_kw) {
                return std::nullopt;
            }
            if(not check(token_kind::kw_if)) {
                report_current(
                    diagnostic_kind::expected_token,
                    "expected 'if' after 'else template'"
                );
                return std::nullopt;
            }
            auto branch = parse_template_if_branch(conditions, combine_spans(else_kw.span, template_kw->span));
            if(not branch) {
                return std::nullopt;
            }
            end = arena.span(branch->body);
            branches.emplace_back(std::move(*branch));
            continue;
        }

        auto body = parse_block_statement();
        if(not body) {
            return std::nullopt;
        }
        end = arena.span(*body);
        else_branch = *body;
        break;
    }

    return arena.add(statement_syntax {
        template_if_statement_syntax {
            .full_span = combine_spans(start->span, end),
            .conditions = std::move(conditions),
            .branches = std::move(branches),
            .else_branch = else_branch,
        }
    });
}

auto parser::parse_template_if_branch(std::vector<template_if_condition_syntax>& conditions, source_span start)
    -> std::optional<template_if_branch_syntax>
{
    auto if_kw = expect(token_kind::kw_if);
    auto l_paren = expect(token_kind::l_paren);
    if(not if_kw or not l_paren) {
        return std::nullopt;
    }

    auto condition = parse_template_if_condition(conditions);
    auto r_paren = expect(token_kind::r_paren);
    auto body = parse_block_statement();
    if(not condition or not r_paren or not body) {
        return std::nullopt;
    }

    return template_if_branch_syntax {
        .full_span = combine_spans(start, arena.span(*body)),
        .condition = *condition,
        .body = *body,
    };
}

auto parser::parse_template_if_condition(std::vector<template_if_condition_syntax>& conditions) -> std::optional<std::uint32_t>
{
    return parse_template_if_or_condition(conditions);
}

auto parser::parse_template_if_or_condition(std::vector<template_if_condition_syntax>& conditions) -> std::optional<std::uint32_t>
{
    auto left = parse_template_if_and_condition(conditions);
    if(not left) {
        return std::nullopt;
    }

    while(check(token_kind::kw_or)) {
        consume();
        auto right = parse_template_if_and_condition(conditions);
        if(not right) {
            return std::nullopt;
        }
        auto span = combine_spans(conditions[*left].full_span, conditions[*right].full_span);
        left = add_template_if_condition(conditions, template_if_condition_syntax {
            .full_span = span,
            .kind = template_if_condition_kind::or_,
            .left_condition = *left,
            .right_condition = *right,
        });
    }

    return left;
}

auto parser::parse_template_if_and_condition(std::vector<template_if_condition_syntax>& conditions) -> std::optional<std::uint32_t>
{
    auto left = parse_template_if_unary_condition(conditions);
    if(not left) {
        return std::nullopt;
    }

    while(check(token_kind::kw_and)) {
        consume();
        auto right = parse_template_if_unary_condition(conditions);
        if(not right) {
            return std::nullopt;
        }
        auto span = combine_spans(conditions[*left].full_span, conditions[*right].full_span);
        left = add_template_if_condition(conditions, template_if_condition_syntax {
            .full_span = span,
            .kind = template_if_condition_kind::and_,
            .left_condition = *left,
            .right_condition = *right,
        });
    }

    return left;
}

auto parser::parse_template_if_unary_condition(std::vector<template_if_condition_syntax>& conditions) -> std::optional<std::uint32_t>
{
    if(check(token_kind::kw_not)) {
        auto op = consume();
        auto operand = parse_template_if_unary_condition(conditions);
        if(not operand) {
            return std::nullopt;
        }
        return add_template_if_condition(conditions, template_if_condition_syntax {
            .full_span = combine_spans(op.span, conditions[*operand].full_span),
            .kind = template_if_condition_kind::not_,
            .left_condition = *operand,
        });
    }

    return parse_template_if_primary_condition(conditions);
}

auto parser::parse_template_if_primary_condition(std::vector<template_if_condition_syntax>& conditions) -> std::optional<std::uint32_t>
{
    if(looks_like_template_if_type_condition()) {
        auto left = parse_expected_type();
        if(not left) {
            return std::nullopt;
        }

        if(check(token_kind::colon)) {
            consume();
            auto concept_ref = parse_concept_id();
            if(not concept_ref) {
                return std::nullopt;
            }
            return add_template_if_condition(conditions, template_if_condition_syntax {
                .full_span = combine_spans(arena.span(*left), concept_ref->full_span),
                .kind = template_if_condition_kind::concept_bound,
                .left_type = *left,
                .concept_bound = std::move(*concept_ref),
            });
        }

        auto equal = expect(token_kind::equal_equal);
        auto right = parse_expected_type();
        if(not equal or not right) {
            return std::nullopt;
        }
        return add_template_if_condition(conditions, template_if_condition_syntax {
            .full_span = combine_spans(arena.span(*left), arena.span(*right)),
            .kind = template_if_condition_kind::type_equality,
            .left_type = *left,
            .right_type = *right,
        });
    }

    if(check(token_kind::l_paren)) {
        consume();
        auto condition = parse_template_if_condition(conditions);
        auto close = expect(token_kind::r_paren);
        if(not condition or not close) {
            return std::nullopt;
        }
        return condition;
    }

    if(not expect_expression_start()) {
        return std::nullopt;
    }
    auto expression = parse_expression_pratt(50);
    if(not expression) {
        return std::nullopt;
    }
    return add_template_if_condition(conditions, template_if_condition_syntax {
        .full_span = arena.span(*expression),
        .kind = template_if_condition_kind::expression,
        .expression = *expression,
    });
}

auto parser::add_template_if_condition(std::vector<template_if_condition_syntax>& conditions, template_if_condition_syntax condition) -> std::uint32_t
{
    auto id = static_cast<std::uint32_t>(conditions.size());
    conditions.emplace_back(std::move(condition));
    return id;
}

auto parser::looks_like_template_if_type_condition(std::size_t lookahead) const -> bool
{
    auto type_end = skip_requires_type(lookahead);
    return type_end and (peek(*type_end).kind == token_kind::equal_equal or peek(*type_end).kind == token_kind::colon);
}

auto parser::parse_break_continue_statement(bool is_break) -> std::optional<stmt_id>
{
    auto start = consume();
    std::optional<source_span> label{};
    if(check(token_kind::identifier)) {
        label = consume().span;
    }
    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    auto span = combine_spans(start.span, semicolon->span);
    if(is_break) {
        return arena.add(statement_syntax {
            break_statement_syntax {
                .full_span = span,
                .label = label,
            }
        });
    }

    return arena.add(statement_syntax {
        continue_statement_syntax {
            .full_span = span,
            .label = label,
        }
    });
}

auto parser::parse_return_statement() -> std::optional<stmt_id>
{
    auto start = expect(token_kind::kw_return);
    if(not start) {
        return std::nullopt;
    }

    std::optional<expr_id> value{};
    if(starts_expression(peek().kind)) {
        value = parse_expression();
        if(not value) {
            return std::nullopt;
        }
    } else if(not check(token_kind::semicolon)) {
        expect_expression_start();
        return std::nullopt;
    }

    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    auto statement = return_statement_syntax {
        .full_span = combine_spans(start->span, semicolon->span),
        .value = value,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}

auto parser::parse_expression_statement() -> std::optional<stmt_id>
{
    auto expression = parse_expected_expression();
    if(not expression) {
        return std::nullopt;
    }
    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    auto statement = expression_statement_syntax {
        .full_span = combine_spans(arena.span(*expression), semicolon->span),
        .expression = *expression,
    };
    return arena.add(statement_syntax{ std::move(statement) });
}
