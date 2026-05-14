module parser.syntax.recursive_descent:type;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import :context;

struct type_parser
{
    explicit type_parser(parser_context& context) :
        context(context) {}

    auto starts(token_kind kind) const -> bool
    {
        return kind == token_kind::identifier;
    }

    auto expect_start() -> bool
    {
        if(starts(context.peek().kind)) {
            return true;
        }

        context.report_current(
            parser_diagnostic_code::expected_type,
            std::format("expected type, got {}", context.token_name(context.peek().kind))
        );
        return false;
    }

    auto parse_type() -> std::optional<type_id>
    {
        if(not context.check(token_kind::identifier)) {
            context.report_current(
                parser_diagnostic_code::expected_type,
                std::format("expected type, got {}", context.token_name(context.peek().kind))
            );
            return std::nullopt;
        }

        auto name = context.expect_identifier("type name");
        if(not name) {
            return std::nullopt;
        }

        auto type = type_syntax{};
        type.name = name->span;
        type.full_span = type.name;

        if(context.check(token_kind::less)) {
            context.consume();
            if(context.check(token_kind::greater)) {
                context.report_current(
                    parser_diagnostic_code::expected_type,
                    "empty type argument list is not allowed"
                );
                return std::nullopt;
            }

            if(not expect_type_argument_start()) {
                return std::nullopt;
            }
            auto argument = parse_type_argument();
            if(not argument) {
                return std::nullopt;
            }

            type.arguments.emplace_back(*argument);

            while(context.check(token_kind::comma)) {
                context.consume();
                if(not expect_type_argument_start()) {
                    return std::nullopt;
                }
                auto next = parse_type_argument();
                if(not next) {
                    return std::nullopt;
                }
                type.arguments.emplace_back(*next);
            }

            auto close = context.expect_closing_angle();
            if(not close) {
                return std::nullopt;
            }
            type.full_span = combine_spans(type.full_span, close->span);
        }

        if(context.check(token_kind::kw_const)) {
            type.is_const = true;
            auto qualifier = context.consume();
            type.full_span = combine_spans(type.full_span, qualifier.span);
        }

        while(context.check(token_kind::star)) {
            auto suffix = context.consume();
            type.suffix_operators.emplace_back(suffix.kind);
            type.full_span = combine_spans(type.full_span, suffix.span);
        }

        if(context.check(token_kind::amp)) {
            auto suffix = context.consume();
            type.suffix_operators.emplace_back(suffix.kind);
            type.full_span = combine_spans(type.full_span, suffix.span);
        }
        return context.arena.add(std::move(type));
    }

    auto starts_type_argument(token_kind kind) const -> bool
    {
        return kind == token_kind::integer_literal or starts(kind);
    }

    auto expect_type_argument_start() -> bool
    {
        if(starts_type_argument(context.peek().kind)) {
            return true;
        }

        if(context.check_any({
            token_kind::greater,
            token_kind::greater_greater,
            token_kind::greater_greater_equal,
        })) {
            context.report_current(
                parser_diagnostic_code::expected_type,
                "expected type argument before closing angle bracket"
            );
            return false;
        }

        context.report_current(
            parser_diagnostic_code::expected_type,
            std::format("expected type, got {}", context.token_name(context.peek().kind))
        );
        return false;
    }

    auto parse_type_argument() -> std::optional<type_argument_syntax>
    {
        if(context.check(token_kind::integer_literal)) {
            return type_argument_syntax {
                type_argument_literal_syntax {
                    .literal = context.consume().span,
                }
            };
        }

        auto nested = parse_type();
        if(not nested) {
            return std::nullopt;
        }

        return type_argument_syntax {
            type_argument_type_syntax {
                .type = *nested,
            }
        };
    }

    parser_context& context;
};
