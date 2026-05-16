module semantic:return_infer;

import semantic;

auto semantic_analyzer::infer_return_types() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
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

    update_inferred_function_return(unit_index, id, inferred);
}

auto semantic_analyzer::infer_function_body_return(std::size_t unit_index, function_id id) -> semantic_type_id
{
    auto const& unit = units[unit_index];
    auto const& ast = unit.ast;
    auto const& function = ast.node(id);

    auto old_unit = return_unit;
    auto old_return_scopes = std::move(return_scopes);
    return_unit = unit_index;

    auto observed = std::vector<semantic_type_id>{};
    return_scopes.emplace_back();
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
    return_scopes = std::move(old_return_scopes);
    return_unit = old_unit;
    return join_return_types(observed);
}

auto semantic_analyzer::infer_statement_returns(
    ast_arena const& ast,
    stmt_id id,
    std::vector<semantic_type_id>& observed
) -> void
{
    auto const& statement = ast.node(id);
    std::visit (
        overloaded {
            [&](block_statement_syntax const& node) {
                return_scopes.emplace_back();
                for(auto child : node.statements) {
                    infer_statement_returns(ast, child, observed);
                }
                return_scopes.pop_back();
            },
            [&](declaration_statement_syntax const& node) {
                auto declared = std::optional<semantic_type_id>{};
                if(node.declared_type) {
                    declared = lower_type(ast, *node.declared_type);
                }
                auto initializer = infer_expression_type(ast, node.initializer, declared);
                return_scopes.back().emplace (
                    std::string{ ast_source.slice(node.name) },
                    return_inference_binding {
                        .type = declared.value_or(initializer.type),
                        .is_const = node.is_const,
                    }
                );
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
                return_scopes.emplace_back();
                return_scopes.back().emplace (
                    std::string{ ast_source.slice(node.name) },
                    return_inference_binding {
                        .type = element,
                        .is_const = node.is_const,
                    }
                );
                infer_statement_returns(ast, node.body, observed);
                return_scopes.pop_back();
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

auto semantic_analyzer::infer_expression_type(
    ast_arena const& ast,
    expr_id id,
    std::optional<semantic_type_id> expected
) -> expression_info
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
                return unary_type(node.operator_kind, operand);
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
        },
        expression
    );
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

auto semantic_analyzer::infer_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info
{
    if(auto cast_type = function_style_cast_type(ast, node)) {
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        return expression_info{ .type = *cast_type };
    }

    if(auto function = callee_function_symbol(ast, node.callee)) {
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        return expression_info{ .type = infer_callable_return(*function) };
    }

    auto callee = infer_expression_type(ast, node.callee, std::nullopt);
    for(auto argument : node.arguments) {
        infer_expression_type(ast, argument, std::nullopt);
    }
    if(auto const* callable = std::get_if<function_type>(&result.types.get(callee.type))) {
        return expression_info{ .type = callable->returns };
    }
    return expression_info{ .type = semantic_type_ids::error };
}

auto semantic_analyzer::infer_array_literal(
    ast_arena const& ast,
    std::vector<expr_id> const& elements,
    std::optional<semantic_type_id> expected
) -> expression_info
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
            .length = elements.size(),
        }),
    };
}

auto semantic_analyzer::infer_tuple_literal(
    ast_arena const& ast,
    std::vector<expr_id> const& elements,
    std::optional<semantic_type_id> expected
) -> expression_info
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

auto semantic_analyzer::update_inferred_function_return(std::size_t unit_index, function_id id, semantic_type_id type) -> void
{
    auto signature_id = result.signature_of(unit_index, id);
    if(signature_id.valid()) {
        result.signatures[signature_id.value].returns = type;
    }

    auto symbol_id = result.function_symbol_of(unit_index, id);
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
