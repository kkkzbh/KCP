module semantic:expressions;

import semantic;

auto semantic_analyzer::check_expression(
    ast_arena const& ast,
    expr_id id,
    std::optional<semantic_type_id> expected
) -> expression_info
{
    auto const& expression = ast.node(id);
    auto info = (
        std::visit (
            overloaded {
                [&](name_expr_syntax const& node) {
                    return check_name_expression(node, id);
                },
                [&](literal_expr_syntax const& node) {
                    return check_literal_expression(node, id);
                },
                [&](unary_expr_syntax const& node) {
                    return check_unary_expression(ast, node);
                },
                [&](binary_expr_syntax const& node) {
                    return check_binary_expression(ast, node);
                },
                [&](assignment_expr_syntax const& node) {
                    return check_assignment_expression(ast, node);
                },
                [&](call_expr_syntax const& node) {
                    return check_call_expression(ast, node);
                },
                [&](cast_expr_syntax const& node) {
                    return check_cast_expression(ast, node);
                },
                [&](array_literal_expr_syntax const& node) {
                    return check_array_like_literal(ast, node.full_span, node.elements, expected, true);
                },
                [&](sequence_literal_expr_syntax const& node) {
                    return check_array_like_literal(ast, node.full_span, node.elements, expected, false);
                },
                [&](tuple_literal_expr_syntax const& node) {
                    return check_tuple_literal(ast, node, expected);
                },
                [&](grouped_expr_syntax const& node) {
                    return check_expression(ast, node.expression, expected);
                },
            },
            expression
        )
    );

    record_expression(
        id,
        active_unit_index,
        semantic_expression_info {
            .type = info.type,
            .read_type = read_type(info.type),
            .is_lvalue = info.is_lvalue,
            .is_const = info.is_const,
        }
    );
    if(expected and not can_implicitly_convert(info, *expected)) {
        report(
            diagnostic_kind::type_mismatch,
            ast.span(id),
            "expression type does not match contextual type"
        );
    }
    if (
        expected
        and can_implicitly_convert(info, *expected)
        and read_type(info.type) != read_type(*expected)
    ) {
        result.expression_conversions[semantic_node_key{active_unit_index, id}] = *expected;
    }
    return info;
}

auto semantic_analyzer::check_name_expression(name_expr_syntax const& node, expr_id id) -> expression_info
{
    auto name = std::string{ ast_source.identifier(node.name) };
    auto symbol = resolve(name);
    if(not symbol.valid()) {
        report(
            diagnostic_kind::unknown_name,
            node.full_span,
            std::format("unknown name '{}'", name)
        );
        return expression_info{ .type = semantic_type_ids::error };
    }

    result.expression_symbols[semantic_node_key{active_unit_index, id}] = symbol;
    auto const& value = result.symbols[symbol.value];
    return expression_info {
        .type = value.type,
        .is_lvalue = value.kind != symbol_kind::function,
        .is_const = value.kind != symbol_kind::function and value.is_const,
    };
}

auto semantic_analyzer::check_literal_expression(literal_expr_syntax const& node, expr_id id) -> expression_info
{
    auto text = ast_source.slice(node.full_span);
    auto key = semantic_node_key{active_unit_index, id};
    if(text == "true" or text == "false") {
        result.literal_values[key] = semantic_literal_value {
            .value = text == "true",
        };
    } else if(text.starts_with("'")) {
        result.literal_values[key] = semantic_literal_value {
            .value = parse_char_literal(text),
        };
    } else if(text.starts_with("\"")) {
        result.literal_values[key] = semantic_literal_value {
            .value = parse_string_literal(text),
        };
    } else if(text.contains('.') or text.contains('e') or text.contains('E')) {
        result.literal_values[key] = semantic_literal_value {
            .value = parse_float_literal(text),
        };
    } else {
        result.literal_values[key] = semantic_literal_value {
            .value = parse_integer_literal(text),
        };
    }
    return expression_info{ .type = literal_type(node.full_span) };
}

auto semantic_analyzer::check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info
{
    auto operand = check_expression(ast, node.operand, std::nullopt);
    auto operand_value = read_type(operand.type);
    using enum token_kind;
    switch(node.operator_kind) {
        case amp:
            if(not operand.is_lvalue) {
                report(
                    diagnostic_kind::invalid_operator,
                    node.full_span,
                    "address-of operand must be an lvalue"
                );
                return expression_info{ .type = semantic_type_ids::error };
            }
            return unary_type(node.operator_kind, operand);
        case star:
            if(auto pointee = pointer_pointee(operand_value)) {
                return *pointee;
            }
            report(
                diagnostic_kind::invalid_operator,
                node.full_span,
                "cannot dereference non-pointer value"
            );
            return expression_info{ .type = semantic_type_ids::error };
        case kw_not:
            if(read_type(operand.type) != semantic_type_ids::bool_) {
                report(
                    diagnostic_kind::invalid_operator,
                    node.full_span,
                    "'not' operand must be bool"
                );
            }
            return unary_type(node.operator_kind, operand);
        case plus:
        case minus:
        case tilde:
            if(not is_numeric_type(operand_value)) {
                report(
                    diagnostic_kind::invalid_operator,
                    node.full_span,
                    "unary operand must be numeric"
                );
            }
            return unary_type(node.operator_kind, operand);
        case plus_plus:
        case minus_minus:
            if(not operand.is_lvalue or operand.is_const) {
                report(
                    diagnostic_kind::invalid_assignment_target,
                    node.full_span,
                    "increment operand must be a non-const lvalue"
                );
            }
            if(not is_numeric_type(operand_value)) {
                report(
                    diagnostic_kind::invalid_operator,
                    node.full_span,
                    "increment operand must be numeric"
                );
            }
            return unary_type(node.operator_kind, operand);
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info
{
    auto left = check_expression(ast, node.left, std::nullopt);
    auto right = check_expression(ast, node.right, std::nullopt);
    return check_binary_operator(
        node.operator_kind,
        read_type(left.type),
        read_type(right.type),
        node.full_span
    );
}

auto semantic_analyzer::check_binary_operator(
    token_kind operator_kind,
    semantic_type_id left_type,
    semantic_type_id right_type,
    source_span span
) -> expression_info
{
    using enum token_kind;
    switch(operator_kind) {
        case kw_and:
        case kw_or:
            if(left_type != semantic_type_ids::bool_ or right_type != semantic_type_ids::bool_) {
                report(
                    diagnostic_kind::invalid_operator,
                    span,
                    "logical operands must be bool"
                );
            }
            return binary_type(operator_kind, left_type, right_type);
        case equal_equal:
        case bang_equal:
        case less:
        case less_equal:
        case greater:
        case greater_equal:
            if(not common_numeric_type(left_type, right_type) and left_type != right_type) {
                report(
                    diagnostic_kind::invalid_operator,
                    span,
                    "comparison operands are incompatible"
                );
            }
            return binary_type(operator_kind, left_type, right_type);
        case plus:
        case minus:
        case star:
        case slash:
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            report(
                diagnostic_kind::invalid_operator,
                span,
                "arithmetic operands must be numeric"
            );
            return expression_info{ .type = semantic_type_ids::error };
        case percent:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            report(
                diagnostic_kind::invalid_operator,
                span,
                "remainder operands must be integers"
            );
            return expression_info{ .type = semantic_type_ids::error };
        case pipe:
        case caret:
        case amp:
        case less_less:
        case greater_greater:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            report(
                diagnostic_kind::invalid_operator,
                span,
                "bitwise operands must be integers"
            );
            return expression_info{ .type = semantic_type_ids::error };
        default:
            report(
                diagnostic_kind::invalid_operator,
                span,
                "unsupported binary operator"
            );
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node) -> expression_info
{
    auto left = check_expression(ast, node.left, std::nullopt);
    auto left_type = read_type(left.type);
    if(not left.is_lvalue) {
        report(
            diagnostic_kind::invalid_assignment_target,
            node.full_span,
            "assignment target must be an lvalue"
        );
    }
    if(left.is_const) {
        report(diagnostic_kind::assign_to_const, node.full_span, "cannot assign to const binding");
    }

    if(node.operator_kind == token_kind::equal) {
        auto right = check_expression(ast, node.right, left_type);
        if(not can_implicitly_convert(right, left_type)) {
            report(
                diagnostic_kind::type_mismatch,
                node.full_span,
                "assignment value does not match target type"
            );
        }
        return expression_info{ .type = left_type };
    }

    auto right = check_expression(ast, node.right, std::nullopt);
    auto operation = compound_assignment_operator(node.operator_kind);
    if(not operation) {
        report(
            diagnostic_kind::invalid_operator,
            node.full_span,
            "unsupported assignment operator"
        );
        return expression_info{ .type = semantic_type_ids::error };
    }
    auto value = check_binary_operator(*operation, left_type, read_type(right.type), node.full_span);
    if(not can_implicitly_convert(value.type, left_type)) {
        report(
            diagnostic_kind::type_mismatch,
            node.full_span,
            "compound assignment result does not match target type"
        );
    }
    return expression_info{ .type = left_type };
}

auto semantic_analyzer::compound_assignment_operator(token_kind operator_kind) -> std::optional<token_kind>
{
    using enum token_kind;
    switch(operator_kind) {
        case plus_equal: return plus;
        case minus_equal: return minus;
        case star_equal: return star;
        case slash_equal: return slash;
        case percent_equal: return percent;
        case amp_equal: return amp;
        case pipe_equal: return pipe;
        case caret_equal: return caret;
        case less_less_equal: return less_less;
        case greater_greater_equal: return greater_greater;
        default: return std::nullopt;
    }
}

auto semantic_analyzer::check_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info
{
    if(auto cast_type = function_style_cast_type(ast, node)) {
        if(node.arguments.size() != 1uz) {
            report(
                diagnostic_kind::argument_count_mismatch,
                node.full_span,
                "function-style cast expects one argument"
            );
            return expression_info{ .type = *cast_type };
        }
        auto argument = check_expression(ast, node.arguments.front(), std::nullopt);
        if(not can_explicitly_convert(argument.type, *cast_type)) {
            report(diagnostic_kind::invalid_cast, node.full_span, "invalid function-style cast");
        }
        return expression_info{ .type = *cast_type };
    }

    auto callee = check_expression(ast, node.callee, std::nullopt);
    auto const* callable = std::get_if<function_type>(&result.types.get(callee.type));
    if(callable == nullptr) {
        report(diagnostic_kind::not_callable, node.full_span, "callee is not callable");
        for(auto argument : node.arguments) {
            check_expression(ast, argument, std::nullopt);
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    if(callable->parameters.size() != node.arguments.size()) {
        report(
            diagnostic_kind::argument_count_mismatch,
            node.full_span,
            "call argument count does not match function signature"
        );
    }

    auto count = std::min(callable->parameters.size(), node.arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        check_expression(ast, node.arguments[index], callable->parameters[index]);
    }
    for(auto index = count; index < node.arguments.size(); ++index) {
        check_expression(ast, node.arguments[index], std::nullopt);
    }
    return expression_info{ .type = callable->returns };
}

auto semantic_analyzer::check_cast_expression(ast_arena const& ast, cast_expr_syntax const& node) -> expression_info
{
    auto target = lower_type(ast, node.type);
    auto operand = check_expression(ast, node.operand, std::nullopt);
    if(not can_explicitly_convert(operand.type, target)) {
        report(diagnostic_kind::invalid_cast, node.full_span, "invalid explicit cast");
    }
    return expression_info{ .type = target };
}

auto semantic_analyzer::check_array_like_literal(
    ast_arena const& ast,
    source_span span,
    std::vector<expr_id> const& elements,
    std::optional<semantic_type_id> expected,
    bool is_array
) -> expression_info
{
    if(auto aggregate = aggregate_context_for(expected, is_array)) {
        if(aggregate->length != elements.size()) {
            report(
                diagnostic_kind::aggregate_length_mismatch,
                span,
                "aggregate literal length does not match contextual type"
            );
        }
        for(auto element : elements) {
            check_expression(ast, element, aggregate->element);
        }
        return expression_info{ .type = *expected };
    }

    if(expected) {
        report(
            diagnostic_kind::type_mismatch,
            span,
            is_array ? "array literal requires array context" : "sequence literal requires sequence context"
        );
    }

    if(elements.empty()) {
        report(
            diagnostic_kind::empty_aggregate_without_context,
            span,
            "empty aggregate literal requires a contextual type"
        );
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto element_types = std::vector<semantic_type_id>{};
    for(auto element : elements) {
        element_types.emplace_back(read_type(check_expression(ast, element, std::nullopt).type));
    }

    auto joined = join_same_class_numeric_or_equal(element_types, span);
    if(is_array) {
        return expression_info {
            .type = intern_type (array_type {
                .element = joined,
                .length = elements.size(),
            }),
        };
    }
    return expression_info {
        .type = intern_type (sequence_type {
            .element = joined,
            .length = elements.size(),
        }),
    };
}

auto semantic_analyzer::check_tuple_literal(
    ast_arena const& ast,
    tuple_literal_expr_syntax const& node,
    std::optional<semantic_type_id> expected
) -> expression_info
{
    if(expected) {
        if(auto const* tuple = std::get_if<tuple_type>(&result.types.get(*expected))) {
            if(tuple->elements.size() != node.elements.size()) {
                report(
                    diagnostic_kind::aggregate_length_mismatch,
                    node.full_span,
                    "tuple literal length does not match contextual type"
                );
            }
            auto count = std::min(tuple->elements.size(), node.elements.size());
            for(auto index = 0uz; index < count; ++index) {
                check_expression(ast, node.elements[index], tuple->elements[index]);
            }
            for(auto index = count; index < node.elements.size(); ++index) {
                check_expression(ast, node.elements[index], std::nullopt);
            }
            return expression_info{ .type = *expected };
        }
        report(diagnostic_kind::type_mismatch, node.full_span, "tuple literal requires tuple context");
    }

    auto element_types = std::vector<semantic_type_id>{};
    for(auto element : node.elements) {
        element_types.emplace_back(read_type(check_expression(ast, element, std::nullopt).type));
    }
    return expression_info {
        .type = intern_type(tuple_type{ std::move(element_types) }),
    };
}

auto semantic_analyzer::parse_integer_literal(std::string_view text) -> std::int64_t
{
    auto value = std::int64_t{};
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if(result.ec != std::errc{}) {
        return 0;
    }
    return value;
}

auto semantic_analyzer::parse_float_literal(std::string_view text) -> double
{
    auto owned = std::string{text};
    return std::stod(owned);
}

auto semantic_analyzer::parse_char_literal(std::string_view text) -> char
{
    if(text.size() < 3uz) {
        return '\0';
    }
    if(text[1] != '\\') {
        return text[1];
    }
    if(text.size() < 4uz) {
        return '\0';
    }
    switch(text[2]) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '0': return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        default: return text[2];
    }
}

auto semantic_analyzer::parse_string_literal(std::string_view text) -> std::string
{
    auto value = std::string{};
    if(text.size() < 2uz) {
        return value;
    }
    for(auto index = 1uz; index + 1uz < text.size(); ++index) {
        auto ch = text[index];
        if(ch != '\\' or index + 1uz >= text.size()) {
            value += ch;
            continue;
        }
        ++index;
        switch(text[index]) {
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case '0': value += '\0'; break;
            case '\\': value += '\\'; break;
            case '\'': value += '\''; break;
            case '"': value += '"'; break;
            default: value += text[index]; break;
        }
    }
    return value;
}
