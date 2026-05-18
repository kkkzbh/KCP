module semantic:types;

import semantic;

auto semantic_analyzer::lower_type(ast_arena const& ast, type_id id) -> semantic_type_id
{
    auto const& syntax = ast.node(id);
    if(syntax.is_function_type) {
        auto parameters = (
            syntax.function_parameters
            | std::views::transform([&](function_type_parameter_syntax const& parameter) {
                return lower_type(ast, parameter.type);
            }) | std::ranges::to<std::vector<semantic_type_id>>()
        );
        auto function = result.types.intern(function_type {
            .parameters = std::move(parameters),
            .returns = lower_type(ast, syntax.function_return),
        });
        return syntax.is_function_pointer
            ? result.types.intern(pointer_type{ function })
            : function;
    }

    if(syntax.is_decltype) {
        auto info = expression_info{};
        if(active_ast == &ast) {
            info = check_expression(ast, syntax.decltype_expression, std::nullopt);
        } else {
            info = infer_expression_type(ast, syntax.decltype_expression, std::nullopt);
        }
        return read_type(info.type);
    }

    auto name = ast_source.identifier(syntax.name);
    auto lowered = semantic_type_id{};
    if(name == "Self" and active_self_type.valid()) {
        lowered = active_self_type;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "Self does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(active_type_substitutions and active_type_substitutions->contains(std::string{ name })) {
        lowered = active_type_substitutions->find(std::string{ name })->second;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "substituted type does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(active_generic_parameters.contains(std::string{ name })) {
        lowered = active_generic_parameters.find(std::string{ name })->second;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "generic parameter does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(auto alias = resolve_type_alias(name)) {
        lowered = *alias;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "type alias does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(active_type_aliases and active_type_aliases->contains(std::string{ name })) {
        lowered = active_type_aliases->find(std::string{ name })->second;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "associated type alias does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(auto builtin = is_builtin_name(name)) {
        lowered = semantic_type_ids::builtin(*builtin);
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "builtin type does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(name == "array") {
        lowered = lower_array_type(ast, syntax);
    } else if(name == "tuple") {
        lowered = lower_tuple_type(ast, syntax);
    } else if(auto symbol = resolve_type_symbol(active_unit_index, name)) {
        auto const& value = result.symbols[symbol->value];
        if(value.variant_index < result.variants.size() and result.variants[value.variant_index].symbol == *symbol) {
            auto const& variant = result.variants[value.variant_index];
            if(syntax.arguments.size() != variant.generic_parameters.size()) {
                report (
                    diagnostic_kind::invalid_type_argument,
                    syntax.full_span,
                    std::format(
                        "variant type '{}' expects {} type argument(s)",
                        variant.name,
                        variant.generic_parameters.size()
                    )
                );
                lowered = semantic_type_ids::error;
            } else {
                auto arguments = (
                    syntax.arguments
                    | std::views::transform([&](type_argument_syntax const& argument) {
                        if(not std::holds_alternative<type_argument_type_syntax>(argument)) {
                            report(diagnostic_kind::invalid_type_argument, syntax.full_span, "variant type arguments must be types");
                            return semantic_type_ids::error;
                        }
                        return lower_type(ast, std::get<type_argument_type_syntax>(argument).type);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                lowered = result.types.intern(variant_type {
                    .index = value.variant_index,
                    .arguments = std::move(arguments),
                });
            }
        } else {
            auto const& item = result.structs[value.struct_index];
            if(syntax.arguments.size() != item.generic_parameters.size()) {
                report (
                    diagnostic_kind::invalid_type_argument,
                    syntax.full_span,
                    std::format(
                        "struct type '{}' expects {} type argument(s)",
                        item.name,
                        item.generic_parameters.size()
                    )
                );
                lowered = semantic_type_ids::error;
            } else if(syntax.arguments.empty()) {
                lowered = value.type;
            } else {
                auto arguments = (
                    syntax.arguments
                    | std::views::transform([&](type_argument_syntax const& argument) {
                        if(not std::holds_alternative<type_argument_type_syntax>(argument)) {
                            report(diagnostic_kind::invalid_type_argument, syntax.full_span, "struct type arguments must be types");
                            return semantic_type_ids::error;
                        }
                        return lower_type(ast, std::get<type_argument_type_syntax>(argument).type);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                lowered = result.types.intern(struct_type {
                    .index = value.struct_index,
                    .arguments = std::move(arguments),
                });
            }
        }
    } else {
        report (
            diagnostic_kind::unknown_type,
            syntax.full_span,
            std::format("unknown type '{}'", name)
        );
        lowered = semantic_type_ids::error;
    }

    for(auto associated : syntax.associated_names) {
        auto key = std::string{ ast_source.identifier(associated) };
        auto found_types = associated_types.find(read_type(lowered));
        if(found_types == associated_types.end()) {
            report(
                diagnostic_kind::unknown_type,
                associated,
                std::format("unknown associated type '{}'", key)
            );
            lowered = semantic_type_ids::error;
            break;
        }
        auto found = found_types->second.find(key);
        if(found == found_types->second.end()) {
            report(
                diagnostic_kind::unknown_type,
                associated,
                std::format("unknown associated type '{}'", key)
            );
            lowered = semantic_type_ids::error;
            break;
        }
        lowered = found->second;
    }

    for(auto suffix : syntax.suffix_operators) {
        if(suffix == token_kind::star) {
            lowered = result.types.intern(pointer_type{ lowered, syntax.is_const });
        } else if(suffix == token_kind::amp) {
            lowered = result.types.intern(reference_type{ lowered, syntax.is_const });
        }
    }
    return lowered;
}

auto semantic_analyzer::lower_type_with_substitutions(
    ast_arena const& ast,
    type_id id,
    std::map<std::string, semantic_type_id> const& substitutions
) -> semantic_type_id
{
    auto old_substitutions = active_type_substitutions;
    active_type_substitutions = &substitutions;
    auto result_type = lower_type(ast, id);
    active_type_substitutions = old_substitutions;
    return result_type;
}

auto semantic_analyzer::resolve_type_symbol(std::size_t unit_index, std::string_view name) const
    -> std::optional<symbol_id>
{
    auto key = std::string{ name };
    auto const& unit = units[unit_index];
    if(auto local = module_types.find(unit.module_key); local != module_types.end()) {
        if(auto found = local->second.find(key); found != local->second.end()) {
            return found->second;
        }
    }

    for(auto const& import : unit.root.imports) {
        auto module_name = ast_source.module_name(import.name);
        auto visiting = std::set<std::string>{};
        if(auto found = resolve_exported_type_symbol(module_name, key, visiting)) {
            return *found;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::resolve_concept_symbol(std::size_t unit_index, std::string_view name) const
    -> std::optional<symbol_id>
{
    auto key = std::string{ name };
    auto const& unit = units[unit_index];
    if(auto local = module_concepts.find(unit.module_key); local != module_concepts.end()) {
        if(auto found = local->second.find(key); found != local->second.end()) {
            return found->second;
        }
    }

    for(auto const& import : unit.root.imports) {
        auto module_name = ast_source.module_name(import.name);
        auto visiting = std::set<std::string>{};
        if(auto found = resolve_exported_concept_symbol(module_name, key, visiting)) {
            return *found;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::resolve_exported_type_symbol(
    std::string_view module_name,
    std::string_view name,
    std::set<std::string>& visiting
) const -> std::optional<symbol_id>
{
    auto module_key = std::string{ module_name };
    if(not visiting.emplace(module_key).second) {
        return std::nullopt;
    }

    if(auto exports = module_type_exports.find(module_key); exports != module_type_exports.end()) {
        if(auto found = exports->second.find(std::string{ name }); found != exports->second.end()) {
            return found->second;
        }
    }

    for(auto const& unit : units) {
        if(not unit.named_module or unit.module_name != module_key) {
            continue;
        }
        for(auto const& import : unit.root.imports) {
            if(not import.exported) {
                continue;
            }
            auto imported_module = ast_source.module_name(import.name);
            if(auto found = resolve_exported_type_symbol(imported_module, name, visiting)) {
                return *found;
            }
        }
    }

    return std::nullopt;
}

auto semantic_analyzer::resolve_exported_concept_symbol(
    std::string_view module_name,
    std::string_view name,
    std::set<std::string>& visiting
) const -> std::optional<symbol_id>
{
    auto module_key = std::string{ module_name };
    if(not visiting.emplace(module_key).second) {
        return std::nullopt;
    }

    if(auto exports = module_concept_exports.find(module_key); exports != module_concept_exports.end()) {
        if(auto found = exports->second.find(std::string{ name }); found != exports->second.end()) {
            return found->second;
        }
    }

    for(auto const& unit : units) {
        if(not unit.named_module or unit.module_name != module_key) {
            continue;
        }
        for(auto const& import : unit.root.imports) {
            if(not import.exported) {
                continue;
            }
            auto imported_module = ast_source.module_name(import.name);
            if(auto found = resolve_exported_concept_symbol(imported_module, name, visiting)) {
                return *found;
            }
        }
    }

    return std::nullopt;
}

auto semantic_analyzer::struct_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* value = std::get_if<struct_type>(&kind)) {
        return value->index;
    }
    return std::nullopt;
}

auto semantic_analyzer::struct_named(std::size_t unit_index, std::string_view name) const
    -> std::optional<std::uint32_t>
{
    auto symbol = resolve_type_symbol(unit_index, name);
    if(not symbol) {
        return std::nullopt;
    }
    auto const& value = result.symbols[symbol->value];
    if(value.kind != symbol_kind::type) {
        return std::nullopt;
    }
    return value.struct_index;
}

auto semantic_analyzer::field_index(std::uint32_t struct_index, std::string_view name) const
    -> std::optional<std::uint32_t>
{
    auto const& item = result.structs[struct_index];
    for(auto index = 0uz; index < item.fields.size(); ++index) {
        if(item.fields[index].name == name) {
            return static_cast<std::uint32_t>(index);
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::is_default_initializable(semantic_type_id type) -> bool
{
    auto const& kind = result.types.get(type);
    return std::visit (
        overloaded {
            [](unit_type const&) { return true; },
            [](error_type const&) { return true; },
            [](inferred_type const&) { return false; },
            [](builtin_type const&) { return true; },
            [&](array_type const& value) { return is_default_initializable(value.element); },
            [&](tuple_type const& value) {
                return std::ranges::all_of(value.elements, [&](auto element) {
                    return is_default_initializable(element);
                });
            },
            [](reference_type const&) { return false; },
            [](pointer_type const&) { return true; },
            [](function_type const&) { return false; },
            [](generic_parameter_type const&) { return false; },
            [&](struct_type const& value) {
                auto const& item = result.structs[value.index];
                return std::ranges::all_of(item.fields, [&](auto const& field) {
                    return is_default_initializable(substitute_type(field.type, value.arguments));
                });
            },
            [](variant_type const&) { return false; },
        },
        kind
    );
}

auto semantic_analyzer::is_dependent_type(semantic_type_id type) const -> bool
{
    auto const& kind = result.types.get(read_type(type));
    return std::visit (
        overloaded {
            [](unit_type const&) { return false; },
            [](error_type const&) { return false; },
            [](inferred_type const&) { return false; },
            [](builtin_type const&) { return false; },
            [&](array_type const& value) { return is_dependent_type(value.element); },
            [&](tuple_type const& value) {
                return std::ranges::any_of(value.elements, [&](auto element) {
                    return is_dependent_type(element);
                });
            },
            [&](reference_type const& value) { return is_dependent_type(value.pointee); },
            [&](pointer_type const& value) { return is_dependent_type(value.pointee); },
            [&](function_type const& value) {
                return is_dependent_type(value.returns)
                    or std::ranges::any_of(value.parameters, [&](auto parameter) {
                        return is_dependent_type(parameter);
                    });
            },
            [](generic_parameter_type const&) { return true; },
            [&](struct_type const& value) {
                return std::ranges::any_of(value.arguments, [&](auto argument) {
                    return is_dependent_type(argument);
                });
            },
            [&](variant_type const& value) {
                return std::ranges::any_of(value.arguments, [&](auto argument) {
                    return is_dependent_type(argument);
                });
            },
        },
        kind
    );
}

auto semantic_analyzer::range_element_type(semantic_type_id type) -> semantic_type_id
{
    auto value = read_type(type);
    auto const& kind = result.types.get(value);
    if(auto const* array = std::get_if<array_type>(&kind)) {
        return array->element;
    }
    return {};
}

auto semantic_analyzer::pointer_pointee(semantic_type_id type) -> std::optional<expression_info>
{
    auto const& kind = result.types.get(type);
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return expression_info {
            .type = pointer->pointee,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(pointer->pointee, pointer->is_const),
        };
    }
    if(auto const* reference = std::get_if<reference_type>(&kind)) {
        return expression_info {
            .type = reference->pointee,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(reference->pointee, reference->is_const),
        };
    }
    return std::nullopt;
}

auto semantic_analyzer::callable_type(semantic_type_id type) const -> function_type const*
{
    auto const& kind = result.types.get(type);
    if(auto const* callable = std::get_if<function_type>(&kind)) {
        return callable;
    }
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return std::get_if<function_type>(&result.types.get(pointer->pointee));
    }
    return nullptr;
}

auto semantic_analyzer::pointer_value_pointee(semantic_type_id type) const -> std::optional<semantic_type_id>
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return pointer->pointee;
    }
    return std::nullopt;
}

auto semantic_analyzer::target_const(semantic_type_id type, bool lvalue_const) -> bool
{
    auto const& kind = result.types.get(type);
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return pointer->is_const;
    }
    if(auto const* reference = std::get_if<reference_type>(&kind)) {
        return reference->is_const;
    }
    return lvalue_const;
}

auto semantic_analyzer::read_type(semantic_type_id type) const -> semantic_type_id
{
    auto current = type;
    while(true) {
        auto const& kind = result.types.get(current);
        if(auto const* reference = std::get_if<reference_type>(&kind)) {
            current = reference->pointee;
            continue;
        }
        return current;
    }
}

auto semantic_analyzer::can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool
{
    if(auto const* reference = std::get_if<reference_type>(&result.types.get(to))) {
        return from.is_lvalue and read_type(from.type) == read_type(reference->pointee);
    }
    return can_implicitly_convert(from.type, to);
}

auto semantic_analyzer::can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
{
    if(from == to or is_error(from) or is_error(to)) {
        return true;
    }

    if(auto const* target_pointer = std::get_if<pointer_type>(&result.types.get(to))) {
        if(std::holds_alternative<function_type>(result.types.get(target_pointer->pointee))) {
            return from == target_pointer->pointee;
        }
    }

    auto source = read_type(from);
    auto target = read_type(to);
    if(source == target) {
        return true;
    }

    auto source_builtin = as_builtin(source);
    auto target_builtin = as_builtin(target);
    if(not source_builtin or not target_builtin) {
        return false;
    }

    if(is_integer(*source_builtin) and is_integer(*target_builtin)) {
        return true;
    }
    if(is_float(*source_builtin) and is_float(*target_builtin)) {
        return float_rank(*source_builtin) <= float_rank(*target_builtin);
    }
    return false;
}

auto semantic_analyzer::can_explicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
{
    if(can_implicitly_convert(from, to)) {
        return true;
    }

    auto source = as_builtin(read_type(from));
    auto target = as_builtin(read_type(to));
    if(source and target) {
        return is_numeric(*source) and is_numeric(*target);
    }
    return false;
}

auto semantic_analyzer::is_numeric_type(semantic_type_id id) -> bool
{
    auto builtin = as_builtin(read_type(id));
    return builtin and is_numeric(*builtin);
}

auto semantic_analyzer::common_numeric_type(semantic_type_id left, semantic_type_id right)
    -> std::optional<semantic_type_id>
{
    auto left_builtin = as_builtin(read_type(left));
    auto right_builtin = as_builtin(read_type(right));
    if(not left_builtin or not right_builtin) {
        return std::nullopt;
    }
    if(is_float(*left_builtin) or is_float(*right_builtin)) {
        auto kind = (
            float_rank(*left_builtin) > float_rank(*right_builtin)
            ? *left_builtin
            : *right_builtin
        );
        if(not is_float(kind)) {
            kind = builtin_type_kind::f64;
        }
        return semantic_type_ids::builtin(kind);
    }
    return common_integer_type(left, right);
}

auto semantic_analyzer::common_integer_type(semantic_type_id left, semantic_type_id right)
    -> std::optional<semantic_type_id>
{
    auto left_builtin = as_builtin(read_type(left));
    auto right_builtin = as_builtin(read_type(right));
    if (
        not left_builtin or not right_builtin
        or not is_integer(*left_builtin) or not is_integer(*right_builtin)
    ) {
        return std::nullopt;
    }

    auto kind = integer_rank(*left_builtin) >= integer_rank(*right_builtin)
        ? *left_builtin
        : *right_builtin;
    return semantic_type_ids::builtin(kind);
}

auto semantic_analyzer::join_same_class_numeric_or_equal(std::vector<semantic_type_id> const& values, source_span span)
    -> semantic_type_id
{
    auto current = read_type(values.front());
    for(auto index = 1uz; index < values.size(); ++index) {
        auto next = read_type(values[index]);
        if(current == next) {
            continue;
        }

        auto left = as_builtin(current);
        auto right = as_builtin(next);
        if(left and right and is_integer(*left) and is_integer(*right)) {
            current = *common_integer_type(current, next);
            continue;
        }
        if(left and right and is_float(*left) and is_float(*right)) {
            current = semantic_type_ids::builtin(float_rank(*left) >= float_rank(*right) ? *left : *right);
            continue;
        }

        report (
            diagnostic_kind::heterogeneous_aggregate,
            span,
            "aggregate elements must have one type family"
        );
        return semantic_type_ids::error;
    }
    return current;
}

auto semantic_analyzer::terminal_pointee_const(semantic_type_id pointee, bool target_const) -> bool
{
    if(not target_const) {
        return false;
    }

    auto const& kind = result.types.get(read_type(pointee));
    return not std::holds_alternative<pointer_type>(kind)
           and not std::holds_alternative<reference_type>(kind);
}

auto semantic_analyzer::lower_array_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id
{
    if(syntax.arguments.size() != 2uz) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "array requires <type,length>"
        );
        return semantic_type_ids::error;
    }

    if (
        not is<type_argument_type_syntax>(syntax.arguments.front())
        or not is<type_argument_literal_syntax>(syntax.arguments.back())
    ) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "array requires <type,length>"
        );
        return semantic_type_ids::error;
    }

    auto element = lower_type(ast, as<type_argument_type_syntax>(syntax.arguments.front()).type);
    auto length = parse_length(as<type_argument_literal_syntax>(syntax.arguments.back()).literal);
    return result.types.intern (array_type {
        .element = element,
        .length = length,
    });
}

auto semantic_analyzer::lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id
{
    if(syntax.arguments.empty()) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "tuple requires element types"
        );
        return semantic_type_ids::error;
    }

    auto elements = std::vector<semantic_type_id>{};
    for(auto const& argument : syntax.arguments) {
        if(not is<type_argument_type_syntax>(argument)) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "tuple arguments must be types"
            );
            return semantic_type_ids::error;
        }
        elements.emplace_back(lower_type(ast, as<type_argument_type_syntax>(argument).type));
    }
    return result.types.intern(tuple_type{ std::move(elements) });
}

auto semantic_analyzer::parse_length(source_span span) -> std::uint64_t
{
    auto text = ast_source.slice(span);
    auto value = std::uint64_t{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if(parsed.ec != std::errc{}) {
        report(diagnostic_kind::invalid_type_argument, span, "invalid array length");
        return 0;
    }
    return value;
}

auto semantic_analyzer::literal_type(source_span span) -> semantic_type_id
{
    auto text = ast_source.slice(span);
    if(text == "true" or text == "false") {
        return semantic_type_ids::bool_;
    }
    if(text.starts_with("'")) {
        return semantic_type_ids::char_;
    }
    if(text.starts_with("\"")) {
        return semantic_type_ids::str;
    }
    if(text.contains('.') or text.contains('e') or text.contains('E')) {
        return semantic_type_ids::f64;
    }
    return semantic_type_ids::i32;
}

auto semantic_analyzer::unary_type(token_kind operator_kind, expression_info operand) -> expression_info
{
    auto operand_value = read_type(operand.type);
    using enum token_kind;
    switch(operator_kind) {
        case amp:
            return expression_info {
                .type = intern_type(pointer_type {
                    operand_value,
                    target_const(operand_value, operand.is_const),
                }),
            };
        case star:
            if(auto pointee = pointer_pointee(operand_value)) {
                return *pointee;
            }
            return expression_info{ .type = semantic_type_ids::error };
        case kw_not:
            return expression_info{ .type = semantic_type_ids::bool_ };
        case plus:
        case minus:
        case tilde:
        case plus_plus:
        case minus_minus:
            return expression_info{ .type = operand_value };
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::binary_type(token_kind operator_kind, semantic_type_id left, semantic_type_id right) -> expression_info
{
    auto left_type = read_type(left);
    auto right_type = read_type(right);
    using enum token_kind;
    switch(operator_kind) {
        case kw_and:
        case kw_or:
        case equal_equal:
        case bang_equal:
        case less:
        case less_equal:
        case greater:
        case greater_equal:
            return expression_info{ .type = semantic_type_ids::bool_ };
        case plus:
            if(pointer_value_pointee(left_type)) {
                return expression_info{ .type = left_type };
            }
            if(pointer_value_pointee(right_type)) {
                return expression_info{ .type = right_type };
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        case minus:
            if(pointer_value_pointee(left_type) and pointer_value_pointee(right_type)) {
                return expression_info{ .type = semantic_type_ids::builtin(builtin_type_kind::isize) };
            }
            if(pointer_value_pointee(left_type)) {
                return expression_info{ .type = left_type };
            }
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        case star:
        case slash:
        case percent:
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        case pipe:
        case caret:
        case amp:
        case less_less:
        case greater_greater:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
    -> std::optional<semantic_type_id>
{
    auto const& callee = ast.node(node.callee);
    if(not is<name_expr_syntax>(callee)) {
        return std::nullopt;
    }

    auto text = ast_source.identifier(as<name_expr_syntax>(callee).name);
    if(auto builtin = is_builtin_name(text)) {
        return semantic_type_ids::builtin(*builtin);
    }
    if(auto alias = resolve_type_alias(text)) {
        return *alias;
    }
    if(active_type_aliases and active_type_aliases->contains(std::string{ text })) {
        return active_type_aliases->find(std::string{ text })->second;
    }
    if(auto symbol = resolve_type_symbol(active_unit_index, text)) {
        return result.symbols[symbol->value].type;
    }
    return std::nullopt;
}

auto semantic_analyzer::aggregate_context_for(std::optional<semantic_type_id> expected)
    -> std::optional<aggregate_context>
{
    if(not expected) {
        return std::nullopt;
    }
    auto const& type = result.types.get(*expected);
    if(auto const* array = std::get_if<array_type>(&type)) {
        return aggregate_context {
            .element = array->element,
            .length = array->length,
        };
    }
    return std::nullopt;
}

auto semantic_analyzer::struct_context_for(std::optional<semantic_type_id> expected)
    -> std::optional<std::uint32_t>
{
    if(not expected) {
        return std::nullopt;
    }
    return struct_index_of(*expected);
}

auto semantic_analyzer::join_return_types(std::vector<semantic_type_id> const& values) -> semantic_type_id
{
    if(values.empty()) {
        return semantic_type_ids::unit;
    }

    auto current = read_type(values.front());
    if(is_error(current) or is_inferred(current)) {
        return current;
    }
    for(auto index = 1uz; index < values.size(); ++index) {
        auto next = read_type(values[index]);
        if(is_error(next) or is_inferred(next)) {
            return next;
        }
        if(current == next) {
            continue;
        }

        auto left = as_builtin(current);
        auto right = as_builtin(next);
        if(left and right and is_integer(*left) and is_integer(*right)) {
            current = *common_integer_type(current, next);
            continue;
        }
        if(left and right and is_float(*left) and is_float(*right)) {
            current = semantic_type_ids::builtin(float_rank(*left) >= float_rank(*right) ? *left : *right);
            continue;
        }
        return semantic_type_ids::inferred;
    }
    return current;
}
