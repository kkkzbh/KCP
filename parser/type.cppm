module parser:type;

import std;
import lexer;
import parser.ast;
import diagnostic;
import parser;

auto parser::starts_type(token_kind kind) const -> bool
{
    return kind == token_kind::identifier;
}

auto parser::expect_type_start() -> bool
{
    if(starts_type(peek().kind)) {
        return true;
    }

    report_current(
        diagnostic_kind::expected_type,
        std::format("expected type, got {}", token_name(peek().kind))
    );
    return false;
}

auto parser::parse_type() -> std::optional<type_id>
{
    if(not check(token_kind::identifier)) {
        report_current(
            diagnostic_kind::expected_type,
            std::format("expected type, got {}", token_name(peek().kind))
        );
        return std::nullopt;
    }

    auto name = expect_identifier("type name");
    if(not name) {
        return std::nullopt;
    }

    auto type = type_syntax{};
    type.name = name->span;
    type.full_span = type.name;

    if(check(token_kind::less)) {
        consume();
        if(check(token_kind::greater)) {
            report_current(
                diagnostic_kind::expected_type,
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

        while(check(token_kind::comma)) {
            consume();
            if(not expect_type_argument_start()) {
                return std::nullopt;
            }
            auto next = parse_type_argument();
            if(not next) {
                return std::nullopt;
            }
            type.arguments.emplace_back(*next);
        }

        auto close = expect_closing_angle();
        if(not close) {
            return std::nullopt;
        }
        type.full_span = combine_spans(type.full_span, close->span);
    }

    if(check(token_kind::kw_const)) {
        type.is_const = true;
        auto qualifier = consume();
        type.full_span = combine_spans(type.full_span, qualifier.span);
    }

    while(check(token_kind::star)) {
        auto suffix = consume();
        type.suffix_operators.emplace_back(suffix.kind);
        type.full_span = combine_spans(type.full_span, suffix.span);
    }

    if(check(token_kind::amp)) {
        auto suffix = consume();
        type.suffix_operators.emplace_back(suffix.kind);
        type.full_span = combine_spans(type.full_span, suffix.span);
    }
    return arena.add(std::move(type));
}

auto parser::starts_type_argument(token_kind kind) const -> bool
{
    return kind == token_kind::integer_literal or starts_type(kind);
}

auto parser::expect_type_argument_start() -> bool
{
    if(starts_type_argument(peek().kind)) {
        return true;
    }

    if(check_any({
        token_kind::greater,
        token_kind::greater_greater,
        token_kind::greater_greater_equal,
    })) {
        report_current(
            diagnostic_kind::expected_type,
            "expected type argument before closing angle bracket"
        );
        return false;
    }

    report_current(
        diagnostic_kind::expected_type,
        std::format("expected type, got {}", token_name(peek().kind))
    );
    return false;
}

auto parser::parse_type_argument() -> std::optional<type_argument_syntax>
{
    if(check(token_kind::integer_literal)) {
        return type_argument_syntax {
            type_argument_literal_syntax {
                .literal = consume().span,
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
