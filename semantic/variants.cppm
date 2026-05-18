module semantic:variants;

import semantic;

auto semantic_analyzer::collect_variant_declaration(
    std::size_t unit_index,
    ast_arena const& ast,
    variant_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto name = std::string{ ast_source.identifier(syntax.name) };
    auto const& state = units[unit_index];
    auto& local_types = module_types[state.module_key];
    if (
        local_types.contains(name)
        or module_functions[state.module_key].contains(name)
        or module_concepts[state.module_key].contains(name)
    ) {
        report(
            diagnostic_kind::duplicate_symbol,
            syntax.name,
            std::format("duplicate top-level name '{}'", name)
        );
        return;
    }

    auto index = static_cast<std::uint32_t>(result.variants.size());
    auto type = result.types.intern(variant_type{ .index = index });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::type,
        .name = name,
        .span = syntax.name,
        .type = type,
        .exported = syntax.exported,
        .unit_index = unit_index,
        .variant_index = index,
    });

    result.variants.emplace_back(name, syntax.name, type, syntax.exported, unit_index, id, symbol);
    auto& item = result.variants.back();
    for(auto const& parameter : syntax.generic_parameters) {
        item.generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }

    if(syntax.exported and not state.named_module) {
        report(
            diagnostic_kind::export_requires_module,
            syntax.name,
            "exported variant requires an export module declaration"
        );
    }
    if(syntax.exported and state.named_module) {
        module_type_exports[state.module_name].emplace(name, symbol);
    }
    local_types.emplace(std::move(name), symbol);
}

auto semantic_analyzer::collect_variant_cases(
    std::size_t unit_index,
    ast_arena const& ast,
    variant_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto symbol = resolve_type_symbol(unit_index, ast_source.identifier(syntax.name));
    if(not symbol) {
        return;
    }

    auto variant_index = result.symbols[symbol->value].variant_index;
    auto& item = result.variants[variant_index];
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    active_generic_parameters.clear();
    active_generic_parameter_packs.clear();
    for(auto index = 0uz; index < syntax.generic_parameters.size(); ++index) {
        active_generic_parameters.emplace(
            std::string{ ast_source.identifier(syntax.generic_parameters[index].name) },
            result.types.intern(generic_parameter_type{ .index = static_cast<std::uint32_t>(index) })
        );
    }

    auto names = std::set<std::string>{};
    for(auto const& variant_case : syntax.cases) {
        auto name = std::string{ ast_source.identifier(variant_case.name) };
        if(names.contains(name)) {
            report(
                diagnostic_kind::duplicate_variant_case,
                variant_case.name,
                std::format("duplicate variant case '{}'", name)
            );
            continue;
        }
        names.emplace(name);

        auto payloads = std::vector<semantic_type_id>{};
        for(auto payload : variant_case.payloads) {
            payloads.emplace_back(lower_type(ast, payload));
        }

        auto case_index = static_cast<std::uint32_t>(item.cases.size());
        item.case_indices.emplace(name, case_index);
        item.cases.emplace_back(std::move(name), variant_case.name, std::move(payloads));
    }

    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
}

auto semantic_analyzer::variant_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* value = std::get_if<variant_type>(&kind)) {
        return value->index;
    }
    return std::nullopt;
}

auto semantic_analyzer::substitute_type(
    semantic_type_id type,
    std::vector<semantic_type_id> const& arguments
) -> semantic_type_id
{
    auto const& kind = result.types.get(type);
    return std::visit (
        overloaded {
            [&](unit_type const&) { return type; },
            [&](error_type const&) { return type; },
            [&](inferred_type const&) { return type; },
            [&](builtin_type const&) { return type; },
            [&](generic_parameter_type const& value) {
                if(value.index < arguments.size()) {
                    return arguments[value.index];
                }
                return semantic_type_ids::error;
            },
            [&](array_type const& value) {
                return result.types.intern(array_type {
                    .element = substitute_type(value.element, arguments),
                    .length = value.length,
                });
            },
            [&](tuple_type const& value) {
                auto elements = (
                    value.elements
                    | std::views::transform([&](auto element) {
                        return substitute_type(element, arguments);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(tuple_type{ std::move(elements) });
            },
            [&](reference_type const& value) {
                return result.types.intern(reference_type {
                    substitute_type(value.pointee, arguments),
                    value.is_const,
                });
            },
            [&](pointer_type const& value) {
                return result.types.intern(pointer_type {
                    substitute_type(value.pointee, arguments),
                    value.is_const,
                });
            },
            [&](function_type const& value) {
                auto parameters = (
                    value.parameters
                    | std::views::transform([&](auto parameter) {
                        return substitute_type(parameter, arguments);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(function_type {
                    .parameters = std::move(parameters),
                    .returns = substitute_type(value.returns, arguments),
                });
            },
            [&](struct_type const& value) {
                auto substituted = (
                    value.arguments
                    | std::views::transform([&](auto argument) {
                        return substitute_type(argument, arguments);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(struct_type {
                    .index = value.index,
                    .arguments = std::move(substituted),
                });
            },
            [&](variant_type const& value) {
                auto substituted = (
                    value.arguments
                    | std::views::transform([&](auto argument) {
                        return substitute_type(argument, arguments);
                    }) | std::ranges::to<std::vector<semantic_type_id>>()
                );
                return result.types.intern(variant_type {
                    .index = value.index,
                    .arguments = std::move(substituted),
                });
            },
        },
        kind
    );
}

auto semantic_analyzer::variant_case_payload_types(
    semantic_type_id type,
    semantic_variant_case const& variant_case
) -> std::vector<semantic_type_id>
{
    auto const* variant = std::get_if<variant_type>(&result.types.get(read_type(type)));
    if(variant == nullptr) {
        return {};
    }

    return (
        variant_case.payload_types
        | std::views::transform([&](auto payload) {
            return substitute_type(payload, variant->arguments);
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
}
