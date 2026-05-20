module semantic:generics;

import semantic;

auto semantic_analyzer::function_key(std::size_t unit_index, function_id id) const -> return_inference_key
{
    return return_inference_key{ unit_index, id };
}

auto semantic_analyzer::collect_type_pattern_parameters(ast_arena const& ast, type_id id) -> std::vector<std::string>
{
    auto names = std::vector<std::string>{};
    collect_type_pattern_parameters(ast, ast.node(id), names);
    return names;
}

auto semantic_analyzer::collect_type_pattern_parameters(ast_arena const& ast, type_syntax const& syntax, std::vector<std::string>& names) -> void
{
    if(syntax.is_array_type) {
        collect_type_pattern_parameters(ast, ast.node(syntax.array_element), names);
        if(auto const* length = std::get_if<type_argument_name_syntax>(&syntax.array_length)) {
            auto name = std::string{ ast_source.identifier(length->name) };
            if(not std::ranges::contains(names, name)) {
                names.emplace_back(std::move(name));
            }
        }
        return;
    }
    if(syntax.is_tuple_type) {
        for(auto element : syntax.tuple_elements) {
            collect_type_pattern_parameters(ast, ast.node(element), names);
        }
        return;
    }
    if(syntax.is_grouped_type) {
        collect_type_pattern_parameters(ast, ast.node(syntax.grouped_type), names);
        return;
    }

    for(auto const& argument : syntax.arguments) {
        if(not std::holds_alternative<type_argument_type_syntax>(argument)) {
            continue;
        }

        auto const& type = ast.node(std::get<type_argument_type_syntax>(argument).type);
        auto name = std::string{ ast_source.identifier(type.name) };
        auto bare_type_variable = (
            type.arguments.empty()
            and type.associated_names.empty()
            and type.suffix_operators.empty()
            and not type.is_const
            and not type.is_function_type
            and not type.is_decltype
            and name != "this"
            and name != "array"
            and name != "tuple"
            and not is_builtin_name(name)
            and not resolve_type_symbol(active_unit_index, name)
        );
        if(bare_type_variable and not std::ranges::contains(names, name)) {
            names.emplace_back(std::move(name));
        }
        collect_type_pattern_parameters(ast, type, names);
    }
}

auto semantic_analyzer::bind_active_generic_parameters(std::vector<std::string> const& names) -> void
{
    active_generic_parameters.clear();
    active_generic_parameter_packs.clear();
    for(auto index = 0uz; index < names.size(); ++index) {
        active_generic_parameters.emplace(
            names[index],
            result.types.intern(generic_parameter_type{ .index = static_cast<std::uint32_t>(index) })
        );
    }
}

auto semantic_analyzer::bind_active_generic_parameters(std::vector<generic_parameter_syntax> const& parameters) -> void
{
    active_generic_parameters.clear();
    active_generic_parameter_packs.clear();
    for(auto index = 0uz; index < parameters.size(); ++index) {
        auto const& parameter = parameters[index];
        auto name = std::string{ ast_source.identifier(parameter.name) };
        if(parameter.is_pack) {
            active_generic_parameter_packs.emplace(name);
        }
        active_generic_parameters.emplace(
            std::move(name),
            generic_parameter_type_for(static_cast<std::uint32_t>(index), parameter.parameter_kind)
        );
    }
}

auto semantic_analyzer::generic_parameter_type_for(std::uint32_t index, generic_parameter_syntax::kind kind) -> semantic_type_id
{
    using enum generic_parameter_syntax::kind;
    if(kind == const_usize or kind == const_isize) {
        return result.types.intern(generic_integer_parameter_type {
            .index = index,
            .type = kind == const_usize ? builtin_type_kind::usize : builtin_type_kind::isize,
        });
    }
    return result.types.intern(generic_parameter_type{ .index = index });
}

auto semantic_analyzer::bind_active_function_generic_parameters(std::size_t unit_index, function_id id) -> void
{
    active_generic_parameters.clear();
    active_generic_parameter_packs.clear();

    auto names = function_generic_parameter_names(unit_index, id);
    auto implicit_count = function_implicit_generic_count(unit_index, id);
    for(auto index = 0uz; index < implicit_count and index < names.size(); ++index) {
        active_generic_parameters.emplace(
            names[index],
            result.types.intern(generic_parameter_type{ .index = static_cast<std::uint32_t>(index) })
        );
    }

    auto const& function = units[unit_index].ast.node(id);
    for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
        auto argument_index = implicit_count + index;
        if(argument_index < names.size()) {
            active_generic_parameters.emplace(
                names[argument_index],
                generic_parameter_type_for(static_cast<std::uint32_t>(argument_index), function.generic_parameters[index].parameter_kind)
            );
        }
        if(function.generic_parameters[index].is_pack) {
            active_generic_parameter_packs.emplace(ast_source.identifier(function.generic_parameters[index].name));
            if(implicit_count + index >= names.size()) {
                continue;
            }
        }
    }
}

auto semantic_analyzer::function_generic_parameter_names(std::size_t unit_index, function_id id) const
    -> std::vector<std::string>
{
    auto names = std::vector<std::string>{};
    auto key = function_key(unit_index, id);
    if(auto implicit = implicit_function_generic_parameters.find(key); implicit != implicit_function_generic_parameters.end()) {
        names.insert(names.end(), implicit->second.begin(), implicit->second.end());
    }

    auto const& function = units[unit_index].ast.node(id);
    for(auto const& parameter : function.generic_parameters) {
        names.emplace_back(ast_source.identifier(parameter.name));
    }
    return names;
}

auto semantic_analyzer::function_implicit_generic_count(std::size_t unit_index, function_id id) const -> std::size_t
{
    auto key = function_key(unit_index, id);
    auto found = implicit_function_generic_parameters.find(key);
    return found == implicit_function_generic_parameters.end() ? 0uz : found->second.size();
}

auto semantic_analyzer::function_explicit_generic_count(std::size_t unit_index, function_id id) const -> std::size_t
{
    return units[unit_index].ast.node(id).generic_parameters.size();
}

auto semantic_analyzer::function_pack_generic_index(std::size_t unit_index, function_id id) const
    -> std::optional<std::size_t>
{
    auto implicit_count = function_implicit_generic_count(unit_index, id);
    auto const& function = units[unit_index].ast.node(id);
    for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
        if(function.generic_parameters[index].is_pack) {
            return implicit_count + index;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::function_value_pack_parameter_index(std::size_t unit_index, function_id id) const
    -> std::optional<std::size_t>
{
    auto const& function = units[unit_index].ast.node(id);
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        if(function.parameters[index].is_pack) {
            return index;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::function_is_generic(std::size_t unit_index, function_id id) const -> bool
{
    return not function_generic_parameter_names(unit_index, id).empty();
}

auto semantic_analyzer::function_impl_target_pattern(std::size_t unit_index, function_id id) const -> semantic_type_id
{
    auto found = function_impl_target_patterns.find(function_key(unit_index, id));
    if(found == function_impl_target_patterns.end()) {
        return {};
    }
    return found->second;
}

auto semantic_analyzer::function_signature_for_symbol(symbol_id symbol) const -> function_signature const*
{
    if(not symbol.valid()) {
        return nullptr;
    }
    auto const& value = result.symbols[symbol.value];
    auto signature_id = result.signature_of(value.unit_index, value.function);
    if(not signature_id.valid()) {
        return nullptr;
    }
    return &result.signatures[signature_id.value];
}

auto semantic_analyzer::generic_function_symbol(name_expr_syntax const& callee) const -> std::optional<symbol_id>
{
    auto name = std::string{ ast_source.identifier(callee.name) };
    auto symbol = resolve(name);
    if(not symbol.valid()) {
        return std::nullopt;
    }
    auto const& value = result.symbols[symbol.value];
    if(value.kind != symbol_kind::function) {
        return std::nullopt;
    }
    if(not function_is_generic(value.unit_index, value.function)) {
        return std::nullopt;
    }
    return symbol;
}

auto semantic_analyzer::explicit_type_arguments(ast_arena const& ast, call_expr_syntax const& node)
    -> std::optional<std::vector<semantic_type_id>>
{
    auto arguments = std::vector<semantic_type_id>{};
    arguments.reserve(node.type_arguments.size());
    for(auto const& argument : node.type_arguments) {
        if(auto const* type = std::get_if<type_argument_type_syntax>(&argument)) {
            arguments.emplace_back(lower_type(ast, type->type));
        } else if(auto const* literal = std::get_if<type_argument_literal_syntax>(&argument)) {
            arguments.emplace_back(result.types.intern(integer_constant_type {
                .value = parse_integer_literal(ast_source.slice(literal->literal)),
                .type = builtin_type_kind::usize,
            }));
        } else {
            report(diagnostic_kind::invalid_type_argument, node.full_span, "generic function arguments must be types or integers");
            return std::nullopt;
        }
    }
    return arguments;
}

auto semantic_analyzer::infer_type_argument(semantic_type_id pattern, semantic_type_id argument, std::map<std::uint32_t, semantic_type_id>& inferred) -> bool
{
    auto const& pattern_kind = result.types.get(pattern);
    if(auto const* generic = std::get_if<generic_parameter_type>(&pattern_kind)) {
        auto concrete = argument;
        auto found = inferred.find(generic->index);
        if(found == inferred.end()) {
            inferred.emplace(generic->index, concrete);
            return true;
        }
        return found->second == concrete;
    }
    if(auto const* generic = std::get_if<generic_integer_parameter_type>(&pattern_kind)) {
        auto concrete = read_type(argument);
        if(not std::holds_alternative<integer_constant_type>(result.types.get(concrete))) {
            return false;
        }
        auto found = inferred.find(generic->index);
        if(found == inferred.end()) {
            inferred.emplace(generic->index, concrete);
            return true;
        }
        return found->second == concrete;
    }

    if(auto const* reference = std::get_if<reference_type>(&pattern_kind)) {
        return infer_type_argument(reference->pointee, read_type(argument), inferred);
    }

    auto argument_value = read_type(argument);
    auto const& argument_kind = result.types.get(argument_value);
    if(auto const* pattern_pointer = std::get_if<pointer_type>(&pattern_kind)) {
        auto const* argument_pointer = std::get_if<pointer_type>(&argument_kind);
        return argument_pointer != nullptr
               and (pattern_pointer->is_const or pattern_pointer->is_like or not argument_pointer->is_const)
               and (pattern_pointer->is_like or pattern_pointer->is_like == argument_pointer->is_like)
               and infer_type_argument(pattern_pointer->pointee, argument_pointer->pointee, inferred);
    }
    if(auto const* pattern_array = std::get_if<array_type>(&pattern_kind)) {
        auto const* argument_array = std::get_if<array_type>(&argument_kind);
        return argument_array != nullptr
               and infer_type_argument(pattern_array->length, argument_array->length, inferred)
               and infer_type_argument(pattern_array->element, argument_array->element, inferred);
    }
    if(auto const* pattern_tuple = std::get_if<tuple_type>(&pattern_kind)) {
        auto const* argument_tuple = std::get_if<tuple_type>(&argument_kind);
        if(argument_tuple == nullptr or pattern_tuple->elements.size() != argument_tuple->elements.size()) {
            return false;
        }
        for(auto index = 0uz; index < pattern_tuple->elements.size(); ++index) {
            if(not infer_type_argument(pattern_tuple->elements[index], argument_tuple->elements[index], inferred)) {
                return false;
            }
        }
        return true;
    }
    if(auto const* pattern_function = std::get_if<function_type>(&pattern_kind)) {
        auto const* argument_function = std::get_if<function_type>(&argument_kind);
        if(argument_function == nullptr or pattern_function->parameters.size() != argument_function->parameters.size()) {
            return false;
        }
        for(auto index = 0uz; index < pattern_function->parameters.size(); ++index) {
            if(not infer_type_argument(pattern_function->parameters[index], argument_function->parameters[index], inferred)) {
                return false;
            }
        }
        return infer_type_argument(pattern_function->returns, argument_function->returns, inferred);
    }
    if(auto const* pattern_struct = std::get_if<struct_type>(&pattern_kind)) {
        auto const* argument_struct = std::get_if<struct_type>(&argument_kind);
        if(
            argument_struct == nullptr
            or pattern_struct->index != argument_struct->index
            or pattern_struct->arguments.size() != argument_struct->arguments.size()
        ) {
            return false;
        }
        for(auto index = 0uz; index < pattern_struct->arguments.size(); ++index) {
            if(not infer_type_argument(pattern_struct->arguments[index], argument_struct->arguments[index], inferred)) {
                return false;
            }
        }
        return true;
    }
    if(auto const* pattern_variant = std::get_if<variant_type>(&pattern_kind)) {
        auto const* argument_variant = std::get_if<variant_type>(&argument_kind);
        if(
            argument_variant == nullptr
            or pattern_variant->index != argument_variant->index
            or pattern_variant->arguments.size() != argument_variant->arguments.size()
        ) {
            return false;
        }
        for(auto index = 0uz; index < pattern_variant->arguments.size(); ++index) {
            if(not infer_type_argument(pattern_variant->arguments[index], argument_variant->arguments[index], inferred)) {
                return false;
            }
        }
        return true;
    }

    return pattern == argument_value;
}

auto semantic_analyzer::infer_type_argument_with_pack(semantic_type_id pattern, semantic_type_id argument, std::optional<std::size_t> pack_index, std::map<std::uint32_t, semantic_type_id>& inferred, std::optional<semantic_type_id>& pack_element) -> bool
{
    auto const& pattern_kind = result.types.get(pattern);
    if(auto const* generic = std::get_if<generic_parameter_type>(&pattern_kind)) {
        if(pack_index and generic->index == *pack_index) {
            auto concrete = argument;
            if(not pack_element) {
                pack_element = concrete;
                return true;
            }
            return *pack_element == concrete;
        }
        return infer_type_argument(pattern, argument, inferred);
    }

    if(auto const* reference = std::get_if<reference_type>(&pattern_kind)) {
        return infer_type_argument_with_pack(reference->pointee, read_type(argument), pack_index, inferred, pack_element);
    }

    auto argument_value = read_type(argument);
    auto const& argument_kind = result.types.get(argument_value);
    if(auto const* pattern_pointer = std::get_if<pointer_type>(&pattern_kind)) {
        auto const* argument_pointer = std::get_if<pointer_type>(&argument_kind);
        return argument_pointer != nullptr
               and (pattern_pointer->is_const or pattern_pointer->is_like or not argument_pointer->is_const)
               and (pattern_pointer->is_like or pattern_pointer->is_like == argument_pointer->is_like)
               and infer_type_argument_with_pack(pattern_pointer->pointee, argument_pointer->pointee, pack_index, inferred, pack_element);
    }
    if(auto const* pattern_array = std::get_if<array_type>(&pattern_kind)) {
        auto const* argument_array = std::get_if<array_type>(&argument_kind);
        return argument_array != nullptr
               and infer_type_argument(pattern_array->length, argument_array->length, inferred)
               and infer_type_argument_with_pack(pattern_array->element, argument_array->element, pack_index, inferred, pack_element);
    }
    if(auto const* pattern_tuple = std::get_if<tuple_type>(&pattern_kind)) {
        auto const* argument_tuple = std::get_if<tuple_type>(&argument_kind);
        if(argument_tuple == nullptr or pattern_tuple->elements.size() != argument_tuple->elements.size()) {
            return false;
        }
        for(auto index = 0uz; index < pattern_tuple->elements.size(); ++index) {
            if(not infer_type_argument_with_pack(
                pattern_tuple->elements[index],
                argument_tuple->elements[index],
                pack_index,
                inferred,
                pack_element
            )) {
                return false;
            }
        }
        return true;
    }
    if(auto const* pattern_function = std::get_if<function_type>(&pattern_kind)) {
        auto const* argument_function = std::get_if<function_type>(&argument_kind);
        if(argument_function == nullptr or pattern_function->parameters.size() != argument_function->parameters.size()) {
            return false;
        }
        for(auto index = 0uz; index < pattern_function->parameters.size(); ++index) {
            if(not infer_type_argument_with_pack(
                pattern_function->parameters[index],
                argument_function->parameters[index],
                pack_index,
                inferred,
                pack_element
            )) {
                return false;
            }
        }
        return infer_type_argument_with_pack(pattern_function->returns, argument_function->returns, pack_index, inferred, pack_element);
    }
    if(auto const* pattern_struct = std::get_if<struct_type>(&pattern_kind)) {
        auto const* argument_struct = std::get_if<struct_type>(&argument_kind);
        if(
            argument_struct == nullptr
            or pattern_struct->index != argument_struct->index
            or pattern_struct->arguments.size() != argument_struct->arguments.size()
        ) {
            return false;
        }
        for(auto index = 0uz; index < pattern_struct->arguments.size(); ++index) {
            if(not infer_type_argument_with_pack(
                pattern_struct->arguments[index],
                argument_struct->arguments[index],
                pack_index,
                inferred,
                pack_element
            )) {
                return false;
            }
        }
        return true;
    }
    if(auto const* pattern_variant = std::get_if<variant_type>(&pattern_kind)) {
        auto const* argument_variant = std::get_if<variant_type>(&argument_kind);
        if(
            argument_variant == nullptr
            or pattern_variant->index != argument_variant->index
            or pattern_variant->arguments.size() != argument_variant->arguments.size()
        ) {
            return false;
        }
        for(auto index = 0uz; index < pattern_variant->arguments.size(); ++index) {
            if(not infer_type_argument_with_pack(
                pattern_variant->arguments[index],
                argument_variant->arguments[index],
                pack_index,
                inferred,
                pack_element
            )) {
                return false;
            }
        }
        return true;
    }

    return pattern == argument_value;
}

auto semantic_analyzer::infer_generic_type_arguments(function_signature const& signature, std::vector<expression_info> const& arguments, std::size_t parameter_count) -> std::optional<std::vector<semantic_type_id>>
{
    auto inferred = std::map<std::uint32_t, semantic_type_id>{};
    auto count = std::min(signature.parameters.size(), arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        auto parameter = signature.parameters[index];
        auto argument_type = arguments[index].type;
        if(std::holds_alternative<generic_parameter_type>(result.types.get(parameter))) {
            argument_type = read_type(argument_type);
        }
        if(not infer_type_argument(parameter, argument_type, inferred)) {
            return std::nullopt;
        }
    }

    auto type_arguments = std::vector<semantic_type_id>(parameter_count, semantic_type_id{});
    for(auto index = 0uz; index < parameter_count; ++index) {
        auto found = inferred.find(static_cast<std::uint32_t>(index));
        if(found == inferred.end()) {
            return std::nullopt;
        }
        type_arguments[index] = found->second;
    }
    return type_arguments;
}

auto semantic_analyzer::generic_substitution_map(function_syntax const& function, std::vector<semantic_type_id> const& type_arguments) -> std::map<std::string, semantic_type_id>
{
    auto substitutions = std::map<std::string, semantic_type_id>{};
    auto count = std::min(function.generic_parameters.size(), type_arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        substitutions.emplace(
            std::string{ ast_source.identifier(function.generic_parameters[index].name) },
            type_arguments[index]
        );
    }
    return substitutions;
}

auto semantic_analyzer::function_substitution_map(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments) -> std::map<std::string, semantic_type_id>
{
    auto substitutions = std::map<std::string, semantic_type_id>{};
    auto names = function_generic_parameter_names(unit_index, id);
    auto pack_index = function_pack_generic_index(unit_index, id);
    auto count = std::min(names.size(), type_arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        if(pack_index and index == *pack_index) {
            continue;
        }
        substitutions.emplace(names[index], type_arguments[index]);
    }
    return substitutions;
}

auto semantic_analyzer::function_type_pack_substitution_map(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments) -> std::map<std::string, std::vector<semantic_type_id>>
{
    auto substitutions = std::map<std::string, std::vector<semantic_type_id>>{};
    auto pack_index = function_pack_generic_index(unit_index, id);
    if(not pack_index) {
        return substitutions;
    }

    auto names = function_generic_parameter_names(unit_index, id);
    if(*pack_index >= names.size() or *pack_index > type_arguments.size()) {
        return substitutions;
    }

    substitutions.emplace(
        names[*pack_index],
        std::vector<semantic_type_id>{ type_arguments.begin() + static_cast<std::ptrdiff_t>(*pack_index), type_arguments.end() }
    );
    return substitutions;
}

auto semantic_analyzer::validate_function_pack_shape(std::size_t unit_index, function_id id) -> void
{
    auto const& function = units[unit_index].ast.node(id);
    auto seen_generic_pack = false;
    for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
        auto const& parameter = function.generic_parameters[index];
        if(not parameter.is_pack) {
            continue;
        }
        if(seen_generic_pack or index + 1uz != function.generic_parameters.size()) {
            report(
                diagnostic_kind::invalid_type_argument,
                parameter.full_span,
                "generic parameter pack must be the final generic parameter"
            );
        }
        seen_generic_pack = true;
    }

    auto seen_value_pack = false;
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        if(parameter.is_self_receiver and index != 0uz) {
            report(
                diagnostic_kind::invalid_self_parameter,
                parameter.full_span,
                "self receiver must be the first function parameter"
            );
        }
        if(not parameter.is_pack) {
            continue;
        }
        if(seen_value_pack or index + 1uz != function.parameters.size()) {
            report(
                diagnostic_kind::invalid_type_argument,
                parameter.full_span,
                "value parameter pack must be the final function parameter"
            );
        }
        seen_value_pack = true;
    }
}

auto semantic_analyzer::record_function_parameter_defaults(std::size_t context_index, std::size_t unit_index, function_id id) -> void
{
    auto const& function = units[unit_index].ast.node(id);
    auto defaults = (
        function.parameters
        | std::views::transform([](parameter_syntax const& parameter) { return static_cast<bool>(parameter.default_value); })
        | std::ranges::to<std::vector<bool>>()
    );
    result.function_parameter_defaults[semantic_node_key{context_index, unit_index, id}] = std::move(defaults);
}

auto semantic_analyzer::validate_parameter_defaults(std::size_t unit_index, function_id id) -> void
{
    auto const& function = units[unit_index].ast.node(id);
    auto seen_default = false;
    for(auto const& parameter : function.parameters) {
        if(parameter.default_value) {
            seen_default = true;
            if(function.kind == function_syntax_kind::lambda) {
                report(diagnostic_kind::invalid_type_argument, parameter.full_span, "lambda parameter default values are not supported");
            }
            if(parameter.is_pack) {
                report(diagnostic_kind::invalid_type_argument, parameter.full_span, "parameter pack cannot have a default value");
            }
            if(parameter.is_self_receiver) {
                report(diagnostic_kind::invalid_self_parameter, parameter.full_span, "self receiver cannot have a default value");
            }
            continue;
        }
        if(seen_default) {
            report(diagnostic_kind::invalid_type_argument, parameter.full_span, "parameter without a default value cannot follow a defaulted parameter");
        }
    }
}

auto semantic_analyzer::required_parameter_count(std::size_t unit_index, function_id id, std::size_t parameter_offset) const -> std::size_t
{
    auto const& function = units[unit_index].ast.node(id);
    auto count = function.parameters.size();
    while(count > parameter_offset and function.parameters[count - 1uz].default_value) {
        --count;
    }
    return count - parameter_offset;
}

auto semantic_analyzer::has_valid_argument_count(std::size_t unit_index, function_id id, std::size_t provided, std::size_t available, std::size_t parameter_offset) const -> bool
{
    auto required = required_parameter_count(unit_index, id, parameter_offset);
    auto exposed = available >= parameter_offset ? available - parameter_offset : 0uz;
    return provided >= required and provided <= exposed;
}

auto semantic_analyzer::check_default_argument_expressions(
    std::size_t unit_index,
    function_id id,
    std::size_t context_index,
    function_signature const& signature,
    std::size_t provided,
    std::size_t parameter_offset,
    std::map<std::string, semantic_type_id> const* substitutions,
    std::map<std::string, std::vector<semantic_type_id>> const* pack_substitutions
) -> void
{
    auto const& function = units[unit_index].ast.node(id);
    auto old_context = active_context_index;
    auto old_unit_index = active_unit_index;
    auto old_unit = active_unit;
    auto old_ast = active_ast;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;

    active_context_index = context_index;
    active_unit_index = unit_index;
    active_unit = &units[unit_index];
    active_ast = &active_unit->ast;
    active_type_substitutions = substitutions;
    active_type_pack_substitutions = pack_substitutions;

    for(auto index = provided + parameter_offset; index < signature.parameters.size() and index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        if(parameter.default_value) {
            check_expression(*active_ast, *parameter.default_value, signature.parameters[index]);
        }
    }

    active_type_pack_substitutions = old_pack_substitutions;
    active_type_substitutions = old_substitutions;
    active_ast = old_ast;
    active_unit = old_unit;
    active_unit_index = old_unit_index;
    active_context_index = old_context;
}

auto semantic_analyzer::validate_function_type_arguments(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments, source_span span) -> bool
{
    auto const& function = units[unit_index].ast.node(id);
    auto names = function_generic_parameter_names(unit_index, id);
    auto pack_index = function_pack_generic_index(unit_index, id);
    auto valid_argument_count = pack_index
        ? type_arguments.size() >= *pack_index
        : names.size() == type_arguments.size();
    if(not valid_argument_count) {
        report(
            diagnostic_kind::invalid_type_argument,
            span,
            std::format(
                "generic function expects {} type argument(s)",
                names.size()
            )
        );
        return false;
    }

    auto diagnostic_count = diagnostics.size();
    auto implicit_count = function_implicit_generic_count(unit_index, id);
    auto const& ast = units[unit_index].ast;
    auto substitutions = function_substitution_map(unit_index, id, type_arguments);
    auto pack_substitutions = function_type_pack_substitution_map(unit_index, id, type_arguments);
    auto old_unit_index = active_unit_index;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;
    active_unit_index = unit_index;
    active_type_substitutions = &substitutions;
    active_type_pack_substitutions = &pack_substitutions;
    for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
        auto const& parameter = function.generic_parameters[index];
        auto argument_index = implicit_count + index;
        if(argument_index < type_arguments.size()) {
            auto const& argument_kind = result.types.get(type_arguments[argument_index]);
            if(parameter.parameter_kind == generic_parameter_syntax::kind::type) {
                if(std::holds_alternative<integer_constant_type>(argument_kind)) {
                    report(diagnostic_kind::invalid_type_argument, parameter.full_span, "type generic parameter requires a type argument");
                    continue;
                }
            } else if(not std::holds_alternative<integer_constant_type>(argument_kind)) {
                report(diagnostic_kind::invalid_type_argument, parameter.full_span, "integer generic parameter requires an integer argument");
                continue;
            }
        }
        for(auto bound : parameter.concept_bounds) {
            auto concept_name = ast_source.identifier(bound.name);
            auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
            if(not concept_symbol) {
                report(
                    diagnostic_kind::unknown_concept,
                    bound.name,
                    std::format("unknown concept '{}'", concept_name)
                );
                continue;
            }
            if(parameter.is_pack) {
                for(auto pack_argument_index = argument_index; pack_argument_index < type_arguments.size(); ++pack_argument_index) {
                    auto const& ast = units[unit_index].ast;
                    if(target_implements(unit_index, ast, bound, type_arguments[pack_argument_index])) {
                        continue;
                    }
                    report(
                        diagnostic_kind::missing_concept_item,
                        parameter.full_span,
                        std::format("type argument does not implement concept '{}'", concept_name)
                    );
                }
            } else if(argument_index < type_arguments.size() and target_implements(unit_index, units[unit_index].ast, bound, type_arguments[argument_index])) {
                continue;
            } else {
                report(
                    diagnostic_kind::missing_concept_item,
                    parameter.full_span,
                    std::format("type argument does not implement concept '{}'", concept_name)
                );
            }
        }
    }

    if(auto impl_requires = function_impl_requires.find(function_key(unit_index, id)); impl_requires != function_impl_requires.end()) {
        validate_requires_clause(unit_index, ast, impl_requires->second);
    }
    if(function.requires_clause) {
        validate_requires_clause(unit_index, ast, *function.requires_clause);
    }
    active_type_pack_substitutions = old_pack_substitutions;
    active_type_substitutions = old_substitutions;
    active_unit_index = old_unit_index;
    return diagnostics.size() == diagnostic_count;
}

auto semantic_analyzer::substitute_signature(function_signature const& signature, std::vector<semantic_type_id> const& type_arguments) -> function_signature
{
    auto parameters = (
        signature.parameters
        | std::views::transform([&](semantic_type_id parameter) {
            return substitute_type(parameter, type_arguments);
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
    return function_signature {
        .parameters = std::move(parameters),
        .returns = substitute_type(signature.returns, type_arguments),
        .inferred_return = false,
    };
}

auto semantic_analyzer::substitute_type_for_instance(semantic_type_id type, std::optional<std::size_t> pack_index, std::vector<semantic_type_id> const& type_arguments, std::optional<semantic_type_id> pack_element, source_span span) -> semantic_type_id
{
    auto const& kind = result.types.get(type);
    return std::visit (
        overloaded {
            [&](unit_type const&) { return type; },
            [&](error_type const&) { return type; },
            [&](inferred_type const&) { return type; },
            [&](never_type const&) { return type; },
            [&](builtin_type const&) { return type; },
            [&](generic_parameter_type const& value) {
                if(pack_index and value.index == *pack_index) {
                    if(pack_element) {
                        return *pack_element;
                    }
                    report(diagnostic_kind::invalid_type_argument, span, "type parameter pack must be expanded with '...'");
                    return semantic_type_ids::error;
                }
                if(value.index < type_arguments.size()) {
                    return type_arguments[value.index];
                }
                return semantic_type_ids::error;
            },
            [&](integer_constant_type const&) { return type; },
            [&](generic_integer_parameter_type const& value) {
                if(value.index < type_arguments.size()) {
                    return type_arguments[value.index];
                }
                return semantic_type_ids::error;
            },
            [&](array_type const& value) {
                return result.types.intern(array_type {
                    .element = substitute_type_for_instance(value.element, pack_index, type_arguments, pack_element, span),
                    .length = substitute_type_for_instance(value.length, pack_index, type_arguments, pack_element, span),
                });
            },
            [&](tuple_type const& value) {
                auto elements = (
                    value.elements
                    | std::views::transform([&](auto element) {
                        return substitute_type_for_instance(element, pack_index, type_arguments, pack_element, span);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(tuple_type{ std::move(elements) });
            },
            [&](reference_type const& value) {
                auto pointee = substitute_type_for_instance(value.pointee, pack_index, type_arguments, pack_element, span);
                if(auto const* inner = std::get_if<reference_type>(&result.types.get(pointee))) {
                    return result.types.intern(reference_type {
                        inner->pointee,
                        value.is_const or inner->is_const,
                        value.reference_kind,
                    });
                }
                return result.types.intern(reference_type {
                    pointee,
                    value.is_const,
                    value.reference_kind,
                });
            },
            [&](pointer_type const& value) {
                return result.types.intern(pointer_type {
                    substitute_type_for_instance(value.pointee, pack_index, type_arguments, pack_element, span),
                    value.is_const,
                    value.is_like,
                });
            },
            [&](function_type const& value) {
                auto parameters = (
                    value.parameters
                    | std::views::transform([&](auto parameter) {
                        return substitute_type_for_instance(parameter, pack_index, type_arguments, pack_element, span);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(function_type {
                    .parameters = std::move(parameters),
                    .returns = substitute_type_for_instance(value.returns, pack_index, type_arguments, pack_element, span),
                });
            },
            [&](struct_type const& value) {
                auto arguments = (
                    value.arguments
                    | std::views::transform([&](auto argument) {
                        return substitute_type_for_instance(argument, pack_index, type_arguments, pack_element, span);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(struct_type {
                    .index = value.index,
                    .arguments = std::move(arguments),
                });
            },
            [&](enum_type const&) { return type; },
            [&](opaque_type const&) { return type; },
            [&](variant_type const& value) {
                auto arguments = (
                    value.arguments
                    | std::views::transform([&](auto argument) {
                        return substitute_type_for_instance(argument, pack_index, type_arguments, pack_element, span);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(variant_type {
                    .index = value.index,
                    .arguments = std::move(arguments),
                });
            },
        },
        kind
    );
}

auto semantic_analyzer::substitute_signature_for_instance(std::size_t unit_index, function_id id, function_signature const& signature, std::vector<semantic_type_id> const& type_arguments, source_span span) -> function_signature
{
    auto pack_index = function_pack_generic_index(unit_index, id);
    if(not pack_index) {
        return substitute_signature(signature, type_arguments);
    }

    auto const& function = units[unit_index].ast.node(id);
    auto parameters = std::vector<semantic_type_id>{};
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto parameter_type = signature.parameters[index];
        if(not function.parameters[index].is_pack) {
            parameters.emplace_back(
                substitute_type_for_instance(parameter_type, pack_index, type_arguments, std::nullopt, function.parameters[index].full_span)
            );
            continue;
        }

        for(auto argument_index = *pack_index; argument_index < type_arguments.size(); ++argument_index) {
            parameters.emplace_back(
                substitute_type_for_instance(
                    parameter_type,
                    pack_index,
                    type_arguments,
                    type_arguments[argument_index],
                    function.parameters[index].full_span
                )
            );
        }
    }

    return function_signature {
        .parameters = std::move(parameters),
        .returns = substitute_type_for_instance(signature.returns, pack_index, type_arguments, std::nullopt, span),
        .inferred_return = false,
    };
}

auto semantic_analyzer::instantiate_function(std::size_t unit_index, function_id id, std::vector<semantic_type_id> type_arguments, source_span span) -> semantic_function_instance const*
{
    auto key = semantic_function_instance_key{ unit_index, id, type_arguments };
    if(auto found = result.function_instance_indices.find(key); found != result.function_instance_indices.end()) {
        return &result.function_instances[found->second];
    }

    auto const& ast = units[unit_index].ast;
    auto const& function = ast.node(id);
    if(not validate_function_type_arguments(unit_index, id, type_arguments, span)) {
        return nullptr;
    }

    auto source_signature_id = result.signature_of(unit_index, id);
    auto source_symbol = result.function_symbol_of(unit_index, id);
    if(not source_signature_id.valid() or not source_symbol.valid()) {
        return nullptr;
    }

    auto const& source_signature = result.signatures[source_signature_id.value];
    if(source_signature.inferred_return) {
        report(
            diagnostic_kind::cannot_infer_return_type,
            function.name,
            "generic function return type must be explicit for monomorphization"
        );
        return nullptr;
    }

    auto signature = substitute_signature_for_instance(unit_index, id, source_signature, type_arguments, span);
    auto signature_id = add_signature(signature);
    auto const& source = result.symbols[source_symbol.value];
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
        .body_kind = source.body_kind,
        .unit_index = unit_index,
        .function = id,
        .struct_index = source.struct_index,
        .variant_index = source.variant_index,
        .concept_index = source.concept_index,
        .function_kind = source.function_kind,
    });

    result.function_signatures.emplace(semantic_node_key{context_index, unit_index, id}, signature_id);
    result.function_symbols.emplace(semantic_node_key{context_index, unit_index, id}, instance_symbol);
    record_function_parameter_defaults(context_index, unit_index, id);
    result.function_instances.emplace_back(
        key,
        context_index,
        instance_symbol,
        signature_id,
        function_substitution_map(unit_index, id, type_arguments),
        function_type_pack_substitution_map(unit_index, id, type_arguments)
    );
    result.function_instance_indices.emplace(key, instance_index);
    result.function_instance_by_symbol.emplace(instance_symbol, instance_index);

    check_function_instance(instance_index);
    return &result.function_instances[instance_index];
}

auto semantic_analyzer::infer_function_type_arguments_for_call(symbol_id symbol, std::optional<semantic_type_id> receiver_type, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> const& explicit_arguments, source_span span) -> std::optional<std::vector<semantic_type_id>>
{
    auto const& value = result.symbols[symbol.value];
    auto names = function_generic_parameter_names(value.unit_index, value.function);
    if(names.empty()) {
        return std::vector<semantic_type_id>{};
    }

    auto pack_index = function_pack_generic_index(value.unit_index, value.function);
    auto type_arguments = std::vector<semantic_type_id>(
        pack_index ? *pack_index : names.size(),
        semantic_type_id{}
    );
    auto inferred = std::map<std::uint32_t, semantic_type_id>{};
    if(receiver_type) {
        auto pattern = function_impl_target_pattern(value.unit_index, value.function);
        if(pattern.valid() and not infer_type_argument(pattern, *receiver_type, inferred)) {
            report(diagnostic_kind::invalid_type_argument, span, "receiver type does not match generic impl target");
            return std::nullopt;
        }
    }

    auto const* signature = function_signature_for_symbol(symbol);
    if(signature != nullptr) {
        auto value_pack_parameter = function_value_pack_parameter_index(value.unit_index, value.function);
        auto fixed_parameter_count = value_pack_parameter.value_or(signature->parameters.size());
        if(
            (not value_pack_parameter and not has_valid_argument_count(value.unit_index, value.function, arguments.size(), signature->parameters.size()))
            or (value_pack_parameter and arguments.size() < fixed_parameter_count)
        ) {
            report(diagnostic_kind::argument_count_mismatch, span, "generic call argument count does not match function signature");
            return std::nullopt;
        }

        for(auto index = 0uz; index < std::min(fixed_parameter_count, arguments.size()); ++index) {
            if(not is_dependent_type(signature->parameters[index])) {
                continue;
            }
            auto parameter = signature->parameters[index];
            auto argument_type = arguments[index].type;
            if(std::holds_alternative<generic_parameter_type>(result.types.get(parameter))) {
                argument_type = read_type(argument_type);
            }
            if(not infer_type_argument(parameter, argument_type, inferred)) {
                report(diagnostic_kind::invalid_type_argument, span, "cannot infer generic function type arguments");
                return std::nullopt;
            }
        }
        if(value_pack_parameter) {
            if(not pack_index) {
                report(diagnostic_kind::invalid_type_argument, span, "value parameter pack requires a type parameter pack");
                return std::nullopt;
            }
            auto pack_pattern = signature->parameters[*value_pack_parameter];
            for(auto index = fixed_parameter_count; index < arguments.size(); ++index) {
                auto pack_element = std::optional<semantic_type_id>{};
                auto argument_type = arguments[index].type;
                if(std::holds_alternative<generic_parameter_type>(result.types.get(pack_pattern))) {
                    argument_type = read_type(argument_type);
                }
                if(not infer_type_argument_with_pack(pack_pattern, argument_type, pack_index, inferred, pack_element)) {
                    report(diagnostic_kind::invalid_type_argument, span, "cannot infer generic function parameter pack");
                    return std::nullopt;
                }
                type_arguments.emplace_back(pack_element.value_or(read_type(arguments[index].type)));
            }
        }
    }

    auto implicit_count = function_implicit_generic_count(value.unit_index, value.function);
    auto explicit_count = function_explicit_generic_count(value.unit_index, value.function);
    auto const& function = units[value.unit_index].ast.node(value.function);
    if(not explicit_arguments.empty()) {
        auto explicit_pack_index = pack_index and *pack_index >= implicit_count
            ? std::optional<std::size_t>{ *pack_index - implicit_count }
            : std::nullopt;
        auto minimum_explicit_count = explicit_count;
        for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
            if(function.generic_parameters[index].default_argument) {
                minimum_explicit_count = index;
                break;
            }
        }
        auto valid_explicit_count = explicit_pack_index
            ? explicit_arguments.size() >= *explicit_pack_index
            : explicit_arguments.size() >= minimum_explicit_count and explicit_arguments.size() <= explicit_count;
        if(not valid_explicit_count) {
            report(
                diagnostic_kind::invalid_type_argument,
                span,
                std::format("generic call expects {} explicit type argument(s)", explicit_count)
            );
            return std::nullopt;
        }
        if(explicit_pack_index) {
            type_arguments.resize(implicit_count + explicit_arguments.size());
        }
        for(auto index = 0uz; index < explicit_arguments.size(); ++index) {
            auto argument_index = implicit_count + index;
            if(argument_index >= type_arguments.size()) {
                type_arguments.resize(argument_index + 1uz);
            }
            type_arguments[argument_index] = explicit_arguments[index];
        }
    }

    for(auto const& [index, type] : inferred) {
        if(index >= type_arguments.size()) {
            continue;
        }
        if(type_arguments[index].valid() and type_arguments[index] != type) {
            report(diagnostic_kind::invalid_type_argument, span, "explicit type argument conflicts with inferred type");
            return std::nullopt;
        }
        type_arguments[index] = type;
    }

    for(auto index = implicit_count; index < type_arguments.size() and index - implicit_count < function.generic_parameters.size(); ++index) {
        if(type_arguments[index].valid()) {
            continue;
        }
        auto const& parameter = function.generic_parameters[index - implicit_count];
        if(not parameter.default_argument) {
            continue;
        }

        auto substitutions = std::map<std::string, semantic_type_id>{};
        for(auto substitution_index = 0uz; substitution_index < index and substitution_index < names.size(); ++substitution_index) {
            if(type_arguments[substitution_index].valid()) {
                substitutions.emplace(names[substitution_index], type_arguments[substitution_index]);
            }
        }
        auto old_unit_index = active_unit_index;
        auto old_substitutions = active_type_substitutions;
        active_unit_index = value.unit_index;
        active_type_substitutions = &substitutions;
        type_arguments[index] = lower_generic_type_argument(
            units[value.unit_index].ast,
            *parameter.default_argument,
            parameter.parameter_kind,
            span
        );
        active_type_substitutions = old_substitutions;
        active_unit_index = old_unit_index;
    }

    auto fixed_argument_count = pack_index ? *pack_index : type_arguments.size();
    for(auto index = 0uz; index < fixed_argument_count; ++index) {
        if(type_arguments[index].valid()) {
            continue;
        }
        report(diagnostic_kind::invalid_type_argument, span, "cannot infer generic function type arguments");
        return std::nullopt;
    }
    return type_arguments;
}

auto semantic_analyzer::instantiate_function_symbol(symbol_id symbol, std::optional<semantic_type_id> receiver_type, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> explicit_arguments, source_span span) -> semantic_function_instance const*
{
    if(not symbol.valid()) {
        return nullptr;
    }
    auto const& value = result.symbols[symbol.value];
    if(not function_is_generic(value.unit_index, value.function)) {
        return nullptr;
    }

    auto type_arguments = infer_function_type_arguments_for_call(
        symbol,
        receiver_type,
        arguments,
        explicit_arguments,
        span
    );
    if(not type_arguments) {
        return nullptr;
    }
    return instantiate_function(value.unit_index, value.function, *type_arguments, span);
}

auto semantic_analyzer::concrete_destructor_symbol(semantic_type_id type, source_span span) -> symbol_id
{
    auto owner = struct_index_of(type);
    if(not owner) {
        return {};
    }

    auto symbol = result.structs[*owner].destructor;
    if(not symbol.valid()) {
        return {};
    }

    auto const& value = result.symbols[symbol.value];
    if(not function_is_generic(value.unit_index, value.function)) {
        return symbol;
    }

    auto self_type = result.types.intern(reference_type{ read_type(type) });
    auto arguments = std::vector<expression_info>{
        expression_info {
            .type = self_type,
            .is_lvalue = true,
        },
    };
    auto instance = instantiate_function_symbol(symbol, read_type(type), arguments, {}, span);
    return instance == nullptr ? symbol : instance->symbol;
}

auto semantic_analyzer::check_generic_function_call(ast_arena const& ast, call_expr_syntax const& node, expr_id id)
    -> std::optional<expression_info>
{
    auto const& callee_node = ast.node(node.callee);
    auto const* callee_name = std::get_if<name_expr_syntax>(&callee_node);
    if(callee_name == nullptr) {
        return std::nullopt;
    }

    auto template_symbol = generic_function_symbol(*callee_name);
    if(not template_symbol) {
        return std::nullopt;
    }

    auto const& symbol = result.symbols[template_symbol->value];
    auto source_unit = symbol.unit_index;
    auto source_function = symbol.function;
    auto signature_id = result.signature_of(source_unit, source_function);
    if(not signature_id.valid()) {
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto const& template_signature = result.signatures[signature_id.value];
    static_cast<void>(template_signature);

    if(not node.type_arguments.empty()) {
        auto type_arguments = explicit_type_arguments(ast, node);
        if(not type_arguments) {
            return expression_info{ .type = semantic_type_ids::error };
        }

        if(function_pack_generic_index(source_unit, source_function)) {
            auto arguments = std::vector<expression_info>{};
            arguments.reserve(node.arguments.size());
            for(auto argument : node.arguments) {
                arguments.emplace_back(check_expression(ast, argument, std::nullopt));
            }
            auto const* instance = instantiate_function_symbol(
                *template_symbol,
                std::nullopt,
                arguments,
                std::move(*type_arguments),
                node.full_span
            );
            if(instance == nullptr) {
                return expression_info{ .type = semantic_type_ids::error };
            }

            auto signature = result.signatures[instance->signature.value];
            auto valid_count = (
                function_value_pack_parameter_index(source_unit, source_function)
                    ? signature.parameters.size() == node.arguments.size()
                    : has_valid_argument_count(source_unit, source_function, node.arguments.size(), signature.parameters.size())
            );
            if(not valid_count) {
                report(
                    diagnostic_kind::argument_count_mismatch,
                    node.full_span,
                    "generic call argument count does not match function signature"
                );
                return expression_info{ .type = semantic_type_ids::error };
            }
            check_default_argument_expressions(
                source_unit,
                source_function,
                instance->context_index,
                signature,
                node.arguments.size(),
                0uz,
                &instance->substitutions,
                &instance->pack_substitutions
            );

            for(auto index = 0uz; index < node.arguments.size(); ++index) {
                if(not can_implicitly_convert(arguments[index], signature.parameters[index])) {
                    report(
                        diagnostic_kind::type_mismatch,
                        ast.span(node.arguments[index]),
                        "call argument does not match generic function instance"
                    );
                    continue;
                }
                if(read_type(arguments[index].type) != read_type(signature.parameters[index])) {
                    result.expression_conversions[node_key(node.arguments[index])] = signature.parameters[index];
                }
            }

            result.expression_symbols[node_key(node.callee)] = instance->symbol;
            record_expression(
                node.callee,
                active_unit_index,
                semantic_expression_info {
                    .type = result.symbols[instance->symbol.value].type,
                    .read_type = result.symbols[instance->symbol.value].type,
                    .is_lvalue = false,
                    .is_const = false,
                }
            );
            return expression_for_return_type(signature.returns);
        }

        auto explicit_arguments = std::move(*type_arguments);
        auto const& function = units[source_unit].ast.node(source_function);
        auto minimum_explicit_count = function.generic_parameters.size();
        for(auto index = 0uz; index < function.generic_parameters.size(); ++index) {
            if(function.generic_parameters[index].default_argument) {
                minimum_explicit_count = index;
                break;
            }
        }
        if(explicit_arguments.size() < minimum_explicit_count or explicit_arguments.size() > function.generic_parameters.size()) {
            report(
                diagnostic_kind::invalid_type_argument,
                node.full_span,
                std::format("generic call expects {} explicit type argument(s)", function.generic_parameters.size())
            );
            return expression_info{ .type = semantic_type_ids::error };
        }
        for(auto index = explicit_arguments.size(); index < function.generic_parameters.size(); ++index) {
            auto const& parameter = function.generic_parameters[index];
            if(not parameter.default_argument) {
                report(diagnostic_kind::invalid_type_argument, node.full_span, "cannot infer generic function type arguments");
                return expression_info{ .type = semantic_type_ids::error };
            }
            auto substitutions = std::map<std::string, semantic_type_id>{};
            for(auto substitution_index = 0uz; substitution_index < index; ++substitution_index) {
                substitutions.emplace(
                    std::string{ ast_source.identifier(function.generic_parameters[substitution_index].name) },
                    explicit_arguments[substitution_index]
                );
            }
            auto old_unit_index = active_unit_index;
            auto old_substitutions = active_type_substitutions;
            active_unit_index = source_unit;
            active_type_substitutions = &substitutions;
            explicit_arguments.emplace_back(lower_generic_type_argument(
                units[source_unit].ast,
                *parameter.default_argument,
                parameter.parameter_kind,
                node.full_span
            ));
            active_type_substitutions = old_substitutions;
            active_unit_index = old_unit_index;
        }

        auto const* instance = instantiate_function(source_unit, source_function, std::move(explicit_arguments), node.full_span);
        if(instance == nullptr) {
            return expression_info{ .type = semantic_type_ids::error };
        }

        auto signature = result.signatures[instance->signature.value];
        auto valid_count = (
            function_value_pack_parameter_index(source_unit, source_function)
                ? signature.parameters.size() == node.arguments.size()
                : has_valid_argument_count(source_unit, source_function, node.arguments.size(), signature.parameters.size())
        );
        if(not valid_count) {
            report(
                diagnostic_kind::argument_count_mismatch,
                node.full_span,
                "generic call argument count does not match function signature"
            );
            return expression_info{ .type = semantic_type_ids::error };
        }
        check_default_argument_expressions(
            source_unit,
            source_function,
            instance->context_index,
            signature,
            node.arguments.size(),
            0uz,
            &instance->substitutions,
            &instance->pack_substitutions
        );

        for(auto index = 0uz; index < node.arguments.size(); ++index) {
            auto argument = check_expression(ast, node.arguments[index], signature.parameters[index]);
            if(not can_implicitly_convert(argument, signature.parameters[index])) {
                report(
                    diagnostic_kind::type_mismatch,
                    ast.span(node.arguments[index]),
                    "call argument does not match generic function instance"
                );
            }
        }

        result.expression_symbols[node_key(node.callee)] = instance->symbol;
        record_expression(
            node.callee,
            active_unit_index,
            semantic_expression_info {
                .type = result.symbols[instance->symbol.value].type,
                .read_type = result.symbols[instance->symbol.value].type,
                .is_lvalue = false,
                .is_const = false,
            }
        );
        return expression_for_return_type(signature.returns);
    }

    auto arguments = std::vector<expression_info>{};
    arguments.reserve(node.arguments.size());
    for(auto argument : node.arguments) {
        arguments.emplace_back(check_expression(ast, argument, std::nullopt));
    }

    auto explicit_arguments = std::vector<semantic_type_id>{};
    auto const* instance = instantiate_function_symbol(
        *template_symbol,
        std::nullopt,
        arguments,
        std::move(explicit_arguments),
        node.full_span
    );
    if(instance == nullptr) {
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto signature = result.signatures[instance->signature.value];
    auto valid_count = (
        function_value_pack_parameter_index(source_unit, source_function)
            ? signature.parameters.size() == node.arguments.size()
            : has_valid_argument_count(source_unit, source_function, node.arguments.size(), signature.parameters.size())
    );
    if(not valid_count) {
        report(
            diagnostic_kind::argument_count_mismatch,
            node.full_span,
            "generic call argument count does not match function signature"
        );
        return expression_info{ .type = semantic_type_ids::error };
    }
    check_default_argument_expressions(
        source_unit,
        source_function,
        instance->context_index,
        signature,
        node.arguments.size(),
        0uz,
        &instance->substitutions,
        &instance->pack_substitutions
    );

    for(auto index = 0uz; index < node.arguments.size(); ++index) {
        if(not can_implicitly_convert(arguments[index], signature.parameters[index])) {
            report(
                diagnostic_kind::type_mismatch,
                ast.span(node.arguments[index]),
                "call argument does not match generic function instance"
            );
            continue;
        }
        if(read_type(arguments[index].type) != read_type(signature.parameters[index])) {
            result.expression_conversions[node_key(node.arguments[index])] = signature.parameters[index];
        }
    }

    result.expression_symbols[node_key(node.callee)] = instance->symbol;
    record_expression(
        node.callee,
        active_unit_index,
        semantic_expression_info {
            .type = result.symbols[instance->symbol.value].type,
            .read_type = result.symbols[instance->symbol.value].type,
            .is_lvalue = false,
            .is_const = false,
        }
    );
    return expression_for_return_type(signature.returns);
}
