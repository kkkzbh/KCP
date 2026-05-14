module parser.syntax.recursive_descent:expression;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import parser.expression;
import :context;
import :type;

struct expression_parser
{
    explicit expression_parser(parser_context& context) :
        context(context) {}

    auto starts(token_kind kind) const -> bool
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

    auto expect_start() -> bool
    {
        if(starts(context.peek().kind)) {
            return true;
        }

        context.report_current(
            parser_diagnostic_code::expected_expression,
            std::format("expected expression, got {}", context.token_name(context.peek().kind))
        );
        return false;
    }

    auto parse_expression() -> std::optional<expr_id>
    {
        return parse_assignment();
    }

    auto parse_assignment() -> std::optional<expr_id>
    {
        auto left = parse_expression_pratt(0);
        if(not left) {
            return std::nullopt;
        }

        if(not is_assignment_operator(context.peek().kind)) {
            return left;
        }

        auto operation = context.consume();
        if(not expect_start()) {
            return std::nullopt;
        }
        auto right = parse_assignment();
        if(not right) {
            return std::nullopt;
        }

        auto result = assignment_expr_syntax {
            .full_span = combine_spans(context.arena.span(*left), context.arena.span(*right)),
            .operator_kind = operation.kind,
            .left = *left,
            .right = *right,
        };
        return context.arena.add(expr_syntax{ std::move(result) });
    }

    auto is_assignment_operator(token_kind kind) const -> bool
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

    auto parse_expression_pratt(int min_bp) -> std::optional<expr_id>
    {
        auto left = parse_unary();
        if(not left) {
            return std::nullopt;
        }

        while(true) {
            auto op_kind = context.peek().kind;
            auto entry = find_binary_operator(op_kind);
            if(not entry or entry->left_bp < min_bp) {
                break;
            }

            if(op_kind == token_kind::kw_as) {
                context.consume();
                auto types = type_parser{ context };
                if(not types.expect_start()) {
                    return std::nullopt;
                }
                auto type = types.parse_type();
                if(not type) {
                    return std::nullopt;
                }

                auto cast = cast_expr_syntax {
                    .full_span = combine_spans(
                        context.arena.span(*left),
                        context.arena.span(*type)
                    ),
                    .operand = *left,
                    .type = *type,
                };
                left = context.arena.add(expr_syntax{ std::move(cast) });
                continue;
            }

            auto operation = context.consume();

            if(not expect_start()) {
                return std::nullopt;
            }
            auto right = parse_expression_pratt(entry->right_bp);
            if(not right) {
                return std::nullopt;
            }

            auto combined = binary_expr_syntax {
                .full_span = combine_spans(context.arena.span(*left), context.arena.span(*right)),
                .operator_kind = operation.kind,
                .left = *left,
                .right = *right,
            };
            left = context.arena.add(expr_syntax{ std::move(combined) });
        }

        return left;
    }

    auto parse_unary() -> std::optional<expr_id>
    {
        if(context.check_any({
            token_kind::plus,
            token_kind::minus,
            token_kind::kw_not,
            token_kind::tilde,
            token_kind::amp,
            token_kind::star,
            token_kind::plus_plus,
            token_kind::minus_minus,
        })) {
            auto operation = context.consume();
            if(not expect_start()) {
                return std::nullopt;
            }
            auto operand = parse_unary();
            if(not operand) {
                return std::nullopt;
            }

            auto expression = unary_expr_syntax {
                .full_span = combine_spans(operation.span, context.arena.span(*operand)),
                .operator_kind = operation.kind,
                .position = unary_position::prefix,
                .operand = *operand,
            };
            return context.arena.add(expr_syntax{ std::move(expression) });
        }

        return parse_postfix();
    }

    auto parse_postfix() -> std::optional<expr_id>
    {
        auto operand = parse_primary();
        if(not operand) {
            return std::nullopt;
        }

        while(true) {
            if(context.check(token_kind::l_paren)) {
                auto open = context.consume();
                auto call = call_expr_syntax {
                    .callee = *operand,
                };

                if(not context.check(token_kind::r_paren)) {
                    if(not expect_start()) {
                        return std::nullopt;
                    }
                    auto argument = parse_expression();
                    if(not argument) {
                        return std::nullopt;
                    }
                    call.arguments.emplace_back(*argument);

                    while(context.check(token_kind::comma)) {
                        context.consume();
                        if(not expect_start()) {
                            return std::nullopt;
                        }
                        auto next = parse_expression();
                        if(not next) {
                            return std::nullopt;
                        }
                        call.arguments.emplace_back(*next);
                    }
                }

                auto close = context.expect(token_kind::r_paren);
                if(not close) {
                    return std::nullopt;
                }

                call.full_span = combine_spans(context.arena.span(*operand), close->span);
                operand = context.arena.add(expr_syntax{ std::move(call) });
                continue;
            }

            if(context.check_any({ token_kind::plus_plus, token_kind::minus_minus })) {
                auto operation = context.consume();
                auto unary = unary_expr_syntax {
                    .full_span = combine_spans(context.arena.span(*operand), operation.span),
                    .operator_kind = operation.kind,
                    .position = unary_position::postfix,
                    .operand = *operand,
                };
                operand = context.arena.add(expr_syntax{ std::move(unary) });
                continue;
            }

            break;
        }
        return operand;
    }

    auto parse_primary() -> std::optional<expr_id>
    {
        std::optional<expr_id> expression{};

        switch(context.peek().kind) {
            case token_kind::identifier: {
                auto name = context.expect_identifier("expression name");
                if(not name) {
                    return std::nullopt;
                }
                auto node = name_expr_syntax {
                    .full_span = name->span,
                    .name = name->span,
                };
                expression = context.arena.add(expr_syntax{ std::move(node) });
                break;
            }
            case token_kind::integer_literal:
            case token_kind::float_literal:
            case token_kind::char_literal:
            case token_kind::string_literal:
            case token_kind::kw_true:
            case token_kind::kw_false: {
                auto literal = context.consume();
                expression = context.arena.add(expr_syntax {
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
                context.report_current(
                    parser_diagnostic_code::expected_expression,
                    std::format(
                        "expected expression, got {}",
                        context.token_name(context.peek().kind)
                    )
                );
                return std::nullopt;
        }
        return expression;
    }

    auto parse_bracket_literal(
        token_kind open_kind,
        token_kind close_kind
    ) -> std::optional<expr_id>
    {
        auto open = context.expect(open_kind);
        if(not open) {
            return std::nullopt;
        }

        auto elements = std::vector<expr_id>{};
        if(not context.check(close_kind)) {
            if(not expect_start()) {
                return std::nullopt;
            }
            auto element = parse_expression();
            if(not element) {
                return std::nullopt;
            }
            elements.emplace_back(*element);

            while(context.check(token_kind::comma)) {
                context.consume();
                if(not expect_start()) {
                    return std::nullopt;
                }
                auto next = parse_expression();
                if(not next) {
                    return std::nullopt;
                }
                elements.emplace_back(*next);
            }
        }

        auto close = context.expect(close_kind);
        if(not close) {
            return std::nullopt;
        }

        auto span = combine_spans(open->span, close->span);
        if(open_kind == token_kind::l_bracket) {
            return context.arena.add(expr_syntax {
                array_literal_expr_syntax {
                    .full_span = span,
                    .elements = std::move(elements),
                }
            });
        }

        return context.arena.add(expr_syntax {
            sequence_literal_expr_syntax {
                .full_span = span,
                .elements = std::move(elements),
            }
        });
    }

    auto parse_paren_expression() -> std::optional<expr_id>
    {
        auto open = context.expect(token_kind::l_paren);
        if(not open) {
            return std::nullopt;
        }

        if(not expect_start()) {
            return std::nullopt;
        }
        auto first = parse_expression();
        if(not first) {
            return std::nullopt;
        }

        auto elements = std::vector<expr_id>{};
        if(context.check(token_kind::comma)) {
            elements.emplace_back(*first);

            while(context.check(token_kind::comma)) {
                context.consume();
                if(not expect_start()) {
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

        auto close = context.expect(token_kind::r_paren);
        if(not close) {
            return std::nullopt;
        }

        auto span = combine_spans(open->span, close->span);
        if(elements.size() > 1uz) {
            return context.arena.add(expr_syntax {
                tuple_literal_expr_syntax {
                    .full_span = span,
                    .elements = std::move(elements),
                }
            });
        }

        return context.arena.add(expr_syntax {
            grouped_expr_syntax {
                .full_span = span,
                .expression = elements.front(),
            }
        });
    }

    parser_context& context;
};
