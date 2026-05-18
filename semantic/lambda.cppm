module semantic:lambda;

import semantic;

auto semantic_analyzer::apply_lambda_parameter_context(
    lambda_expr_syntax const& node,
    std::optional<semantic_type_id> expected
) -> void
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

    return build_lambda_info(node, std::move(captures), std::move(callable));
}

auto semantic_analyzer::build_lambda_info(
    lambda_expr_syntax const& node,
    std::vector<semantic_lambda_capture> captures,
    function_type callable
) -> semantic_lambda_info
{
    auto function_symbol = result.function_symbol_of(active_context_index, active_unit_index, node.function);
    auto info = semantic_lambda_info {
        .function = node.function,
        .function_symbol = function_symbol,
        .captures = std::move(captures),
        .callable = std::move(callable),
    };

    if(not info.captures.empty()) {
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
