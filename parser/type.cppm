module parser:type;

import std;
import lexer;
import parser.ast;
import diagnostic;
import :state;

auto parser::starts_type(token_kind kind) const -> bool
{
    return kind == token_kind::identifier
           or kind == token_kind::bang
           or kind == token_kind::l_bracket
           or kind == token_kind::l_paren;
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

auto parser::parse_type(bool allow_associated_names) -> std::optional<type_id>
{
    if(looks_like_decltype()) {
        return parse_decltype();
    }

    if(looks_like_function_type()) {
        return parse_function_type();
    }

    auto type = std::optional<type_syntax>{};
    if(check(token_kind::bang)) {
        auto token = consume();
        type = type_syntax {
            .full_span = token.span,
            .name = token.span,
            .is_never_type = true,
        };
    } else if(check(token_kind::l_bracket)) {
        type = parse_array_type();
    } else if(check(token_kind::l_paren)) {
        type = parse_tuple_or_grouped_type();
    } else {
        type = parse_named_type(allow_associated_names);
    }
    if(not type) {
        return std::nullopt;
    }
    return finish_type_suffix(std::move(*type));
}

auto parser::parse_named_type(bool allow_associated_names) -> std::optional<type_syntax>
{
    auto name = expect_identifier("type name");
    if(not name) {
        return std::nullopt;
    }

    auto type = type_syntax{};
    type.name = name->span;
    type.full_span = type.name;

    if(check(token_kind::less)) {
        auto arguments = parse_type_argument_list();
        if(not arguments) {
            return std::nullopt;
        }
        type.arguments = std::move(*arguments);
        auto const& last = type.arguments.back();
        if(auto const* argument = std::get_if<type_argument_type_syntax>(&last)) {
            type.full_span = combine_spans(type.full_span, arena.span(argument->type));
        } else if(auto const* argument = std::get_if<type_argument_literal_syntax>(&last)) {
            type.full_span = combine_spans(type.full_span, std::get<type_argument_literal_syntax>(last).literal);
        } else {
            type.full_span = combine_spans(type.full_span, std::get<type_argument_name_syntax>(last).name);
        }
    }

    while(allow_associated_names and check(token_kind::colon_colon)) {
        consume();
        auto name = expect_identifier("associated type name");
        if(not name) {
            return std::nullopt;
        }
        type.associated_names.emplace_back(name->span);
        type.full_span = combine_spans(type.full_span, name->span);
    }

    return type;
}

auto parser::parse_array_type() -> std::optional<type_syntax>
{
    auto open = expect(token_kind::l_bracket);
    auto element = parse_expected_type();
    auto semicolon = expect(token_kind::semicolon);
    auto length = parse_type_length_argument();
    auto close = expect(token_kind::r_bracket);
    if(not open or not element or not semicolon or not length or not close) {
        return std::nullopt;
    }

    return type_syntax {
        .full_span = combine_spans(open->span, close->span),
        .name = open->span,
        .is_array_type = true,
        .array_element = *element,
        .array_length = *length,
    };
}

auto parser::parse_tuple_or_grouped_type() -> std::optional<type_syntax>
{
    auto open = expect(token_kind::l_paren);
    if(not open) {
        return std::nullopt;
    }
    if(check(token_kind::r_paren)) {
        report_current(diagnostic_kind::expected_type, "empty tuple type is not supported");
        return std::nullopt;
    }

    auto first = parse_expected_type();
    if(not first) {
        return std::nullopt;
    }
    if(not check(token_kind::comma)) {
        auto close = expect(token_kind::r_paren);
        if(not close) {
            return std::nullopt;
        }
        return type_syntax {
            .full_span = combine_spans(open->span, close->span),
            .name = open->span,
            .is_grouped_type = true,
            .grouped_type = *first,
        };
    }

    auto elements = std::vector<type_id>{ *first };
    while(check(token_kind::comma)) {
        consume();
        if(check(token_kind::r_paren)) {
            break;
        }
        auto element = parse_expected_type();
        if(not element) {
            return std::nullopt;
        }
        elements.emplace_back(*element);
    }

    auto close = expect(token_kind::r_paren);
    if(not close) {
        return std::nullopt;
    }
    return type_syntax {
        .full_span = combine_spans(open->span, close->span),
        .name = open->span,
        .is_tuple_type = true,
        .tuple_elements = std::move(elements),
    };
}

auto parser::parse_type_length_argument() -> std::optional<type_argument_syntax>
{
    if(check(token_kind::integer_literal)) {
        return type_argument_syntax {
            type_argument_literal_syntax {
                .literal = consume().span,
            }
        };
    }
    if(check(token_kind::identifier)) {
        return type_argument_syntax {
            type_argument_name_syntax {
                .name = consume().span,
            }
        };
    }

    report_current(
        diagnostic_kind::expected_type,
        std::format("expected array length, got {}", token_name(peek().kind))
    );
    return std::nullopt;
}

auto parser::finish_type_suffix(type_syntax type) -> std::optional<type_id>
{
    if(check_any({ token_kind::kw_const, token_kind::kw_like })) {
        auto qualifier = consume();
        type.is_const = qualifier.kind == token_kind::kw_const;
        type.is_like = qualifier.kind == token_kind::kw_like;
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
    } else if(check(token_kind::kw_move)) {
        auto suffix = consume();
        auto amp = expect(token_kind::amp);
        if(not amp) {
            return std::nullopt;
        }
        type.suffix_operators.emplace_back(suffix.kind);
        type.full_span = combine_spans(type.full_span, amp->span);
    }
    return arena.add(std::move(type));
}

auto parser::looks_like_function_type() const -> bool
{
    return (
        check_contextual("f")
        and (
            peek(1uz).kind == token_kind::l_paren
            or (peek(1uz).kind == token_kind::star and peek(2uz).kind == token_kind::l_paren)
        )
    );
}

auto parser::parse_function_type() -> std::optional<type_id>
{
    auto function_marker = expect_identifier("function type marker");
    auto is_pointer = false;
    if(check(token_kind::star)) {
        consume();
        is_pointer = true;
    }

    auto open = expect(token_kind::l_paren);
    if(not function_marker or not open) {
        return std::nullopt;
    }

    auto parameters = std::vector<function_type_parameter_syntax>{};
    if(not check(token_kind::r_paren)) {
        auto parameter = parse_function_type_parameter();
        if(not parameter) {
            return std::nullopt;
        }
        parameters.emplace_back(std::move(*parameter));

        while(check(token_kind::comma)) {
            consume();
            auto next = parse_function_type_parameter();
            if(not next) {
                return std::nullopt;
            }
            parameters.emplace_back(std::move(*next));
        }
    }

    auto close = expect(token_kind::r_paren);
    auto arrow = expect(token_kind::arrow);
    auto return_type = parse_expected_type();
    if(not close or not arrow or not return_type) {
        return std::nullopt;
    }

    return arena.add(type_syntax {
        .full_span = combine_spans(function_marker->span, arena.span(*return_type)),
        .name = function_marker->span,
        .is_function_type = true,
        .is_function_pointer = is_pointer,
        .function_parameters = std::move(parameters),
        .function_return = *return_type,
    });
}

auto parser::parse_function_type_parameter() -> std::optional<function_type_parameter_syntax>
{
    auto name = std::optional<source_span>{};
    auto start = peek().span;
    if(check(token_kind::identifier) and peek(1uz).kind == token_kind::colon) {
        name = consume().span;
        consume();
    }

    auto type = parse_expected_type();
    if(not type) {
        return std::nullopt;
    }

    return function_type_parameter_syntax {
        .full_span = combine_spans(name.value_or(start), arena.span(*type)),
        .name = name,
        .type = *type,
    };
}

auto parser::looks_like_decltype() const -> bool
{
    return check_contextual("decltype") and peek(1uz).kind == token_kind::l_paren;
}

auto parser::parse_decltype() -> std::optional<type_id>
{
    auto marker = expect_identifier("decltype");
    auto open = expect(token_kind::l_paren);
    auto expression = parse_expected_expression();
    auto close = expect(token_kind::r_paren);
    if(not marker or not open or not expression or not close) {
        return std::nullopt;
    }

    return arena.add(type_syntax {
        .full_span = combine_spans(marker->span, close->span),
        .name = marker->span,
        .is_decltype = true,
        .decltype_expression = *expression,
    });
}

auto parser::parse_type_argument_list() -> std::optional<std::vector<type_argument_syntax>>
{
    auto open = expect(token_kind::less);
    if(not open) {
        return std::nullopt;
    }

    if(check(token_kind::greater)) {
        report_current(
            diagnostic_kind::expected_type,
            "empty type argument list is not allowed"
        );
        return std::nullopt;
    }

    auto arguments = std::vector<type_argument_syntax>{};
    if(not expect_type_argument_start()) {
        return std::nullopt;
    }
    auto argument = parse_type_argument();
    if(not argument) {
        return std::nullopt;
    }
    arguments.emplace_back(*argument);

    while(check(token_kind::comma)) {
        consume();
        if(not expect_type_argument_start()) {
            return std::nullopt;
        }
        auto next = parse_type_argument();
        if(not next) {
            return std::nullopt;
        }
        arguments.emplace_back(*next);
    }

    auto close = expect_closing_angle();
    if(not close) {
        return std::nullopt;
    }
    return arguments;
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
