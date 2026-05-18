module semantic:statements;

import semantic;

auto semantic_analyzer::check_bodies() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            if(unit.ast.node(function_id).kind == function_syntax_kind::lambda) {
                continue;
            }
            check_function(unit_index, function_id);
        }
        for(auto impl_id : syntax.impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                check_function(unit_index, function_id);
            }
        }
        for(auto impl_id : syntax.concept_impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                check_function(unit_index, function_id);
            }
        }
    }
}

auto semantic_analyzer::check_function(std::size_t unit_index, function_id id) -> void
{
    active_unit_index = unit_index;
    active_unit = &units[unit_index];
    active_ast = &active_unit->ast;
    active_function = id;

    auto const& function = active_ast->node(active_function);
    if(function.defaulted) {
        return;
    }
    auto signature_id = result.signature_of(active_unit_index, active_function);
    if(not signature_id.valid()) {
        return;
    }

    enter_function_scope();
    active_self = {};

    auto& signature = result.signatures[signature_id.value];
    auto returns = return_state{};
    if(not is_error(signature.returns)) {
        returns.declared_return = signature.returns;
    }

    auto const function_symbol = result.function_symbol_of(active_unit_index, active_function);
    auto implicit_destructor_self = (
        function_symbol.valid()
        and result.symbols[function_symbol.value].function_kind == semantic_function_kind::destructor
    );

    if(implicit_destructor_self and function_symbol.valid()) {
        auto const& owner = result.structs[result.symbols[function_symbol.value].struct_index];
        auto self_type = result.types.intern(reference_type{ owner.type });
        auto symbol = bind_symbol(semantic_symbol {
            .kind = symbol_kind::parameter,
            .name = "self",
            .span = function.name,
            .type = self_type,
            .unit_index = active_unit_index,
            .function = active_function,
            .struct_index = result.symbols[function_symbol.value].struct_index,
        });
        active_self = symbol;
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
            if(name == "self") {
                active_self = symbol;
            }
        }
    }

    check_statement(function.body, returns);
}

auto semantic_analyzer::enter_function_scope() -> void
{
    scopes.clear();
    type_scopes.clear();
    loops.clear();
    scopes.emplace_back(active_unit->visible_functions);
    scopes.emplace_back();
    type_scopes.emplace_back();
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

auto semantic_analyzer::bind_type_alias(source_span name, semantic_type_id type) -> void
{
    auto& scope = type_scopes.back();
    auto key = std::string{ ast_source.slice(name) };
    if(scope.contains(key)) {
        report(
            diagnostic_kind::duplicate_symbol,
            name,
            std::format("duplicate type alias '{}'", key)
        );
        return;
    }

    scope.emplace(std::move(key), type);
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

auto semantic_analyzer::resolve_type_alias(std::string_view name) const -> std::optional<semantic_type_id>
{
    for(auto scope = type_scopes.rbegin(); scope != type_scopes.rend(); ++scope) {
        if(auto found = scope->find(std::string{ name }); found != scope->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::self_symbol() const -> symbol_id
{
    return active_self;
}

auto semantic_analyzer::self_struct_index() const -> std::optional<std::uint32_t>
{
    if(not active_self.valid()) {
        return std::nullopt;
    }
    auto type = result.symbols[active_self.value].type;
    return struct_index_of(type);
}

auto semantic_analyzer::self_field(std::string_view name) const -> std::optional<semantic_field_access>
{
    auto owner = self_struct_index();
    if(not owner) {
        return std::nullopt;
    }
    auto field = field_index(*owner, name);
    if(not field) {
        return std::nullopt;
    }
    return semantic_field_access {
        .struct_index = *owner,
        .field_index = *field,
        .implicit_self = true,
    };
}

auto semantic_analyzer::push_scope() -> void
{
    scopes.emplace_back();
    type_scopes.emplace_back();
}

auto semantic_analyzer::pop_scope() -> void
{
    scopes.pop_back();
    type_scopes.pop_back();
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
    auto const& statement = active_ast->node(id);
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
                if(not node.binding_names.empty()) {
                    auto initializer_type = expected.value_or(read_type(initializer.type));
                    auto const* tuple = std::get_if<tuple_type>(&result.types.get(read_type(initializer_type)));
                    if(tuple == nullptr) {
                        report(
                            diagnostic_kind::type_mismatch,
                            node.full_span,
                            "destructuring declaration requires a tuple initializer"
                        );
                        return;
                    }
                    if(tuple->elements.size() != node.binding_names.size()) {
                        report(
                            diagnostic_kind::aggregate_length_mismatch,
                            node.full_span,
                            "destructuring binding count does not match tuple element count"
                        );
                    }
                    if(node.is_ref and not initializer.is_lvalue) {
                        report(
                            diagnostic_kind::invalid_assignment_target,
                            node.full_span,
                            "ref destructuring initializer must be an lvalue"
                        );
                    }
                    auto elements = tuple->elements;
                    auto count = std::min(elements.size(), node.binding_names.size());
                    for(auto index = 0uz; index < count; ++index) {
                        auto binding = node.binding_names[index];
                        auto element = elements[index];
                        auto binding_type = node.is_ref
                            ? result.types.intern(reference_type {
                                element,
                                node.is_const or target_const(initializer.type, initializer.is_const),
                            })
                            : element;
                        auto symbol = (
                        bind_symbol (semantic_symbol {
                            .kind = symbol_kind::local,
                            .name = std::string{ ast_source.slice(binding) },
                            .span = binding,
                            .type = binding_type,
                            .is_const = node.is_const,
                            .unit_index = active_unit_index,
                            .function = active_function,
                        })
                    );
                        if(symbol.valid()) {
                            result.local_bindings[semantic_parameter_key{active_unit_index, binding.start}] = symbol;
                        }
                    }
                    return;
                }

                auto declared_type = expected.value_or(initializer.type);
                if(node.is_ref) {
                    if(not initializer.is_lvalue) {
                        report(
                            diagnostic_kind::invalid_assignment_target,
                            node.full_span,
                            "ref declaration initializer must be an lvalue"
                        );
                    }
                    declared_type = result.types.intern(reference_type {
                        read_type(declared_type),
                        node.is_const or target_const(initializer.type, initializer.is_const),
                    });
                }
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
                    .function = active_function,
                })
            );
                if(symbol.valid()) {
                    result.statement_bindings[semantic_node_key{active_unit_index, id}] = symbol;
                    result.local_bindings[semantic_parameter_key{active_unit_index, node.name.start}] = symbol;
                }
            },
            [&](type_alias_statement_syntax const& node) {
                bind_type_alias(node.name, lower_type(*active_ast, node.type));
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
                        "for range must be array<T,N>"
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
                    .function = active_function,
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
