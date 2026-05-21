module semantic:statements;

import semantic;

auto semantic_analyzer::check_bodies() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            auto const& function = unit.ast.node(function_id);
            if(function.kind == function_syntax_kind::lambda or function_is_generic(unit_index, function_id)) {
                continue;
            }
            if(function_has_source_body(result.function_symbol_of(unit_index, function_id))) {
                check_function(unit_index, function_id);
            }
        }
        for(auto impl_id : syntax.impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                if(function_has_source_body(result.function_symbol_of(unit_index, function_id)) and not function_is_generic(unit_index, function_id)) {
                    check_function(unit_index, function_id);
                }
            }
        }
        for(auto impl_id : syntax.concept_impls) {
            auto const& impl = unit.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                if(function_has_source_body(result.function_symbol_of(unit_index, function_id)) and not function_is_generic(unit_index, function_id)) {
                    check_function(unit_index, function_id);
                }
            }
        }
    }
}

auto semantic_analyzer::check_function(std::size_t unit_index, function_id id) -> void
{
    auto signature_id = result.signature_of(unit_index, id);
    auto function_symbol = result.function_symbol_of(unit_index, id);
    check_function_body(unit_index, id, 0uz, signature_id, function_symbol, nullptr, nullptr);
}

auto semantic_analyzer::check_function_instance(std::size_t instance_index) -> void
{
    auto instance = result.function_instances[instance_index];
    check_function_body(
        instance.key.unit_index,
        function_id{ instance.key.function_id_value },
        instance.context_index,
        instance.signature,
        instance.symbol,
        &instance.substitutions,
        &instance.pack_substitutions
    );
}

auto semantic_analyzer::check_function_body(std::size_t unit_index, function_id id, std::size_t context_index, function_signature_id signature_id, symbol_id function_symbol, std::map<std::string, semantic_type_id> const* substitutions, std::map<std::string, std::vector<semantic_type_id>> const* pack_substitutions) -> void
{
    auto old_context = active_context_index;
    auto old_unit_index = active_unit_index;
    auto old_unit = active_unit;
    auto old_ast = active_ast;
    auto old_function = active_function;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;
    auto old_self = active_self;
    auto old_scopes = scopes;
    auto old_type_scopes = type_scopes;
    auto old_value_packs = std::move(active_value_packs);
    auto old_loops = loops;
    auto old_template_for_depth = active_template_for_depth;

    active_context_index = context_index;
    active_unit_index = unit_index;
    active_unit = &units[unit_index];
    active_ast = &active_unit->ast;
    active_function = id;
    active_type_substitutions = substitutions;
    active_type_pack_substitutions = pack_substitutions;
    active_value_packs.clear();
    active_template_for_depth = 0uz;

    auto restore_state = [&] {
        active_type_substitutions = old_substitutions;
        active_type_pack_substitutions = old_pack_substitutions;
        active_function = old_function;
        active_self = old_self;
        scopes = std::move(old_scopes);
        type_scopes = std::move(old_type_scopes);
        active_value_packs = std::move(old_value_packs);
        loops = std::move(old_loops);
        active_template_for_depth = old_template_for_depth;
        active_ast = old_ast;
        active_unit = old_unit;
        active_unit_index = old_unit_index;
        active_context_index = old_context;
    };

    auto const& function = active_ast->node(active_function);
    if(not function_has_source_body(function_symbol)) {
        restore_state();
        return;
    }
    if(not signature_id.valid()) {
        restore_state();
        return;
    }

    enter_function_scope();
    active_self = {};

    auto& signature = result.signatures[signature_id.value];
    auto returns = return_state{};
    if(not is_error(signature.returns)) {
        returns.declared_return = signature.returns;
    }

    auto implicit_destructor_self = (
        function_symbol.valid()
        and result.symbols[function_symbol.value].function_kind == semantic_function_kind::destructor
    );

    if(implicit_destructor_self and function_symbol.valid()) {
        auto self_type = signature.parameters.empty()
            ? result.types.intern(reference_type{ result.structs[result.symbols[function_symbol.value].struct_index].type })
            : signature.parameters.front();
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

    auto signature_parameter_index = 0uz;
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        auto name = std::string{ ast_source.slice(parameter.name) };
        if(parameter.is_pack) {
            auto bindings = std::vector<symbol_id>{};
            auto remaining_syntax_parameters = function.parameters.size() - index - 1uz;
            auto pack_end = signature.parameters.size() >= remaining_syntax_parameters
                ? signature.parameters.size() - remaining_syntax_parameters
                : signature_parameter_index;
            while(signature_parameter_index < pack_end) {
                auto symbol = (
                    bind_symbol (semantic_symbol {
                        .kind = symbol_kind::parameter,
                        .name = std::format("{}.{}", name, bindings.size()),
                        .span = parameter.name,
                        .type = signature.parameters[signature_parameter_index++],
                        .is_const = parameter.is_const,
                        .unit_index = active_unit_index,
                        .function = active_function,
                    })
                );
                if(symbol.valid()) {
                    bindings.emplace_back(symbol);
                }
            }
            active_value_packs[name] = bindings;
            result.parameter_pack_bindings[parameter_key(parameter.name)] = std::move(bindings);
            continue;
        }
        if(signature_parameter_index >= signature.parameters.size()) {
            continue;
        }
        auto symbol = (
            bind_symbol (semantic_symbol {
                .kind = symbol_kind::parameter,
                .name = name,
                .span = parameter.name,
                .type = signature.parameters[signature_parameter_index++],
                .is_const = parameter.is_const,
                .unit_index = active_unit_index,
                .function = active_function,
            })
        );
        if(symbol.valid()) {
            result.parameter_bindings[parameter_key(parameter.name)] = symbol;
            if(name == "self") {
                active_self = symbol;
            }
        }
    }

    check_statement(function.body, returns);
    finish_nrvo(id, returns);
    if(statement_can_complete_normally(function.body)) {
        if(is_never(signature.returns)) {
            report(
                diagnostic_kind::return_type_mismatch,
                function.full_span,
                "function returning ! can complete normally"
            );
        } else if(returns.declared_return and not is_error(*returns.declared_return) and not is_unit(read_type(*returns.declared_return))) {
            report(
                diagnostic_kind::missing_return,
                function.full_span,
                "function with a non-unit return type can complete without returning a value"
            );
        }
    }
    restore_state();
}

auto semantic_analyzer::stripped_expression(expr_id id) const -> expr_id
{
    auto current = id;
    while(true) {
        auto const& expression = active_ast->node(current);
        if(auto const* grouped = std::get_if<grouped_expr_syntax>(&expression)) {
            current = grouped->expression;
            continue;
        }
        return current;
    }
}

auto semantic_analyzer::direct_initializer_expression(expr_id id) const -> bool
{
    auto const& expression = active_ast->node(stripped_expression(id));
    return std::holds_alternative<call_expr_syntax>(expression)
           or std::holds_alternative<array_literal_expr_syntax>(expression)
           or std::holds_alternative<tuple_literal_expr_syntax>(expression)
           or std::holds_alternative<struct_init_expr_syntax>(expression);
}

auto semantic_analyzer::nrvo_candidate_for_return(expr_id value, expression_info const& returned, return_state const& returns) -> symbol_id
{
    if(not returns.declared_return or returned.explicit_borrow or returned.is_move) {
        return {};
    }

    auto stripped = stripped_expression(value);
    auto const& expression = active_ast->node(stripped);
    if(not std::holds_alternative<name_expr_syntax>(expression)) {
        return {};
    }

    auto found = result.expression_symbols.find(node_key(stripped));
    if(found == result.expression_symbols.end()) {
        return {};
    }

    auto symbol = found->second;
    if(not symbol.valid()) {
        return {};
    }

    auto const& candidate = result.symbols[symbol.value];
    if(candidate.kind != symbol_kind::local or candidate.unit_index != active_unit_index or candidate.function != active_function) {
        return {};
    }
    if(std::holds_alternative<reference_type>(result.types.get(candidate.type))) {
        return {};
    }

    auto return_type = read_type(*returns.declared_return);
    if(read_type(candidate.type) != return_type or read_type(returned.type) != return_type) {
        return {};
    }
    return symbol;
}

auto semantic_analyzer::record_direct_initializer(stmt_id id, expr_id initializer) -> void
{
    if(direct_initializer_expression(initializer)) {
        result.direct_initializers[node_key(id)] = true;
    }
}

auto semantic_analyzer::finish_nrvo(function_id id, return_state const& returns) -> void
{
    if(not returns.nrvo_possible or not returns.nrvo_candidate.valid() or returns.nrvo_return_statements.empty()) {
        return;
    }

    result.function_nrvo_candidates[node_key(id)] = returns.nrvo_candidate;
    for(auto statement : returns.nrvo_return_statements) {
        result.return_nrvo_candidates[node_key(statement)] = returns.nrvo_candidate;
    }
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

auto semantic_analyzer::unexported_imported_function(std::string_view name) const -> std::optional<unexported_imported_symbol>
{
    if(active_unit == nullptr) {
        return std::nullopt;
    }

    auto key = std::string{ name };
    for(auto const& import : active_unit->root.imports) {
        auto module_name = ast_source.module_name(import.name);
        auto local_functions = module_functions.find(module_name);
        if(local_functions == module_functions.end()) {
            continue;
        }
        auto local_function = local_functions->second.find(key);
        if(local_function == local_functions->second.end()) {
            continue;
        }

        auto exported_functions = module_exports.find(module_name);
        if(exported_functions == module_exports.end() or not exported_functions->second.contains(key)) {
            return unexported_imported_symbol {
                .module_name = std::move(module_name),
                .symbol = local_function->second,
            };
        }
    }

    return std::nullopt;
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
    if(active_self.valid() and read_type(result.symbols[active_self.value].type) == semantic_type_ids::str) {
        auto field = str_field_index(name);
        if(not field) {
            return std::nullopt;
        }
        return semantic_field_access {
            .field_index = *field,
            .owner_type = semantic_type_ids::str,
            .implicit_self = true,
        };
    }

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
    label.template_for_depth = active_template_for_depth;
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
                            result.local_bindings[parameter_key(binding)] = symbol;
                        }
                    }
                    return;
                }

                auto declared_type = expected.value_or(read_type(initializer.type));
                if(not expected and is_nullptr(read_type(declared_type))) {
                    report(
                        diagnostic_kind::type_mismatch,
                        node.full_span,
                        "nullptr requires a contextual pointer type"
                    );
                    declared_type = semantic_type_ids::error;
                }
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
                if(not node.is_ref) {
                    record_direct_initializer(id, node.initializer);
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
                    result.statement_bindings[node_key(id)] = symbol;
                    result.local_bindings[parameter_key(node.name)] = symbol;
                    if(not std::holds_alternative<reference_type>(result.types.get(declared_type))) {
                        static_cast<void>(concrete_destructor_symbol(declared_type, node.full_span));
                    }
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
                auto range_metadata = range_info(range, node.full_span);
                auto element = range_metadata.element_type;
                if(not range_metadata.valid()) {
                    report(
                        diagnostic_kind::invalid_range,
                        node.full_span,
                        "for range must be [T; N], iterable, or iterator"
                    );
                    element = semantic_type_ids::error;
                }
                result.for_ranges[node_key(id)] = range_metadata;

                auto label = loop_label{};
                if(node.label) {
                    label = loop_label {
                        .name = std::string{ ast_source.slice(*node.label) },
                        .span = *node.label,
                        .template_for_depth = active_template_for_depth,
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
                    result.statement_bindings[node_key(id)] = symbol;
                }
                check_statement(node.body, returns);
                pop_scope();
                pop_loop();
            },
            [&](template_for_statement_syntax const& node) {
                check_template_for_statement(node, id, returns);
            },
            [&](break_statement_syntax const& node) {
                check_loop_jump(node.full_span, node.label, true);
            },
            [&](continue_statement_syntax const& node) {
                check_loop_jump(node.full_span, node.label, false);
            },
            [&](return_statement_syntax const& node) {
                auto returned = expression_info{ .type = semantic_type_ids::unit };
                if(node.value) {
                    if(returns.declared_return) {
                        returned = check_expression(*active_ast, *node.value, returns.declared_return);
                    } else {
                        returned = check_expression(*active_ast, *node.value, std::nullopt);
                    }
                    auto candidate = nrvo_candidate_for_return(*node.value, returned, returns);
                    if(not candidate.valid()) {
                        returns.nrvo_possible = false;
                    } else if(not returns.nrvo_candidate.valid()) {
                        returns.nrvo_candidate = candidate;
                    } else if(returns.nrvo_candidate != candidate) {
                        returns.nrvo_possible = false;
                    }
                    if(returns.nrvo_possible) {
                        returns.nrvo_return_statements.emplace_back(id);
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

auto semantic_analyzer::statement_can_complete_normally(stmt_id id) const -> bool
{
    auto const& statement = active_ast->node(id);
    return std::visit (
        overloaded {
            [&](block_statement_syntax const& node) {
                for(auto child : node.statements) {
                    if(not statement_can_complete_normally(child)) {
                        return false;
                    }
                }
                return true;
            },
            [](declaration_statement_syntax const&) {
                return true;
            },
            [](type_alias_statement_syntax const&) {
                return true;
            },
            [&](if_statement_syntax const& node) {
                return not node.else_branch
                       or statement_can_complete_normally(node.then_branch)
                       or statement_can_complete_normally(*node.else_branch);
            },
            [](while_statement_syntax const&) {
                return true;
            },
            [](do_while_statement_syntax const&) {
                return true;
            },
            [](for_statement_syntax const&) {
                return true;
            },
            [](template_for_statement_syntax const&) {
                return true;
            },
            [](break_statement_syntax const&) {
                return false;
            },
            [](continue_statement_syntax const&) {
                return false;
            },
            [](return_statement_syntax const&) {
                return false;
            },
            [&](expression_statement_syntax const& node) {
                auto found = result.expression_infos.find(node_key(node.expression));
                return (found == result.expression_infos.end() or not is_never(read_type(found->second.type)))
                       and expression_can_complete_normally(node.expression);
            },
        },
        statement
    );
}

auto semantic_analyzer::expression_can_complete_normally(expr_id id) const -> bool
{
    auto const& expression = active_ast->node(id);
    return std::visit (
        overloaded {
            [&](grouped_expr_syntax const& node) {
                return expression_can_complete_normally(node.expression);
            },
            [&](block_expr_syntax const& node) {
                for(auto statement : node.statements) {
                    if(not statement_can_complete_normally(statement)) {
                        return false;
                    }
                }
                return not node.tail or expression_can_complete_normally(*node.tail);
            },
            [&](match_expr_syntax const& node) {
                if(node.arms.empty()) {
                    return true;
                }
                return std::ranges::any_of (
                    node.arms,
                    [&](match_arm_syntax const& arm) {
                        return expression_can_complete_normally(arm.value);
                    }
                );
            },
            [](auto const&) {
                return true;
            },
        },
        expression
    );
}

auto semantic_analyzer::check_template_for_statement(template_for_statement_syntax const& node, stmt_id id, return_state& returns) -> void
{
    auto pack_name = std::string{ ast_source.slice(node.pack_name) };
    auto expansions = std::vector<semantic_template_for_expansion>{};
    auto parent_context = active_context_index;
    auto binding_name = std::string{ ast_source.slice(node.name) };

    if(node.binding_kind == template_for_binding_kind::type_binding) {
        if(active_type_pack_substitutions == nullptr) {
            report(diagnostic_kind::invalid_range, node.full_span, "unknown type parameter pack");
            return;
        }
        auto pack = active_type_pack_substitutions->find(pack_name);
        if(pack == active_type_pack_substitutions->end()) {
            report(diagnostic_kind::invalid_range, node.pack_name, std::format("unknown type parameter pack '{}'", pack_name));
            return;
        }

        for(auto type : pack->second) {
            auto expansion_context = next_context_index++;
            active_context_index = expansion_context;
            ++active_template_for_depth;
            push_scope();
            bind_type_alias(node.name, type);
            check_statement(node.body, returns);
            pop_scope();
            --active_template_for_depth;
            auto& expansion = expansions.emplace_back();
            expansion.kind = semantic_template_for_expansion_kind::type;
            expansion.context_index = expansion_context;
            expansion.bound_type = type;
        }
        active_context_index = parent_context;
        result.template_for_expansions[semantic_node_key{parent_context, active_unit_index, id}] = std::move(expansions);
        return;
    }

    auto pack = active_value_packs.find(pack_name);
    if(pack == active_value_packs.end()) {
        report(diagnostic_kind::invalid_range, node.pack_name, std::format("unknown value parameter pack '{}'", pack_name));
        return;
    }

    for(auto pack_symbol : pack->second) {
        auto expansion_context = next_context_index++;
        active_context_index = expansion_context;
        ++active_template_for_depth;
        push_scope();
        auto const& source_symbol = result.symbols[pack_symbol.value];
        auto binding_symbol = bind_symbol(semantic_symbol {
            .kind = symbol_kind::local,
            .name = binding_name,
            .span = node.name,
            .type = source_symbol.type,
            .is_const = node.binding_kind == template_for_binding_kind::const_binding or source_symbol.is_const,
            .unit_index = active_unit_index,
            .function = active_function,
        });
        if(binding_symbol.valid()) {
            result.statement_bindings[semantic_node_key{expansion_context, active_unit_index, id}] = binding_symbol;
            result.local_bindings[semantic_parameter_key{expansion_context, active_unit_index, node.name.start}] = binding_symbol;
        }
        check_statement(node.body, returns);
        pop_scope();
        --active_template_for_depth;
        auto& expansion = expansions.emplace_back();
        expansion.kind = semantic_template_for_expansion_kind::value;
        expansion.context_index = expansion_context;
        expansion.binding_symbol = binding_symbol;
        expansion.pack_symbol = pack_symbol;
        expansion.bound_type = source_symbol.type;
    }
    active_context_index = parent_context;
    result.template_for_expansions[semantic_node_key{parent_context, active_unit_index, id}] = std::move(expansions);
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
        if(loops.back().template_for_depth < active_template_for_depth) {
            report(
                is_break ? diagnostic_kind::invalid_break : diagnostic_kind::invalid_continue,
                span,
                is_break ? "break cannot target a loop outside template for" : "continue cannot target a loop outside template for"
            );
        }
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
        return;
    }
    if(found->template_for_depth < active_template_for_depth) {
        report(
            is_break ? diagnostic_kind::invalid_break : diagnostic_kind::invalid_continue,
            span,
            is_break ? "break cannot target a loop outside template for" : "continue cannot target a loop outside template for"
        );
    }
}
