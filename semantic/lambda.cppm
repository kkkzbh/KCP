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

auto lambda_escape_reason_rank(semantic_lambda_escape_reason reason) -> int
{
    using enum semantic_lambda_escape_reason;
    switch(reason) {
        case none: return 0;
        case passed: return 1;
        case stored: return 2;
        case returned: return 3;
    }
    std::unreachable();
}

auto semantic_analyzer::collect_lambda_escapes_for_function(std::size_t unit_index, function_id id) -> void
{
    auto const& unit = units[unit_index];
    auto const& function = unit.ast.node(id);
    auto scopes = std::vector<std::map<std::string, std::set<std::uint32_t>>>{};
    scopes.emplace_back();
    for(auto const& parameter : function.parameters) {
        scopes.back().emplace(std::string{ ast_source.slice(parameter.name) }, std::set<std::uint32_t>{});
    }
    collect_lambda_escapes_in_statement(unit.ast, function.body, scopes);
}

auto semantic_analyzer::collect_lambda_escapes_in_statement(ast_arena const& ast, stmt_id id, std::vector<std::map<std::string, std::set<std::uint32_t>>>& scopes) -> void
{
    std::visit(
        overloaded {
            [&](block_statement_syntax const& node) {
                scopes.emplace_back();
                for(auto child : node.statements) {
                    collect_lambda_escapes_in_statement(ast, child, scopes);
                }
                scopes.pop_back();
            },
            [&](declaration_statement_syntax const& node) {
                auto initializer_lambdas = collect_lambda_escapes_in_expression(ast, node.initializer, scopes);
                if(not node.binding_names.empty()) {
                    for(auto binding : node.binding_names) {
                        scopes.back().emplace(std::string{ ast_source.slice(binding) }, std::set<std::uint32_t>{});
                    }
                    return;
                }
                scopes.back().emplace(std::string{ ast_source.slice(node.name) }, std::move(initializer_lambdas));
            },
            [&](type_alias_statement_syntax const&) {},
            [&](if_statement_syntax const& node) {
                collect_lambda_escapes_in_expression(ast, node.condition, scopes);
                collect_lambda_escapes_in_statement(ast, node.then_branch, scopes);
                if(node.else_branch) {
                    collect_lambda_escapes_in_statement(ast, *node.else_branch, scopes);
                }
            },
            [&](while_statement_syntax const& node) {
                collect_lambda_escapes_in_expression(ast, node.condition, scopes);
                collect_lambda_escapes_in_statement(ast, node.body, scopes);
            },
            [&](do_while_statement_syntax const& node) {
                collect_lambda_escapes_in_statement(ast, node.body, scopes);
                collect_lambda_escapes_in_expression(ast, node.condition, scopes);
            },
            [&](for_statement_syntax const& node) {
                collect_lambda_escapes_in_expression(ast, node.range, scopes);
                scopes.emplace_back();
                scopes.back().emplace(std::string{ ast_source.slice(node.name) }, std::set<std::uint32_t>{});
                collect_lambda_escapes_in_statement(ast, node.body, scopes);
                scopes.pop_back();
            },
            [&](template_for_statement_syntax const& node) {
                scopes.emplace_back();
                scopes.back().emplace(std::string{ ast_source.slice(node.name) }, std::set<std::uint32_t>{});
                collect_lambda_escapes_in_statement(ast, node.body, scopes);
                scopes.pop_back();
            },
            [&](template_if_statement_syntax const& node) {
                for(auto const& condition : node.conditions) {
                    if(condition.expression) {
                        collect_lambda_escapes_in_expression(ast, *condition.expression, scopes);
                    }
                }
                for(auto const& branch : node.branches) {
                    collect_lambda_escapes_in_statement(ast, branch.body, scopes);
                }
                if(node.else_branch) {
                    collect_lambda_escapes_in_statement(ast, *node.else_branch, scopes);
                }
            },
            [&](break_statement_syntax const&) {},
            [&](continue_statement_syntax const&) {},
            [&](return_statement_syntax const& node) {
                if(node.value) {
                    auto lambdas = collect_lambda_escapes_in_expression(ast, *node.value, scopes);
                    mark_lambdas_escaped(active_unit_index, lambdas, semantic_lambda_escape_reason::returned);
                }
            },
            [&](expression_statement_syntax const& node) {
                collect_lambda_escapes_in_expression(ast, node.expression, scopes);
            },
        },
        ast.node(id)
    );
}

auto semantic_analyzer::collect_lambda_escapes_in_expression(ast_arena const& ast, expr_id id, std::vector<std::map<std::string, std::set<std::uint32_t>>>& scopes) -> std::set<std::uint32_t>
{
    auto merge = [](std::set<std::uint32_t>& target, std::set<std::uint32_t> source) {
        target.insert(source.begin(), source.end());
    };

    return std::visit(
        overloaded {
            [&](name_expr_syntax const& node) {
                auto name = std::string{ ast_source.identifier(node.name) };
                for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
                    if(auto found = scope->find(name); found != scope->end()) {
                        return found->second;
                    }
                }
                return std::set<std::uint32_t>{};
            },
            [&](literal_expr_syntax const&) {
                return std::set<std::uint32_t>{};
            },
            [&](unary_expr_syntax const& node) {
                return collect_lambda_escapes_in_expression(ast, node.operand, scopes);
            },
            [&](binary_expr_syntax const& node) {
                auto result = collect_lambda_escapes_in_expression(ast, node.left, scopes);
                merge(result, collect_lambda_escapes_in_expression(ast, node.right, scopes));
                return result;
            },
            [&](assignment_expr_syntax const& node) {
                auto result = collect_lambda_escapes_in_expression(ast, node.left, scopes);
                auto right = collect_lambda_escapes_in_expression(ast, node.right, scopes);
                mark_lambdas_escaped(active_unit_index, right, semantic_lambda_escape_reason::stored);
                merge(result, std::move(right));
                return result;
            },
            [&](call_expr_syntax const& node) {
                collect_lambda_escapes_in_expression(ast, node.callee, scopes);
                for(auto argument : node.arguments) {
                    auto lambdas = collect_lambda_escapes_in_expression(ast, argument, scopes);
                    mark_lambdas_escaped(active_unit_index, lambdas, semantic_lambda_escape_reason::passed);
                }
                return std::set<std::uint32_t>{};
            },
            [&](member_expr_syntax const& node) {
                return collect_lambda_escapes_in_expression(ast, node.object, scopes);
            },
            [&](index_expr_syntax const& node) {
                auto result = collect_lambda_escapes_in_expression(ast, node.object, scopes);
                merge(result, collect_lambda_escapes_in_expression(ast, node.index, scopes));
                return result;
            },
            [&](associated_name_expr_syntax const&) {
                return std::set<std::uint32_t>{};
            },
            [&](cast_expr_syntax const& node) {
                return collect_lambda_escapes_in_expression(ast, node.operand, scopes);
            },
            [&](array_literal_expr_syntax const& node) {
                auto result = std::set<std::uint32_t>{};
                for(auto element : node.elements) {
                    merge(result, collect_lambda_escapes_in_expression(ast, element, scopes));
                }
                return result;
            },
            [&](tuple_literal_expr_syntax const& node) {
                auto result = std::set<std::uint32_t>{};
                for(auto element : node.elements) {
                    merge(result, collect_lambda_escapes_in_expression(ast, element, scopes));
                }
                return result;
            },
            [&](grouped_expr_syntax const& node) {
                return collect_lambda_escapes_in_expression(ast, node.expression, scopes);
            },
            [&](struct_init_expr_syntax const& node) {
                auto result = std::set<std::uint32_t>{};
                for(auto const& initializer : node.initializers) {
                    std::visit(
                        overloaded {
                            [&](named_field_initializer_syntax const& value) {
                                merge(result, collect_lambda_escapes_in_expression(ast, value.value, scopes));
                            },
                            [&](positional_initializer_syntax const& value) {
                                merge(result, collect_lambda_escapes_in_expression(ast, value.value, scopes));
                            },
                        },
                        initializer
                    );
                }
                return result;
            },
            [&](new_expr_syntax const& node) {
                auto result = collect_lambda_escapes_in_expression(ast, node.initializer, scopes);
                mark_lambdas_escaped(active_unit_index, result, semantic_lambda_escape_reason::stored);
                return result;
            },
            [&](block_expr_syntax const& node) {
                scopes.emplace_back();
                for(auto statement : node.statements) {
                    collect_lambda_escapes_in_statement(ast, statement, scopes);
                }
                auto result = node.tail
                    ? collect_lambda_escapes_in_expression(ast, *node.tail, scopes)
                    : std::set<std::uint32_t>{};
                mark_lambdas_escaped(active_unit_index, result, semantic_lambda_escape_reason::stored);
                scopes.pop_back();
                return result;
            },
            [&](match_expr_syntax const& node) {
                auto result = collect_lambda_escapes_in_expression(ast, node.value, scopes);
                for(auto const& arm : node.arms) {
                    scopes.emplace_back();
                    if(auto const* pattern = std::get_if<match_case_pattern_syntax>(&arm.pattern)) {
                        for(auto binding : pattern->bindings) {
                            scopes.back().emplace(std::string{ ast_source.identifier(binding) }, std::set<std::uint32_t>{});
                        }
                    }
                    merge(result, collect_lambda_escapes_in_expression(ast, arm.value, scopes));
                    scopes.pop_back();
                }
                return result;
            },
            [&](lambda_expr_syntax const& node) {
                return std::set<std::uint32_t>{ node.function.value };
            },
        },
        ast.node(id)
    );
}

auto semantic_analyzer::mark_lambdas_escaped(std::size_t unit_index, std::set<std::uint32_t> const& lambdas, semantic_lambda_escape_reason reason) -> void
{
    if(reason == semantic_lambda_escape_reason::none) {
        return;
    }
    for(auto raw : lambdas) {
        auto key = function_key(unit_index, function_id{ raw });
        auto found = lambda_escape_reasons.find(key);
        if(found == lambda_escape_reasons.end() or lambda_escape_reason_rank(found->second) < lambda_escape_reason_rank(reason)) {
            lambda_escape_reasons[key] = reason;
        }
    }
}

auto semantic_analyzer::lambda_escapes(std::size_t unit_index, function_id id) const -> bool
{
    return lambda_escape_reasons.contains(return_inference_key{ unit_index, id });
}

auto semantic_analyzer::lambda_escape_reason(std::size_t unit_index, function_id id) const -> semantic_lambda_escape_reason
{
    auto found = lambda_escape_reasons.find(return_inference_key{ unit_index, id });
    if(found == lambda_escape_reasons.end()) {
        return semantic_lambda_escape_reason::none;
    }
    return found->second;
}

auto semantic_analyzer::finalize_lambda_captures(function_id id, std::vector<semantic_lambda_capture>& captures) -> void
{
    auto escaped = lambda_escapes(active_unit_index, id);
    auto reason = lambda_escape_reason(active_unit_index, id);
    for(auto& capture : captures) {
        capture.value_type = read_type(capture.value_type.valid() ? capture.value_type : capture.type);
        capture.escaped = escaped;
        capture.escape_reason = reason;
        if(escaped) {
            capture.mode = capture.mutated ? semantic_lambda_capture_mode::owned_mut_copy : semantic_lambda_capture_mode::copy;
            capture.type = capture.value_type;
            continue;
        }
        auto writable = capture.mutated and not capture.is_const;
        capture.mode = writable ? semantic_lambda_capture_mode::ref : semantic_lambda_capture_mode::const_ref;
        capture.type = intern_type(reference_type{ capture.value_type, not writable });
    }
}

auto semantic_analyzer::record_escaping_lambda_captures(std::vector<semantic_lambda_capture> const& captures) -> void
{
    for(auto const& capture : captures) {
        if(not capture.escaped) {
            continue;
        }
        auto& records = escaping_capture_records[capture.symbol];
        records.emplace_back(
            capture.mode,
            capture.span
        );
        auto has_mutable_copy = std::ranges::any_of(records, [](lambda_escape_capture_record const& record) {
            return record.mode == semantic_lambda_capture_mode::owned_mut_copy;
        });
        if(records.size() < 2uz or not has_mutable_copy or warned_independent_captures.contains(capture.symbol)) {
            continue;
        }
        warned_independent_captures.emplace(capture.symbol);
        report(
            diagnostic_kind::independent_closure_capture,
            capture.span,
            std::format("{} is copied into multiple escaping closures; mutations are not shared", capture.name)
        );
    }
}

auto semantic_analyzer::check_lambda_body(lambda_expr_syntax const& node) -> semantic_lambda_info
{
    infer_lambda_return_from_current_value_scope(active_unit_index, node.function);
    collect_lambda_escapes_for_function(active_unit_index, node.function);

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
    auto old_function_context = active_function_context_index;
    auto old_self = active_self;
    active_function = node.function;
    active_function_context_index = active_context_index;
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

    auto returns = return_state{ .inferred_return = signature.inferred_return };
    if(not is_error(signature.returns)) {
        returns.declared_return = signature.returns;
    }
    check_statement(function.body, returns);
    pop_scope();

    auto captures = std::move(lambda_capture_stack.back().captures);
    lambda_capture_stack.pop_back();

    active_self = old_self;
    active_function_context_index = old_function_context;
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
                [&](new_expr_syntax const& node) {
                    collect_expression(node.initializer);
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
                [&](template_if_statement_syntax const& node) {
                    for(auto const& condition : node.conditions) {
                        if(condition.expression) {
                            collect_expression(*condition.expression);
                        }
                    }
                    for(auto const& branch : node.branches) {
                        collect_statement(branch.body);
                    }
                    if(node.else_branch) {
                        collect_statement(*node.else_branch);
                    }
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
    auto source_unit_index = source.unit_index;
    auto source_function = source.function;
    auto source_name = source.name;
    auto source_span = source.span;
    auto source_body_kind = source.body_kind;
    auto source_struct_index = source.struct_index;
    auto source_variant_index = source.variant_index;
    auto source_concept_index = source.concept_index;
    auto source_function_kind = source.function_kind;
    auto type_arguments = infer_function_type_arguments_for_call(
        lambda.function_symbol,
        std::nullopt,
        arguments,
        explicit_arguments,
        span
    );
    if(not type_arguments or not validate_function_type_arguments(source_unit_index, source_function, *type_arguments, span)) {
        return {};
    }

    auto source_signature_id = result.signature_of(source_unit_index, source_function);
    if(not source_signature_id.valid()) {
        return {};
    }

    auto const& source_signature = result.signatures[source_signature_id.value];
    if(source_signature.inferred_return) {
        report(
            diagnostic_kind::cannot_infer_return_type,
            source_span,
            "generic lambda return type must be explicit for monomorphization"
        );
        return {};
    }

    auto forward_bindings = forward_bindings_for_call(source_unit_index, source_function, source_signature, arguments);
    auto key = semantic_function_instance_key{ source_unit_index, source_function, *type_arguments, forward_bindings };
    if(auto found = result.function_instance_indices.find(key); found != result.function_instance_indices.end()) {
        auto const& instance = result.function_instances[found->second];
        return result.lambda_of(instance.context_index, source_unit_index, source_function);
    }

    auto signature = substitute_signature_for_instance(
        source_unit_index,
        source_function,
        source_signature,
        *type_arguments,
        forward_bindings,
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
        .name = std::format("{}.mono.{}", source_name, instance_index),
        .span = source_span,
        .type = function_type_id,
        .body_kind = source_body_kind,
        .unit_index = source_unit_index,
        .function = source_function,
        .struct_index = source_struct_index,
        .variant_index = source_variant_index,
        .concept_index = source_concept_index,
        .function_kind = source_function_kind,
    });

    result.function_signatures.emplace(semantic_node_key{context_index, source_unit_index, source_function}, signature_id);
    result.function_symbols.emplace(semantic_node_key{context_index, source_unit_index, source_function}, instance_symbol);
    result.function_instances.emplace_back(
        key,
        context_index,
        instance_symbol,
        signature_id,
        function_substitution_map(source_unit_index, source_function, *type_arguments),
        function_type_pack_substitution_map(source_unit_index, source_function, *type_arguments)
    );
    result.function_instance_indices.emplace(key, instance_index);
    result.function_instance_by_symbol.emplace(instance_symbol, instance_index);

    auto old_context = active_context_index;
    auto old_unit_index = active_unit_index;
    auto old_unit = active_unit;
    auto old_ast = active_ast;
    auto old_function = active_function;
    auto old_function_context = active_function_context_index;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;
    auto old_value_packs = std::move(active_value_packs);
    auto old_forward_parameters = std::move(active_forward_parameters);
    auto old_self = active_self;
    auto old_loops = loops;
    auto old_template_for_depth = active_template_for_depth;
    auto old_return_state = active_return_state;

    auto instance = result.function_instances[instance_index];
    active_context_index = context_index;
    active_unit_index = source_unit_index;
    active_unit = &units[source_unit_index];
    active_ast = &active_unit->ast;
    active_function = source_function;
    active_function_context_index = context_index;
    active_type_substitutions = &instance.substitutions;
    active_type_pack_substitutions = &instance.pack_substitutions;
    active_value_packs.clear();
    active_forward_parameters.clear();
    active_self = {};
    loops.clear();
    active_template_for_depth = 0uz;

    auto const& function = active_ast->node(source_function);
    lambda_capture_stack.emplace_back(source_function);

    push_scope();
    auto signature_parameter_index = 0uz;
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        auto name = std::string{ ast_source.slice(parameter.name) };
        if(parameter.is_pack) {
            auto bindings = std::vector<symbol_id>{};
            auto const source_is_forward = source_parameter_is_forward_reference(*active_ast, parameter);
            auto remaining_syntax_parameters = function.parameters.size() - index - 1uz;
            auto pack_end = signature.parameters.size() >= remaining_syntax_parameters
                ? signature.parameters.size() - remaining_syntax_parameters
                : signature_parameter_index;
            while(signature_parameter_index < pack_end) {
                auto concrete_parameter_index = signature_parameter_index;
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
                    if(source_is_forward and concrete_parameter_index < instance.key.forward_bindings.size()) {
                        active_forward_parameters.emplace(symbol, instance.key.forward_bindings[concrete_parameter_index]);
                    }
                }
            }
            active_value_packs[name] = bindings;
            result.parameter_pack_bindings[parameter_key(parameter.name)] = std::move(bindings);
            continue;
        }
        if(signature_parameter_index >= signature.parameters.size()) {
            continue;
        }

        auto concrete_parameter_index = signature_parameter_index;
        auto parameter_symbol = (
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
        if(parameter_symbol.valid()) {
            result.parameter_bindings[parameter_key(parameter.name)] = parameter_symbol;
            if(
                source_parameter_is_forward_reference(*active_ast, parameter)
                and concrete_parameter_index < instance.key.forward_bindings.size()
            ) {
                active_forward_parameters.emplace(parameter_symbol, instance.key.forward_bindings[concrete_parameter_index]);
            }
        }
    }

    auto returns = return_state{ .declared_return = signature.returns };
    active_return_state = &returns;
    check_statement(function.body, returns);
    pop_scope();

    auto captures = std::move(lambda_capture_stack.back().captures);
    lambda_capture_stack.pop_back();
    if(captures.empty()) {
        captures = lambda.captures;
    }

    auto info = semantic_lambda_info {
        .function = source_function,
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
            .name = std::format("{}.env", source_name),
            .span = source_span,
            .type = intern_type(reference_type{ info.closure_type, true }),
            .is_const = true,
            .unit_index = source_unit_index,
            .function = source_function,
        });
    }
    result.lambda_infos[semantic_node_key{context_index, source_unit_index, source_function}] = info;

    active_return_state = old_return_state;
    active_template_for_depth = old_template_for_depth;
    loops = std::move(old_loops);
    active_self = old_self;
    active_forward_parameters = std::move(old_forward_parameters);
    active_value_packs = std::move(old_value_packs);
    active_type_pack_substitutions = old_pack_substitutions;
    active_type_substitutions = old_substitutions;
    active_function_context_index = old_function_context;
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
    finalize_lambda_captures(node.function, captures);
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
            closure.fields.emplace_back(capture.name, capture.span, capture.type, std::nullopt);
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
    record_escaping_lambda_captures(info.captures);
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

auto semantic_analyzer::mark_lambda_capture_mutated(expr_id id) -> void
{
    if(lambda_capture_stack.empty()) {
        return;
    }

    auto stripped = stripped_expression(id);
    auto found = result.lambda_capture_accesses.find(node_key(stripped));
    if(found == result.lambda_capture_accesses.end()) {
        return;
    }

    auto access = found->second;
    for(auto context = lambda_capture_stack.rbegin(); context != lambda_capture_stack.rend(); ++context) {
        if(context->function != access.function) {
            continue;
        }
        if(access.field_index < context->captures.size()) {
            context->captures[access.field_index].mutated = true;
        }
        return;
    }
}

auto semantic_analyzer::mark_lambda_capture_mutated_for_parameter(expr_id id, semantic_type_id parameter) -> void
{
    auto const* reference = std::get_if<reference_type>(&result.types.get(parameter));
    if(reference == nullptr or reference->is_const or reference->reference_kind != reference_type::kind::regular) {
        return;
    }
    mark_lambda_capture_mutated(id);
}
