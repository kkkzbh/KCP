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
            .returns = lower_return_type(ast, syntax.function_return),
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
        return info.explicit_borrow ? info.type : read_type(info.type);
    }

    if(syntax.is_never_type) {
        if(
            syntax.is_const
            or syntax.is_like
            or not syntax.suffix_operators.empty()
        ) {
            report(diagnostic_kind::invalid_type_argument, syntax.full_span, "! cannot be qualified");
            return semantic_type_ids::error;
        }
        return semantic_type_ids::never;
    }

    if(syntax.is_array_type) {
        auto lowered = lower_array_type(ast, syntax);
        for(auto suffix : syntax.suffix_operators) {
            if(suffix == token_kind::star) {
                lowered = result.types.intern(pointer_type{ lowered, syntax.is_const, syntax.is_like });
            } else if(suffix == token_kind::amp) {
                auto kind = syntax.is_like ? reference_type::kind::like : reference_type::kind::regular;
                lowered = result.types.intern(reference_type{ lowered, syntax.is_const, kind });
            } else if(suffix == token_kind::kw_move) {
                lowered = result.types.intern(reference_type{ lowered, false, reference_type::kind::move });
            }
        }
        return lowered;
    }

    if(syntax.is_tuple_type) {
        auto lowered = lower_tuple_type(ast, syntax);
        for(auto suffix : syntax.suffix_operators) {
            if(suffix == token_kind::star) {
                lowered = result.types.intern(pointer_type{ lowered, syntax.is_const, syntax.is_like });
            } else if(suffix == token_kind::amp) {
                auto kind = syntax.is_like ? reference_type::kind::like : reference_type::kind::regular;
                lowered = result.types.intern(reference_type{ lowered, syntax.is_const, kind });
            } else if(suffix == token_kind::kw_move) {
                lowered = result.types.intern(reference_type{ lowered, false, reference_type::kind::move });
            }
        }
        return lowered;
    }

    if(syntax.is_grouped_type) {
        auto lowered = lower_type(ast, syntax.grouped_type);
        for(auto suffix : syntax.suffix_operators) {
            if(suffix == token_kind::star) {
                lowered = result.types.intern(pointer_type{ lowered, syntax.is_const, syntax.is_like });
            } else if(suffix == token_kind::amp) {
                auto kind = syntax.is_like ? reference_type::kind::like : reference_type::kind::regular;
                lowered = result.types.intern(reference_type{ lowered, syntax.is_const, kind });
            } else if(suffix == token_kind::kw_move) {
                lowered = result.types.intern(reference_type{ lowered, false, reference_type::kind::move });
            }
        }
        return lowered;
    }

    auto name = ast_source.identifier(syntax.name);
    auto lowered = semantic_type_id{};
    if(name == "this" and active_self_type.valid()) {
        lowered = active_self_type;
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "this does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(active_type_substitutions and active_type_substitutions->contains(std::string{ name })) {
        lowered = active_type_substitutions->find(std::string{ name })->second;
        if(std::holds_alternative<integer_constant_type>(result.types.get(lowered))) {
            report(diagnostic_kind::expected_type, syntax.full_span, "integer generic argument is not a type");
            lowered = semantic_type_ids::error;
        }
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "substituted type does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(active_type_pack_substitutions and active_type_pack_substitutions->contains(std::string{ name })) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "type parameter pack must be expanded with '...'"
        );
        lowered = semantic_type_ids::error;
    } else if(active_generic_parameters.contains(std::string{ name })) {
        lowered = active_generic_parameters.find(std::string{ name })->second;
        if(std::holds_alternative<generic_integer_parameter_type>(result.types.get(lowered))) {
            report(diagnostic_kind::expected_type, syntax.full_span, "integer generic parameter is not a type");
            lowered = semantic_type_ids::error;
        }
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
        if(value.enum_index < result.enums.size() and result.enums[value.enum_index].symbol == *symbol) {
            if(not syntax.arguments.empty()) {
                report (
                    diagnostic_kind::invalid_type_argument,
                    syntax.full_span,
                    "enum type does not take arguments"
                );
                lowered = semantic_type_ids::error;
            } else {
                lowered = value.type;
            }
        } else if(value.opaque_index < result.opaque_aliases.size() and result.opaque_aliases[value.opaque_index].symbol == *symbol) {
            if(not syntax.arguments.empty()) {
                report (
                    diagnostic_kind::invalid_type_argument,
                    syntax.full_span,
                    "opaque type does not take arguments"
                );
                lowered = semantic_type_ids::error;
            } else {
                lowered = value.type;
            }
        } else if(value.variant_index < result.variants.size() and result.variants[value.variant_index].symbol == *symbol) {
            auto const& variant = result.variants[value.variant_index];
            auto const& variant_ast = units[variant.unit_index].ast;
            auto const& variant_syntax = variant_ast.node(variant.syntax);
            if(syntax.arguments.size() > variant.generic_parameters.size()) {
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
                auto arguments = std::vector<semantic_type_id>{};
                auto substitutions = std::map<std::string, semantic_type_id>{};
                auto old_substitutions = active_type_substitutions;
                for(auto index = 0uz; index < variant.generic_parameters.size(); ++index) {
                    auto argument = semantic_type_id{};
                    if(index < syntax.arguments.size()) {
                        argument = lower_generic_type_argument(ast, syntax.arguments[index], variant.generic_parameters[index].parameter_kind, syntax.full_span);
                    } else if(variant_syntax.generic_parameters[index].default_argument) {
                        active_type_substitutions = &substitutions;
                        auto old_unit = active_unit_index;
                        active_unit_index = variant.unit_index;
                        argument = lower_generic_type_argument(variant_ast, *variant_syntax.generic_parameters[index].default_argument, variant.generic_parameters[index].parameter_kind, syntax.full_span);
                        active_unit_index = old_unit;
                    } else {
                        report(
                            diagnostic_kind::invalid_type_argument,
                            syntax.full_span,
                            std::format("variant type '{}' expects {} type argument(s)", variant.name, variant.generic_parameters.size())
                        );
                        argument = semantic_type_ids::error;
                    }
                    arguments.emplace_back(argument);
                    substitutions.emplace(variant.generic_parameters[index].name, argument);
                }
                active_type_substitutions = old_substitutions;
                lowered = result.types.intern(variant_type {
                    .index = value.variant_index,
                    .arguments = std::move(arguments),
                });
            }
        } else if(value.struct_index < result.structs.size()) {
            auto const& item = result.structs[value.struct_index];
            auto const& item_ast = units[item.unit_index].ast;
            auto const& item_syntax = item_ast.node(item.syntax);
            if(syntax.arguments.size() > item.generic_parameters.size()) {
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
                if(item.generic_parameters.empty()) {
                    lowered = value.type;
                } else {
                    auto arguments = std::vector<semantic_type_id>{};
                    auto substitutions = std::map<std::string, semantic_type_id>{};
                    auto old_substitutions = active_type_substitutions;
                    for(auto index = 0uz; index < item.generic_parameters.size(); ++index) {
                        if(not item_syntax.generic_parameters[index].default_argument) {
                            report(
                                diagnostic_kind::invalid_type_argument,
                                syntax.full_span,
                                std::format("struct type '{}' expects {} type argument(s)", item.name, item.generic_parameters.size())
                            );
                            arguments.emplace_back(semantic_type_ids::error);
                            continue;
                        }
                        active_type_substitutions = &substitutions;
                        auto old_unit = active_unit_index;
                        active_unit_index = item.unit_index;
                        auto argument = lower_generic_type_argument(item_ast, *item_syntax.generic_parameters[index].default_argument, item.generic_parameters[index].parameter_kind, syntax.full_span);
                        active_unit_index = old_unit;
                        arguments.emplace_back(argument);
                        substitutions.emplace(item.generic_parameters[index].name, argument);
                    }
                    active_type_substitutions = old_substitutions;
                    lowered = result.types.intern(struct_type {
                        .index = value.struct_index,
                        .arguments = std::move(arguments),
                    });
                }
            } else {
                auto arguments = std::vector<semantic_type_id>{};
                auto substitutions = std::map<std::string, semantic_type_id>{};
                auto old_substitutions = active_type_substitutions;
                for(auto index = 0uz; index < item.generic_parameters.size(); ++index) {
                    auto argument = semantic_type_id{};
                    if(index < syntax.arguments.size()) {
                        argument = lower_generic_type_argument(ast, syntax.arguments[index], item.generic_parameters[index].parameter_kind, syntax.full_span);
                    } else if(item_syntax.generic_parameters[index].default_argument) {
                        active_type_substitutions = &substitutions;
                        auto old_unit = active_unit_index;
                        active_unit_index = item.unit_index;
                        argument = lower_generic_type_argument(item_ast, *item_syntax.generic_parameters[index].default_argument, item.generic_parameters[index].parameter_kind, syntax.full_span);
                        active_unit_index = old_unit;
                    } else {
                        report(
                            diagnostic_kind::invalid_type_argument,
                            syntax.full_span,
                            std::format("struct type '{}' expects {} type argument(s)", item.name, item.generic_parameters.size())
                        );
                        argument = semantic_type_ids::error;
                    }
                    arguments.emplace_back(argument);
                    substitutions.emplace(item.generic_parameters[index].name, argument);
                }
                active_type_substitutions = old_substitutions;
                lowered = result.types.intern(struct_type {
                    .index = value.struct_index,
                    .arguments = std::move(arguments),
                });
            }
        } else {
            if(not syntax.arguments.empty()) {
                report (
                    diagnostic_kind::invalid_type_argument,
                    syntax.full_span,
                    "type alias does not take arguments"
                );
                lowered = semantic_type_ids::error;
            } else {
                lowered = value.type;
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
        auto found = associated_type(read_type(lowered), key);
        if(not found) {
            report(
                diagnostic_kind::unknown_type,
                associated,
                std::format("unknown associated type '{}'", key)
            );
            lowered = semantic_type_ids::error;
            break;
        }
        lowered = *found;
    }

    if(syntax.is_like and syntax.suffix_operators.empty()) {
        report(diagnostic_kind::invalid_type_argument, syntax.full_span, "like requires a pointer or reference suffix");
    }

    for(auto suffix : syntax.suffix_operators) {
        if(suffix == token_kind::star) {
            lowered = result.types.intern(pointer_type{ lowered, syntax.is_const, syntax.is_like });
        } else if(suffix == token_kind::amp) {
            auto kind = syntax.is_like ? reference_type::kind::like : reference_type::kind::regular;
            lowered = result.types.intern(reference_type{ lowered, syntax.is_const, kind });
        } else if(suffix == token_kind::kw_move) {
            if(syntax.is_const or syntax.is_like) {
                report(diagnostic_kind::invalid_type_argument, syntax.full_span, "move& cannot be combined with const or like");
            }
            lowered = result.types.intern(reference_type{ lowered, false, reference_type::kind::move });
        }
    }
    return lowered;
}

auto semantic_analyzer::lower_return_type(ast_arena const& ast, type_id id) -> semantic_type_id
{
    auto const& syntax = ast.node(id);
    auto const is_plain_named_type = (
        not syntax.is_function_type
        and not syntax.is_decltype
        and not syntax.is_never_type
        and not syntax.is_array_type
        and not syntax.is_tuple_type
        and not syntax.is_grouped_type
    );
    if(is_plain_named_type and ast_source.identifier(syntax.name) == "void") {
        if(
            not syntax.arguments.empty()
            or not syntax.associated_names.empty()
            or syntax.is_const
            or syntax.is_like
            or not syntax.suffix_operators.empty()
        ) {
            report(diagnostic_kind::invalid_type_argument, syntax.full_span, "void cannot be qualified or take arguments");
            return semantic_type_ids::error;
        }
        return semantic_type_ids::unit;
    }
    return lower_type(ast, id);
}

auto semantic_analyzer::lower_parameter_type(ast_arena const& ast, parameter_syntax const& parameter, std::optional<semantic_type_id> self_type) -> semantic_type_id
{
    if(parameter.is_self_receiver) {
        if(not self_type) {
            report(
                diagnostic_kind::invalid_self_parameter,
                parameter.full_span,
                "self receiver is only valid when a current type is available"
            );
            return semantic_type_ids::error;
        }
        if(parameter.self_is_reference) {
            auto kind = reference_type::kind::regular;
            if(parameter.self_is_like) {
                kind = reference_type::kind::like;
            } else if(parameter.self_is_move) {
                kind = reference_type::kind::move;
            }
            return result.types.intern(reference_type{ *self_type, parameter.is_const, kind });
        }
        return *self_type;
    }
    return parameter.type
        ? lower_type(ast, *parameter.type)
        : semantic_type_ids::inferred;
}

auto semantic_analyzer::lower_function_parameter_type(ast_arena const& ast, function_syntax const& function, std::size_t index, std::optional<semantic_type_id> self_type) -> semantic_type_id
{
    auto const& parameter = function.parameters[index];
    if(parameter.type or parameter.is_self_receiver or function.kind == function_syntax_kind::lambda) {
        return lower_parameter_type(ast, parameter, self_type);
    }

    auto name = inferred_parameter_generic_name(index);
    auto found = active_generic_parameters.find(name);
    auto lowered = found == active_generic_parameters.end()
        ? semantic_type_ids::error
        : found->second;
    if(not parameter.inferred_is_reference) {
        return lowered;
    }

    auto kind = parameter.inferred_reference_is_move
        ? reference_type::kind::move
        : reference_type::kind::regular;
    return result.types.intern(reference_type {
        lowered,
        parameter.inferred_reference_is_const,
        kind,
    });
}

auto semantic_analyzer::materialize_like_type(semantic_type_id type, bool is_const) -> semantic_type_id
{
    auto const& kind = result.types.get(type);
    if(auto const* reference = std::get_if<reference_type>(&kind)) {
        auto pointee = materialize_like_type(reference->pointee, is_const);
        auto target_const = reference->reference_kind == reference_type::kind::like ? is_const : reference->is_const;
        auto target_kind = reference->reference_kind == reference_type::kind::like ? reference_type::kind::regular : reference->reference_kind;
        if(auto const* inner = std::get_if<reference_type>(&result.types.get(pointee))) {
            return result.types.intern(reference_type{ inner->pointee, target_const or inner->is_const, target_kind });
        }
        if(reference->reference_kind == reference_type::kind::like) {
            return result.types.intern(reference_type{ pointee, is_const });
        }
        if(pointee != reference->pointee) {
            return result.types.intern(reference_type{ pointee, reference->is_const, reference->reference_kind });
        }
        return type;
    }
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        auto pointee = materialize_like_type(pointer->pointee, is_const);
        if(pointer->is_like) {
            return result.types.intern(pointer_type{ pointee, is_const });
        }
        if(pointee != pointer->pointee) {
            return result.types.intern(pointer_type{ pointee, pointer->is_const, pointer->is_like });
        }
    }
    return type;
}

auto semantic_analyzer::expression_for_return_type(semantic_type_id type) -> expression_info
{
    if(auto const* reference = std::get_if<reference_type>(&result.types.get(type))) {
        return expression_info {
            .type = type,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(reference->pointee, reference->is_const),
        };
    }
    return expression_info{ .type = type };
}

auto semantic_analyzer::associated_type(semantic_type_id owner, std::string_view name)
    -> std::optional<semantic_type_id>
{
    auto key = std::string{ name };
    if(auto found_types = associated_types.find(owner); found_types != associated_types.end()) {
        if(auto found = found_types->second.find(key); found != found_types->second.end()) {
            return found->second;
        }
    }

    for(auto const& [pattern, aliases] : associated_types) {
        auto found = aliases.find(key);
        if(found == aliases.end()) {
            continue;
        }
        auto inferred = std::map<std::uint32_t, semantic_type_id>{};
        if(not infer_type_argument(pattern, owner, inferred)) {
            continue;
        }
        auto type_arguments = std::vector<semantic_type_id>{};
        for(auto const& [index, value] : inferred) {
            if(index >= type_arguments.size()) {
                type_arguments.resize(index + 1uz, semantic_type_ids::error);
            }
            type_arguments[index] = value;
        }
        return substitute_type(found->second, type_arguments);
    }
    return std::nullopt;
}

auto semantic_analyzer::lower_type_with_substitutions(ast_arena const& ast, type_id id, std::map<std::string, semantic_type_id> const& substitutions) -> semantic_type_id
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

auto semantic_analyzer::resolve_exported_type_symbol(std::string_view module_name, std::string_view name, std::set<std::string>& visiting) const -> std::optional<symbol_id>
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

auto semantic_analyzer::resolve_exported_concept_symbol(std::string_view module_name, std::string_view name, std::set<std::string>& visiting) const -> std::optional<symbol_id>
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

auto semantic_analyzer::str_field_index(std::string_view name) const -> std::optional<std::uint32_t>
{
    if(name == "ptr") {
        return 0u;
    }
    if(name == "len") {
        return 1u;
    }
    return std::nullopt;
}

auto semantic_analyzer::str_field_type(std::uint32_t index) -> semantic_type_id
{
    if(index == 0u) {
        return intern_type(pointer_type{ semantic_type_ids::char_, true });
    }
    return semantic_type_ids::builtin(builtin_type_kind::usize);
}

auto semantic_analyzer::is_default_initializable(semantic_type_id type) -> bool
{
    auto const& kind = result.types.get(type);
    return std::visit (
        overloaded {
            [](unit_type const&) { return true; },
            [](error_type const&) { return true; },
            [](inferred_type const&) { return false; },
            [](never_type const&) { return false; },
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
            [](integer_constant_type const&) { return false; },
            [](generic_integer_parameter_type const&) { return false; },
            [&](struct_type const& value) {
                auto const& item = result.structs[value.index];
                return std::ranges::all_of(item.fields, [&](auto const& field) {
                    return is_default_initializable(substitute_type(field.type, value.arguments));
                });
            },
            [](enum_type const&) { return false; },
            [](opaque_type const&) { return true; },
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
            [](never_type const&) { return false; },
            [](builtin_type const&) { return false; },
            [&](array_type const& value) { return is_dependent_type(value.element) or is_dependent_type(value.length); },
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
            [](integer_constant_type const&) { return false; },
            [](generic_integer_parameter_type const&) { return true; },
            [&](struct_type const& value) {
                return std::ranges::any_of(value.arguments, [&](auto argument) {
                    return is_dependent_type(argument);
                });
            },
            [](enum_type const&) { return false; },
            [](opaque_type const&) { return false; },
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
    auto iterable = resolve_concept_symbol(active_unit_index, "iterable");
    if(iterable and target_implements(*iterable, value)) {
        if(auto item = associated_type(value, "iter_item")) {
            return *item;
        }
    }
    auto iterator = resolve_concept_symbol(active_unit_index, "iterator");
    if(iterator and target_implements(*iterator, value)) {
        if(auto item = associated_type(value, "iter_item")) {
            return *item;
        }
    }
    return {};
}

auto semantic_analyzer::method_symbol(semantic_type_id owner, std::string_view name) const -> std::optional<symbol_id>
{
    auto key = std::string{ name };
    auto type = read_type(owner);
    if(auto struct_index = struct_index_of(type)) {
        auto const& methods = result.structs[*struct_index].methods;
        if(auto found = methods.find(key); found != methods.end()) {
            return found->second;
        }
    }
    if(auto variant_index = variant_index_of(type)) {
        auto const& methods = result.variants[*variant_index].methods;
        if(auto found = methods.find(key); found != methods.end()) {
            return found->second;
        }
    }
    if(auto opaque_index = opaque_index_of(type)) {
        auto const& methods = result.opaque_aliases[*opaque_index].methods;
        if(auto found = methods.find(key); found != methods.end()) {
            return found->second;
        }
    }
    auto const* unit = active_unit_index < units.size() ? &units[active_unit_index] : active_unit;
    if(unit == nullptr) {
        return std::nullopt;
    }
    if(auto found_methods = unit->visible_extension_methods.find(type); found_methods != unit->visible_extension_methods.end()) {
        if(auto found = found_methods->second.find(key); found != found_methods->second.end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::concrete_method_symbol(symbol_id symbol, semantic_type_id receiver_type, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>
{
    auto const& value = result.symbols[symbol.value];
    if(not function_is_generic(value.unit_index, value.function)) {
        return symbol;
    }

    auto instance = instantiate_function_symbol(
        symbol,
        read_type(receiver_type),
        arguments,
        {},
        span
    );
    if(instance == nullptr) {
        return std::nullopt;
    }
    return instance->symbol;
}

auto semantic_analyzer::range_info(expression_info range, source_span span) -> semantic_for_range_info
{
    auto value = read_type(range.type);
    auto const& kind = result.types.get(value);
    if(auto const* array = std::get_if<array_type>(&kind)) {
        return semantic_for_range_info {
            .kind = semantic_for_range_kind::array,
            .element_type = array->element,
        };
    }

    auto iterable = resolve_concept_symbol(active_unit_index, "iterable");
    auto iterator = resolve_concept_symbol(active_unit_index, "iterator");
    auto iterator_type = semantic_type_id{};
    auto iter_symbol = symbol_id{};
    if(iterable and target_implements(*iterable, value)) {
        auto associated_iter = associated_type(value, "iter_type");
        auto associated_item = associated_type(value, "iter_item");
        if(not associated_iter or not associated_item) {
            report(diagnostic_kind::missing_concept_item, span, "iterable range is missing iter_type or iter_item");
            return {};
        }
        auto method = method_symbol(value, "iter");
        if(not method) {
            report(diagnostic_kind::unknown_member, span, "iterable range is missing iter()");
            return {};
        }
        auto arguments = std::vector<expression_info>{ range };
        auto concrete = concrete_method_symbol(*method, value, arguments, span);
        if(not concrete) {
            return {};
        }
        iter_symbol = *concrete;
        auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[iter_symbol.value].type));
        auto receiver_matches = false;
        if(callable != nullptr and not callable->parameters.empty()) {
            receiver_matches = can_implicitly_convert(range, callable->parameters.front());
            if(not receiver_matches) {
                if(auto const* reference = std::get_if<reference_type>(&result.types.get(callable->parameters.front()))) {
                    receiver_matches = reference->reference_kind == reference_type::kind::regular
                        and read_type(range.type) == read_type(reference->pointee)
                        and not range.is_const
                        and not range.is_move;
                }
            }
        }
        if(callable == nullptr or callable->parameters.empty() or not receiver_matches) {
            report(diagnostic_kind::type_mismatch, span, "iterable iter() receiver type mismatch");
            return {};
        }
        if(read_type(callable->returns) != read_type(*associated_iter)) {
            report(diagnostic_kind::type_mismatch, span, "iterable iter() return type mismatch");
            return {};
        }
        iterator_type = *associated_iter;
    } else if(iterator and target_implements(*iterator, value)) {
        iterator_type = value;
    } else {
        return {};
    }

    auto item = associated_type(iterator_type, "iter_item");
    if(not item) {
        report(diagnostic_kind::missing_concept_item, span, "iterator range is missing iter_item");
        return {};
    }

    auto next = method_symbol(iterator_type, "next");
    if(not next) {
        report(diagnostic_kind::unknown_member, span, "iterator range is missing next()");
        return {};
    }
    auto iterator_argument = expression_info {
        .type = iterator_type,
        .is_lvalue = true,
        .is_const = false,
    };
    auto next_symbol = concrete_method_symbol(*next, iterator_type, { iterator_argument }, span);
    if(not next_symbol) {
        return {};
    }
    auto const* next_callable = std::get_if<function_type>(&result.types.get(result.symbols[next_symbol->value].type));
    if(next_callable == nullptr or next_callable->parameters.empty()) {
        report(diagnostic_kind::type_mismatch, span, "iterator next() signature is invalid");
        return {};
    }
    if(not can_implicitly_convert(iterator_argument, next_callable->parameters.front())) {
        report(diagnostic_kind::type_mismatch, span, "iterator next() receiver type mismatch");
        return {};
    }

    auto returned = read_type(next_callable->returns);
    auto optional_variant = variant_index_of(returned);
    if(not optional_variant) {
        report(diagnostic_kind::type_mismatch, span, "iterator next() must return optional<T>");
        return {};
    }
    auto const& variant = result.variants[*optional_variant];
    auto some = variant.case_indices.find("some");
    auto none = variant.case_indices.find("none");
    if(some == variant.case_indices.end() or none == variant.case_indices.end()) {
        report(diagnostic_kind::type_mismatch, span, "iterator next() result must have some and none cases");
        return {};
    }
    auto payloads = variant_case_payload_types(returned, variant.cases[some->second]);
    if(payloads.size() != 1uz or read_type(payloads.front()) != read_type(*item)) {
        report(diagnostic_kind::type_mismatch, span, "iterator next() some payload must match iter_item");
        return {};
    }

    return semantic_for_range_info {
        .kind = semantic_for_range_kind::iterator_protocol,
        .element_type = *item,
        .iterator_type = iterator_type,
        .iter_symbol = iter_symbol,
        .next_symbol = *next_symbol,
        .optional_variant_index = *optional_variant,
        .some_case_index = some->second,
        .none_case_index = none->second,
    };
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

auto semantic_analyzer::can_qualification_convert(semantic_type_id from, semantic_type_id to) const -> bool
{
    if(from == to or is_error(from) or is_error(to)) {
        return true;
    }

    auto const& source = result.types.get(from);
    auto const& target = result.types.get(to);
    if(auto const* target_pointer = std::get_if<pointer_type>(&target)) {
        auto const* source_pointer = std::get_if<pointer_type>(&source);
        return source_pointer != nullptr
            and (target_pointer->is_const or target_pointer->is_like or not source_pointer->is_const)
            and can_qualification_convert(source_pointer->pointee, target_pointer->pointee);
    }
    if(auto const* target_reference = std::get_if<reference_type>(&target)) {
        auto const* source_reference = std::get_if<reference_type>(&source);
        return source_reference != nullptr
            and source_reference->reference_kind == target_reference->reference_kind
            and (target_reference->is_const or not source_reference->is_const)
            and can_qualification_convert(source_reference->pointee, target_reference->pointee);
    }
    return false;
}

auto semantic_analyzer::can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool
{
    if(is_never(read_type(from.type))) {
        return true;
    }
    if(auto const* reference = std::get_if<reference_type>(&result.types.get(to))) {
        auto same_target = can_qualification_convert(read_type(from.type), reference->pointee);
        if(reference->reference_kind == reference_type::kind::move) {
            return same_target and (from.is_move or not from.is_lvalue);
        }
        if(reference->reference_kind == reference_type::kind::like) {
            return same_target and from.is_lvalue;
        }
        if(from.is_lvalue) {
            return same_target and not from.is_move and (reference->is_const or not from.is_const);
        }
        return same_target and not from.is_move and reference->is_const;
    }
    if(from.explicit_borrow) {
        return false;
    }
    return can_implicitly_convert(from.type, to);
}

auto semantic_analyzer::can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
{
    if(from == to or is_error(from) or is_error(to)) {
        return true;
    }

    if(is_never(read_type(from))) {
        return true;
    }
    if(is_never(read_type(to))) {
        return false;
    }

    if(auto const* target_pointer = std::get_if<pointer_type>(&result.types.get(to))) {
        if(from == semantic_type_ids::nullptr_) {
            return true;
        }
        if(std::holds_alternative<function_type>(result.types.get(target_pointer->pointee))) {
            return from == target_pointer->pointee;
        }
    }
    if(can_qualification_convert(from, to)) {
        return true;
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

    auto source_value = read_type(from);
    auto target_value = read_type(to);
    auto source = as_builtin(source_value);
    auto target = as_builtin(target_value);
    if(source and target) {
        return is_numeric(*source) and is_numeric(*target);
    }
    if(
        std::holds_alternative<pointer_type>(result.types.get(source_value))
        and std::holds_alternative<pointer_type>(result.types.get(target_value))
    ) {
        return true;
    }
    if(auto source_enum = enum_index_of(source_value)) {
        return underlying_type(source_value) == target_value;
    }
    if(auto source_opaque = opaque_index_of(source_value)) {
        auto const& opaque = result.opaque_aliases[*source_opaque];
        return opaque.unit_index == active_unit_index and opaque.underlying_type == target_value;
    }
    if(auto target_opaque = opaque_index_of(target_value)) {
        auto const& opaque = result.opaque_aliases[*target_opaque];
        return opaque.unit_index == active_unit_index and opaque.underlying_type == source_value;
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
    if(syntax.is_array_type) {
        return result.types.intern (array_type {
            .element = lower_type(ast, syntax.array_element),
            .length = lower_array_length(syntax.array_length),
        });
    }

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
        or (
            not is<type_argument_literal_syntax>(syntax.arguments.back())
            and not is<type_argument_name_syntax>(syntax.arguments.back())
        )
    ) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "array requires <type,length>"
        );
        return semantic_type_ids::error;
    }

    auto element = lower_type(ast, as<type_argument_type_syntax>(syntax.arguments.front()).type);
    return result.types.intern (array_type {
        .element = element,
        .length = lower_array_length(syntax.arguments.back()),
    });
}

auto semantic_analyzer::lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id
{
    if(syntax.is_tuple_type) {
        auto elements = (
            syntax.tuple_elements
            | std::views::transform([&](type_id element) {
                return lower_type(ast, element);
            }) | std::ranges::to<std::vector<semantic_type_id>>()
        );
        return result.types.intern(tuple_type{ std::move(elements) });
    }

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

auto semantic_analyzer::lower_generic_type_argument(ast_arena const& ast, type_argument_syntax const& argument, generic_parameter_syntax::kind parameter_kind, source_span span) -> semantic_type_id
{
    using enum generic_parameter_syntax::kind;
    if(parameter_kind == type) {
        if(auto const* type_argument = std::get_if<type_argument_type_syntax>(&argument)) {
            auto lowered = lower_type(ast, type_argument->type);
            auto const& kind = result.types.get(lowered);
            if(std::holds_alternative<integer_constant_type>(kind) or std::holds_alternative<generic_integer_parameter_type>(kind)) {
                report(diagnostic_kind::invalid_type_argument, span, "type generic parameter requires a type argument");
                return semantic_type_ids::error;
            }
            return lowered;
        }
        report(diagnostic_kind::invalid_type_argument, span, "type generic parameter requires a type argument");
        return semantic_type_ids::error;
    }

    if(auto const* literal = std::get_if<type_argument_literal_syntax>(&argument)) {
        return result.types.intern(integer_constant_type {
            .value = parse_integer_literal(ast_source.slice(literal->literal)),
            .type = parameter_kind == const_usize ? builtin_type_kind::usize : builtin_type_kind::isize,
        });
    }

    if(auto const* type_argument = std::get_if<type_argument_type_syntax>(&argument)) {
        auto const& syntax = ast.node(type_argument->type);
        auto bare_name = (
            syntax.arguments.empty()
            and syntax.associated_names.empty()
            and syntax.suffix_operators.empty()
            and not syntax.is_const
            and not syntax.is_like
            and not syntax.is_function_type
            and not syntax.is_decltype
            and not syntax.is_array_type
            and not syntax.is_tuple_type
            and not syntax.is_grouped_type
        );
        if(bare_name) {
            auto key = std::string{ ast_source.identifier(syntax.name) };
            if(active_type_substitutions and active_type_substitutions->contains(key)) {
                auto value = active_type_substitutions->find(key)->second;
                if(std::holds_alternative<integer_constant_type>(result.types.get(value))) {
                    return value;
                }
            }
            if(active_generic_parameters.contains(key)) {
                auto value = active_generic_parameters.find(key)->second;
                if(std::holds_alternative<generic_integer_parameter_type>(result.types.get(value))) {
                    return value;
                }
            }
        }
    }

    report(diagnostic_kind::invalid_type_argument, span, "integer generic parameter requires an integer argument");
    return semantic_type_ids::error;
}

auto semantic_analyzer::lower_array_length(type_argument_syntax const& syntax) -> semantic_type_id
{
    if(auto const* literal = std::get_if<type_argument_literal_syntax>(&syntax)) {
        return result.types.intern(integer_constant_type {
            .value = static_cast<std::int64_t>(parse_length(literal->literal)),
            .type = builtin_type_kind::usize,
        });
    }
    if(auto const* name = std::get_if<type_argument_name_syntax>(&syntax)) {
        auto key = std::string{ ast_source.identifier(name->name) };
        if(active_type_substitutions and active_type_substitutions->contains(key)) {
            auto value = active_type_substitutions->find(key)->second;
            if(std::holds_alternative<integer_constant_type>(result.types.get(value))) {
                return value;
            }
        }
        if(active_generic_parameters.contains(key)) {
            auto value = active_generic_parameters.find(key)->second;
            if(std::holds_alternative<generic_integer_parameter_type>(result.types.get(value))) {
                return value;
            }
        }
        report(diagnostic_kind::invalid_type_argument, name->name, "array length must be a usize integer parameter");
        return semantic_type_ids::error;
    }

    report(diagnostic_kind::invalid_type_argument, source_span{}, "array length must be an integer literal or parameter");
    return semantic_type_ids::error;
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

auto semantic_analyzer::array_length_value(semantic_type_id length) const -> std::optional<std::uint64_t>
{
    auto const& kind = result.types.get(length);
    if(auto const* constant = std::get_if<integer_constant_type>(&kind)) {
        if(constant->value < 0) {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(constant->value);
    }
    return std::nullopt;
}

auto semantic_analyzer::literal_type(source_span span) -> semantic_type_id
{
    auto text = ast_source.slice(span);
    if(text == "nullptr") {
        return semantic_type_ids::nullptr_;
    }
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

auto semantic_analyzer::unary_type(token_kind operator_kind, unary_position position, expression_info operand) -> expression_info
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
        case kw_ref:
            return expression_info {
                .type = intern_type(reference_type { operand_value }),
                .is_lvalue = true,
                .explicit_borrow = true,
            };
        case kw_const:
            return expression_info {
                .type = intern_type(reference_type { operand_value, true }),
                .is_lvalue = true,
                .is_const = true,
                .explicit_borrow = true,
            };
        case kw_move:
            return expression_info {
                .type = intern_type(reference_type { operand_value, false, reference_type::kind::move }),
                .is_lvalue = true,
                .is_move = true,
            };
        case kw_delete:
            return expression_info{ .type = semantic_type_ids::unit };
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
            return expression_info{ .type = operand_value };
        case minus_minus:
        case plus_plus:
            if(position == unary_position::prefix) {
                auto reference = intern_type(reference_type { operand_value, target_const(operand.type, operand.is_const) });
                return expression_info {
                    .type = reference,
                    .is_lvalue = true,
                    .is_const = target_const(reference, operand.is_const),
                };
            }
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
        case spaceship:
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

auto semantic_analyzer::aggregate_context_for(std::optional<semantic_type_id> expected)
    -> std::optional<aggregate_context>
{
    if(not expected) {
        return std::nullopt;
    }
    auto const& type = result.types.get(*expected);
    if(auto const* array = std::get_if<array_type>(&type)) {
        auto length = array_length_value(array->length);
        if(not length) {
            return std::nullopt;
        }
        return aggregate_context {
            .element = array->element,
            .length = static_cast<std::size_t>(*length),
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

    auto first = std::ranges::find_if(values, [&](semantic_type_id value) {
        return not is_never(read_type(value));
    });
    if(first == values.end()) {
        return semantic_type_ids::never;
    }

    auto current = read_type(*first);
    if(is_error(current) or is_inferred(current)) {
        return current;
    }
    for(auto index = static_cast<std::size_t>(std::distance(values.begin(), first)) + 1uz; index < values.size(); ++index) {
        auto next = read_type(values[index]);
        if(is_never(next)) {
            continue;
        }
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
