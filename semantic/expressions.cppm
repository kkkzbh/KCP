module semantic:expressions;

import semantic;

auto semantic_analyzer::check_expression(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected) -> expression_info
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
                    return check_unary_expression(ast, node, id);
                },
                [&](binary_expr_syntax const& node) {
                    return check_binary_expression(ast, node, id);
                },
                [&](assignment_expr_syntax const& node) {
                    return check_assignment_expression(ast, node, id);
                },
                [&](call_expr_syntax const& node) {
                    return check_call_expression(ast, node, id);
                },
                [&](member_expr_syntax const& node) {
                    return check_member_expression(ast, node, id);
                },
                [&](index_expr_syntax const& node) {
                    return check_index_expression(ast, node, id);
                },
                [&](associated_name_expr_syntax const& node) {
                    return check_associated_name_expression(node, id);
                },
                [&](cast_expr_syntax const& node) {
                    return check_cast_expression(ast, node);
                },
                [&](array_literal_expr_syntax const& node) {
                    return check_array_literal(ast, node.full_span, node.elements, expected);
                },
                [&](tuple_literal_expr_syntax const& node) {
                    return check_tuple_literal(ast, node, expected);
                },
                [&](grouped_expr_syntax const& node) {
                    return check_expression(ast, node.expression, expected);
                },
                [&](struct_init_expr_syntax const& node) {
                    return check_struct_initializer(ast, node, id, expected);
                },
                [&](block_expr_syntax const& node) {
                    return check_block_expression(ast, node);
                },
                [&](match_expr_syntax const& node) {
                    return check_match_expression(ast, node);
                },
                [&](lambda_expr_syntax const& node) {
                    return check_lambda_expression(node, id, expected);
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
        and not is_error(*expected)
        and not is_inferred(*expected)
        and not is_unit(*expected)
        and read_type(info.type) != read_type(*expected)
    ) {
        result.expression_conversions[node_key(id)] = *expected;
    }
    return info;
}

auto semantic_analyzer::check_name_expression(name_expr_syntax const& node, expr_id id) -> expression_info
{
    auto name = std::string{ ast_source.identifier(node.name) };
    auto symbol = resolve(name);
    if(not symbol.valid()) {
        if(auto field = self_field(name)) {
            result.expression_fields[node_key(id)] = *field;
            auto const& item = result.structs[field->struct_index];
            auto const& value = item.fields[field->field_index];
            auto self = result.symbols[active_self.value];
            auto field_type = value.type;
            if(auto const* instance = std::get_if<struct_type>(&result.types.get(read_type(self.type)))) {
                field_type = substitute_type(value.type, instance->arguments);
            }
            return expression_info {
                .type = field_type,
                .is_lvalue = true,
                .is_const = self.is_const,
            };
        }
        report(
            diagnostic_kind::unknown_name,
            node.full_span,
            std::format("unknown name '{}'", name)
        );
        return expression_info{ .type = semantic_type_ids::error };
    }

    result.expression_symbols[node_key(id)] = symbol;
    record_lambda_capture(symbol, id);
    auto const& value = result.symbols[symbol.value];
    auto const* reference = std::get_if<reference_type>(&result.types.get(value.type));
    return expression_info {
        .type = value.type,
        .is_lvalue = value.kind != symbol_kind::function,
        .is_const = value.kind != symbol_kind::function
                    and (
                        value.is_const
                        or (
                            reference
                            and terminal_pointee_const(reference->pointee, reference->is_const)
                        )
                    ),
    };
}

auto semantic_analyzer::check_literal_expression(literal_expr_syntax const& node, expr_id id) -> expression_info
{
    auto text = ast_source.slice(node.full_span);
    auto key = node_key(id);
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

auto semantic_analyzer::operator_token(overload_operator_kind kind) const -> token_kind
{
    using enum overload_operator_kind;
    switch(kind) {
        case plus: return token_kind::plus;
        case minus: return token_kind::minus;
        case star: return token_kind::star;
        case slash: return token_kind::slash;
        case percent: return token_kind::percent;
        case amp: return token_kind::amp;
        case pipe: return token_kind::pipe;
        case caret: return token_kind::caret;
        case less_less: return token_kind::less_less;
        case greater_greater: return token_kind::greater_greater;
        case tilde: return token_kind::tilde;
        case equal_equal: return token_kind::equal_equal;
        case bang_equal: return token_kind::bang_equal;
        case less: return token_kind::less;
        case less_equal: return token_kind::less_equal;
        case greater: return token_kind::greater;
        case greater_equal: return token_kind::greater_equal;
        case equal: return token_kind::equal;
        case plus_equal: return token_kind::plus_equal;
        case minus_equal: return token_kind::minus_equal;
        case star_equal: return token_kind::star_equal;
        case slash_equal: return token_kind::slash_equal;
        case percent_equal: return token_kind::percent_equal;
        case amp_equal: return token_kind::amp_equal;
        case pipe_equal: return token_kind::pipe_equal;
        case caret_equal: return token_kind::caret_equal;
        case less_less_equal: return token_kind::less_less_equal;
        case greater_greater_equal: return token_kind::greater_greater_equal;
        case subscript: return token_kind::l_bracket;
    }
    std::unreachable();
}

auto semantic_analyzer::overload_kind(token_kind kind) const -> std::optional<overload_operator_kind>
{
    using enum token_kind;
    switch(kind) {
        case plus: return overload_operator_kind::plus;
        case minus: return overload_operator_kind::minus;
        case star: return overload_operator_kind::star;
        case slash: return overload_operator_kind::slash;
        case percent: return overload_operator_kind::percent;
        case amp: return overload_operator_kind::amp;
        case pipe: return overload_operator_kind::pipe;
        case caret: return overload_operator_kind::caret;
        case less_less: return overload_operator_kind::less_less;
        case greater_greater: return overload_operator_kind::greater_greater;
        case tilde: return overload_operator_kind::tilde;
        case equal_equal: return overload_operator_kind::equal_equal;
        case bang_equal: return overload_operator_kind::bang_equal;
        case less: return overload_operator_kind::less;
        case less_equal: return overload_operator_kind::less_equal;
        case greater: return overload_operator_kind::greater;
        case greater_equal: return overload_operator_kind::greater_equal;
        case equal: return overload_operator_kind::equal;
        case plus_equal: return overload_operator_kind::plus_equal;
        case minus_equal: return overload_operator_kind::minus_equal;
        case star_equal: return overload_operator_kind::star_equal;
        case slash_equal: return overload_operator_kind::slash_equal;
        case percent_equal: return overload_operator_kind::percent_equal;
        case amp_equal: return overload_operator_kind::amp_equal;
        case pipe_equal: return overload_operator_kind::pipe_equal;
        case caret_equal: return overload_operator_kind::caret_equal;
        case less_less_equal: return overload_operator_kind::less_less_equal;
        case greater_greater_equal: return overload_operator_kind::greater_greater_equal;
        default: return std::nullopt;
    }
}

auto semantic_analyzer::operator_expression_info(symbol_id symbol) -> expression_info
{
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol.value].type));
    if(callable == nullptr) {
        return expression_info{ .type = semantic_type_ids::error };
    }
    if(auto const* reference = std::get_if<reference_type>(&result.types.get(callable->returns))) {
        return expression_info {
            .type = callable->returns,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(reference->pointee, reference->is_const),
        };
    }
    return expression_info{ .type = callable->returns };
}

auto semantic_analyzer::operator_score(symbol_id symbol, std::vector<expression_info> const& arguments, std::optional<semantic_type_id> receiver_type, source_span span) -> std::optional<operator_match>
{
    auto actual_symbol = symbol;
    auto generic_score = 0;
    auto const& value = result.symbols[symbol.value];
    if(function_is_generic(value.unit_index, value.function)) {
        auto const* instance = instantiate_function_symbol(symbol, receiver_type, arguments, {}, span);
        if(instance == nullptr) {
            return std::nullopt;
        }
        actual_symbol = instance->symbol;
        generic_score = 1;
    }

    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[actual_symbol.value].type));
    if(callable == nullptr or callable->parameters.size() != arguments.size()) {
        return std::nullopt;
    }

    auto score = 0;
    for(auto index = 0uz; index < arguments.size(); ++index) {
        auto parameter = callable->parameters[index];
        if(auto const* reference = std::get_if<reference_type>(&result.types.get(parameter))) {
            if(arguments[index].is_const and not reference->is_const) {
                return std::nullopt;
            }
        }
        if(read_type(arguments[index].type) == read_type(parameter) and can_implicitly_convert(arguments[index], parameter)) {
            continue;
        }
        if(can_implicitly_convert(arguments[index], parameter)) {
            ++score;
            continue;
        }
        return std::nullopt;
    }
    return operator_match{ actual_symbol, score, generic_score };
}

auto semantic_analyzer::choose_operator(std::span<symbol_id const> candidates, std::vector<expression_info> const& arguments, std::optional<semantic_type_id> receiver_type, source_span span) -> std::optional<symbol_id>
{
    auto matches = std::vector<operator_match>{};
    for(auto symbol : candidates) {
        if(auto match = operator_score(symbol, arguments, receiver_type, span)) {
            matches.emplace_back(*match);
        }
    }
    if(matches.empty()) {
        report(diagnostic_kind::invalid_operator, span, "operator arguments do not match any candidate");
        return std::nullopt;
    }

    std::ranges::sort(matches, [](operator_match const& left, operator_match const& right) {
        if(left.score != right.score) {
            return left.score < right.score;
        }
        return left.generic_score < right.generic_score;
    });
    if (
        matches.size() > 1uz
        and matches[0].score == matches[1].score
        and matches[0].generic_score == matches[1].generic_score
    ) {
        report(diagnostic_kind::invalid_operator, span, "operator call is ambiguous");
        return matches.front().symbol;
    }
    return matches.front().symbol;
}

auto semantic_analyzer::resolve_operator(overload_operator_kind kind, std::span<semantic_type_id const> owner_types, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>
{
    auto visited = std::set<semantic_type_id>{};
    for(auto owner : owner_types) {
        auto type = read_type(owner);
        if(not visited.emplace(type).second) {
            continue;
        }

        std::vector<symbol_id> const* candidates{};
        auto receiver_type = std::optional<semantic_type_id>{};
        if(auto struct_index = struct_index_of(type)) {
            auto found = result.structs[*struct_index].operators.find(kind);
            if(found != result.structs[*struct_index].operators.end()) {
                candidates = &found->second;
                receiver_type = type;
            }
        } else if(auto variant_index = variant_index_of(type)) {
            auto found = result.variants[*variant_index].operators.find(kind);
            if(found != result.variants[*variant_index].operators.end()) {
                candidates = &found->second;
                receiver_type = type;
            }
        }
        if(candidates != nullptr) {
            return choose_operator(*candidates, arguments, receiver_type, span);
        }
    }

    auto found = active_unit->visible_operators.find(kind);
    if(found == active_unit->visible_operators.end()) {
        return std::nullopt;
    }
    return choose_operator(found->second, arguments, std::nullopt, span);
}

auto semantic_analyzer::check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info
{
    return check_unary_expression(ast, node, {});
}

auto semantic_analyzer::check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node, expr_id id) -> expression_info
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
            if(is_numeric_type(operand_value)) {
                return unary_type(node.operator_kind, operand);
            }
            if(id.valid()) {
                if(auto kind = overload_kind(node.operator_kind)) {
                    auto owners = std::array{ operand_value };
                    auto arguments = std::vector<expression_info>{ operand };
                    if(auto symbol = resolve_operator(*kind, owners, arguments, node.full_span)) {
                        result.expression_operators[node_key(id)] = *symbol;
                        return operator_expression_info(*symbol);
                    }
                }
            }
            report(diagnostic_kind::invalid_operator, node.full_span, "unary operand must be numeric");
            return expression_info{ .type = semantic_type_ids::error };
        case tilde:
            if(auto builtin = as_builtin(operand_value); builtin and is_integer(*builtin)) {
                return unary_type(node.operator_kind, operand);
            }
            if(id.valid()) {
                auto owners = std::array{ operand_value };
                auto arguments = std::vector<expression_info>{ operand };
                if(auto symbol = resolve_operator(overload_operator_kind::tilde, owners, arguments, node.full_span)) {
                    result.expression_operators[node_key(id)] = *symbol;
                    return operator_expression_info(*symbol);
                }
            }
            report(diagnostic_kind::invalid_operator, node.full_span, "bitwise not operand must be integer");
            return expression_info{ .type = semantic_type_ids::error };
        case plus_plus:
        case minus_minus:
            if(not operand.is_lvalue or operand.is_const) {
                report(
                    diagnostic_kind::invalid_assignment_target,
                    node.full_span,
                    "increment operand must be a non-const lvalue"
                );
            }
            if(auto builtin = as_builtin(operand_value); not builtin or not is_integer(*builtin)) {
                report(
                    diagnostic_kind::invalid_operator,
                    node.full_span,
                    "increment operand must be integer"
                );
            }
            return unary_type(node.operator_kind, operand);
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info
{
    return check_binary_expression(ast, node, {});
}

auto semantic_analyzer::check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node, expr_id id) -> expression_info
{
    auto left = check_expression(ast, node.left, std::nullopt);
    auto right = check_expression(ast, node.right, std::nullopt);
    auto left_type = read_type(left.type);
    auto right_type = read_type(right.type);
    if(auto builtin = try_builtin_binary_operator(node.operator_kind, left_type, right_type)) {
        return *builtin;
    }
    if(id.valid()) {
        if(auto kind = overload_kind(node.operator_kind)) {
            auto owners = std::array{ left_type, right_type };
            auto arguments = std::vector<expression_info>{ left, right };
            if(auto symbol = resolve_operator(*kind, owners, arguments, node.full_span)) {
                result.expression_operators[node_key(id)] = *symbol;
                return operator_expression_info(*symbol);
            }
        }
    }
    return check_binary_operator(node.operator_kind, left_type, right_type, node.full_span);
}

auto semantic_analyzer::try_builtin_binary_operator(token_kind operator_kind, semantic_type_id left_type, semantic_type_id right_type) -> std::optional<expression_info>
{
    using enum token_kind;
    switch(operator_kind) {
        case kw_and:
        case kw_or:
            if(left_type == semantic_type_ids::bool_ and right_type == semantic_type_ids::bool_) {
                return binary_type(operator_kind, left_type, right_type);
            }
            return std::nullopt;
        case equal_equal:
        case bang_equal:
        case less:
        case less_equal:
        case greater:
        case greater_equal:
            if(pointer_value_pointee(left_type) and pointer_value_pointee(left_type) == pointer_value_pointee(right_type)) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(common_numeric_type(left_type, right_type) or (as_builtin(left_type) and left_type == right_type)) {
                return binary_type(operator_kind, left_type, right_type);
            }
            return std::nullopt;
        case plus:
            if((pointer_value_pointee(left_type) and as_builtin(read_type(right_type)) and is_integer(*as_builtin(read_type(right_type))))
               or (pointer_value_pointee(right_type) and as_builtin(read_type(left_type)) and is_integer(*as_builtin(read_type(left_type))))) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return std::nullopt;
        case minus:
            if(pointer_value_pointee(left_type) and as_builtin(read_type(right_type)) and is_integer(*as_builtin(read_type(right_type)))) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(pointer_value_pointee(left_type) and pointer_value_pointee(left_type) == pointer_value_pointee(right_type)) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return std::nullopt;
        case star:
        case slash:
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return std::nullopt;
        case percent:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return std::nullopt;
        case pipe:
        case caret:
        case amp:
        case less_less:
        case greater_greater:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

auto semantic_analyzer::check_binary_operator(token_kind operator_kind, semantic_type_id left_type, semantic_type_id right_type, source_span span) -> expression_info
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
            if(pointer_value_pointee(left_type) and pointer_value_pointee(left_type) == pointer_value_pointee(right_type)) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(not common_numeric_type(left_type, right_type) and not (as_builtin(left_type) and left_type == right_type)) {
                report(
                    diagnostic_kind::invalid_operator,
                    span,
                    "comparison operands are incompatible"
                );
            }
            return binary_type(operator_kind, left_type, right_type);
        case plus:
            if((pointer_value_pointee(left_type) and as_builtin(read_type(right_type)) and is_integer(*as_builtin(read_type(right_type))))
               or (pointer_value_pointee(right_type) and as_builtin(read_type(left_type)) and is_integer(*as_builtin(read_type(left_type))))) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            report(
                diagnostic_kind::invalid_operator,
                span,
                "arithmetic operands must be numeric or pointer plus integer"
            );
            return expression_info{ .type = semantic_type_ids::error };
        case minus:
            if(pointer_value_pointee(left_type) and as_builtin(read_type(right_type)) and is_integer(*as_builtin(read_type(right_type)))) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(pointer_value_pointee(left_type) and pointer_value_pointee(left_type) == pointer_value_pointee(right_type)) {
                return binary_type(operator_kind, left_type, right_type);
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            report(
                diagnostic_kind::invalid_operator,
                span,
                "arithmetic operands must be numeric or valid pointer arithmetic"
            );
            return expression_info{ .type = semantic_type_ids::error };
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
    return check_assignment_expression(ast, node, {});
}

auto semantic_analyzer::check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node, expr_id id) -> expression_info
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

    if(id.valid()) {
        if(auto kind = overload_kind(node.operator_kind)) {
            auto right = check_expression(ast, node.right, std::nullopt);
            auto owners = std::array{ left_type };
            auto arguments = std::vector<expression_info>{ left, right };
            if(auto symbol = resolve_operator(*kind, owners, arguments, node.full_span)) {
                result.expression_operators[node_key(id)] = *symbol;
                return operator_expression_info(*symbol);
            }
        }
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

auto semantic_analyzer::check_builtin_call(ast_arena const& ast, call_expr_syntax const& node, name_expr_syntax const& callee, expr_id id) -> std::optional<expression_info>
{
    auto name = ast_source.identifier(callee.name);
    if(name != "alloc" and name != "free" and name != "construct_at" and name != "destroy_at") {
        return std::nullopt;
    }

    auto key = node_key(id);
    if(name == "alloc") {
        if(node.type_arguments.size() != 1uz) {
            report(diagnostic_kind::invalid_type_argument, node.full_span, "alloc expects one type argument");
        }
        auto element = semantic_type_ids::error;
        if(node.type_arguments.size() == 1uz) {
            auto const& argument = node.type_arguments.front();
            if(auto const* type_argument = std::get_if<type_argument_type_syntax>(&argument)) {
                element = lower_type(ast, type_argument->type);
            } else {
                report(diagnostic_kind::invalid_type_argument, node.full_span, "alloc type argument must be a type");
            }
        }
        if(node.arguments.size() != 1uz) {
            report(diagnostic_kind::argument_count_mismatch, node.full_span, "alloc expects one count argument");
        }
        if(not node.arguments.empty()) {
            auto count = check_expression(ast, node.arguments.front(), std::nullopt);
            auto builtin = as_builtin(read_type(count.type));
            if(not builtin or not is_integer(*builtin)) {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments.front()), "alloc count must be an integer");
            }
        }
        auto pointer = result.types.intern(pointer_type{ element });
        result.builtin_calls[key] = semantic_builtin_call {
            .kind = semantic_builtin_call_kind::alloc,
            .type = element,
        };
        return expression_info{ .type = pointer };
    }

    if(not node.type_arguments.empty()) {
        report(diagnostic_kind::invalid_type_argument, node.full_span, "memory builtin does not take explicit type arguments");
    }

    if(name == "free") {
        if(node.arguments.size() != 1uz) {
            report(diagnostic_kind::argument_count_mismatch, node.full_span, "free expects one pointer argument");
        }
        auto pointee = semantic_type_ids::error;
        if(not node.arguments.empty()) {
            auto pointer = check_expression(ast, node.arguments.front(), std::nullopt);
            if(auto value = pointer_value_pointee(pointer.type)) {
                pointee = *value;
            } else {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments.front()), "free argument must be a pointer");
            }
        }
        result.builtin_calls[key] = semantic_builtin_call {
            .kind = semantic_builtin_call_kind::free,
            .type = pointee,
        };
        return expression_info{ .type = semantic_type_ids::unit };
    }

    if(name == "construct_at") {
        if(node.arguments.size() != 2uz) {
            report(diagnostic_kind::argument_count_mismatch, node.full_span, "construct_at expects pointer and value arguments");
        }
        auto pointee = semantic_type_ids::error;
        if(not node.arguments.empty()) {
            auto pointer = check_expression(ast, node.arguments.front(), std::nullopt);
            if(auto value = pointer_value_pointee(pointer.type)) {
                pointee = *value;
            } else {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments.front()), "construct_at pointer must be a pointer");
            }
        }
        if(node.arguments.size() > 1uz) {
            auto value = check_expression(ast, node.arguments[1uz], pointee);
            if(not can_implicitly_convert(value, pointee)) {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments[1uz]), "construct_at value does not match pointer target");
            }
        }
        result.builtin_calls[key] = semantic_builtin_call {
            .kind = semantic_builtin_call_kind::construct_at,
            .type = pointee,
        };
        return expression_info{ .type = semantic_type_ids::unit };
    }

    if(node.arguments.size() != 1uz) {
        report(diagnostic_kind::argument_count_mismatch, node.full_span, "destroy_at expects one pointer argument");
    }
    auto pointee = semantic_type_ids::error;
    if(not node.arguments.empty()) {
        auto pointer = check_expression(ast, node.arguments.front(), std::nullopt);
        if(auto value = pointer_value_pointee(pointer.type)) {
            pointee = *value;
        } else {
            report(diagnostic_kind::type_mismatch, ast.span(node.arguments.front()), "destroy_at argument must be a pointer");
        }
    }
    result.builtin_calls[key] = semantic_builtin_call {
        .kind = semantic_builtin_call_kind::destroy_at,
        .type = pointee,
    };
    return expression_info{ .type = semantic_type_ids::unit };
}

auto semantic_analyzer::check_call_expression(ast_arena const& ast, call_expr_syntax const& node, expr_id id)
    -> expression_info
{
    auto const& callee_node = ast.node(node.callee);
    if(auto const* name = std::get_if<name_expr_syntax>(&callee_node)) {
        if(auto builtin = check_builtin_call(ast, node, *name, id)) {
            return *builtin;
        }
    }

    if(auto const* member = std::get_if<member_expr_syntax>(&callee_node)) {
        return check_member_call(ast, node, *member);
    }
    if(auto const* associated = std::get_if<associated_name_expr_syntax>(&callee_node)) {
        return check_associated_call(ast, node, *associated);
    }

    if(auto cast_type = function_style_cast_type(ast, node)) {
        if(not node.type_arguments.empty()) {
            report(
                diagnostic_kind::invalid_type_argument,
                node.full_span,
                "function-style cast does not take type arguments"
            );
        }
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

    if(auto generic = check_generic_function_call(ast, node, id)) {
        return *generic;
    }

    auto callee = check_expression(ast, node.callee, std::nullopt);
    auto closure = result.lambda_of_closure(read_type(callee.type));
    if(closure.valid()) {
        auto const& closure_symbol = result.symbols[closure.function_symbol.value];
        if(function_is_generic(closure_symbol.unit_index, closure.function)) {
            auto explicit_arguments = explicit_type_arguments(ast, node);
            if(not explicit_arguments) {
                for(auto argument : node.arguments) {
                    check_expression(ast, argument, std::nullopt);
                }
                return expression_info{ .type = semantic_type_ids::error };
            }

            auto arguments = std::vector<expression_info>{};
            arguments.reserve(node.arguments.size());
            for(auto argument : node.arguments) {
                arguments.emplace_back(check_expression(ast, argument, std::nullopt));
            }

            auto instance = instantiate_lambda(closure, arguments, std::move(*explicit_arguments), node.full_span);
            if(not instance.valid()) {
                return expression_info{ .type = semantic_type_ids::error };
            }

            auto const& callable = instance.callable;
            if(callable.parameters.size() != node.arguments.size()) {
                report(
                    diagnostic_kind::argument_count_mismatch,
                    node.full_span,
                    "generic closure call argument count does not match lambda signature"
                );
            }

            auto count = std::min(callable.parameters.size(), node.arguments.size());
            for(auto index = 0uz; index < count; ++index) {
                if(not can_implicitly_convert(arguments[index], callable.parameters[index])) {
                    report(
                        diagnostic_kind::type_mismatch,
                        ast.span(node.arguments[index]),
                        "closure call argument does not match generic lambda instance"
                    );
                    continue;
                }
                if(read_type(arguments[index].type) != read_type(callable.parameters[index])) {
                    result.expression_conversions[node_key(node.arguments[index])] = callable.parameters[index];
                }
            }

            result.lambda_call_infos[node_key(id)] = instance;
            return expression_info{ .type = callable.returns };
        }

        if(not node.type_arguments.empty()) {
            report(
                diagnostic_kind::invalid_type_argument,
                node.full_span,
                "non-generic closure call does not take type arguments"
            );
        }

        auto const& callable = closure.callable;
        if(callable.parameters.size() != node.arguments.size()) {
            report(
                diagnostic_kind::argument_count_mismatch,
                node.full_span,
                "closure call argument count does not match lambda signature"
            );
        }

        auto count = std::min(callable.parameters.size(), node.arguments.size());
        for(auto index = 0uz; index < count; ++index) {
            check_expression(ast, node.arguments[index], callable.parameters[index]);
        }
        for(auto index = count; index < node.arguments.size(); ++index) {
            check_expression(ast, node.arguments[index], std::nullopt);
        }
        return expression_info{ .type = callable.returns };
    }

    if(not node.type_arguments.empty()) {
        report(
            diagnostic_kind::invalid_type_argument,
            node.full_span,
            "non-generic call does not take type arguments"
        );
    }

    auto const* callable = callable_type(callee.type);
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

auto semantic_analyzer::check_member_call(ast_arena const& ast, call_expr_syntax const& node, member_expr_syntax const& callee) -> expression_info
{
    auto object = check_expression(ast, callee.object, std::nullopt);
    auto struct_index = struct_index_of(object.type);
    auto variant_index = variant_index_of(object.type);
    auto name = std::string{ ast_source.identifier(callee.name) };
    if(not struct_index and not variant_index) {
        report(diagnostic_kind::unknown_member, callee.full_span, "member call requires a struct or variant value");
        for(auto argument : node.arguments) {
            check_expression(ast, argument, std::nullopt);
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto found = struct_index
        ? result.structs[*struct_index].methods.find(name)
        : result.variants[*variant_index].methods.find(name);
    auto end = struct_index
        ? result.structs[*struct_index].methods.end()
        : result.variants[*variant_index].methods.end();
    if(found == end) {
        report(diagnostic_kind::unknown_member, callee.name, std::format("unknown member function '{}'", name));
        for(auto argument : node.arguments) {
            check_expression(ast, argument, std::nullopt);
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto symbol = found->second;
    auto explicit_arguments = std::vector<semantic_type_id>{};
    if(not node.type_arguments.empty()) {
        auto parsed = explicit_type_arguments(ast, node);
        if(not parsed) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        explicit_arguments = std::move(*parsed);
    }

    if(function_is_generic(result.symbols[symbol.value].unit_index, result.symbols[symbol.value].function)) {
        auto arguments = std::vector<expression_info>{ object };
        arguments.reserve(node.arguments.size() + 1uz);
        for(auto argument : node.arguments) {
            arguments.emplace_back(check_expression(ast, argument, std::nullopt));
        }
        auto const* instance = instantiate_function_symbol(
            symbol,
            read_type(object.type),
            arguments,
            std::move(explicit_arguments),
            node.full_span
        );
        if(instance == nullptr) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        symbol = instance->symbol;
        auto const& signature = result.signatures[instance->signature.value];
        auto expected_count = signature.parameters.empty() ? 0uz : signature.parameters.size() - 1uz;
        if(expected_count != node.arguments.size()) {
            report(diagnostic_kind::argument_count_mismatch, node.full_span, "method argument count does not match signature");
        }
        if(not signature.parameters.empty() and not can_implicitly_convert(object, signature.parameters.front())) {
            report(diagnostic_kind::type_mismatch, callee.full_span, "method receiver type mismatch");
        }
        auto count = std::min(expected_count, node.arguments.size());
        for(auto index = 0uz; index < count; ++index) {
            if(not can_implicitly_convert(arguments[index + 1uz], signature.parameters[index + 1uz])) {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments[index]), "method argument type mismatch");
            }
        }
        result.expression_symbols[node_key(node.callee)] = symbol;
        return expression_info{ .type = signature.returns };
    }

    if(not explicit_arguments.empty()) {
        report(diagnostic_kind::invalid_type_argument, node.full_span, "non-generic method call does not take type arguments");
    }

    result.expression_symbols[node_key(node.callee)] = symbol;
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol.value].type));
    if(callable == nullptr) {
        return expression_info{ .type = semantic_type_ids::error };
    }
    auto expected_count = callable->parameters.empty() ? 0uz : callable->parameters.size() - 1uz;
    if(expected_count != node.arguments.size()) {
        report(diagnostic_kind::argument_count_mismatch, node.full_span, "method argument count does not match signature");
    }
    if(not callable->parameters.empty() and not can_implicitly_convert(object, callable->parameters.front())) {
        report(diagnostic_kind::type_mismatch, callee.full_span, "method receiver type mismatch");
    }
    auto count = std::min(expected_count, node.arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        check_expression(ast, node.arguments[index], callable->parameters[index + 1uz]);
    }
    for(auto index = count; index < node.arguments.size(); ++index) {
        check_expression(ast, node.arguments[index], std::nullopt);
    }
    return expression_info{ .type = callable->returns };
}

auto semantic_analyzer::check_associated_call(ast_arena const& ast, call_expr_syntax const& node, associated_name_expr_syntax const& callee) -> expression_info
{
    auto type = lower_type(ast, callee.type);
    auto name = std::string{ ast_source.identifier(callee.name) };
    if(auto variant_index = variant_index_of(type)) {
        auto const& variant = result.variants[*variant_index];
        auto found = variant.case_indices.find(name);
        if(found != variant.case_indices.end()) {
            auto const& variant_case = variant.cases[found->second];
            auto payloads = variant_case_payload_types(type, variant_case);
            if(payloads.empty()) {
                report(diagnostic_kind::not_callable, node.full_span, "unit variant case is not callable");
                return expression_info{ .type = type };
            }
            if(payloads.size() != node.arguments.size()) {
                report(diagnostic_kind::argument_count_mismatch, node.full_span, "variant case argument count mismatch");
            }
            auto count = std::min(payloads.size(), node.arguments.size());
            for(auto index = 0uz; index < count; ++index) {
                check_expression(ast, node.arguments[index], payloads[index]);
            }
            for(auto index = count; index < node.arguments.size(); ++index) {
                check_expression(ast, node.arguments[index], std::nullopt);
            }
            result.expression_variant_cases[node_key(node.callee)] = semantic_variant_case_access {
                .variant_index = *variant_index,
                .case_index = found->second,
            };
            return expression_info{ .type = type };
        }
    }

    auto struct_index = struct_index_of(type);
    auto associated_variant_index = variant_index_of(type);
    if(not struct_index and not associated_variant_index) {
        report(diagnostic_kind::unknown_member, callee.full_span, "associated call requires a struct or variant type");
        return expression_info{ .type = semantic_type_ids::error };
    }
    auto found = struct_index
        ? result.structs[*struct_index].associated_functions.find(name)
        : result.variants[*associated_variant_index].associated_functions.find(name);
    auto end = struct_index
        ? result.structs[*struct_index].associated_functions.end()
        : result.variants[*associated_variant_index].associated_functions.end();
    if(found == end) {
        report(diagnostic_kind::unknown_member, callee.name, std::format("unknown associated function '{}'", name));
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto symbol = found->second;
    auto explicit_arguments = std::vector<semantic_type_id>{};
    if(not node.type_arguments.empty()) {
        auto parsed = explicit_type_arguments(ast, node);
        if(not parsed) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        explicit_arguments = std::move(*parsed);
    }

    if(function_is_generic(result.symbols[symbol.value].unit_index, result.symbols[symbol.value].function)) {
        auto arguments = std::vector<expression_info>{};
        arguments.reserve(node.arguments.size());
        for(auto argument : node.arguments) {
            arguments.emplace_back(check_expression(ast, argument, std::nullopt));
        }
        auto const* instance = instantiate_function_symbol(
            symbol,
            read_type(type),
            arguments,
            std::move(explicit_arguments),
            node.full_span
        );
        if(instance == nullptr) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        symbol = instance->symbol;
        auto const& signature = result.signatures[instance->signature.value];
        if(signature.parameters.size() != node.arguments.size()) {
            report(diagnostic_kind::argument_count_mismatch, node.full_span, "associated function argument count does not match signature");
        }
        auto count = std::min(signature.parameters.size(), node.arguments.size());
        for(auto index = 0uz; index < count; ++index) {
            if(not can_implicitly_convert(arguments[index], signature.parameters[index])) {
                report(diagnostic_kind::type_mismatch, ast.span(node.arguments[index]), "associated function argument type mismatch");
            }
        }
        result.expression_symbols[node_key(node.callee)] = symbol;
        return expression_info{ .type = signature.returns };
    }

    if(not explicit_arguments.empty()) {
        report(diagnostic_kind::invalid_type_argument, node.full_span, "non-generic associated call does not take type arguments");
    }

    result.expression_symbols[node_key(node.callee)] = symbol;
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol.value].type));
    if(callable == nullptr) {
        return expression_info{ .type = semantic_type_ids::error };
    }
    if(callable->parameters.size() != node.arguments.size()) {
        report(diagnostic_kind::argument_count_mismatch, node.full_span, "associated function argument count does not match signature");
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

auto semantic_analyzer::check_member_expression(ast_arena const& ast, member_expr_syntax const& node, expr_id id)
    -> expression_info
{
    auto object = check_expression(ast, node.object, std::nullopt);
    auto struct_index = struct_index_of(object.type);
    auto name = std::string{ ast_source.identifier(node.name) };
    if(not struct_index) {
        report(diagnostic_kind::unknown_member, node.full_span, "field access requires a struct value");
        return expression_info{ .type = semantic_type_ids::error };
    }
    auto field = field_index(*struct_index, name);
    if(not field) {
        report(diagnostic_kind::unknown_field, node.name, std::format("unknown field '{}'", name));
        return expression_info{ .type = semantic_type_ids::error };
    }
    result.expression_fields[node_key(id)] = semantic_field_access {
        .struct_index = *struct_index,
        .field_index = *field,
    };
    auto const* instance = std::get_if<struct_type>(&result.types.get(read_type(object.type)));
    auto const& value = result.structs[*struct_index].fields[*field];
    return expression_info {
        .type = instance == nullptr ? value.type : substitute_type(value.type, instance->arguments),
        .is_lvalue = object.is_lvalue,
        .is_const = object.is_const,
    };
}

auto semantic_analyzer::check_index_expression(ast_arena const& ast, index_expr_syntax const& node) -> expression_info
{
    return check_index_expression(ast, node, {});
}

auto semantic_analyzer::check_index_expression(ast_arena const& ast, index_expr_syntax const& node, expr_id id) -> expression_info
{
    auto object = check_expression(ast, node.object, std::nullopt);
    auto index = check_expression(ast, node.index, std::nullopt);
    auto object_type = read_type(object.type);
    if(as_builtin(object_type) == builtin_type_kind::str) {
        auto index_builtin = as_builtin(read_type(index.type));
        if(not index_builtin or not is_integer(*index_builtin)) {
            report(
                diagnostic_kind::invalid_operator,
                ast.span(node.index),
                "string index must be an integer"
            );
        }
        return expression_info {
            .type = semantic_type_ids::char_,
            .is_lvalue = false,
            .is_const = true,
        };
    }
    if(auto const* tuple = std::get_if<tuple_type>(&result.types.get(object_type))) {
        auto value = constant_integer_index(ast, node.index);
        if(not value) {
            report(
                diagnostic_kind::invalid_operator,
                ast.span(node.index),
                "tuple index must be a compile-time integer"
            );
            return expression_info{ .type = semantic_type_ids::error };
        }
        if(*value < 0 or static_cast<std::uint64_t>(*value) >= tuple->elements.size()) {
            report(
                diagnostic_kind::invalid_operator,
                ast.span(node.index),
                "constant tuple index is out of bounds"
            );
            return expression_info{ .type = semantic_type_ids::error };
        }
        return expression_info {
            .type = tuple->elements[static_cast<std::size_t>(*value)],
            .is_lvalue = object.is_lvalue,
            .is_const = object.is_const,
        };
    }

    if(auto pointee = pointer_value_pointee(object_type)) {
        auto index_builtin = as_builtin(read_type(index.type));
        if(not index_builtin or not is_integer(*index_builtin)) {
            report(
                diagnostic_kind::invalid_operator,
                ast.span(node.index),
                "pointer index must be an integer"
            );
        }
        return expression_info {
            .type = *pointee,
            .is_lvalue = true,
            .is_const = target_const(object.type, object.is_const),
        };
    }

    auto const* array = std::get_if<array_type>(&result.types.get(object_type));
    if(array == nullptr) {
        if(id.valid()) {
            auto owners = std::array{ object_type };
            auto arguments = std::vector<expression_info>{ object, index };
            if(auto symbol = resolve_operator(overload_operator_kind::subscript, owners, arguments, node.full_span)) {
                result.expression_operators[node_key(id)] = *symbol;
                return operator_expression_info(*symbol);
            }
        }
        report(diagnostic_kind::invalid_operator, node.full_span, "index operator requires an array, tuple, string, or pointer value");
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto index_builtin = as_builtin(read_type(index.type));
    if(not index_builtin or not is_integer(*index_builtin)) {
        report(
            diagnostic_kind::invalid_operator,
            ast.span(node.index),
            "array index must be an integer"
        );
    }
    if(auto value = constant_integer_index(ast, node.index)) {
        if(*value < 0 or static_cast<std::uint64_t>(*value) >= array->length) {
            report(
                diagnostic_kind::invalid_operator,
                ast.span(node.index),
                "constant array index is out of bounds"
            );
        }
    }

    return expression_info {
        .type = array->element,
        .is_lvalue = object.is_lvalue,
        .is_const = target_const(object.type, object.is_const),
    };
}

auto semantic_analyzer::check_associated_name_expression(associated_name_expr_syntax const& node, expr_id id)
    -> expression_info
{
    auto type = lower_type(*active_ast, node.type);
    auto name = std::string{ ast_source.identifier(node.name) };
    if(auto variant_index = variant_index_of(type)) {
        auto const& variant = result.variants[*variant_index];
        auto found = variant.case_indices.find(name);
        if(found == variant.case_indices.end()) {
            report(diagnostic_kind::unknown_variant_case, node.name, std::format("unknown variant case '{}'", name));
            return expression_info{ .type = semantic_type_ids::error };
        }

        auto const& variant_case = variant.cases[found->second];
        if(not variant_case_payload_types(type, variant_case).empty()) {
            report(diagnostic_kind::not_callable, node.full_span, "variant case with payload must be called");
            return expression_info{ .type = semantic_type_ids::error };
        }
        result.expression_variant_cases[node_key(id)] = semantic_variant_case_access {
            .variant_index = *variant_index,
            .case_index = found->second,
        };
        return expression_info{ .type = type };
    }

    report(diagnostic_kind::not_callable, node.full_span, "associated name must be called");
    return expression_info{ .type = semantic_type_ids::error };
}

auto semantic_analyzer::constant_integer_index(ast_arena const& ast, expr_id id) -> std::optional<std::int64_t>
{
    auto const& expression = ast.node(id);
    if(auto const* literal = std::get_if<literal_expr_syntax>(&expression)) {
        auto text = ast_source.slice(literal->full_span);
        if(text.contains('.') or text.contains('e') or text.contains('E')) {
            return std::nullopt;
        }
        return parse_integer_literal(text);
    }
    if(auto const* unary = std::get_if<unary_expr_syntax>(&expression); unary and unary->operator_kind == token_kind::minus) {
        auto value = constant_integer_index(ast, unary->operand);
        if(value) {
            return -*value;
        }
    }
    return std::nullopt;
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

auto semantic_analyzer::check_array_literal(ast_arena const& ast, source_span span, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info
{
    if(auto aggregate = aggregate_context_for(expected)) {
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
            "array literal requires array context"
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
    return expression_info {
        .type = intern_type (array_type {
            .element = joined,
            .length = elements.size(),
        }),
    };
}

auto semantic_analyzer::check_tuple_literal(ast_arena const& ast, tuple_literal_expr_syntax const& node, std::optional<semantic_type_id> expected) -> expression_info
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

auto semantic_analyzer::choose_constructor(std::uint32_t struct_index, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>
{
    auto matches = std::vector<constructor_match>{};
    for(auto symbol : result.structs[struct_index].constructors) {
        auto const& function = result.symbols[symbol.value];
        auto const* callable = std::get_if<function_type>(&result.types.get(function.type));
        if(callable == nullptr) {
            continue;
        }
        auto score = constructor_score(
            function_signature {
                .parameters = callable->parameters,
                .returns = callable->returns,
            },
            arguments
        );
        if(score) {
            matches.emplace_back(symbol, *score);
        }
    }
    if(matches.empty()) {
        return std::nullopt;
    }

    std::ranges::sort(matches, {}, &constructor_match::score);
    if(matches.size() > 1uz and matches[0].score == matches[1].score) {
        report(diagnostic_kind::ambiguous_constructor, span, "constructor call is ambiguous");
        return matches.front().symbol;
    }
    return matches.front().symbol;
}

auto semantic_analyzer::constructor_score(function_signature const& signature, std::vector<expression_info> const& arguments) -> std::optional<int>
{
    if(signature.parameters.size() != arguments.size()) {
        return std::nullopt;
    }
    auto score = 0;
    for(auto index = 0uz; index < arguments.size(); ++index) {
        if(read_type(arguments[index].type) == read_type(signature.parameters[index])) {
            continue;
        }
        if(can_implicitly_convert(arguments[index], signature.parameters[index])) {
            ++score;
            continue;
        }
        return std::nullopt;
    }
    return score;
}

auto semantic_analyzer::check_struct_initializer(ast_arena const& ast, struct_init_expr_syntax const& node, expr_id id, std::optional<semantic_type_id> expected) -> expression_info
{
    auto type = lower_type(ast, node.type);
    if(expected and read_type(*expected) != read_type(type)) {
        report(diagnostic_kind::type_mismatch, node.full_span, "initializer type does not match contextual type");
    }
    auto struct_index = struct_index_of(type);

    if(not struct_index) {
        if(not node.initializers.empty()) {
            report(diagnostic_kind::type_mismatch, node.full_span, "only empty Type{} is allowed for non-struct default initialization");
        }
        if(not is_default_initializable(type)) {
            report(diagnostic_kind::default_initialization_failure, node.full_span, "type is not default initializable");
        }
        return expression_info{ .type = type };
    }

    auto const& item = result.structs[*struct_index];
    auto const* instance = std::get_if<struct_type>(&result.types.get(read_type(type)));
    auto field_types = (
        item.fields
        | std::views::transform([&](semantic_struct_field const& field) {
            return instance == nullptr ? field.type : substitute_type(field.type, instance->arguments);
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
    auto positional = std::vector<expr_id>{};
    auto named = std::vector<named_field_initializer_syntax>{};
    for(auto const& initializer : node.initializers) {
        if(auto const* value = std::get_if<positional_initializer_syntax>(&initializer)) {
            positional.emplace_back(value->value);
        } else {
            named.emplace_back(std::get<named_field_initializer_syntax>(initializer));
        }
    }

    if(named.empty()) {
        auto arguments = std::vector<expression_info>{};
        for(auto argument : positional) {
            arguments.emplace_back(check_expression(ast, argument, std::nullopt));
        }
        if(auto constructor = choose_constructor(*struct_index, arguments, node.full_span)) {
            result.expression_symbols[node_key(id)] = *constructor;
            auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[constructor->value].type));
            for(auto index = 0uz; callable and index < positional.size(); ++index) {
                if(can_implicitly_convert(arguments[index], callable->parameters[index])
                   and read_type(arguments[index].type) != read_type(callable->parameters[index])) {
                    result.expression_conversions[node_key(positional[index])] = callable->parameters[index];
                }
            }
            return expression_info{ .type = type };
        }

        if(positional.size() > item.fields.size()) {
            report(diagnostic_kind::aggregate_length_mismatch, node.full_span, "too many struct initializers");
        }
        auto count = std::min(positional.size(), item.fields.size());
        for(auto index = 0uz; index < count; ++index) {
            check_expression(ast, positional[index], field_types[index]);
        }
        for(auto index = count; index < item.fields.size(); ++index) {
            if(not is_default_initializable(field_types[index])) {
                report(
                    diagnostic_kind::default_initialization_failure,
                    item.fields[index].span,
                    std::format("field '{}' is not default initializable", item.fields[index].name)
                );
            }
        }
        return expression_info{ .type = type };
    }

    auto seen = std::set<std::string>{};
    for(auto const& initializer : named) {
        auto name = std::string{ ast_source.identifier(initializer.name) };
        if(seen.contains(name)) {
            report(diagnostic_kind::duplicate_field_initializer, initializer.name, std::format("duplicate field '{}'", name));
            continue;
        }
        seen.emplace(name);
        auto field = field_index(*struct_index, name);
        if(not field) {
            report(diagnostic_kind::unknown_field, initializer.name, std::format("unknown field '{}'", name));
            check_expression(ast, initializer.value, std::nullopt);
            continue;
        }
        check_expression(ast, initializer.value, field_types[*field]);
    }
    for(auto index = 0uz; index < item.fields.size(); ++index) {
        auto const& field = item.fields[index];
        if(not seen.contains(field.name) and not is_default_initializable(field_types[index])) {
            report(
                diagnostic_kind::default_initialization_failure,
                field.span,
                std::format("field '{}' is not default initializable", field.name)
            );
        }
    }
    return expression_info{ .type = type };
}

auto semantic_analyzer::check_block_expression(ast_arena const& ast, block_expr_syntax const& node) -> expression_info
{
    push_scope();
    auto returns = return_state{};
    auto signature_id = result.signature_of(active_unit_index, active_function);
    if(signature_id.valid()) {
        returns.declared_return = result.signatures[signature_id.value].returns;
    }
    for(auto statement : node.statements) {
        check_statement(statement, returns);
    }
    auto type = semantic_type_ids::unit;
    if(node.tail) {
        type = check_expression(ast, *node.tail, std::nullopt).type;
    }
    pop_scope();
    return expression_info{ .type = type };
}

auto semantic_analyzer::check_match_expression(ast_arena const& ast, match_expr_syntax const& node) -> expression_info
{
    auto value = check_expression(ast, node.value, std::nullopt);
    auto variant_index = variant_index_of(value.type);
    if(not variant_index) {
        report(diagnostic_kind::type_mismatch, ast.span(node.value), "match expression requires a variant value");
        for(auto const& arm : node.arms) {
            check_expression(ast, arm.value, std::nullopt);
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto const& variant = result.variants[*variant_index];
    auto seen = std::set<std::uint32_t>{};
    auto wildcard = false;
    auto result_type = std::optional<semantic_type_id>{};

    for(auto const& arm : node.arms) {
        push_scope();
        std::visit (
            overloaded {
                [&](match_case_pattern_syntax const& pattern) {
                    auto name = std::string{ ast_source.identifier(pattern.name) };
                    auto found = variant.case_indices.find(name);
                    if(found == variant.case_indices.end()) {
                        report(
                            diagnostic_kind::unknown_variant_case,
                            pattern.name,
                            std::format("unknown variant case '{}'", name)
                        );
                        return;
                    }
                    if(seen.contains(found->second)) {
                        report(
                            diagnostic_kind::duplicate_variant_case,
                            pattern.name,
                            std::format("duplicate match case '{}'", name)
                        );
                    }
                    seen.emplace(found->second);
                    auto payloads = variant_case_payload_types(value.type, variant.cases[found->second]);
                    if(payloads.size() != pattern.bindings.size()) {
                        report(
                            diagnostic_kind::argument_count_mismatch,
                            pattern.full_span,
                            "match pattern binding count does not match variant case payload"
                        );
                    }
                    auto count = std::min(payloads.size(), pattern.bindings.size());
                    for(auto index = 0uz; index < count; ++index) {
                        auto binding = pattern.bindings[index];
                        auto symbol = bind_symbol(semantic_symbol {
                            .kind = symbol_kind::local,
                            .name = std::string{ ast_source.identifier(binding) },
                            .span = binding,
                            .type = payloads[index],
                            .unit_index = active_unit_index,
                            .function = active_function,
                        });
                        if(symbol.valid()) {
                            result.pattern_bindings[parameter_key(binding)] = symbol;
                        }
                    }
                },
                [&](match_wildcard_pattern_syntax const&) {
                    wildcard = true;
                },
            },
            arm.pattern
        );

        auto branch = check_expression(ast, arm.value, result_type);
        if(not result_type) {
            result_type = branch.type;
        } else if(not can_implicitly_convert(branch, *result_type)) {
            report(diagnostic_kind::type_mismatch, arm.full_span, "match arm type does not match previous arms");
        }
        pop_scope();
    }

    if(not wildcard and seen.size() != variant.cases.size()) {
        report(diagnostic_kind::non_exhaustive_match, node.full_span, "match does not cover all variant cases");
    }

    return expression_info{ .type = result_type.value_or(semantic_type_ids::unit) };
}

auto semantic_analyzer::check_lambda_expression(lambda_expr_syntax const& node, expr_id id, std::optional<semantic_type_id> expected) -> expression_info
{
    auto symbol = result.function_symbol_of(active_unit_index, node.function);
    if(not symbol.valid()) {
        report(diagnostic_kind::unknown_name, node.full_span, "lambda is missing semantic function symbol");
        return expression_info{ .type = semantic_type_ids::error };
    }

    apply_lambda_parameter_context(node, expected);
    result.expression_symbols[node_key(id)] = symbol;
    if(function_is_generic(active_unit_index, node.function)) {
        auto info = result.lambda_of(active_context_index, active_unit_index, node.function);
        if(not info.valid()) {
            info = build_generic_lambda_info(node);
        }
        if(info.closure_type.valid()) {
            return expression_info{ .type = info.closure_type };
        }
        return expression_info{ .type = result.symbols[symbol.value].type };
    }

    auto info = result.lambda_of(active_unit_index, node.function);
    if(not info.valid()) {
        info = check_lambda_body(node);
    }
    if(info.closure_type.valid()) {
        return expression_info{ .type = info.closure_type };
    }
    return expression_info{ .type = result.symbols[symbol.value].type };
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
