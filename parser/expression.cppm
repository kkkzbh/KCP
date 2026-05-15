module parser:parse_expression;

import std;
import lexer;
import parser.ast;
import diagnostic;
import parser;

struct binary_operator_entry
{
    token_kind kind;
    int left_bp;
    int right_bp;
};

auto constexpr make_binary_operator_entry(
    token_kind kind,
    int left_bp,
    int right_bp
) -> binary_operator_entry
{
    return {
        .kind = kind,
        .left_bp = left_bp,
        .right_bp = right_bp,
    };
}

auto constexpr inline cp_binary_operator_table = std::array<binary_operator_entry, 19uz>{{
    make_binary_operator_entry(token_kind::kw_or, 30, 31),
    make_binary_operator_entry(token_kind::kw_and, 40, 41),
    make_binary_operator_entry(token_kind::pipe, 50, 51),
    make_binary_operator_entry(token_kind::caret, 60, 61),
    make_binary_operator_entry(token_kind::amp, 70, 71),
    make_binary_operator_entry(token_kind::equal_equal, 80, 81),
    make_binary_operator_entry(token_kind::bang_equal, 80, 81),
    make_binary_operator_entry(token_kind::less, 90, 91),
    make_binary_operator_entry(token_kind::less_equal, 90, 91),
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
        l_paren,
        l_bracket,
        l_brace,
        plus,
        minus,
        kw_not,
        tilde,
        amp,
        star,
        plus_plus,
        minus_minus,
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
    if(check_any({
        token_kind::plus,
        token_kind::minus,
        token_kind::kw_not,
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
        if(check(token_kind::l_paren)) {
            auto open = consume();
            auto call = call_expr_syntax {
                .callee = *operand,
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

auto parser::parse_primary() -> std::optional<expr_id>
{
    std::optional<expr_id> expression{};

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
        case token_kind::kw_false: {
            auto literal = consume();
            expression = arena.add(expr_syntax {
                literal_expr_syntax {
                    .full_span = literal.span,
                }
            });
            break;
        }
        case token_kind::l_bracket:
            expression = parse_bracket_literal(token_kind::l_bracket, token_kind::r_bracket);
            break;
        case token_kind::l_brace:
            expression = parse_bracket_literal(token_kind::l_brace, token_kind::r_brace);
            break;
        case token_kind::l_paren:
            expression = parse_paren_expression();
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

auto parser::parse_bracket_literal(
    token_kind open_kind,
    token_kind close_kind
) -> std::optional<expr_id>
{
    auto open = expect(open_kind);
    if(not open) {
        return std::nullopt;
    }

    auto elements = std::vector<expr_id>{};
    if(not check(close_kind)) {
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

    auto close = expect(close_kind);
    if(not close) {
        return std::nullopt;
    }

    auto span = combine_spans(open->span, close->span);
    if(open_kind == token_kind::l_bracket) {
        return arena.add(expr_syntax {
            array_literal_expr_syntax {
                .full_span = span,
                .elements = std::move(elements),
            }
        });
    }

    return arena.add(expr_syntax {
        sequence_literal_expr_syntax {
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
    if(check(token_kind::comma)) {
        elements.emplace_back(*first);

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
    } else {
        elements.emplace_back(*first);
    }

    auto close = expect(token_kind::r_paren);
    if(not close) {
        return std::nullopt;
    }

    auto span = combine_spans(open->span, close->span);
    if(elements.size() > 1uz) {
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
