module semantic:lambda;

import semantic;

auto semantic_analyzer::apply_lambda_parameter_context(lambda_expr_syntax const& node, std::optional<semantic_type_id> expected) -> void
{
    auto signature_id = result.signature_of(active_context_index, active_unit_index, node.function);
    auto symbol = result.function_symbol_of(active_context_index, active_unit_index, node.function);
    if(not signature_id.valid() or not symbol.valid()) {
        return;
    }

    auto& signature = result.signatures[signature_id.value];
    if(not std::ranges::contains(signature.parameters, semantic_type_ids::inferred)) {
        return;
    }

    auto const* context = expected ? callable_type(*expected) : nullptr;
    if(context == nullptr or context->parameters.size() != signature.parameters.size()) {
        report(
            diagnostic_kind::type_mismatch,
            node.full_span,
            "lambda parameter types require a contextual function type"
        );
        for(auto& parameter : signature.parameters) {
            if(is_inferred(parameter)) {
                parameter = semantic_type_ids::error;
            }
        }
    } else {
        for(auto index = 0uz; index < signature.parameters.size(); ++index) {
            if(is_inferred(signature.parameters[index])) {
                signature.parameters[index] = context->parameters[index];
            }
        }
    }

    auto& function_symbol = result.symbols[symbol.value];
    auto const* old_function = std::get_if<function_type>(&result.types.get(function_symbol.type));
    function_symbol.type = intern_type(function_type {
        .parameters = signature.parameters,
        .returns = old_function == nullptr ? signature.returns : old_function->returns,
    });
}

auto semantic_analyzer::check_lambda_body(lambda_expr_syntax const& node) -> semantic_lambda_info
{
    infer_lambda_return_from_current_value_scope(active_unit_index, node.function);

    auto symbol = result.function_symbol_of(active_context_index, active_unit_index, node.function);
    auto signature_id = result.signature_of(active_context_index, active_unit_index, node.function);
    if(not symbol.valid() or not signature_id.valid()) {
        return {};
    }

    auto const& function = active_ast->node(node.function);
    auto const& signature = result.signatures[signature_id.value];
    auto callable = function_type {
        .parameters = signature.parameters,
        .returns = signature.returns,
    };

    auto old_function = active_function;
    auto old_self = active_self;
    active_function = node.function;
    active_self = {};

    lambda_capture_stack.emplace_back(node.function);

    push_scope();
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        auto parameter_symbol = (
            bind_symbol (semantic_symbol {
                .kind = symbol_kind::parameter,
                .name = std::string{ ast_source.slice(parameter.name) },
                .span = parameter.name,
                .type = signature.parameters[index],
                .is_const = parameter.is_const,
                .unit_index = active_unit_index,
                .function = active_function,
            })
        );
        if(parameter_symbol.valid()) {
            result.parameter_bindings[parameter_key(parameter.name)] = parameter_symbol;
        }
    }

    auto returns = return_state{};
    if(not is_error(signature.returns)) {
        returns.declared_return = signature.returns;
    }
    check_statement(function.body, returns);
    pop_scope();

    auto captures = std::move(lambda_capture_stack.back().captures);
    lambda_capture_stack.pop_back();

    active_self = old_self;
    active_function = old_function;

    return build_lambda_info(node, std::move(captures), std::move(callable), false);
}

auto semantic_analyzer::build_generic_lambda_info(lambda_expr_syntax const& node) -> semantic_lambda_info
{
    auto symbol = result.function_symbol_of(active_context_index, active_unit_index, node.function);
    auto signature_id = result.signature_of(active_context_index, active_unit_index, node.function);
    if(not symbol.valid() or not signature_id.valid()) {
        return {};
    }

    auto const& signature = result.signatures[signature_id.value];
    auto callable = function_type {
        .parameters = signature.parameters,
        .returns = signature.returns,
    };
    return build_lambda_info(node, collect_generic_lambda_captures(node.function), std::move(callable), true);
}

auto semantic_analyzer::collect_generic_lambda_captures(function_id id) -> std::vector<semantic_lambda_capture>
{
    auto const& function = active_ast->node(id);
    auto captures = std::vector<semantic_lambda_capture>{};
    auto capture_indices = std::map<symbol_id, std::uint32_t>{};
    auto local_scopes = std::vector<std::set<std::string>>{};

    auto push_scope = [&] {
        local_scopes.emplace_back();
    };
    auto pop_scope = [&] {
        local_scopes.pop_back();
    };
    auto add_local = [&](source_span name) {
        local_scopes.back().emplace(ast_source.identifier(name));
    };
    auto has_local = [&](std::string const& name) {
        return std::ranges::any_of(local_scopes, [&](std::set<std::string> const& scope) {
            return scope.contains(name);
        });
    };
    auto add_capture = [&](symbol_id symbol) {
        if(not symbol.valid() or capture_indices.contains(symbol)) {
            return;
        }
        auto const& value = result.symbols[symbol.value];
        if(value.kind != symbol_kind::local and value.kind != symbol_kind::parameter) {
            return;
        }
        if(value.function == id) {
            return;
        }

        auto field_index = static_cast<std::uint32_t>(captures.size());
        capture_indices.emplace(symbol, field_index);
        captures.emplace_back(symbol, value.name, value.span, read_type(value.type), value.is_const);
    };

    auto collect_expression = std::function<void(expr_id)>{};
    auto collect_statement = std::function<void(stmt_id)>{};

    collect_expression = [&](expr_id expression_id) {
        std::visit(
            overloaded {
                [&](name_expr_syntax const& node) {
                    auto name = std::string{ ast_source.identifier(node.name) };
                    if(has_local(name)) {
                        return;
                    }
                    add_capture(resolve(name));
                },
                [&](literal_expr_syntax const&) {},
                [&](unary_expr_syntax const& node) {
                    collect_expression(node.operand);
                },
                [&](binary_expr_syntax const& node) {
                    collect_expression(node.left);
                    collect_expression(node.right);
                },
                [&](assignment_expr_syntax const& node) {
                    collect_expression(node.left);
                    collect_expression(node.right);
                },
                [&](call_expr_syntax const& node) {
                    collect_expression(node.callee);
                    for(auto argument : node.arguments) {
                        collect_expression(argument);
                    }
                },
                [&](member_expr_syntax const& node) {
                    collect_expression(node.object);
                },
                [&](index_expr_syntax const& node) {
                    collect_expression(node.object);
                    collect_expression(node.index);
                },
                [&](associated_name_expr_syntax const&) {},
                [&](cast_expr_syntax const& node) {
                    collect_expression(node.operand);
                },
                [&](array_literal_expr_syntax const& node) {
                    for(auto element : node.elements) {
                        collect_expression(element);
                    }
                },
                [&](tuple_literal_expr_syntax const& node) {
                    for(auto element : node.elements) {
                        collect_expression(element);
                    }
                },
                [&](grouped_expr_syntax const& node) {
                    collect_expression(node.expression);
                },
                [&](struct_init_expr_syntax const& node) {
                    for(auto const& initializer : node.initializers) {
                        std::visit(
                            overloaded {
                                [&](named_field_initializer_syntax const& value) {
                                    collect_expression(value.value);
                                },
                                [&](positional_initializer_syntax const& value) {
                                    collect_expression(value.value);
                                },
                            },
                            initializer
                        );
                    }
                },
                [&](block_expr_syntax const& node) {
                    push_scope();
                    for(auto statement : node.statements) {
                        collect_statement(statement);
                    }
                    if(node.tail) {
                        collect_expression(*node.tail);
                    }
                    pop_scope();
                },
                [&](match_expr_syntax const& node) {
                    collect_expression(node.value);
                    for(auto const& arm : node.arms) {
                        push_scope();
                        if(auto const* pattern = std::get_if<match_case_pattern_syntax>(&arm.pattern)) {
                            for(auto binding : pattern->bindings) {
                                add_local(binding);
                            }
                        }
                        collect_expression(arm.value);
                        pop_scope();
                    }
                },
                [&](lambda_expr_syntax const&) {},
            },
            active_ast->node(expression_id)
        );
    };

    collect_statement = [&](stmt_id statement_id) {
        std::visit(
            overloaded {
                [&](block_statement_syntax const& node) {
                    push_scope();
                    for(auto statement : node.statements) {
                        collect_statement(statement);
                    }
                    pop_scope();
                },
                [&](declaration_statement_syntax const& node) {
                    collect_expression(node.initializer);
                    if(not node.binding_names.empty()) {
                        for(auto binding : node.binding_names) {
                            add_local(binding);
                        }
                    } else {
                        add_local(node.name);
                    }
                },
                [&](type_alias_statement_syntax const&) {},
                [&](if_statement_syntax const& node) {
                    collect_expression(node.condition);
                    collect_statement(node.then_branch);
                    if(node.else_branch) {
                        collect_statement(*node.else_branch);
                    }
                },
                [&](while_statement_syntax const& node) {
                    collect_expression(node.condition);
                    collect_statement(node.body);
                },
                [&](do_while_statement_syntax const& node) {
                    collect_statement(node.body);
                    collect_expression(node.condition);
                },
                [&](for_statement_syntax const& node) {
                    collect_expression(node.range);
                    push_scope();
                    add_local(node.name);
                    collect_statement(node.body);
                    pop_scope();
                },
                [&](template_for_statement_syntax const& node) {
                    push_scope();
                    add_local(node.name);
                    collect_statement(node.body);
                    pop_scope();
                },
                [&](break_statement_syntax const&) {},
                [&](continue_statement_syntax const&) {},
                [&](return_statement_syntax const& node) {
                    if(node.value) {
                        collect_expression(*node.value);
                    }
                },
                [&](expression_statement_syntax const& node) {
                    collect_expression(node.expression);
                },
            },
            active_ast->node(statement_id)
        );
    };

    push_scope();
    for(auto const& parameter : function.parameters) {
        add_local(parameter.name);
    }
    collect_statement(function.body);
    pop_scope();
    return captures;
}

auto semantic_analyzer::instantiate_lambda(semantic_lambda_info const& lambda, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> explicit_arguments, source_span span) -> semantic_lambda_info
{
    if(not lambda.valid()) {
        return {};
    }

    auto const& source = result.symbols[lambda.function_symbol.value];
    auto type_arguments = infer_function_type_arguments_for_call(
        lambda.function_symbol,
        std::nullopt,
        arguments,
        explicit_arguments,
        span
    );
    if(not type_arguments or not validate_function_type_arguments(source.unit_index, source.function, *type_arguments, span)) {
        return {};
    }

    auto key = semantic_function_instance_key{ source.unit_index, source.function, *type_arguments };
    if(auto found = result.function_instance_indices.find(key); found != result.function_instance_indices.end()) {
        auto const& instance = result.function_instances[found->second];
        return result.lambda_of(instance.context_index, source.unit_index, source.function);
    }

    auto source_signature_id = result.signature_of(source.unit_index, source.function);
    if(not source_signature_id.valid()) {
        return {};
    }

    auto const& source_signature = result.signatures[source_signature_id.value];
    if(source_signature.inferred_return) {
        report(
            diagnostic_kind::cannot_infer_return_type,
            source.span,
            "generic lambda return type must be explicit for monomorphization"
        );
        return {};
    }

    auto signature = substitute_signature_for_instance(
        source.unit_index,
        source.function,
        source_signature,
        *type_arguments,
        span
    );
    auto signature_id = add_signature(signature);
    auto function_type_id = intern_type(function_type {
        .parameters = signature.parameters,
        .returns = signature.returns,
    });

    auto instance_index = result.function_instances.size();
    auto context_index = next_context_index++;
    auto instance_symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = std::format("{}.mono.{}", source.name, instance_index),
        .span = source.span,
        .type = function_type_id,
        .unit_index = source.unit_index,
        .function = source.function,
        .function_kind = source.function_kind,
    });

    result.function_signatures.emplace(semantic_node_key{context_index, source.unit_index, source.function}, signature_id);
    result.function_symbols.emplace(semantic_node_key{context_index, source.unit_index, source.function}, instance_symbol);
    result.function_instances.emplace_back(
        key,
        context_index,
        instance_symbol,
        signature_id,
        function_substitution_map(source.unit_index, source.function, *type_arguments),
        function_type_pack_substitution_map(source.unit_index, source.function, *type_arguments)
    );
    result.function_instance_indices.emplace(std::move(key), instance_index);
    result.function_instance_by_symbol.emplace(instance_symbol, instance_index);

    auto old_context = active_context_index;
    auto old_unit_index = active_unit_index;
    auto old_unit = active_unit;
    auto old_ast = active_ast;
    auto old_function = active_function;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;
    auto old_self = active_self;

    auto& instance = result.function_instances[instance_index];
    active_context_index = context_index;
    active_unit_index = source.unit_index;
    active_unit = &units[source.unit_index];
    active_ast = &active_unit->ast;
    active_function = source.function;
    active_type_substitutions = &instance.substitutions;
    active_type_pack_substitutions = &instance.pack_substitutions;
    active_self = {};

    auto const& function = active_ast->node(source.function);
    lambda_capture_stack.emplace_back(source.function);

    push_scope();
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        auto parameter_symbol = (
            bind_symbol (semantic_symbol {
                .kind = symbol_kind::parameter,
                .name = std::string{ ast_source.slice(parameter.name) },
                .span = parameter.name,
                .type = signature.parameters[index],
                .is_const = parameter.is_const,
                .unit_index = active_unit_index,
                .function = active_function,
            })
        );
        if(parameter_symbol.valid()) {
            result.parameter_bindings[parameter_key(parameter.name)] = parameter_symbol;
        }
    }

    auto returns = return_state{ .declared_return = signature.returns };
    check_statement(function.body, returns);
    pop_scope();

    auto captures = std::move(lambda_capture_stack.back().captures);
    lambda_capture_stack.pop_back();
    if(captures.empty()) {
        captures = lambda.captures;
    }

    auto info = semantic_lambda_info {
        .function = source.function,
        .function_symbol = instance_symbol,
        .closure_type = lambda.closure_type,
        .closure_struct_index = lambda.closure_struct_index,
        .captures = std::move(captures),
        .callable = function_type {
            .parameters = signature.parameters,
            .returns = signature.returns,
        },
    };
    if(info.closure_type.valid()) {
        info.env_symbol = add_symbol(semantic_symbol {
            .kind = symbol_kind::parameter,
            .name = std::format("{}.env", source.name),
            .span = source.span,
            .type = intern_type(reference_type{ info.closure_type, true }),
            .is_const = true,
            .unit_index = source.unit_index,
            .function = source.function,
        });
    }
    result.lambda_infos[node_key(source.function)] = info;

    active_self = old_self;
    active_type_pack_substitutions = old_pack_substitutions;
    active_type_substitutions = old_substitutions;
    active_function = old_function;
    active_ast = old_ast;
    active_unit = old_unit;
    active_unit_index = old_unit_index;
    active_context_index = old_context;

    return info;
}

auto semantic_analyzer::build_lambda_info(lambda_expr_syntax const& node, std::vector<semantic_lambda_capture> captures, function_type callable, bool force_closure) -> semantic_lambda_info
{
    auto function_symbol = result.function_symbol_of(active_context_index, active_unit_index, node.function);
    auto info = semantic_lambda_info {
        .function = node.function,
        .function_symbol = function_symbol,
        .captures = std::move(captures),
        .callable = std::move(callable),
    };

    if(force_closure or not info.captures.empty()) {
        auto struct_index = static_cast<std::uint32_t>(result.structs.size());
        auto closure_type = intern_type(struct_type{ .index = struct_index });
        auto closure_name = std::format("cp.lambda.closure.{}.{}", active_unit_index, node.function.value);

        auto closure = semantic_struct {
            closure_name,
            node.full_span,
            closure_type,
            false,
            active_unit_index,
            {},
            {}
        };
        for(auto const& capture : info.captures) {
            closure.fields.emplace_back(capture.name, capture.span, capture.type);
        }
        result.structs.emplace_back(std::move(closure));

        info.closure_type = closure_type;
        info.closure_struct_index = struct_index;
        info.env_symbol = add_symbol(semantic_symbol {
            .kind = symbol_kind::parameter,
            .name = std::format("{}.env", closure_name),
            .span = node.full_span,
            .type = intern_type(reference_type{ closure_type, true }),
            .is_const = true,
            .unit_index = active_unit_index,
            .function = node.function,
        });
        result.closure_lambda_infos[struct_index] = node_key(node.function);
    }

    result.lambda_infos[node_key(node.function)] = info;
    return info;
}

auto semantic_analyzer::record_lambda_capture(symbol_id symbol, expr_id id) -> void
{
    if(lambda_capture_stack.empty() or not symbol.valid()) {
        return;
    }

    auto const& value = result.symbols[symbol.value];
    if(value.kind != symbol_kind::local and value.kind != symbol_kind::parameter) {
        return;
    }
    if(value.function == active_function) {
        return;
    }

    auto& capture = lambda_capture_stack.back();
    auto found = capture.capture_indices.find(symbol);
    if(found == capture.capture_indices.end()) {
        auto field_index = static_cast<std::uint32_t>(capture.captures.size());
        found = capture.capture_indices.emplace(symbol, field_index).first;
        capture.captures.emplace_back(symbol, value.name, value.span, read_type(value.type), value.is_const);
    }

    result.lambda_capture_accesses[node_key(id)] = semantic_lambda_capture_access {
        .function = capture.function,
        .field_index = found->second,
    };
}
