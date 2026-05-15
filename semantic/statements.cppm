module semantic:statements;

import semantic;

auto semantic_analyzer::check_bodies() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            check_function(unit_index, function_id);
        }
    }
}

auto semantic_analyzer::check_function(std::size_t unit_index, function_id id) -> void
{
    active_unit_index = unit_index;
    active_unit = &units[unit_index];
    active_ast = &active_unit->ast;
    active_function = id;

    auto const& function = active_ast->function(active_function);
    auto signature_id = result.signature_of(active_unit_index, active_function);
    if(not signature_id.valid()) {
        return;
    }

    enter_function_scope();

    auto& signature = result.signatures[signature_id.value];
    auto returns = return_state{};
    if(not is_error(signature.returns)) {
        returns.declared_return = signature.returns;
    }

    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        auto name = std::string{ ast_source.slice(parameter.name) };
        auto symbol = (
            bind_symbol (semantic_symbol {
                .kind = symbol_kind::parameter,
                .name = name,
                .span = parameter.name,
                .type = signature.parameters[index],
                .is_const = parameter.is_const,
                .unit_index = active_unit_index,
                .function = active_function,
            })
        );
        if(symbol.valid()) {
            result.parameter_bindings[semantic_parameter_key{active_unit_index, parameter.name.start}] = symbol;
        }
    }

    check_statement(function.body, returns);
}

auto semantic_analyzer::enter_function_scope() -> void
{
    scopes.clear();
    loops.clear();
    scopes.emplace_back(active_unit->visible_functions);
    scopes.emplace_back();
}

auto semantic_analyzer::bind_symbol(semantic_symbol symbol) -> symbol_id
{
    auto& scope = scopes.back();
    if(scope.contains(symbol.name)) {
        report(
            diagnostic_kind::duplicate_symbol,
            symbol.span,
            std::format("duplicate symbol '{}'", symbol.name)
        );
        return {};
    }

    auto id = add_symbol(std::move(symbol));
    scope.emplace(result.symbols[id.value].name, id);
    return id;
}

auto semantic_analyzer::resolve(std::string_view name) const -> symbol_id
{
    for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if(auto found = scope->find(std::string{ name }); found != scope->end()) {
            return found->second;
        }
    }
    return {};
}

auto semantic_analyzer::push_scope() -> void
{
    scopes.emplace_back();
}

auto semantic_analyzer::pop_scope() -> void
{
    scopes.pop_back();
}

auto semantic_analyzer::push_loop(loop_label label) -> void
{
    loops.emplace_back(std::move(label));
}

auto semantic_analyzer::pop_loop() -> void
{
    loops.pop_back();
}

auto semantic_analyzer::check_statement(stmt_id id, return_state& returns) -> void
{
    auto const& statement = active_ast->statement(id);
    std::visit (
        overloaded {
            [&](block_statement_syntax const& node) {
                push_scope();
                for(auto child : node.statements) {
                    check_statement(child, returns);
                }
                pop_scope();
            },
            [&](declaration_statement_syntax const& node) {
                auto expected = std::optional<semantic_type_id>{};
                if(node.declared_type) {
                    expected = lower_type(*active_ast, *node.declared_type);
                }

                auto initializer = check_expression(*active_ast, node.initializer, expected);
                auto declared_type = expected.value_or(initializer.type);
                if(expected and not can_implicitly_convert(initializer, *expected)) {
                    report(
                        diagnostic_kind::type_mismatch,
                        node.full_span,
                        "initializer type does not match declared type"
                    );
                }

                auto symbol = (
                    bind_symbol (semantic_symbol {
                        .kind = symbol_kind::local,
                        .name = std::string{ ast_source.slice(node.name) },
                        .span = node.name,
                        .type = declared_type,
                        .is_const = node.is_const,
                        .unit_index = active_unit_index,
                    })
                );
                if(symbol.valid()) {
                    result.statement_bindings[semantic_node_key{active_unit_index, id}] = symbol;
                }
            },
            [&](if_statement_syntax const& node) {
                check_condition(node.condition);
                check_statement(node.then_branch, returns);
                if(node.else_branch) {
                    check_statement(*node.else_branch, returns);
                }
            },
            [&](while_statement_syntax const& node) {
                check_condition(node.condition);
                push_loop();
                check_statement(node.body, returns);
                pop_loop();
            },
            [&](do_while_statement_syntax const& node) {
                push_loop();
                check_statement(node.body, returns);
                pop_loop();
                check_condition(node.condition);
            },
            [&](for_statement_syntax const& node) {
                auto range = check_expression(*active_ast, node.range, std::nullopt);
                auto element = range_element_type(range.type);
                if(not element.valid()) {
                    report(
                        diagnostic_kind::invalid_range,
                        node.full_span,
                        "for range must be array<T,N> or sequence<T,N>"
                    );
                    element = semantic_type_ids::error;
                }

                auto label = loop_label{};
                if(node.label) {
                    label = loop_label {
                        .name = std::string{ ast_source.slice(*node.label) },
                        .span = *node.label,
                    };
                }
                push_loop(label);
                push_scope();
                auto symbol = (
                    bind_symbol (semantic_symbol {
                        .kind = symbol_kind::local,
                        .name = std::string{ ast_source.slice(node.name) },
                        .span = node.name,
                        .type = element,
                        .is_const = node.is_const,
                        .unit_index = active_unit_index,
                    })
                );
                if(symbol.valid()) {
                    result.statement_bindings[semantic_node_key{active_unit_index, id}] = symbol;
                }
                check_statement(node.body, returns);
                pop_scope();
                pop_loop();
            },
            [&](break_statement_syntax const& node) {
                check_loop_jump(node.full_span, node.label, true);
            },
            [&](continue_statement_syntax const& node) {
                check_loop_jump(node.full_span, node.label, false);
            },
            [&](return_statement_syntax const& node) {
                auto returned = semantic_type_ids::unit;
                if(node.value) {
                    if(returns.declared_return) {
                        returned = check_expression(*active_ast, *node.value, returns.declared_return).type;
                    } else {
                        returned = check_expression(*active_ast, *node.value, std::nullopt).type;
                    }
                }

                if(returns.declared_return and not can_implicitly_convert(returned, *returns.declared_return)) {
                    report(
                        diagnostic_kind::return_type_mismatch,
                        node.full_span,
                        "return type does not match function return type"
                    );
                }
            },
            [&](expression_statement_syntax const& node) {
                check_expression(*active_ast, node.expression, std::nullopt);
            },
        },
        statement
    );
}

auto semantic_analyzer::check_condition(expr_id condition) -> void
{
    auto checked = check_expression(*active_ast, condition, semantic_type_ids::bool_);
    if(not can_implicitly_convert(checked, semantic_type_ids::bool_)) {
        report(
            diagnostic_kind::condition_not_bool,
            active_ast->span(condition),
            "condition expression must be bool"
        );
    }
}

auto semantic_analyzer::check_loop_jump(source_span span, std::optional<source_span> label, bool is_break) -> void
{
    if(loops.empty()) {
        report(
            is_break ? diagnostic_kind::invalid_break : diagnostic_kind::invalid_continue,
            span,
            is_break ? "break must be inside a loop" : "continue must be inside a loop"
        );
        return;
    }

    if(not label) {
        return;
    }

    auto name = ast_source.slice(*label);
    auto found = std::ranges::find(loops, name, &loop_label::name);
    if(found == loops.end()) {
        report(
            diagnostic_kind::unknown_label,
            *label,
            std::format("unknown loop label '{}'", name)
        );
    }
}
