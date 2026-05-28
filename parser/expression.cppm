module parser:parse_expression;

import std;
import lexer;
import parser.ast;
import diagnostic;
import :state;

struct binary_operator_entry
{
    token_kind kind;
    int left_bp;
    int right_bp;
};

auto constexpr make_binary_operator_entry(token_kind kind, int left_bp, int right_bp) -> binary_operator_entry
{
    return {
        .kind = kind,
        .left_bp = left_bp,
        .right_bp = right_bp,
    };
}

auto constexpr inline cp_binary_operator_table = std::array<binary_operator_entry, 20uz>{{
    make_binary_operator_entry(token_kind::kw_or, 30, 31),
    make_binary_operator_entry(token_kind::kw_and, 40, 41),
    make_binary_operator_entry(token_kind::pipe, 50, 51),
    make_binary_operator_entry(token_kind::caret, 60, 61),
    make_binary_operator_entry(token_kind::amp, 70, 71),
    make_binary_operator_entry(token_kind::equal_equal, 80, 81),
    make_binary_operator_entry(token_kind::bang_equal, 80, 81),
    make_binary_operator_entry(token_kind::less, 90, 91),
    make_binary_operator_entry(token_kind::less_equal, 90, 91),
    make_binary_operator_entry(token_kind::spaceship, 90, 91),
    make_binary_operator_entry(token_kind::greater, 90, 91),
    make_binary_operator_entry(token_kind::greater_equal, 90, 91),
    make_binary_operator_entry(token_kind::less_less, 100, 101),
    make_binary_operator_entry(token_kind::greater_greater, 100, 101),
    make_binary_operator_entry(token_kind::plus, 110, 111),
    make_binary_operator_entry(token_kind::minus, 110, 111),
    make_binary_operator_entry(token_kind::star, 120, 121),
    make_binary_operator_entry(token_kind::slash, 120, 121),
    make_binary_operator_entry(token_kind::percent, 120, 121),
    make_binary_operator_entry(token_kind::kw_as, 130, 131),
}};

auto constexpr find_binary_operator(token_kind kind) -> std::optional<binary_operator_entry>
{
    auto entry = std::ranges::find(cp_binary_operator_table, kind, &binary_operator_entry::kind);
    if(entry == cp_binary_operator_table.end()) {
        return std::nullopt;
    }
    return *entry;
}

auto parser::starts_expression(token_kind kind) const -> bool
{
    using enum token_kind;
    auto constexpr expression_starts = std::array {
        identifier,
        integer_literal,
        float_literal,
        char_literal,
        string_literal,
        kw_true,
        kw_false,
        kw_nullptr,
        kw_const,
        kw_ref,
        kw_move,
        kw_forward,
        kw_new,
        kw_delete,
        l_paren,
        l_bracket,
        plus,
        minus,
        kw_not,
        tilde,
        amp,
        star,
        plus_plus,
        minus_minus,
        l_brace,
    };

    return std::ranges::contains(expression_starts, kind);
}

auto parser::expect_expression_start() -> bool
{
    if(starts_expression(peek().kind)) {
        return true;
    }

    report_current(
        diagnostic_kind::expected_expression,
        std::format("expected expression, got {}", token_name(peek().kind))
    );
    return false;
}

auto parser::parse_expression() -> std::optional<expr_id>
{
    return parse_assignment();
}

auto parser::parse_assignment() -> std::optional<expr_id>
{
    auto left = parse_expression_pratt(0);
    if(not left) {
        return std::nullopt;
    }

    if(not is_assignment_operator(peek().kind)) {
        return left;
    }

    auto operation = consume();
    if(not expect_expression_start()) {
        return std::nullopt;
    }
    auto right = parse_assignment();
    if(not right) {
        return std::nullopt;
    }

    auto result = assignment_expr_syntax {
        .full_span = combine_spans(arena.span(*left), arena.span(*right)),
        .operator_kind = operation.kind,
        .left = *left,
        .right = *right,
    };
    return arena.add(expr_syntax{ std::move(result) });
}

auto parser::is_assignment_operator(token_kind kind) const -> bool
{
    using enum token_kind;
    return (
        kind == equal
        or kind == plus_equal
        or kind == minus_equal
        or kind == star_equal
        or kind == slash_equal
        or kind == percent_equal
        or kind == amp_equal
        or kind == pipe_equal
        or kind == caret_equal
        or kind == less_less_equal
        or kind == greater_greater_equal
    );
}

auto parser::parse_expression_pratt(int min_bp) -> std::optional<expr_id>
{
    auto left = parse_unary();
    if(not left) {
        return std::nullopt;
    }

    while(true) {
        auto op_kind = peek().kind;
        auto entry = find_binary_operator(op_kind);
        if(not entry or entry->left_bp < min_bp) {
            break;
        }

        if(op_kind == token_kind::kw_as) {
            consume();
            if(not expect_type_start()) {
                return std::nullopt;
            }
            auto type = parse_type();
            if(not type) {
                return std::nullopt;
            }

            auto cast = cast_expr_syntax {
                .full_span = combine_spans(
                    arena.span(*left),
                    arena.span(*type)
                ),
                .operand = *left,
                .type = *type,
            };
            left = arena.add(expr_syntax{ std::move(cast) });
            continue;
        }

        auto operation = consume();

        if(not expect_expression_start()) {
            return std::nullopt;
        }
        auto right = parse_expression_pratt(entry->right_bp);
        if(not right) {
            return std::nullopt;
        }

        auto combined = binary_expr_syntax {
            .full_span = combine_spans(arena.span(*left), arena.span(*right)),
            .operator_kind = operation.kind,
            .left = *left,
            .right = *right,
        };
        left = arena.add(expr_syntax{ std::move(combined) });
    }

    return left;
}

auto parser::parse_unary() -> std::optional<expr_id>
{
    if(check(token_kind::kw_const) and peek(1uz).kind == token_kind::kw_ref) {
        auto operation = consume();
        consume();
        if(not expect_expression_start()) {
            return std::nullopt;
        }
        auto operand = parse_unary();
        if(not operand) {
            return std::nullopt;
        }

        auto expression = unary_expr_syntax {
            .full_span = combine_spans(operation.span, arena.span(*operand)),
            .operator_kind = operation.kind,
            .position = unary_position::prefix,
            .const_ref = true,
            .operand = *operand,
        };
        return arena.add(expr_syntax{ std::move(expression) });
    }

    if(check_any({
        token_kind::plus,
        token_kind::minus,
        token_kind::kw_not,
        token_kind::kw_ref,
        token_kind::kw_move,
        token_kind::kw_forward,
        token_kind::kw_const,
        token_kind::kw_delete,
        token_kind::tilde,
        token_kind::amp,
        token_kind::star,
        token_kind::plus_plus,
        token_kind::minus_minus,
    })) {
        auto operation = consume();
        if(not expect_expression_start()) {
            return std::nullopt;
        }
        auto operand = parse_unary();
        if(not operand) {
            return std::nullopt;
        }

        auto expression = unary_expr_syntax {
            .full_span = combine_spans(operation.span, arena.span(*operand)),
            .operator_kind = operation.kind,
            .position = unary_position::prefix,
            .operand = *operand,
        };
        return arena.add(expr_syntax{ std::move(expression) });
    }

    return parse_postfix();
}

auto parser::parse_postfix() -> std::optional<expr_id>
{
    auto operand = parse_primary();
    if(not operand) {
        return std::nullopt;
    }

    while(true) {
        auto type_arguments = std::vector<type_argument_syntax>{};
        if(looks_like_generic_call_suffix()) {
            auto parsed = parse_type_argument_list();
            if(not parsed) {
                return std::nullopt;
            }
            type_arguments = std::move(*parsed);
        }

        if(check(token_kind::colon_colon)) {
            auto const& expression = arena.node(*operand);
            if(not is<name_expr_syntax>(expression)) {
                report_current(
                    diagnostic_kind::expected_identifier,
                    "associated function access requires a type name"
                );
                return std::nullopt;
            }
            consume();
            auto name = expect_identifier("associated function name");
            if(not name) {
                return std::nullopt;
            }
            auto const& base = as<name_expr_syntax>(expression);
            auto type = type_from_name(base.name);
            operand = arena.add(expr_syntax {
                associated_name_expr_syntax {
                    .full_span = combine_spans(base.full_span, name->span),
                    .type = type,
                    .name = name->span,
                }
            });
            continue;
        }

        if(check(token_kind::dot)) {
            consume();
            auto name = std::optional<token>{};
            if(check(token_kind::identifier) or check(token_kind::integer_literal)) {
                name = consume();
            } else {
                report_current(
                    diagnostic_kind::expected_identifier,
                    std::format("expected member name, got {}", token_name(peek().kind))
                );
                return std::nullopt;
            }
            operand = arena.add(expr_syntax {
                member_expr_syntax {
                    .full_span = combine_spans(arena.span(*operand), name->span),
                    .object = *operand,
                    .name = name->span,
                }
            });
            continue;
        }

        if(check(token_kind::l_paren)) {
            auto open = consume();
            auto call = call_expr_syntax {
                .callee = *operand,
                .type_arguments = std::move(type_arguments),
            };

            if(not check(token_kind::r_paren)) {
                if(not expect_expression_start()) {
                    return std::nullopt;
                }
                auto argument = parse_expression();
                if(not argument) {
                    return std::nullopt;
                }
                call.arguments.emplace_back(*argument);

                while(check(token_kind::comma)) {
                    consume();
                    if(not expect_expression_start()) {
                        return std::nullopt;
                    }
                    auto next = parse_expression();
                    if(not next) {
                        return std::nullopt;
                    }
                    call.arguments.emplace_back(*next);
                }
            }

            auto close = expect(token_kind::r_paren);
            if(not close) {
                return std::nullopt;
            }

            call.full_span = combine_spans(arena.span(*operand), close->span);
            operand = arena.add(expr_syntax{ std::move(call) });
            continue;
        }

        if(not type_arguments.empty()) {
            report_current(
                diagnostic_kind::expected_token,
                "generic call type arguments must be followed by an argument list"
            );
            return std::nullopt;
        }

        if(check(token_kind::l_bracket)) {
            consume();
            if(not expect_expression_start()) {
                return std::nullopt;
            }
            auto index = parse_expression();
            if(not index) {
                return std::nullopt;
            }
            auto close = expect(token_kind::r_bracket);
            if(not close) {
                return std::nullopt;
            }
            operand = arena.add(expr_syntax {
                index_expr_syntax {
                    .full_span = combine_spans(arena.span(*operand), close->span),
                    .object = *operand,
                    .index = *index,
                }
            });
            continue;
        }

        if(check_any({ token_kind::plus_plus, token_kind::minus_minus })) {
            auto operation = consume();
            auto unary = unary_expr_syntax {
                .full_span = combine_spans(arena.span(*operand), operation.span),
                .operator_kind = operation.kind,
                .position = unary_position::postfix,
                .operand = *operand,
            };
            operand = arena.add(expr_syntax{ std::move(unary) });
            continue;
        }

        break;
    }
    return operand;
}

auto parser::looks_like_generic_call_suffix() const -> bool
{
    if(not check(token_kind::less)) {
        return false;
    }

    auto depth = 0;
    auto lookahead = 0uz;
    while(true) {
        auto kind = peek(lookahead).kind;
        if(kind == token_kind::eof) {
            return false;
        }
        if (
            depth > 0
            and (
                kind == token_kind::r_paren
                or kind == token_kind::r_bracket
                or kind == token_kind::r_brace
                or kind == token_kind::semicolon
            )
        ) {
            return false;
        }

        if(kind == token_kind::less) {
            ++depth;
        } else if(kind == token_kind::greater) {
            --depth;
        } else if(kind == token_kind::greater_greater) {
            depth -= 2;
        } else if(kind == token_kind::greater_greater_equal) {
            return false;
        }

        ++lookahead;
        if(depth == 0) {
            return peek(lookahead).kind == token_kind::l_paren;
        }
        if(depth < 0) {
            return false;
        }
    }
}

auto parser::parse_primary() -> std::optional<expr_id>
{
    std::optional<expr_id> expression{};

    if(check_contextual("match")) {
        return parse_match_expression();
    }

    if(looks_like_lambda_expression()) {
        return parse_lambda_expression();
    }

    if(looks_like_associated_name_expression()) {
        return parse_associated_name_expression();
    }

    if(looks_like_storage_initializer()) {
        return parse_type_initializer();
    }

    if(looks_like_type_initializer()) {
        return parse_type_initializer();
    }

    if(check(token_kind::kw_new)) {
        return parse_new_expression();
    }

    switch(peek().kind) {
        case token_kind::identifier: {
            auto name = expect_identifier("expression name");
            if(not name) {
                return std::nullopt;
            }
            auto node = name_expr_syntax {
                .full_span = name->span,
                .name = name->span,
            };
            expression = arena.add(expr_syntax{ std::move(node) });
            break;
        }
        case token_kind::integer_literal:
        case token_kind::float_literal:
        case token_kind::char_literal:
        case token_kind::string_literal:
        case token_kind::kw_true:
        case token_kind::kw_false:
        case token_kind::kw_nullptr: {
            auto literal = consume();
            expression = arena.add(expr_syntax {
                literal_expr_syntax {
                    .full_span = literal.span,
                }
            });
            break;
        }
        case token_kind::l_bracket:
            expression = parse_array_literal();
            break;
        case token_kind::l_paren:
            expression = parse_paren_expression();
            break;
        case token_kind::l_brace:
            expression = parse_block_expression();
            break;
        default:
            report_current(
                diagnostic_kind::expected_expression,
                std::format(
                    "expected expression, got {}",
                    token_name(peek().kind)
                )
            );
            return std::nullopt;
    }
    return expression;
}

auto parser::looks_like_lambda_expression() const -> bool
{
    if(not check_contextual("f")) {
        return false;
    }

    auto start = 1uz;
    if(peek(start).kind == token_kind::less) {
        auto depth = 0;
        while(true) {
            auto kind = peek(start).kind;
            if(kind == token_kind::eof) {
                return false;
            }
            if(kind == token_kind::less) {
                ++depth;
            } else if(kind == token_kind::greater) {
                --depth;
            } else if(kind == token_kind::greater_greater) {
                depth -= 2;
            } else if(kind == token_kind::greater_greater_equal) {
                return false;
            }

            ++start;
            if(depth == 0) {
                break;
            }
            if(depth < 0) {
                return false;
            }
        }
    }
    if(peek(start).kind != token_kind::l_paren) {
        return false;
    }

    auto depth = 0;
    auto lookahead = start;
    while(true) {
        auto kind = peek(lookahead).kind;
        if(kind == token_kind::eof) {
            return false;
        }

        if(kind == token_kind::l_paren) {
            ++depth;
        } else if(kind == token_kind::r_paren) {
            --depth;
        }

        ++lookahead;
        if(depth == 0) {
            auto next = peek(lookahead).kind;
            return next == token_kind::arrow or next == token_kind::equal or next == token_kind::l_brace;
        }
        if(depth < 0) {
            return false;
        }
    }
}

auto parser::parse_match_expression() -> std::optional<expr_id>
{
    auto match_kw = expect_identifier("match");
    auto value = std::optional<expr_id>{};
    if(check(token_kind::identifier) and peek(1uz).kind == token_kind::l_brace) {
        auto const name = consume();
        value = arena.add(expr_syntax {
            name_expr_syntax {
                .full_span = name.span,
                .name = name.span,
            }
        });
    } else {
        value = parse_expected_expression();
    }
    auto open = expect(token_kind::l_brace);
    if(not match_kw or not value or not open) {
        return std::nullopt;
    }

    auto arms = std::vector<match_arm_syntax>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        auto arm = parse_match_arm();
        if(not arm) {
            synchronize_statement();
            continue;
        }
        arms.emplace_back(std::move(*arm));
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    return arena.add(expr_syntax {
        match_expr_syntax {
            .full_span = combine_spans(match_kw->span, close->span),
            .value = *value,
            .arms = std::move(arms),
        }
    });
}

auto parser::parse_match_arm() -> std::optional<match_arm_syntax>
{
    auto pattern = parse_match_pattern();
    auto equal = expect(token_kind::equal);
    auto greater = expect(token_kind::greater);
    auto value = parse_expected_expression();
    auto comma = expect(token_kind::comma);
    if(not pattern or not equal or not greater or not value or not comma) {
        return std::nullopt;
    }

    return match_arm_syntax {
        .full_span = combine_spans(
            std::visit([](auto const& item) { return item.full_span; }, *pattern),
            comma->span
        ),
        .pattern = std::move(*pattern),
        .value = *value,
    };
}

auto parser::parse_match_pattern() -> std::optional<match_pattern_syntax>
{
    if(check(token_kind::identifier) and peek().text == "_") {
        auto wildcard = consume();
        return match_pattern_syntax {
            match_wildcard_pattern_syntax {
                .full_span = wildcard.span,
            }
        };
    }

    auto dot = expect(token_kind::dot);
    auto name = expect_identifier("variant case name");
    if(not dot or not name) {
        return std::nullopt;
    }

    auto bindings = std::vector<source_span>{};
    auto end = name->span;
    if(check(token_kind::l_paren)) {
        consume();
        auto first = expect_identifier("pattern binding");
        if(not first) {
            return std::nullopt;
        }
        bindings.emplace_back(first->span);
        while(check(token_kind::comma)) {
            consume();
            auto next = expect_identifier("pattern binding");
            if(not next) {
                return std::nullopt;
            }
            bindings.emplace_back(next->span);
        }
        auto close = expect(token_kind::r_paren);
        if(not close) {
            return std::nullopt;
        }
        end = close->span;
    }

    return match_pattern_syntax {
        match_case_pattern_syntax {
            .full_span = combine_spans(dot->span, end),
            .name = name->span,
            .bindings = std::move(bindings),
        }
    };
}

auto parser::parse_lambda_expression() -> std::optional<expr_id>
{
    auto marker = expect_identifier("lambda marker");
    if(not marker) {
        return std::nullopt;
    }

    auto generic_parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::less)) {
        auto parsed = parse_generic_parameter_list();
        if(not parsed) {
            return std::nullopt;
        }
        generic_parameters = std::move(*parsed);
    }

    auto l_paren = expect(token_kind::l_paren);
    if(not l_paren) {
        return std::nullopt;
    }
    auto parameters = parse_lambda_parameter_list();
    auto r_paren = expect(token_kind::r_paren);
    auto return_type = std::optional<type_id>{};
    if(check(token_kind::arrow)) {
        consume();
        return_type = parse_expected_type();
    }
    if(not generic_parameters.empty() and not return_type) {
        report_current(
            diagnostic_kind::expected_token,
            "generic lambda requires an explicit return type"
        );
        return std::nullopt;
    }
    auto body = parse_lambda_body(marker->span);
    if(not parameters or not r_paren or not body) {
        return std::nullopt;
    }

    auto function = arena.add(function_syntax {
        .full_span = combine_spans(marker->span, arena.span(*body)),
        .kind = function_syntax_kind::lambda,
        .name = marker->span,
        .generic_parameters = std::move(generic_parameters),
        .parameters = std::move(*parameters),
        .return_type = return_type,
        .body = *body,
    });
    synthetic_functions.emplace_back(function);
    return arena.add(expr_syntax {
        lambda_expr_syntax {
            .full_span = combine_spans(marker->span, arena.span(*body)),
            .function = function,
        }
    });
}

auto parser::parse_lambda_body(source_span start) -> std::optional<stmt_id>
{
    if(check(token_kind::l_brace)) {
        auto expression = parse_block_expression();
        if(not expression) {
            return std::nullopt;
        }
        auto const& block = as<block_expr_syntax>(arena.node(*expression));
        auto statements = block.statements;
        if(block.tail) {
            statements.emplace_back(arena.add(statement_syntax {
                return_statement_syntax {
                    .full_span = arena.span(*block.tail),
                    .value = *block.tail,
                }
            }));
        }
        return arena.add(statement_syntax {
            block_statement_syntax {
                .full_span = combine_spans(start, arena.span(*expression)),
                .statements = std::move(statements),
            }
        });
    }

    auto equal = expect(token_kind::equal);
    auto greater = expect(token_kind::greater);
    auto expression = parse_expected_expression();
    if(not equal or not greater or not expression) {
        return std::nullopt;
    }

    auto returned = arena.add(statement_syntax {
        return_statement_syntax {
            .full_span = arena.span(*expression),
            .value = *expression,
        }
    });
    return arena.add(statement_syntax {
        block_statement_syntax {
            .full_span = combine_spans(equal->span, arena.span(*expression)),
            .statements = { returned },
        }
    });
}

auto parser::looks_like_associated_name_expression() const -> bool
{
    if(not check(token_kind::identifier)) {
        return false;
    }

    auto lookahead = 1uz;
    if(peek(lookahead).kind == token_kind::less) {
        auto depth = 0;
        while(true) {
            auto kind = peek(lookahead).kind;
            if(kind == token_kind::eof) {
                return false;
            }
            if (
                depth > 0
                and (
                    kind == token_kind::r_paren
                    or kind == token_kind::r_bracket
                    or kind == token_kind::r_brace
                    or kind == token_kind::semicolon
                )
            ) {
                return false;
            }
            if(kind == token_kind::less) {
                ++depth;
            } else if(kind == token_kind::greater) {
                --depth;
            } else if(kind == token_kind::greater_greater) {
                depth -= 2;
            } else if(kind == token_kind::greater_greater_equal) {
                return false;
            }
            ++lookahead;
            if(depth == 0) {
                break;
            }
            if(depth < 0) {
                return false;
            }
        }
    }

    return peek(lookahead).kind == token_kind::colon_colon;
}

auto parser::parse_associated_name_expression() -> std::optional<expr_id>
{
    auto type = parse_type(false);
    auto delimiter = expect(token_kind::colon_colon);
    auto name = expect_identifier("associated function name");
    if(not type or not delimiter or not name) {
        return std::nullopt;
    }

    return arena.add(expr_syntax {
        associated_name_expr_syntax {
            .full_span = combine_spans(arena.span(*type), name->span),
            .type = *type,
            .name = name->span,
        }
    });
}

auto parser::looks_like_type_initializer() const -> bool
{
    if(not check(token_kind::identifier)) {
        return false;
    }

    auto lookahead = 1uz;
    if(peek(lookahead).kind == token_kind::less) {
        auto depth = 0;
        while(true) {
            auto kind = peek(lookahead).kind;
            if(kind == token_kind::eof) {
                return false;
            }
            if (
                depth > 0
                and (
                    kind == token_kind::r_paren
                    or kind == token_kind::r_bracket
                    or kind == token_kind::r_brace
                    or kind == token_kind::semicolon
                )
            ) {
                return false;
            }
            if(kind == token_kind::less) {
                ++depth;
            } else if(kind == token_kind::greater) {
                --depth;
            } else if(kind == token_kind::greater_greater) {
                depth -= 2;
            } else if(kind == token_kind::greater_greater_equal) {
                return false;
            }
            ++lookahead;
            if(depth == 0) {
                break;
            }
            if(depth < 0) {
                return false;
            }
        }
    }

    if(peek(lookahead).kind == token_kind::kw_const) {
        ++lookahead;
    }
    while(peek(lookahead).kind == token_kind::star) {
        ++lookahead;
    }
    if(peek(lookahead).kind == token_kind::amp) {
        ++lookahead;
    }
    return peek(lookahead).kind == token_kind::l_brace;
}

auto parser::looks_like_storage_initializer() const -> bool
{
    if(not check_contextual("storage") or not starts_type(peek(1uz).kind)) {
        return false;
    }

    auto lookahead = 1uz;
    auto bracket_depth = 0;
    auto paren_depth = 0;
    while(true) {
        auto kind = peek(lookahead).kind;
        if(kind == token_kind::eof or (kind == token_kind::semicolon and bracket_depth == 0 and paren_depth == 0)) {
            return false;
        }
        if(kind == token_kind::l_bracket) {
            ++bracket_depth;
        } else if(kind == token_kind::r_bracket) {
            --bracket_depth;
        } else if(kind == token_kind::l_paren) {
            ++paren_depth;
        } else if(kind == token_kind::r_paren) {
            --paren_depth;
        } else if(kind == token_kind::l_brace and bracket_depth == 0 and paren_depth == 0) {
            return true;
        }
        if(bracket_depth < 0 or paren_depth < 0) {
            return false;
        }
        ++lookahead;
    }
}

auto parser::parse_type_initializer() -> std::optional<expr_id>
{
    auto type = parse_type();
    if(not type) {
        return std::nullopt;
    }
    return parse_struct_initializer_list(*type, arena.span(*type));
}

auto parser::parse_new_expression() -> std::optional<expr_id>
{
    auto start = expect(token_kind::kw_new);
    auto type = parse_expected_type();
    if(not start or not type) {
        return std::nullopt;
    }
    auto initializer = parse_struct_initializer_list(*type, arena.span(*type));
    if(not initializer) {
        return std::nullopt;
    }
    return arena.add(expr_syntax {
        new_expr_syntax {
            .full_span = combine_spans(start->span, arena.span(*initializer)),
            .type = *type,
            .initializer = *initializer,
        }
    });
}

auto parser::parse_struct_initializer_list(type_id type, source_span start) -> std::optional<expr_id>
{
    auto open = expect(token_kind::l_brace);
    if(not open) {
        return std::nullopt;
    }

    auto initializers = std::vector<struct_initializer_syntax>{};
    auto named = std::optional<bool>{};
    if(not check(token_kind::r_brace)) {
        while(true) {
            if(check(token_kind::dot)) {
                if(named == false) {
                    report_current(
                        diagnostic_kind::unexpected_token,
                        "named field initializers cannot follow positional initializers"
                    );
                    return std::nullopt;
                }
                named = true;
                auto dot = consume();
                auto name = expect_identifier("field name");
                auto equal = expect(token_kind::equal);
                auto value = parse_expected_expression();
                if(not name or not equal or not value) {
                    return std::nullopt;
                }
                initializers.emplace_back(
                    std::in_place_type<named_field_initializer_syntax>,
                    combine_spans(dot.span, arena.span(*value)),
                    name->span,
                    *value
                );
            } else {
                if(named == true) {
                    report_current(
                        diagnostic_kind::unexpected_token,
                        "positional initializers cannot follow named field initializers"
                    );
                    return std::nullopt;
                }
                named = false;
                auto value = parse_expected_expression();
                if(not value) {
                    return std::nullopt;
                }
                initializers.emplace_back(
                    std::in_place_type<positional_initializer_syntax>,
                    arena.span(*value),
                    *value
                );
            }

            if(not check(token_kind::comma)) {
                break;
            }
            consume();
            if(check(token_kind::r_brace)) {
                break;
            }
        }
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }
    return arena.add(expr_syntax {
        struct_init_expr_syntax {
            .full_span = combine_spans(start, close->span),
            .type = type,
            .initializers = std::move(initializers),
        }
    });
}

auto parser::parse_block_expression() -> std::optional<expr_id>
{
    auto open = expect(token_kind::l_brace);
    if(not open) {
        return std::nullopt;
    }

    auto looks_like_declaration = [&]() {
        if(check(token_kind::kw_let)) {
            return true;
        }
        if(not check(token_kind::kw_const)) {
            return false;
        }

        auto lookahead = 1uz;
        if(peek(lookahead).kind == token_kind::kw_ref) {
            ++lookahead;
        }
        if(peek(lookahead).kind == token_kind::l_paren) {
            return true;
        }
        if(peek(lookahead).kind != token_kind::identifier) {
            return false;
        }
        auto const after_name = peek(lookahead + 1uz).kind;
        return after_name == token_kind::equal or after_name == token_kind::colon;
    };

    auto statements = std::vector<stmt_id>{};
    auto tail = std::optional<expr_id>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        if(consume_empty_statement()) {
            continue;
        }

        if (
            check_contextual("type")
            and peek(1uz).kind == token_kind::identifier
            and peek(2uz).kind == token_kind::equal
        ) {
            auto statement = parse_statement();
            if(not statement) {
                synchronize_statement();
                continue;
            }
            statements.emplace_back(*statement);
            continue;
        }

        if(looks_like_declaration()) {
            auto statement = parse_statement();
            if(not statement) {
                synchronize_statement();
                continue;
            }
            statements.emplace_back(*statement);
            continue;
        }

        if(starts_expression(peek().kind)) {
            auto expression = parse_expression();
            if(not expression) {
                return std::nullopt;
            }
            if(check(token_kind::semicolon)) {
                auto semicolon = consume();
                statements.emplace_back(arena.add(statement_syntax {
                    expression_statement_syntax {
                        .full_span = combine_spans(arena.span(*expression), semicolon.span),
                        .expression = *expression,
                    }
                }));
                continue;
            }
            tail = *expression;
            break;
        }

        auto statement = parse_statement();
        if(not statement) {
            synchronize_statement();
            continue;
        }
        statements.emplace_back(*statement);
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }
    return arena.add(expr_syntax {
        block_expr_syntax {
            .full_span = combine_spans(open->span, close->span),
            .statements = std::move(statements),
            .tail = tail,
        }
    });
}

auto parser::type_from_name(source_span name) -> type_id
{
    return arena.add(type_syntax {
        .full_span = name,
        .name = name,
    });
}

auto parser::parse_array_literal() -> std::optional<expr_id>
{
    auto open = expect(token_kind::l_bracket);
    if(not open) {
        return std::nullopt;
    }

    auto elements = std::vector<expr_id>{};
    if(not check(token_kind::r_bracket)) {
        if(not expect_expression_start()) {
            return std::nullopt;
        }
        auto element = parse_expression();
        if(not element) {
            return std::nullopt;
        }
        elements.emplace_back(*element);

        while(check(token_kind::comma)) {
            consume();
            if(not expect_expression_start()) {
                return std::nullopt;
            }
            auto next = parse_expression();
            if(not next) {
                return std::nullopt;
            }
            elements.emplace_back(*next);
        }
    }

    auto close = expect(token_kind::r_bracket);
    if(not close) {
        return std::nullopt;
    }

    auto span = combine_spans(open->span, close->span);
    return arena.add(expr_syntax {
        array_literal_expr_syntax {
            .full_span = span,
            .elements = std::move(elements),
        }
    });
}

auto parser::parse_paren_expression() -> std::optional<expr_id>
{
    auto open = expect(token_kind::l_paren);
    if(not open) {
        return std::nullopt;
    }

    if(not expect_expression_start()) {
        return std::nullopt;
    }
    auto first = parse_expression();
    if(not first) {
        return std::nullopt;
    }

    auto elements = std::vector<expr_id>{};
    auto is_tuple = false;
    if(check(token_kind::comma)) {
        is_tuple = true;
        elements.emplace_back(*first);

        while(check(token_kind::comma)) {
            consume();
            if(check(token_kind::r_paren)) {
                break;
            }
            if(not expect_expression_start()) {
                return std::nullopt;
            }
            auto next = parse_expression();
            if(not next) {
                return std::nullopt;
            }
            elements.emplace_back(*next);
        }
    } else {
        elements.emplace_back(*first);
    }

    auto close = expect(token_kind::r_paren);
    if(not close) {
        return std::nullopt;
    }

    auto span = combine_spans(open->span, close->span);
    if(is_tuple) {
        return arena.add(expr_syntax {
            tuple_literal_expr_syntax {
                .full_span = span,
                .elements = std::move(elements),
            }
        });
    }

    return arena.add(expr_syntax {
        grouped_expr_syntax {
            .full_span = span,
            .expression = elements.front(),
        }
    });
}
