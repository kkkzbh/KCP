module semantic:return_infer;

import semantic;

auto semantic_analyzer::infer_return_types() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            auto const& function = unit.ast.node(function_id);
            if(function.kind == function_syntax_kind::lambda or function_is_generic(unit_index, function_id)) {
                continue;
            }
            if(not function_has_source_body(result.function_symbol_of(unit_index, function_id))) {
                continue;
            }
            auto signature_id = result.signature_of(unit_index, function_id);
            if(not signature_id.valid()) {
                continue;
            }
            auto const& signature = result.signatures[signature_id.value];
            if(signature.inferred_return) {
                ensure_function_return_inferred(unit_index, function_id);
            }
        }
        for(auto impl_id : syntax.impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                if(not function_has_source_body(result.function_symbol_of(unit_index, function_id)) or function_is_generic(unit_index, function_id)) {
                    continue;
                }
                auto signature_id = result.signature_of(unit_index, function_id);
                if(not signature_id.valid()) {
                    continue;
                }
                auto const& signature = result.signatures[signature_id.value];
                if(signature.inferred_return) {
                    ensure_function_return_inferred(unit_index, function_id);
                }
            }
        }
        for(auto impl_id : syntax.concept_impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                if(not function_has_source_body(result.function_symbol_of(unit_index, function_id)) or function_is_generic(unit_index, function_id)) {
                    continue;
                }
                auto signature_id = result.signature_of(unit_index, function_id);
                if(not signature_id.valid()) {
                    continue;
                }
                auto const& signature = result.signatures[signature_id.value];
                if(signature.inferred_return) {
                    ensure_function_return_inferred(unit_index, function_id);
                }
            }
        }
    }
}

auto semantic_analyzer::infer_function_return(std::size_t unit_index, function_id id) -> semantic_type_id
{
    auto key = return_inference_key{ unit_index, id };
    auto& state = return_states[key];
    if(state == return_inference_state::resolved) {
        auto signature_id = result.signature_of(unit_index, id);
        return result.signatures[signature_id.value].returns;
    }
    if(state == return_inference_state::visiting) {
        return semantic_type_ids::inferred;
    }
    if(state == return_inference_state::failed) {
        return semantic_type_ids::error;
    }

    ensure_function_return_inferred(unit_index, id);
    if(state == return_inference_state::resolved) {
        auto signature_id = result.signature_of(unit_index, id);
        return result.signatures[signature_id.value].returns;
    }
    if(state == return_inference_state::failed) {
        return semantic_type_ids::error;
    }

    std::unreachable();
}

auto semantic_analyzer::ensure_function_return_inferred(std::size_t unit_index, function_id id) -> void
{
    auto key = return_inference_key{ unit_index, id };
    auto& state = return_states[key];
    if(state != return_inference_state::pending) {
        return;
    }

    state = return_inference_state::visiting;
    auto inferred = infer_function_body_return(unit_index, id);
    if(is_inferred(inferred)) {
        report_cannot_infer_return_type(unit_index, id);
        inferred = semantic_type_ids::error;
        state = return_inference_state::failed;
    } else if(is_error(inferred)) {
        state = return_inference_state::failed;
    } else {
        state = return_inference_state::resolved;
    }

    update_inferred_function_return(0uz, unit_index, id, inferred);
}

auto semantic_analyzer::infer_function_body_return(std::size_t unit_index, function_id id) -> semantic_type_id
{
    auto const& unit = units[unit_index];
    auto const& ast = unit.ast;
    auto const& function = ast.node(id);
    active_unit_index = unit_index;
    if(not function_has_source_body(result.function_symbol_of(unit_index, id))) {
        return semantic_type_ids::unit;
    }

    auto old_unit = return_unit;
    auto old_return_scopes = std::move(return_scopes);
    auto old_type_scopes = std::move(type_scopes);
    return_unit = unit_index;
    type_scopes.clear();

    auto observed = std::vector<semantic_type_id>{};
    return_scopes.emplace_back();
    type_scopes.emplace_back();
    auto const& signature = result.signatures[result.signature_of(unit_index, id).value];
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        return_scopes.back().emplace(
            std::string{ ast_source.slice(parameter.name) },
            return_inference_binding {
                .type = signature.parameters[index],
                .is_const = parameter.is_const,
            }
        );
    }

    infer_statement_returns(ast, function.body, observed);
    type_scopes = std::move(old_type_scopes);
    return_scopes = std::move(old_return_scopes);
    return_unit = old_unit;
    return join_return_types(observed);
}

auto semantic_analyzer::infer_statement_returns(ast_arena const& ast, stmt_id id, std::vector<semantic_type_id>& observed) -> void
{
    auto const& statement = ast.node(id);
    std::visit (
        overloaded {
            [&](block_statement_syntax const& node) {
                return_scopes.emplace_back();
                type_scopes.emplace_back();
                for(auto child : node.statements) {
                    infer_statement_returns(ast, child, observed);
                }
                type_scopes.pop_back();
                return_scopes.pop_back();
            },
            [&](declaration_statement_syntax const& node) {
                auto declared = std::optional<semantic_type_id>{};
                if(node.declared_type) {
                    declared = lower_type(ast, *node.declared_type);
                }
                auto initializer = infer_expression_type(ast, node.initializer, declared);
                if(not node.binding_names.empty()) {
                    auto initializer_type = declared.value_or(read_type(initializer.type));
                    auto const* tuple = std::get_if<tuple_type>(&result.types.get(read_type(initializer_type)));
                    if(tuple == nullptr) {
                        return;
                    }
                    auto elements = tuple->elements;
                    auto count = std::min(elements.size(), node.binding_names.size());
                    for(auto index = 0uz; index < count; ++index) {
                        auto element = elements[index];
                        auto element_value = read_type(element);
                        auto element_const = target_const(element, target_const(initializer.type, initializer.is_const));
                        auto binding_type = node.is_ref
                            ? result.types.intern(reference_type {
                                element_value,
                                node.is_const or element_const,
                            })
                            : element_value;
                        return_scopes.back().emplace (
                            std::string{ ast_source.slice(node.binding_names[index]) },
                            return_inference_binding {
                                .type = binding_type,
                                .is_const = node.is_const,
                            }
                        );
                    }
                    return;
                }
                auto binding_type = declared.value_or(read_type(initializer.type));
                if(node.is_ref) {
                    binding_type = result.types.intern(reference_type {
                        read_type(binding_type),
                        node.is_const or target_const(initializer.type, initializer.is_const),
                    });
                }
                return_scopes.back().emplace (
                    std::string{ ast_source.slice(node.name) },
                    return_inference_binding {
                        .type = binding_type,
                        .is_const = node.is_const,
                    }
                );
            },
            [&](type_alias_statement_syntax const& node) {
                bind_type_alias(node.name, lower_type(ast, node.type));
            },
            [&](if_statement_syntax const& node) {
                infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
                infer_statement_returns(ast, node.then_branch, observed);
                if(node.else_branch) {
                    infer_statement_returns(ast, *node.else_branch, observed);
                }
            },
            [&](while_statement_syntax const& node) {
                infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
                infer_statement_returns(ast, node.body, observed);
            },
            [&](do_while_statement_syntax const& node) {
                infer_statement_returns(ast, node.body, observed);
                infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
            },
            [&](for_statement_syntax const& node) {
                auto range = infer_expression_type(ast, node.range, std::nullopt);
                auto element = range_element_type(range.type);
                if(not element.valid()) {
                    element = semantic_type_ids::error;
                }
                auto binding_type = read_type(element);
                if(node.is_ref) {
                    auto const* reference = std::get_if<reference_type>(&result.types.get(element));
                    binding_type = reference == nullptr
                        ? semantic_type_ids::error
                        : result.types.intern(reference_type {
                            read_type(reference->pointee),
                            node.is_const or reference->is_const,
                        });
                }
                return_scopes.emplace_back();
                type_scopes.emplace_back();
                return_scopes.back().emplace (
                    std::string{ ast_source.slice(node.name) },
                    return_inference_binding {
                        .type = binding_type,
                        .is_const = node.is_const,
                    }
                );
                infer_statement_returns(ast, node.body, observed);
                type_scopes.pop_back();
                return_scopes.pop_back();
            },
            [&](template_for_statement_syntax const& node) {
                infer_statement_returns(ast, node.body, observed);
            },
            [&](template_if_statement_syntax const& node) {
                auto fallback = false;
                for(auto const& branch : node.branches) {
                    auto value = infer_template_if_condition(ast, node, branch.condition);
                    if(value == template_if_condition_value::true_) {
                        infer_statement_returns(ast, branch.body, observed);
                        return;
                    }
                    if(value == template_if_condition_value::false_) {
                        continue;
                    }
                    fallback = true;
                    break;
                }
                if(not fallback and node.else_branch) {
                    infer_statement_returns(ast, *node.else_branch, observed);
                    return;
                }
                if(not fallback) {
                    return;
                }

                for(auto const& condition : node.conditions) {
                    if(condition.expression) {
                        infer_expression_type(ast, *condition.expression, semantic_type_ids::bool_);
                    }
                }
                for(auto const& branch : node.branches) {
                    infer_statement_returns(ast, branch.body, observed);
                }
                if(node.else_branch) {
                    infer_statement_returns(ast, *node.else_branch, observed);
                }
            },
            [&](return_statement_syntax const& node) {
                observed.emplace_back (
                    node.value
                    ? infer_expression_type(ast, *node.value, std::nullopt).type
                    : semantic_type_ids::unit
                );
            },
            [&](expression_statement_syntax const& node) {
                infer_expression_type(ast, node.expression, std::nullopt);
            },
            [](break_statement_syntax const&) {},
            [](continue_statement_syntax const&) {},
        },
        statement
    );
}

auto semantic_analyzer::infer_template_if_condition(ast_arena const& ast, template_if_statement_syntax const& statement, std::uint32_t condition_id)
    -> template_if_condition_value
{
    auto const& condition = statement.conditions[condition_id];
    using enum template_if_condition_kind;
    switch(condition.kind) {
        case expression: {
            auto value = infer_template_if_expression_bool(ast, *condition.expression);
            if(not value) {
                return template_if_condition_value::dependent;
            }
            return *value ? template_if_condition_value::true_ : template_if_condition_value::false_;
        }
        case type_equality: {
            auto left = lower_type(ast, *condition.left_type);
            auto right = lower_type(ast, *condition.right_type);
            if(is_error(left) or is_error(right)) {
                return template_if_condition_value::error;
            }
            if(is_dependent_type(left) or is_dependent_type(right)) {
                return template_if_condition_value::dependent;
            }
            return read_type(left) == read_type(right)
                ? template_if_condition_value::true_
                : template_if_condition_value::false_;
        }
        case concept_bound: {
            auto target = lower_type(ast, *condition.left_type);
            if(is_error(target)) {
                return template_if_condition_value::error;
            }
            if(is_dependent_type(target)) {
                return template_if_condition_value::dependent;
            }
            return target_implements(active_unit_index, ast, *condition.concept_bound, target)
                ? template_if_condition_value::true_
                : template_if_condition_value::false_;
        }
        case not_: {
            auto value = infer_template_if_condition(ast, statement, condition.left_condition);
            if(value == template_if_condition_value::true_) {
                return template_if_condition_value::false_;
            }
            if(value == template_if_condition_value::false_) {
                return template_if_condition_value::true_;
            }
            return value;
        }
        case and_: {
            auto left = infer_template_if_condition(ast, statement, condition.left_condition);
            if(left == template_if_condition_value::false_ or left == template_if_condition_value::error) {
                return left;
            }
            auto right = infer_template_if_condition(ast, statement, condition.right_condition);
            if(left == template_if_condition_value::true_) {
                return right;
            }
            if(right == template_if_condition_value::false_ or right == template_if_condition_value::error) {
                return right;
            }
            return template_if_condition_value::dependent;
        }
        case or_: {
            auto left = infer_template_if_condition(ast, statement, condition.left_condition);
            if(left == template_if_condition_value::true_ or left == template_if_condition_value::error) {
                return left;
            }
            auto right = infer_template_if_condition(ast, statement, condition.right_condition);
            if(left == template_if_condition_value::false_) {
                return right;
            }
            if(right == template_if_condition_value::true_ or right == template_if_condition_value::error) {
                return right;
            }
            return template_if_condition_value::dependent;
        }
    }
    std::unreachable();
}

auto semantic_analyzer::infer_template_if_expression_value(ast_arena const& ast, expr_id id) -> std::optional<semantic_literal_value>
{
    return std::visit (
        overloaded {
            [&](literal_expr_syntax const& node) -> std::optional<semantic_literal_value> {
                auto text = ast_source.slice(node.full_span);
                if(text == "true" or text == "false") {
                    return semantic_literal_value { .value = text == "true" };
                }
                if(text.starts_with("'")) {
                    return semantic_literal_value { .value = parse_char_literal(text) };
                }
                if(not text.contains('.') and not text.contains('e') and not text.contains('E') and not text.starts_with("\"") and text != "nullptr") {
                    return semantic_literal_value { .value = parse_integer_literal(text) };
                }
                return std::nullopt;
            },
            [&](grouped_expr_syntax const& node) -> std::optional<semantic_literal_value> {
                return infer_template_if_expression_value(ast, node.expression);
            },
            [&](unary_expr_syntax const& node) -> std::optional<semantic_literal_value> {
                auto operand = infer_template_if_expression_value(ast, node.operand);
                if(not operand) {
                    return std::nullopt;
                }
                if(node.operator_kind == token_kind::kw_not) {
                    auto value = std::get_if<bool>(&operand->value);
                    if(value == nullptr) {
                        return std::nullopt;
                    }
                    return semantic_literal_value { .value = not *value };
                }
                if(node.operator_kind == token_kind::minus) {
                    auto value = std::get_if<std::int64_t>(&operand->value);
                    if(value == nullptr) {
                        return std::nullopt;
                    }
                    return semantic_literal_value { .value = -*value };
                }
                return std::nullopt;
            },
            [&](binary_expr_syntax const& node) -> std::optional<semantic_literal_value> {
                if(node.operator_kind == token_kind::kw_and) {
                    auto left = infer_template_if_expression_bool(ast, node.left);
                    if(left and not *left) {
                        return semantic_literal_value { .value = false };
                    }
                    auto right = infer_template_if_expression_bool(ast, node.right);
                    if(left and right) {
                        return semantic_literal_value { .value = *left and *right };
                    }
                    return std::nullopt;
                }
                if(node.operator_kind == token_kind::kw_or) {
                    auto left = infer_template_if_expression_bool(ast, node.left);
                    if(left and *left) {
                        return semantic_literal_value { .value = true };
                    }
                    auto right = infer_template_if_expression_bool(ast, node.right);
                    if(left and right) {
                        return semantic_literal_value { .value = *left or *right };
                    }
                    return std::nullopt;
                }

                auto left = infer_template_if_expression_value(ast, node.left);
                auto right = infer_template_if_expression_value(ast, node.right);
                if(not left or not right) {
                    return std::nullopt;
                }
                auto equal = left->value == right->value;
                switch(node.operator_kind) {
                    case token_kind::equal_equal:
                        return semantic_literal_value { .value = equal };
                    case token_kind::bang_equal:
                        return semantic_literal_value { .value = not equal };
                    case token_kind::less:
                    case token_kind::less_equal:
                    case token_kind::greater:
                    case token_kind::greater_equal: {
                        auto left_int = std::get_if<std::int64_t>(&left->value);
                        auto right_int = std::get_if<std::int64_t>(&right->value);
                        if(left_int == nullptr or right_int == nullptr) {
                            return std::nullopt;
                        }
                        if(node.operator_kind == token_kind::less) {
                            return semantic_literal_value { .value = *left_int < *right_int };
                        }
                        if(node.operator_kind == token_kind::less_equal) {
                            return semantic_literal_value { .value = *left_int <= *right_int };
                        }
                        if(node.operator_kind == token_kind::greater) {
                            return semantic_literal_value { .value = *left_int > *right_int };
                        }
                        return semantic_literal_value { .value = *left_int >= *right_int };
                    }
                    default:
                        return std::nullopt;
                }
            },
            [](auto const&) -> std::optional<semantic_literal_value> {
                return std::nullopt;
            },
        },
        ast.node(id)
    );
}

auto semantic_analyzer::infer_template_if_expression_bool(ast_arena const& ast, expr_id id) -> std::optional<bool>
{
    auto value = infer_template_if_expression_value(ast, id);
    if(not value) {
        return std::nullopt;
    }
    auto bool_value = std::get_if<bool>(&value->value);
    if(bool_value == nullptr) {
        return std::nullopt;
    }
    return *bool_value;
}

auto semantic_analyzer::infer_expression_type(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected) -> expression_info
{
    auto const& expression = ast.node(id);
    return std::visit (
        overloaded {
            [&](name_expr_syntax const& node) {
                return infer_name_expression(node);
            },
            [&](literal_expr_syntax const& node) {
                return expression_info{ .type = literal_type(node.full_span) };
            },
            [&](unary_expr_syntax const& node) {
                auto operand = infer_expression_type(ast, node.operand, std::nullopt);
                if(node.operator_kind == token_kind::kw_const and node.const_ref) {
                    return expression_info {
                        .type = result.types.intern(reference_type { read_type(operand.type), true }),
                        .is_lvalue = true,
                        .is_const = true,
                        .explicit_borrow = true,
                    };
                }
                return unary_type(node.operator_kind, node.position, operand);
            },
            [&](binary_expr_syntax const& node) {
                auto left = infer_expression_type(ast, node.left, std::nullopt).type;
                auto right = infer_expression_type(ast, node.right, std::nullopt).type;
                return binary_type(node.operator_kind, left, right);
            },
            [&](assignment_expr_syntax const& node) {
                auto left = infer_expression_type(ast, node.left, std::nullopt);
                auto left_type = read_type(left.type);
                infer_expression_type(ast, node.right, left_type);
                return expression_info{ .type = left_type };
            },
            [&](call_expr_syntax const& node) {
                return infer_call_expression(ast, node);
            },
            [&](member_expr_syntax const& node) {
                return infer_expression_type(ast, node.object, std::nullopt);
            },
            [&](index_expr_syntax const& node) {
                return infer_index_expression(ast, node);
            },
            [&](associated_name_expr_syntax const&) {
                return expression_info{ .type = semantic_type_ids::error };
            },
            [&](cast_expr_syntax const& node) {
                infer_expression_type(ast, node.operand, std::nullopt);
                return expression_info{ .type = lower_type(ast, node.type) };
            },
            [&](array_literal_expr_syntax const& node) {
                return infer_array_literal(ast, node.elements, expected);
            },
            [&](tuple_literal_expr_syntax const& node) {
                return infer_tuple_literal(ast, node.elements, expected);
            },
            [&](grouped_expr_syntax const& node) {
                return infer_expression_type(ast, node.expression, expected);
            },
            [&](struct_init_expr_syntax const& node) {
                for(auto const& initializer : node.initializers) {
                    if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                        infer_expression_type(ast, named->value, std::nullopt);
                    } else {
                        infer_expression_type(ast, std::get<positional_initializer_syntax>(initializer).value, std::nullopt);
                    }
                }
                return expression_info{ .type = lower_type(ast, node.type) };
            },
            [&](new_expr_syntax const& node) {
                auto element = lower_type(ast, node.type);
                infer_expression_type(ast, node.initializer, element);
                return expression_info{ .type = result.types.intern(pointer_type{ element }) };
            },
            [&](block_expr_syntax const& node) {
                return_scopes.emplace_back();
                type_scopes.emplace_back();
                auto observed = std::vector<semantic_type_id>{};
                for(auto statement : node.statements) {
                    infer_statement_returns(ast, statement, observed);
                }
                auto type = node.tail
                    ? infer_expression_type(ast, *node.tail, std::nullopt).type
                    : semantic_type_ids::unit;
                type_scopes.pop_back();
                return_scopes.pop_back();
                return expression_info{ .type = type };
            },
            [&](match_expr_syntax const& node) {
                auto matched = infer_expression_type(ast, node.value, std::nullopt);
                auto variant_index = variant_index_of(matched.type);
                auto result_type = std::optional<semantic_type_id>{};
                auto has_never_arm = false;
                for(auto const& arm : node.arms) {
                    return_scopes.emplace_back();
                    if(variant_index) {
                        auto const& variant = result.variants[*variant_index];
                        if(auto const* pattern = std::get_if<match_case_pattern_syntax>(&arm.pattern)) {
                            auto name = std::string{ ast_source.identifier(pattern->name) };
                            if(auto found = variant.case_indices.find(name); found != variant.case_indices.end()) {
                                auto payloads = variant_case_payload_types(matched.type, variant.cases[found->second]);
                                auto count = std::min(payloads.size(), pattern->bindings.size());
                                for(auto index = 0uz; index < count; ++index) {
                                    return_scopes.back().emplace(
                                        std::string{ ast_source.identifier(pattern->bindings[index]) },
                                        return_inference_binding {
                                            .type = payloads[index],
                                        }
                                    );
                                }
                            }
                        }
                    }
                    auto branch = infer_expression_type(ast, arm.value, result_type);
                    if(is_never(read_type(branch.type))) {
                        has_never_arm = true;
                        return_scopes.pop_back();
                        continue;
                    }
                    if(not result_type or is_never(read_type(*result_type))) {
                        result_type = branch.type;
                    }
                    return_scopes.pop_back();
                }
                return expression_info{ .type = result_type.value_or(has_never_arm ? semantic_type_ids::never : semantic_type_ids::unit) };
            },
            [&](lambda_expr_syntax const& node) {
                return infer_lambda_expression(node, expected);
            },
        },
        expression
    );
}

auto semantic_analyzer::infer_lambda_expression(lambda_expr_syntax const& node, std::optional<semantic_type_id> expected) -> expression_info
{
    auto symbol = result.function_symbol_of(return_unit, node.function);
    if(not symbol.valid()) {
        return expression_info{ .type = semantic_type_ids::error };
    }
    auto old_active_unit = active_unit_index;
    active_unit_index = return_unit;
    apply_lambda_parameter_context(node, expected);
    active_unit_index = old_active_unit;
    if(function_is_generic(return_unit, node.function)) {
        return expression_info{ .type = result.symbols[symbol.value].type };
    }
    infer_lambda_return_from_current_return_scope(return_unit, node.function);
    return expression_info{ .type = result.symbols[symbol.value].type };
}

auto semantic_analyzer::infer_lambda_return_from_current_return_scope(std::size_t unit_index, function_id id) -> void
{
    infer_lambda_return_with_base_scopes(unit_index, id, return_scopes, type_scopes);
}

auto semantic_analyzer::infer_lambda_return_from_current_value_scope(std::size_t unit_index, function_id id) -> void
{
    auto base_scopes = std::vector<std::map<std::string, return_inference_binding>>{};
    base_scopes.reserve(scopes.size());
    for(auto const& scope : scopes) {
        auto converted = std::map<std::string, return_inference_binding>{};
        for(auto const& [name, symbol] : scope) {
            if(not symbol.valid()) {
                continue;
            }
            auto const& value = result.symbols[symbol.value];
            converted.emplace(
                name,
                return_inference_binding {
                    .type = value.type,
                    .is_const = value.is_const,
                }
            );
        }
        base_scopes.emplace_back(std::move(converted));
    }

    infer_lambda_return_with_base_scopes(unit_index, id, std::move(base_scopes), type_scopes);
}

auto semantic_analyzer::infer_lambda_return_with_base_scopes(std::size_t unit_index, function_id id, std::vector<std::map<std::string, return_inference_binding>> base_scopes, std::vector<std::map<std::string, semantic_type_id>> base_type_scopes) -> void
{
    auto signature_id = result.signature_of(unit_index, id);
    if(not signature_id.valid()) {
        return;
    }
    auto& signature = result.signatures[signature_id.value];
    if(not signature.inferred_return or not is_inferred(signature.returns)) {
        return;
    }

    auto key = return_inference_key{ unit_index, id };
    auto& state = return_states[key];
    if(state == return_inference_state::resolved or state == return_inference_state::failed) {
        return;
    }
    if(state == return_inference_state::visiting) {
        return;
    }

    state = return_inference_state::visiting;
    auto const& unit = units[unit_index];
    auto const& ast = unit.ast;
    auto const& function = ast.node(id);

    auto old_unit = return_unit;
    auto old_return_scopes = std::move(return_scopes);
    auto old_type_scopes = std::move(type_scopes);
    return_unit = unit_index;
    return_scopes = std::move(base_scopes);
    type_scopes = std::move(base_type_scopes);

    auto observed = std::vector<semantic_type_id>{};
    return_scopes.emplace_back();
    type_scopes.emplace_back();
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        return_scopes.back().emplace(
            std::string{ ast_source.slice(parameter.name) },
            return_inference_binding {
                .type = signature.parameters[index],
                .is_const = parameter.is_const,
            }
        );
    }

    infer_statement_returns(ast, function.body, observed);
    auto inferred = join_return_types(observed);
    if(is_inferred(inferred)) {
        report_cannot_infer_return_type(unit_index, id);
        inferred = semantic_type_ids::error;
        state = return_inference_state::failed;
    } else if(is_error(inferred)) {
        state = return_inference_state::failed;
    } else {
        state = return_inference_state::resolved;
    }

    type_scopes = std::move(old_type_scopes);
    return_scopes = std::move(old_return_scopes);
    return_unit = old_unit;
    update_inferred_function_return(0uz, unit_index, id, inferred);
}

auto semantic_analyzer::infer_name_expression(name_expr_syntax const& node) -> expression_info
{
    auto name = std::string{ ast_source.identifier(node.name) };
    if(auto binding = resolve_binding(name)) {
        return expression_info {
            .type = binding->type,
            .is_lvalue = true,
            .is_const = binding->is_const,
        };
    }
    if(auto symbol = resolve_visible_function(name)) {
        return expression_info{ .type = result.symbols[symbol->value].type };
    }
    return expression_info{ .type = semantic_type_ids::error };
}

auto semantic_analyzer::infer_index_expression(ast_arena const& ast, index_expr_syntax const& node) -> expression_info
{
    auto object = infer_expression_type(ast, node.object, std::nullopt);
    infer_expression_type(ast, node.index, std::nullopt);
    auto const& type = result.types.get(read_type(object.type));
    if(auto const* tuple = std::get_if<tuple_type>(&type)) {
        auto value = constant_integer_index(ast, node.index);
        if(value and *value >= 0 and static_cast<std::uint64_t>(*value) < tuple->elements.size()) {
            return expression_info {
                .type = tuple->elements[static_cast<std::size_t>(*value)],
                .is_lvalue = object.is_lvalue,
                .is_const = object.is_const,
            };
        }
        return expression_info{ .type = semantic_type_ids::error };
    }
    if(auto const* array = std::get_if<array_type>(&type)) {
        return expression_info {
            .type = array->element,
            .is_lvalue = object.is_lvalue,
            .is_const = target_const(object.type, object.is_const),
        };
    }
    return expression_info{ .type = semantic_type_ids::error };
}

auto semantic_analyzer::infer_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info
{
    auto const& callee_syntax = ast.node(node.callee);
    if(auto const* name_expression = std::get_if<name_expr_syntax>(&callee_syntax)) {
        auto name = ast_source.identifier(name_expression->name);
        if(name == "alloc") {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            if(node.type_arguments.size() != 1uz) {
                return expression_info{ .type = semantic_type_ids::error };
            }
            auto const* type_argument = std::get_if<type_argument_type_syntax>(&node.type_arguments.front());
            if(type_argument == nullptr) {
                return expression_info{ .type = semantic_type_ids::error };
            }
            return expression_info {
                .type = intern_type(pointer_type{ lower_type(ast, type_argument->type) }),
            };
        }
        if(name == "free" or name == "construct_at" or name == "destroy_at") {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            return expression_info{ .type = semantic_type_ids::unit };
        }
        if(name == "panic" or name == "unreachable") {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            return expression_info{ .type = semantic_type_ids::never };
        }
        if(name == "assert") {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            return expression_info{ .type = semantic_type_ids::unit };
        }
        if(name == "builtin") {
            if(node.arguments.size() != 1uz) {
                for(auto argument : node.arguments) {
                    infer_expression_type(ast, argument, std::nullopt);
                }
                return expression_info{ .type = semantic_type_ids::error };
            }
            return infer_expression_type(ast, node.arguments.front(), std::nullopt);
        }
    }

    if(auto const* associated = std::get_if<associated_name_expr_syntax>(&callee_syntax)) {
        auto type = lower_type(ast, associated->type);
        auto owner = struct_index_of(type);
        if(not owner) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        auto name = std::string{ ast_source.identifier(associated->name) };
        auto found = result.structs[*owner].associated_functions.find(name);
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        if(found == result.structs[*owner].associated_functions.end()) {
            return expression_info{ .type = semantic_type_ids::error };
        }
        return expression_info{ .type = infer_callable_return(found->second) };
    }
    if(auto const* member = std::get_if<member_expr_syntax>(&callee_syntax)) {
        auto object = infer_expression_type(ast, member->object, std::nullopt);
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        auto name = std::string{ ast_source.identifier(member->name) };
        if(auto method = method_symbol(object.type, name)) {
            return expression_info{ .type = infer_callable_return(*method) };
        }
        if(auto function = resolve_visible_function(name)) {
            return expression_info{ .type = infer_callable_return(*function) };
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    if(auto function = callee_function_symbol(ast, node.callee)) {
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        return expression_info{ .type = infer_callable_return(*function) };
    }
    if(auto const* name_expression = std::get_if<name_expr_syntax>(&callee_syntax)) {
        auto name = std::string{ ast_source.identifier(name_expression->name) };
        if(not resolve_binding(name)) {
            if(auto self = resolve_binding("self")) {
                if(auto method = method_symbol(self->type, name)) {
                    for(auto argument : node.arguments) {
                        infer_expression_type(ast, argument, std::nullopt);
                    }
                    return expression_info{ .type = infer_callable_return(*method) };
                }
            }
        }
    }

    auto callee = infer_expression_type(ast, node.callee, std::nullopt);
    for(auto argument : node.arguments) {
        infer_expression_type(ast, argument, std::nullopt);
    }
    if(auto closure = result.lambda_of_closure(read_type(callee.type)); closure.valid()) {
        return expression_info{ .type = closure.callable.returns };
    }
    if(auto const* callable = callable_type(callee.type)) {
        return expression_info{ .type = callable->returns };
    }
    return expression_info{ .type = semantic_type_ids::error };
}

auto semantic_analyzer::infer_array_literal(ast_arena const& ast, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info
{
    if(expected) {
        for(auto element : elements) {
            infer_expression_type(ast, element, std::nullopt);
        }
        return expression_info{ .type = *expected };
    }
    if(elements.empty()) {
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto values = std::vector<semantic_type_id>{};
    for(auto element : elements) {
        values.emplace_back(infer_expression_type(ast, element, std::nullopt).type);
    }
    auto joined = join_return_types(values);
    if(is_error(joined) or is_inferred(joined)) {
        return expression_info{ .type = joined };
    }
    return expression_info {
        .type = intern_type (array_type {
            .element = joined,
            .length = result.types.intern(integer_constant_type {
                .value = static_cast<std::int64_t>(elements.size()),
                .type = builtin_type_kind::usize,
            }),
        }),
    };
}

auto semantic_analyzer::infer_tuple_literal(ast_arena const& ast, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info
{
    if(expected) {
        for(auto element : elements) {
            infer_expression_type(ast, element, std::nullopt);
        }
        return expression_info{ .type = *expected };
    }

    auto values = std::vector<semantic_type_id>{};
    for(auto element : elements) {
        values.emplace_back(read_type(infer_expression_type(ast, element, std::nullopt).type));
    }
    return expression_info{ .type = intern_type(tuple_type{ std::move(values) }) };
}

auto semantic_analyzer::infer_callable_return(symbol_id symbol) -> semantic_type_id
{
    auto const& value = result.symbols[symbol.value];
    auto signature_id = result.signature_of(value.unit_index, value.function);
    if(not signature_id.valid()) {
        return semantic_type_ids::error;
    }
    auto const& signature = result.signatures[signature_id.value];
    if(not signature.inferred_return) {
        return signature.returns;
    }
    return infer_function_return(value.unit_index, value.function);
}

auto semantic_analyzer::callee_function_symbol(ast_arena const& ast, expr_id id) -> std::optional<symbol_id>
{
    auto const& expression = ast.node(id);
    if(not is<name_expr_syntax>(expression)) {
        return std::nullopt;
    }

    auto name = ast_source.identifier(as<name_expr_syntax>(expression).name);
    return resolve_visible_function(name);
}

auto semantic_analyzer::resolve_binding(std::string_view name) const -> std::optional<return_inference_binding>
{
    for(auto scope = return_scopes.rbegin(); scope != return_scopes.rend(); ++scope) {
        if(auto found = scope->find(std::string{ name }); found != scope->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::resolve_visible_function(std::string_view name) const -> std::optional<symbol_id>
{
    auto const& visible = units[return_unit].visible_functions;
    if(auto found = visible.find(std::string{ name }); found != visible.end()) {
        return found->second;
    }
    return std::nullopt;
}

auto semantic_analyzer::update_inferred_function_return(std::size_t context_index, std::size_t unit_index, function_id id, semantic_type_id type) -> void
{
    auto signature_id = result.signature_of(context_index, unit_index, id);
    if(signature_id.valid()) {
        result.signatures[signature_id.value].returns = type;
    }

    auto symbol_id = result.function_symbol_of(context_index, unit_index, id);
    if(not symbol_id.valid()) {
        return;
    }
    auto& symbol = result.symbols[symbol_id.value];
    auto const& old_function_type = std::get<function_type>(result.types.get(symbol.type));
    symbol.type = intern_type (function_type {
        .parameters = old_function_type.parameters,
        .returns = type,
    });
}

auto semantic_analyzer::report_cannot_infer_return_type(std::size_t unit_index, function_id id) -> void
{
    auto const& function = units[unit_index].ast.node(id);
    report(
        diagnostic_kind::cannot_infer_return_type,
        function.full_span,
        "cannot infer function return type"
    );
}
