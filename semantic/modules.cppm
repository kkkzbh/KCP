module semantic:modules;

import semantic;

auto semantic_body_kind(function_syntax const& function) -> semantic_function_body_kind
{
    if(function.deleted) {
        return semantic_function_body_kind::deleted;
    }
    if(function.defaulted) {
        return semantic_function_body_kind::defaulted;
    }
    if(function.extern_abi and not function.has_body) {
        return semantic_function_body_kind::extern_declaration;
    }
    if(function.has_body) {
        return semantic_function_body_kind::source_body;
    }
    return semantic_function_body_kind::synthesized;
}

auto semantic_analyzer::build_module_index() -> void
{
    collect_modules();
    collect_declarations();
    resolve_imports();
}

auto semantic_analyzer::collect_modules() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto& state = units[unit_index];
        auto const& syntax = state.root;
        if(syntax.module_header) {
            state.module_name = ast_source.module_name(syntax.module_header->name);
            state.module_key = state.module_name;
            state.named_module = true;
        } else {
            state.module_key = std::format("$anonymous.{}", unit_index);
        }
    }
}

auto semantic_analyzer::collect_declarations() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto concept_id : syntax.concepts) {
            collect_concept_declaration(unit_index, ast, concept_id);
        }
        for(auto struct_id : syntax.structs) {
            collect_struct_declaration(unit_index, ast, struct_id);
        }
        for(auto enum_id : syntax.enums) {
            collect_enum_declaration(unit_index, ast, enum_id);
        }
        for(auto alias : syntax.type_aliases) {
            collect_type_alias_declaration(unit_index, ast, alias);
        }
        for(auto variant_id : syntax.variants) {
            collect_variant_declaration(unit_index, ast, variant_id);
        }
    }

    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto concept_id : syntax.concepts) {
            collect_concept_items(unit_index, ast, concept_id);
        }
    }

    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto struct_id : syntax.structs) {
            collect_struct_fields(unit_index, ast, struct_id);
        }
        for(auto enum_id : syntax.enums) {
            collect_enum_cases(unit_index, ast, enum_id);
        }
        for(auto variant_id : syntax.variants) {
            collect_variant_cases(unit_index, ast, variant_id);
        }
    }

    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            collect_function_declaration(unit_index, ast, function_id);
        }
        for(auto impl_id : syntax.impls) {
            collect_impl_declarations(unit_index, ast, impl_id);
        }
        for(auto impl_id : syntax.concept_impls) {
            collect_concept_impl_declarations(unit_index, ast, impl_id);
        }
    }

    validate_requires_clauses();
}

auto semantic_analyzer::resolve_imports() -> void
{
    auto named_modules = std::set<std::string>{};
    for(auto const& state : units) {
        if(state.named_module) {
            named_modules.emplace(state.module_name);
        }
    }

    auto changed = true;
    while(changed) {
        changed = false;
        for(auto const& state : units) {
            if(not state.named_module) {
                continue;
            }
            for(auto const& import : state.root.imports) {
                if(not import.exported) {
                    continue;
                }
                auto module_name = ast_source.module_name(import.name);
                if(auto found_functions = module_exports.find(module_name); found_functions != module_exports.end()) {
                    auto& exports = module_exports[state.module_name];
                    for(auto const& [name, symbol] : found_functions->second) {
                        changed = exports.emplace(name, symbol).second or changed;
                    }
                }
                if(auto found_operators = module_operator_exports.find(module_name); found_operators != module_operator_exports.end()) {
                    for(auto const& [kind, symbols] : found_operators->second) {
                        auto& exports = module_operator_exports[state.module_name][kind];
                        for(auto symbol : symbols) {
                            if(not std::ranges::contains(exports, symbol)) {
                                exports.emplace_back(symbol);
                                changed = true;
                            }
                        }
                    }
                }
                if(auto found_methods = module_extension_method_exports.find(module_name); found_methods != module_extension_method_exports.end()) {
                    auto& exports = module_extension_method_exports[state.module_name];
                    for(auto const& [type, methods] : found_methods->second) {
                        for(auto const& [name, symbols] : methods) {
                            auto& target = exports[type][name];
                            for(auto symbol : symbols) {
                                if(not std::ranges::contains(target, symbol)) {
                                    target.emplace_back(symbol);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
                if(auto found_extension_operators = module_extension_operator_exports.find(module_name); found_extension_operators != module_extension_operator_exports.end()) {
                    auto& exports = module_extension_operator_exports[state.module_name];
                    for(auto const& [type, operators] : found_extension_operators->second) {
                        for(auto const& [kind, symbols] : operators) {
                            auto& exported_symbols = exports[type][kind];
                            for(auto symbol : symbols) {
                                if(not std::ranges::contains(exported_symbols, symbol)) {
                                    exported_symbols.emplace_back(symbol);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
                if(auto found_types = module_type_exports.find(module_name); found_types != module_type_exports.end()) {
                    auto& exports = module_type_exports[state.module_name];
                    for(auto const& [name, symbol] : found_types->second) {
                        changed = exports.emplace(name, symbol).second or changed;
                    }
                }
                if(auto found_concepts = module_concept_exports.find(module_name); found_concepts != module_concept_exports.end()) {
                    auto& exports = module_concept_exports[state.module_name];
                    for(auto const& [name, symbol] : found_concepts->second) {
                        changed = exports.emplace(name, symbol).second or changed;
                    }
                }
            }
        }
    }

    for(auto& state : units) {
        auto const& syntax = state.root;
        if(auto local = module_functions.find(state.module_key); local != module_functions.end()) {
            state.visible_functions.insert_range(local->second);
        }
        if(auto local = module_operators.find(state.module_key); local != module_operators.end()) {
            for(auto const& [kind, symbols] : local->second) {
                state.visible_operators[kind].insert(
                    state.visible_operators[kind].end(),
                    symbols.begin(),
                    symbols.end()
                );
            }
        }
        if(auto local = module_extension_methods.find(state.module_key); local != module_extension_methods.end()) {
            for(auto const& [type, methods] : local->second) {
                auto& visible_methods = state.visible_extension_methods[type];
                for(auto const& [name, symbols] : methods) {
                    auto& target = visible_methods[name];
                    target.insert(target.end(), symbols.begin(), symbols.end());
                }
            }
        }
        if(auto local = module_extension_operators.find(state.module_key); local != module_extension_operators.end()) {
            for(auto const& [type, operators] : local->second) {
                for(auto const& [kind, symbols] : operators) {
                    auto& target = state.visible_extension_operators[type][kind];
                    target.insert(target.end(), symbols.begin(), symbols.end());
                }
            }
        }
        if(auto local = module_types.find(state.module_key); local != module_types.end()) {
            state.visible_types.insert_range(local->second);
        }
        if(auto local = module_concepts.find(state.module_key); local != module_concepts.end()) {
            state.visible_concepts.insert_range(local->second);
        }

        for(auto const& import : syntax.imports) {
            auto module_name = ast_source.module_name(import.name);
            if(import.exported and not state.named_module) {
                report(
                    diagnostic_kind::export_requires_module,
                    import.full_span,
                    "exported import requires an export module declaration"
                );
            }
            auto found_functions = module_exports.find(module_name);
            auto found_operators = module_operator_exports.find(module_name);
            auto found_methods = module_extension_method_exports.find(module_name);
            auto found_extension_operators = module_extension_operator_exports.find(module_name);
            auto found_types = module_type_exports.find(module_name);
            auto found_concepts = module_concept_exports.find(module_name);
            if (
                not named_modules.contains(module_name)
                and found_functions == module_exports.end()
                and found_operators == module_operator_exports.end()
                and found_methods == module_extension_method_exports.end()
                and found_extension_operators == module_extension_operator_exports.end()
                and found_types == module_type_exports.end()
                and found_concepts == module_concept_exports.end()
            ) {
                report(
                    diagnostic_kind::unknown_module,
                    import.full_span,
                    std::format("unknown imported module '{}'", module_name)
                );
                continue;
            }

            if(found_functions != module_exports.end()) {
                for(auto const& [name, symbol] : found_functions->second) {
                    if(auto existing = state.visible_functions.find(name); existing != state.visible_functions.end()) {
                        if(existing->second == symbol) {
                            if(import.exported and state.named_module) {
                                module_exports[state.module_name].emplace(name, symbol);
                            }
                            continue;
                        }
                        report(
                            diagnostic_kind::import_conflict,
                            import.full_span,
                            std::format("imported symbol '{}' conflicts with an existing visible symbol", name)
                        );
                        continue;
                    }
                    state.visible_functions.emplace(name, symbol);
                    if(import.exported and state.named_module) {
                        module_exports[state.module_name].emplace(name, symbol);
                    }
                }
            }

            if(found_operators != module_operator_exports.end()) {
                for(auto const& [kind, symbols] : found_operators->second) {
                    auto& target = state.visible_operators[kind];
                    target.insert(target.end(), symbols.begin(), symbols.end());
                    if(import.exported and state.named_module) {
                        auto& exported_operators = module_operator_exports[state.module_name][kind];
                        for(auto symbol : symbols) {
                            if(not std::ranges::contains(exported_operators, symbol)) {
                                exported_operators.emplace_back(symbol);
                            }
                        }
                    }
                }
            }

            if(found_methods != module_extension_method_exports.end()) {
                for(auto const& [type, methods] : found_methods->second) {
                    for(auto const& [name, symbols] : methods) {
                        auto& visible_methods = state.visible_extension_methods[type];
                        auto& target = visible_methods[name];
                        for(auto symbol : symbols) {
                            if(not std::ranges::contains(target, symbol)) {
                                target.emplace_back(symbol);
                            }
                        }
                        if(import.exported and state.named_module) {
                            auto& exported = module_extension_method_exports[state.module_name][type][name];
                            for(auto symbol : symbols) {
                                if(not std::ranges::contains(exported, symbol)) {
                                    exported.emplace_back(symbol);
                                }
                            }
                        }
                    }
                }
            }

            if(found_extension_operators != module_extension_operator_exports.end()) {
                for(auto const& [type, operators] : found_extension_operators->second) {
                    for(auto const& [kind, symbols] : operators) {
                        auto& target = state.visible_extension_operators[type][kind];
                        for(auto symbol : symbols) {
                            if(not std::ranges::contains(target, symbol)) {
                                target.emplace_back(symbol);
                            }
                        }
                        if(import.exported and state.named_module) {
                            auto& exported_operators = module_extension_operator_exports[state.module_name][type][kind];
                            for(auto symbol : symbols) {
                                if(not std::ranges::contains(exported_operators, symbol)) {
                                    exported_operators.emplace_back(symbol);
                                }
                            }
                        }
                    }
                }
            }

            if(found_types != module_type_exports.end()) {
                for(auto const& [name, symbol] : found_types->second) {
                    if(state.visible_functions.contains(name)) {
                        report(
                            diagnostic_kind::import_conflict,
                            import.full_span,
                            std::format("imported symbol '{}' conflicts with an existing visible symbol", name)
                        );
                        continue;
                    }
                    if(auto existing = state.visible_types.find(name); existing != state.visible_types.end()) {
                        if(existing->second == symbol) {
                            if(import.exported and state.named_module) {
                                module_type_exports[state.module_name].emplace(name, symbol);
                            }
                            continue;
                        }
                        report(
                            diagnostic_kind::import_conflict,
                            import.full_span,
                            std::format("imported type '{}' conflicts with an existing visible type", name)
                        );
                        continue;
                    }
                    state.visible_types.emplace(name, symbol);
                    if(import.exported and state.named_module) {
                        module_type_exports[state.module_name].emplace(name, symbol);
                    }
                }
            }

            if(found_concepts == module_concept_exports.end()) {
                continue;
            }
            for(auto const& [name, symbol] : found_concepts->second) {
                if(state.visible_functions.contains(name) or state.visible_types.contains(name)) {
                    report(
                        diagnostic_kind::import_conflict,
                        import.full_span,
                        std::format("imported concept '{}' conflicts with an existing visible symbol", name)
                    );
                    continue;
                }
                if(auto existing = state.visible_concepts.find(name); existing != state.visible_concepts.end()) {
                    if(existing->second == symbol) {
                        if(import.exported and state.named_module) {
                            module_concept_exports[state.module_name].emplace(name, symbol);
                        }
                        continue;
                    }
                    report(
                        diagnostic_kind::import_conflict,
                        import.full_span,
                        std::format("imported concept '{}' conflicts with an existing visible concept", name)
                    );
                    continue;
                }
                state.visible_concepts.emplace(name, symbol);
                if(import.exported and state.named_module) {
                    module_concept_exports[state.module_name].emplace(name, symbol);
                }
            }
        }
    }
}

auto semantic_analyzer::collect_concept_declaration(std::size_t unit_index, ast_arena const& ast, concept_id id) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto name = std::string{ ast_source.identifier(syntax.name) };
    auto const& state = units[unit_index];
    auto& local_concepts = module_concepts[state.module_key];
    if (
        local_concepts.contains(name)
        or module_types[state.module_key].contains(name)
        or module_functions[state.module_key].contains(name)
    ) {
        report(
            diagnostic_kind::duplicate_symbol,
            syntax.name,
            std::format("duplicate top-level name '{}'", name)
        );
        return;
    }
    auto index = static_cast<std::uint32_t>(result.concepts.size());
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::concept_,
        .name = name,
        .span = syntax.name,
        .type = semantic_type_ids::unit,
        .exported = syntax.exported,
        .unit_index = unit_index,
        .concept_index = index,
    });
    result.concepts.emplace_back(name, syntax.name, syntax.exported, unit_index, id, symbol);

    if(syntax.exported and not state.named_module) {
        report(
            diagnostic_kind::export_requires_module,
            syntax.name,
            "exported concept requires an export module declaration"
        );
    }
    if(syntax.exported and state.named_module) {
        module_concept_exports[state.module_name].emplace(name, symbol);
    }
    local_concepts.emplace(std::move(name), symbol);
}

auto semantic_analyzer::collect_concept_items(std::size_t unit_index, ast_arena const& ast, concept_id id) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto concept_symbol = resolve_concept_symbol(unit_index, ast_source.identifier(syntax.name));
    if(not concept_symbol) {
        return;
    }

    auto& concept_value = result.concepts[result.symbols[concept_symbol->value].concept_index];
    for(auto const& item : syntax.items) {
        std::visit (
            overloaded {
                [&](concept_requires_syntax const& value) {
                    for(auto const& constraint : value.constraints) {
                        std::visit (
                            overloaded {
                                [&](concept_parent_constraint_syntax const& parent_value) {
                                    auto parent_name = ast_source.identifier(parent_value.parent.name);
                                    auto parent = resolve_concept_symbol(unit_index, parent_name);
                                    if(not parent) {
                                        report(
                                            diagnostic_kind::unknown_concept,
                                            parent_value.parent.name,
                                            std::format("unknown parent concept '{}'", parent_name)
                                        );
                                        return;
                                    }
                                    concept_value.parents.emplace_back(parent_value.parent);
                                },
                                [&](concept_type_bound_constraint_syntax const& bound_value) {
                                    auto concepts = std::vector<concept_id_syntax>{};
                                    for(auto bound : bound_value.concept_bounds) {
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
                                        concepts.emplace_back(bound);
                                    }
                                    concept_value.type_bounds.emplace_back(
                                        unit_index,
                                        bound_value.type,
                                        std::move(concepts),
                                        bound_value.full_span
                                    );
                                },
                                [&](concept_type_equality_constraint_syntax const& equal_value) {
                                    concept_value.type_equalities.emplace_back(
                                        unit_index,
                                        equal_value.left,
                                        equal_value.right,
                                        equal_value.full_span
                                    );
                                },
                            },
                            constraint
                        );
                    }
                },
                [&](type_alias_syntax const& value) {
                    auto name = std::string{ ast_source.identifier(value.name) };
                    if(concept_value.associated_types.contains(name) or concept_value.functions.contains(name)) {
                        report(
                            diagnostic_kind::duplicate_symbol,
                            value.name,
                            std::format("duplicate concept item '{}'", name)
                        );
                        return;
                    }
                    concept_value.associated_types.emplace(
                        name,
                        semantic_concept_associated_type {
                            .name = name,
                            .span = value.name,
                            .unit_index = unit_index,
                            .default_type = value.value,
                        }
                    );
                },
                [&](concept_function_requirement_syntax const& value) {
                    auto name = std::string{ ast_source.identifier(value.name) };
                    if(concept_value.associated_types.contains(name) or concept_value.functions.contains(name)) {
                        report(
                            diagnostic_kind::duplicate_symbol,
                            value.name,
                            std::format("duplicate concept item '{}'", name)
                        );
                        return;
                    }
                    for(auto index = 0uz; index < value.parameters.size(); ++index) {
                        auto const& parameter = value.parameters[index];
                        if(ast_source.slice(parameter.name) == "self") {
                            if(index != 0uz or not parameter.is_self_receiver) {
                                report(
                                    diagnostic_kind::invalid_self_parameter,
                                    parameter.full_span,
                                    "concept self parameter must use receiver syntax"
                                );
                            }
                        } else if(not parameter.type) {
                            report(
                                diagnostic_kind::invalid_type_argument,
                                parameter.full_span,
                                "concept function requirement parameters must declare a type"
                            );
                        }
                    }
                    concept_value.functions.emplace(
                        name,
                        semantic_concept_function_requirement {
                            .name = name,
                            .span = value.name,
                            .unit_index = unit_index,
                            .overload_operator = value.overload_operator,
                            .parameters = value.parameters,
                            .return_type = value.return_type,
                            .default_function = value.default_function,
                        }
                    );
                },
            },
            item
        );
    }
}

auto semantic_analyzer::collect_struct_declaration(std::size_t unit_index, ast_arena const& ast, struct_id id) -> void
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
    validate_non_function_generic_parameter_packs(syntax.generic_parameters, "struct");

    auto index = static_cast<std::uint32_t>(result.structs.size());
    auto type = result.types.intern(struct_type{ .index = index });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::type,
        .name = name,
        .span = syntax.name,
        .type = type,
        .exported = syntax.exported,
        .unit_index = unit_index,
        .struct_index = index,
    });

    result.structs.emplace_back(name, syntax.name, type, syntax.exported, unit_index, id, symbol);
    auto& item = result.structs.back();
    for(auto const& parameter : syntax.generic_parameters) {
        auto parameter_name = std::string{ ast_source.identifier(parameter.name) };
        item.generic_parameters.emplace_back(std::move(parameter_name), parameter.parameter_kind);
    }

    if(syntax.exported and not state.named_module) {
        report(
            diagnostic_kind::export_requires_module,
            syntax.name,
            "exported struct requires an export module declaration"
        );
    }
    if(syntax.exported and state.named_module) {
        module_type_exports[state.module_name].emplace(name, symbol);
    }
    local_types.emplace(std::move(name), symbol);
}

auto semantic_analyzer::collect_struct_fields(std::size_t unit_index, ast_arena const& ast, struct_id id) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto name = std::string{ ast_source.identifier(syntax.name) };
    auto symbol = resolve_type_symbol(unit_index, name);
    if(not symbol) {
        return;
    }

    auto struct_index = result.symbols[symbol->value].struct_index;
    auto& item = result.structs[struct_index];
    auto names = std::set<std::string>{};
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    active_generic_parameters.clear();
    active_generic_parameter_packs.clear();
    for(auto index = 0uz; index < syntax.generic_parameters.size(); ++index) {
        active_generic_parameters.emplace(
            std::string{ ast_source.identifier(syntax.generic_parameters[index].name) },
            generic_parameter_type_for(static_cast<std::uint32_t>(index), syntax.generic_parameters[index].parameter_kind)
        );
    }
    for(auto const& field : syntax.fields) {
        auto field_name = std::string{ ast_source.identifier(field.name) };
        if(names.contains(field_name)) {
            report(
                diagnostic_kind::duplicate_field,
                field.name,
                std::format("duplicate field '{}'", field_name)
            );
            continue;
        }
        names.emplace(field_name);
        item.fields.emplace_back(std::move(field_name), field.name, lower_type(ast, field.type), field.default_value);
    }
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
}

auto semantic_analyzer::collect_function_declaration(std::size_t unit_index, ast_arena const& ast, function_id id) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    if(function.overload_operator) {
        collect_operator_declaration(unit_index, ast, id);
        return;
    }
    auto name = function.kind == function_syntax_kind::lambda
        ? std::format("cp.lambda.{}.{}", unit_index, id.value)
        : std::string{ ast_source.identifier(function.name) };
    auto const& state = units[unit_index];
    auto& local_names = module_functions[state.module_key];
    if (
        function.kind != function_syntax_kind::lambda
        and (
        local_names.contains(name)
        or module_types[state.module_key].contains(name)
        or module_concepts[state.module_key].contains(name)
        )
    ) {
        report(
            diagnostic_kind::duplicate_symbol,
            function.name,
            std::format("duplicate top-level name '{}'", name)
        );
        return;
    }

    validate_function_pack_shape(unit_index, id);
    validate_parameter_defaults(unit_index, id);
    record_function_parameter_defaults(0uz, unit_index, id);
    auto all_generic_parameters = implicit_function_generic_names({}, function);
    for(auto const& parameter : function.generic_parameters) {
        all_generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }
    if(auto implicit_parameters = inferred_parameter_generic_names(function); not implicit_parameters.empty()) {
        implicit_function_generic_parameters[function_key(unit_index, id)] = std::move(implicit_parameters);
    }
    auto body_kind = semantic_body_kind(function);
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    bind_active_function_generic_parameters(unit_index, id);

    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        parameter_types.emplace_back(lower_function_parameter_type(ast, function, index));
    }
    auto inferred_return = not function.return_type and semantic_function_body_has_source(body_kind);
    auto return_type = semantic_type_ids::inferred;
    if(function.return_type) {
        return_type = lower_return_type(ast, *function.return_type);
    } else if(not semantic_function_body_has_source(body_kind)) {
        return_type = semantic_type_ids::unit;
    }
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);

    if(function.extern_abi) {
        validate_extern_c_function(function, parameter_types, return_type);
    }

    auto signature_id = (
        add_signature (function_signature {
            .parameters = parameter_types,
            .returns = return_type,
            .inferred_return = inferred_return,
        })
    );
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);
    if(not all_generic_parameters.empty()) {
        result.function_generic_parameter_counts.emplace(semantic_node_key{unit_index, id}, all_generic_parameters.size());
    }

    auto function_type_id = (
        intern_type (function_type {
            .parameters = parameter_types,
            .returns = return_type,
        })
    );
    auto symbol = (
        add_symbol (semantic_symbol {
            .kind = symbol_kind::function,
            .name = name,
            .span = function.name,
            .type = function_type_id,
            .exported = function.exported,
            .body_kind = body_kind,
            .unit_index = unit_index,
            .function = id,
            .function_kind = semantic_function_kind::free_function,
        })
    );
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    if(function.exported and not state.named_module) {
        report(
            diagnostic_kind::export_requires_module,
            function.name,
            "exported function requires an export module declaration"
        );
    }
    if(function.exported and state.named_module) {
        auto& exports = module_exports[state.module_name];
        exports.emplace(name, symbol);
    }
    if(function.kind != function_syntax_kind::lambda) {
        local_names.emplace(std::move(name), symbol);
    }
}

auto semantic_analyzer::collect_operator_declaration(std::size_t unit_index, ast_arena const& ast, function_id id) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    if(not function.overload_operator) {
        return;
    }

    validate_function_pack_shape(unit_index, id);
    validate_parameter_defaults(unit_index, id);
    record_function_parameter_defaults(0uz, unit_index, id);
    auto all_generic_parameters = implicit_function_generic_names({}, function);
    for(auto const& parameter : function.generic_parameters) {
        all_generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }
    if(auto implicit_parameters = inferred_parameter_generic_names(function); not implicit_parameters.empty()) {
        implicit_function_generic_parameters[function_key(unit_index, id)] = std::move(implicit_parameters);
    }
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    bind_active_function_generic_parameters(unit_index, id);
    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        parameter_types.emplace_back(lower_function_parameter_type(ast, function, index));
    }
    auto return_type = semantic_type_ids::unit;
    auto inferred_return = false;
    if(function.return_type) {
        return_type = lower_return_type(ast, *function.return_type);
    } else {
        return_type = semantic_type_ids::inferred;
        inferred_return = true;
    }
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);

    auto has_operator_owner_parameter = std::ranges::any_of(parameter_types, [&](semantic_type_id type) {
        auto read = read_type(type);
        return struct_index_of(read) or variant_index_of(read) or opaque_index_of(read) or as_builtin(read);
    });
    if(not has_operator_owner_parameter) {
        report(
            diagnostic_kind::invalid_operator,
            function.full_span,
            "top-level operator requires a user-defined or builtin parameter type"
        );
    }

    if(*function.overload_operator == overload_operator_kind::equal or compound_assignment_operator(operator_token(*function.overload_operator))) {
        auto const* reference = parameter_types.empty()
            ? nullptr
            : std::get_if<reference_type>(&result.types.get(parameter_types.front()));
        if(reference == nullptr or reference->is_const) {
            report(diagnostic_kind::invalid_operator, function.full_span, "assignment operator left parameter must be a writable reference");
        }
    }

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
        .inferred_return = inferred_return,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);
    if(not all_generic_parameters.empty()) {
        result.function_generic_parameter_counts.emplace(semantic_node_key{unit_index, id}, all_generic_parameters.size());
    }

    auto name = std::format("operator.{}.{}", std::to_underlying(*function.overload_operator), id.value);
    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .exported = function.exported,
        .body_kind = semantic_body_kind(function),
        .unit_index = unit_index,
        .function = id,
        .function_kind = semantic_function_kind::free_function,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    auto const& state = units[unit_index];
    module_operators[state.module_key][*function.overload_operator].emplace_back(symbol);
    if(function.exported and not state.named_module) {
        report(
            diagnostic_kind::export_requires_module,
            function.name,
            "exported operator requires an export module declaration"
        );
    }
    if(function.exported and state.named_module) {
        module_operator_exports[state.module_name][*function.overload_operator].emplace_back(symbol);
    }
}

auto semantic_analyzer::validate_extern_c_function(function_syntax const& function, std::span<semantic_type_id const> parameter_types, semantic_type_id return_type) -> void
{
    if(not function.extern_abi) {
        return;
    }

    if(ast_source.slice(*function.extern_abi) != "\"C\"") {
        report(diagnostic_kind::invalid_type_argument, *function.extern_abi, "extern ABI must be \"C\"");
    }
    if(not function.generic_parameters.empty()) {
        report(diagnostic_kind::invalid_type_argument, function.full_span, "extern \"C\" function cannot be generic");
    }
    if(function.requires_clause) {
        report(diagnostic_kind::invalid_type_argument, function.requires_clause->full_span, "extern \"C\" function cannot have a requires clause");
    }
    if(not is_extern_c_compatible_type(return_type, true)) {
        report(diagnostic_kind::invalid_type_argument, function.full_span, "extern \"C\" return type is not C-compatible");
    }
    for(auto type : parameter_types) {
        if(not is_extern_c_compatible_type(type, false)) {
            report(diagnostic_kind::invalid_type_argument, function.full_span, "extern \"C\" parameter type is not C-compatible");
        }
    }
}

auto semantic_analyzer::is_extern_c_compatible_type(semantic_type_id type, bool allow_unit) const -> bool
{
    if(is_error(type)) {
        return true;
    }
    if(is_inferred(type)) {
        return false;
    }
    auto const& kind = result.types.get(type);
    return std::visit(overloaded {
        [&](unit_type const&) {
            return allow_unit;
        },
        [&](never_type const&) {
            return false;
        },
        [&](builtin_type const& value) {
            return value.kind != builtin_type_kind::str;
        },
        [&](pointer_type const&) {
            return true;
        },
        [](auto const&) {
            return false;
        },
    }, kind);
}

auto semantic_analyzer::collect_impl_declarations(std::size_t unit_index, ast_arena const& ast, impl_id id) -> void
{
    active_unit_index = unit_index;
    auto const& impl = ast.node(id);
    validate_non_function_generic_parameter_packs(impl.generic_parameters, "impl");
    auto impl_generic_parameters = impl.generic_parameters.empty()
        ? collect_type_pattern_parameters(ast, impl.type)
        : (
            impl.generic_parameters
            | std::views::transform([&](generic_parameter_syntax const& parameter) {
                return semantic_generic_parameter{ std::string{ ast_source.identifier(parameter.name) }, parameter.parameter_kind };
            }) | std::ranges::to<std::vector<semantic_generic_parameter>>()
        );
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    if(impl.generic_parameters.empty()) {
        bind_active_generic_parameters(impl_generic_parameters);
    } else {
        bind_active_generic_parameters(impl.generic_parameters);
    }
    auto type = lower_type(ast, impl.type);
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
    auto builtin_impl_allowed = as_builtin(read_type(type));
    auto array_impl_allowed = std::holds_alternative<array_type>(result.types.get(read_type(type)));
    if(not struct_index_of(type) and not variant_index_of(type) and not opaque_index_of(type) and not builtin_impl_allowed and not array_impl_allowed) {
        report(
            diagnostic_kind::unknown_type,
            ast.span(impl.type),
            "impl target must be a struct, variant, opaque type, array type, or compiler-recognized std type"
        );
        return;
    }

    auto& aliases = associated_types[type];
    for(auto const& alias : impl.type_aliases) {
        auto name = std::string{ ast_source.identifier(alias.name) };
        if(aliases.contains(name)) {
            report(
                diagnostic_kind::duplicate_symbol,
                alias.name,
                std::format("duplicate associated type '{}'", name)
            );
            continue;
        }
        if(not alias.value) {
            report(diagnostic_kind::expected_type, alias.full_span, "impl type alias requires a value");
            continue;
        }
        auto old_parameters_for_alias = std::move(active_generic_parameters);
        auto old_packs_for_alias = std::move(active_generic_parameter_packs);
        if(impl.generic_parameters.empty()) {
            bind_active_generic_parameters(impl_generic_parameters);
        } else {
            bind_active_generic_parameters(impl.generic_parameters);
        }
        auto alias_type = lower_type(ast, *alias.value);
        active_generic_parameters = std::move(old_parameters_for_alias);
        active_generic_parameter_packs = std::move(old_packs_for_alias);
        aliases.emplace(std::move(name), alias_type);
    }

    for(auto function_id : impl.functions) {
        if(ast.node(function_id).overload_operator) {
            collect_impl_operator(unit_index, ast, impl, type, impl_generic_parameters, function_id);
            continue;
        }
        collect_impl_function(unit_index, ast, impl, type, impl_generic_parameters, function_id);
    }
}

auto semantic_analyzer::collect_impl_function(std::size_t unit_index, ast_arena const& ast, impl_syntax const& impl, semantic_type_id impl_type, std::vector<semantic_generic_parameter> const& impl_generic_parameters, function_id id) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    auto struct_index = struct_index_of(impl_type);
    auto variant_index = variant_index_of(impl_type);
    auto opaque_index = opaque_index_of(impl_type);
    auto target_builtin = as_builtin(read_type(impl_type));
    auto target_array = std::holds_alternative<array_type>(result.types.get(read_type(impl_type)));
    auto builtin_impl_allowed = target_builtin;
    if(not struct_index and not variant_index and not opaque_index and not builtin_impl_allowed and not target_array) {
        return;
    }
    auto item_name = std::string{};
    auto item_type = impl_type;
    if(struct_index) {
        item_name = result.structs[*struct_index].name;
        item_type = result.structs[*struct_index].type;
    } else if(variant_index) {
        item_name = result.variants[*variant_index].name;
        item_type = result.variants[*variant_index].type;
    } else if(opaque_index) {
        item_name = result.opaque_aliases[*opaque_index].name;
        item_type = result.opaque_aliases[*opaque_index].type;
    } else if(target_builtin) {
        item_name = type_name(*target_builtin);
    }
    auto name = std::string{ ast_source.identifier(function.name) };
    auto body_kind = semantic_body_kind(function);
    auto is_constructor = struct_index and (function.defaulted or name == item_name);
    auto is_destructor = function.kind == function_syntax_kind::destructor;
    if((variant_index or opaque_index or target_builtin or target_array) and (function.defaulted or is_destructor or name == item_name)) {
        report(diagnostic_kind::invalid_constructor, function.full_span, "only struct impl can declare constructors or destructors");
        return;
    }

    auto impl_generic_parameter_names = generic_parameter_names(impl_generic_parameters);
    auto all_generic_parameters = implicit_function_generic_names(impl_generic_parameter_names, function);
    for(auto const& parameter : function.generic_parameters) {
        all_generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }
    implicit_function_generic_parameters[function_key(unit_index, id)] = implicit_function_generic_names(impl_generic_parameter_names, function);
    if(not impl_generic_parameters.empty()) {
        implicit_function_generic_parameter_kinds[function_key(unit_index, id)] = generic_parameter_kinds(impl_generic_parameters);
    }
    function_impl_target_patterns[function_key(unit_index, id)] = impl_type;
    if(impl.requires_clause) {
        function_impl_requires[function_key(unit_index, id)] = *impl.requires_clause;
    }
    validate_function_pack_shape(unit_index, id);
    validate_parameter_defaults(unit_index, id);
    record_function_parameter_defaults(0uz, unit_index, id);

    if(function.defaulted) {
        if(not function.default_marker or ast_source.slice(*function.default_marker) != "default") {
            report(diagnostic_kind::invalid_constructor, function.full_span, "expected '= default;' constructor");
        }
        if(not function.parameters.empty()) {
            report(diagnostic_kind::invalid_constructor, function.full_span, "default constructor takes no parameters");
        }
    }

    auto function_kind = semantic_function_kind::associated_function;
    auto return_type = semantic_type_ids::unit;
    auto inferred_return = false;
    if(is_destructor) {
        function_kind = semantic_function_kind::destructor;
        if(name != item_name) {
            report(diagnostic_kind::invalid_destructor, function.name, "destructor name must match impl type");
        }
        if(not function.parameters.empty()) {
            report(diagnostic_kind::invalid_destructor, function.full_span, "destructor cannot have parameters");
        }
        if(function.return_type) {
            report(diagnostic_kind::invalid_destructor, function.full_span, "destructor cannot declare a return type");
        }
        if(result.structs[*struct_index].destructor.valid()) {
            report(diagnostic_kind::duplicate_symbol, function.name, "struct already has a destructor");
        }
    } else if(is_constructor) {
        function_kind = semantic_function_kind::constructor;
        return_type = impl_type;
        if(function.return_type) {
            report(diagnostic_kind::invalid_constructor, function.full_span, "constructor cannot declare a return type");
        }
    } else {
        auto old_self = active_self_type;
        auto old_parameters = std::move(active_generic_parameters);
        auto old_packs = std::move(active_generic_parameter_packs);
        active_self_type = impl_type;
        bind_active_function_generic_parameters(unit_index, id);
        if(function.return_type) {
            return_type = lower_return_type(ast, *function.return_type);
        } else if(semantic_function_body_has_source(body_kind)) {
            return_type = semantic_type_ids::inferred;
            inferred_return = true;
        }
        if(not function.parameters.empty() and ast_source.slice(function.parameters.front().name) == "self") {
            function_kind = semantic_function_kind::member_function;
            if(not function.parameters.front().is_self_receiver) {
                report(
                    diagnostic_kind::invalid_self_parameter,
                    function.parameters.front().full_span,
                    "self parameter must use receiver syntax"
                );
            }
            auto lowered_self = lower_parameter_type(ast, function.parameters.front(), impl_type);
            auto target = read_type(lowered_self);
            if(target != impl_type) {
                report(
                    diagnostic_kind::invalid_self_parameter,
                    function.parameters.front().full_span,
                    "self parameter must be the current impl type or reference"
                );
            }
            auto duplicate = associated_types[impl_type].contains(name);
            if(duplicate) {
                report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate member '{}'", name));
            }
        } else {
            if(target_builtin or target_array) {
                report(diagnostic_kind::invalid_self_parameter, function.full_span, "array and builtin type impl functions must use self receiver");
                return;
            }
            auto duplicate = false;
            if(struct_index) {
                duplicate = result.structs[*struct_index].associated_functions.contains(name);
            } else if(variant_index) {
                duplicate = result.variants[*variant_index].associated_functions.contains(name);
            } else if(opaque_index) {
                duplicate = result.opaque_aliases[*opaque_index].associated_functions.contains(name);
            }
            duplicate = duplicate or associated_types[impl_type].contains(name);
            if(duplicate) {
                report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate associated function '{}'", name));
            }
        }
        active_self_type = old_self;
        active_generic_parameters = std::move(old_parameters);
        active_generic_parameter_packs = std::move(old_packs);
    }

    auto old_self = active_self_type;
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    active_self_type = impl_type;
    bind_active_function_generic_parameters(unit_index, id);
    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        parameter_types.emplace_back(lower_function_parameter_type(ast, function, index, impl_type));
    }
    active_self_type = old_self;
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
    if(is_destructor) {
        parameter_types.emplace_back(result.types.intern(reference_type{ impl_type }));
    }

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
        .inferred_return = inferred_return,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);
    if(not all_generic_parameters.empty()) {
        result.function_generic_parameter_counts.emplace(semantic_node_key{unit_index, id}, all_generic_parameters.size());
    }

    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .body_kind = body_kind,
        .unit_index = unit_index,
        .function = id,
        .struct_index = struct_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .opaque_index = opaque_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .variant_index = variant_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .owner_type = (target_builtin or target_array) ? impl_type : semantic_type_id{},
        .function_kind = function_kind,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    if(function_kind == semantic_function_kind::constructor) {
        result.structs[*struct_index].constructors.emplace_back(symbol);
    } else if(function_kind == semantic_function_kind::destructor) {
        result.structs[*struct_index].destructor = symbol;
    } else if(function_kind == semantic_function_kind::member_function) {
        if(struct_index) {
            result.structs[*struct_index].methods[name].emplace_back(symbol);
        } else if(variant_index) {
            result.variants[*variant_index].methods[name].emplace_back(symbol);
        } else if(opaque_index) {
            result.opaque_aliases[*opaque_index].methods[name].emplace_back(symbol);
        } else {
            auto const& state = units[unit_index];
            module_extension_methods[state.module_key][impl_type][name].emplace_back(symbol);
            if(state.named_module) {
                module_extension_method_exports[state.module_name][impl_type][name].emplace_back(symbol);
            }
        }
    } else {
        if(struct_index) {
            result.structs[*struct_index].associated_functions.emplace(name, symbol);
        } else if(variant_index) {
            result.variants[*variant_index].associated_functions.emplace(name, symbol);
        } else {
            result.opaque_aliases[*opaque_index].associated_functions.emplace(name, symbol);
        }
    }
}

auto semantic_analyzer::default_compare_to_weak_method(semantic_type_id result_type, semantic_type_id weak_type, source_span span) -> std::optional<symbol_id>
{
    if(can_implicitly_convert(result_type, weak_type)) {
        return symbol_id{};
    }

    auto method = method_symbol(result_type, "to_weak");
    if(not method) {
        report(diagnostic_kind::invalid_operator, span, "field comparison result must provide to_weak()");
        return std::nullopt;
    }

    auto argument = expression_info{ .type = result_type };
    auto concrete = concrete_method_symbol(*method, result_type, { argument }, span);
    if(not concrete) {
        return std::nullopt;
    }

    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[concrete->value].type));
    if(
        callable == nullptr
        or callable->parameters.size() != 1uz
        or not can_implicitly_convert(argument, callable->parameters.front())
        or not can_implicitly_convert(callable->returns, weak_type)
    ) {
        report(diagnostic_kind::invalid_operator, span, "field comparison to_weak() must return weak_ordering");
        return std::nullopt;
    }
    return *concrete;
}

auto semantic_analyzer::default_compare_field(std::uint32_t field_index, semantic_type_id field_type, source_span span) -> std::optional<semantic_default_compare_field>
{
    auto weak_type = weak_ordering_type();
    if(not weak_type) {
        report(diagnostic_kind::invalid_operator, span, "defaulted comparison requires weak_ordering");
        return std::nullopt;
    }

    auto field_value = read_type(field_type);
    if(enum_index_of(field_value)) {
        return semantic_default_compare_field {
            .field_index = field_index,
            .enum_builtin = true,
        };
    }

    auto argument_type = result.types.intern(reference_type{ field_value, true });
    auto arguments = std::vector<expression_info> {
        expression_info {
            .type = argument_type,
            .is_lvalue = true,
            .is_const = true,
        },
        expression_info {
            .type = argument_type,
            .is_lvalue = true,
            .is_const = true,
        },
    };
    auto owners = std::array{ field_value, field_value };
    auto compare = resolve_operator(overload_operator_kind::spaceship, owners, arguments, span);
    if(not compare) {
        report(diagnostic_kind::invalid_operator, span, "defaulted comparison field does not support operator <=>");
        return std::nullopt;
    }
    if(function_is_deleted(*compare)) {
        report(diagnostic_kind::invalid_operator, span, "defaulted comparison field uses a deleted operator <=>");
        return std::nullopt;
    }

    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[compare->value].type));
    if(callable == nullptr or callable->parameters.size() != 2uz) {
        report(diagnostic_kind::invalid_operator, span, "field comparison operator has an invalid signature");
        return std::nullopt;
    }

    auto to_weak = default_compare_to_weak_method(callable->returns, *weak_type, span);
    if(not to_weak) {
        return std::nullopt;
    }
    return semantic_default_compare_field {
        .field_index = field_index,
        .compare_operator = *compare,
        .to_weak_method = *to_weak,
    };
}

auto semantic_analyzer::validate_default_compare_operator(function_syntax const& function, semantic_type_id impl_type, std::optional<std::uint32_t> struct_index, std::span<semantic_type_id const> parameter_types, semantic_type_id return_type, bool is_generic) -> std::optional<std::vector<semantic_default_compare_field>>
{
    auto valid = true;
    auto expect = [&](bool condition, diagnostic_kind kind, source_span span, std::string message) {
        if(condition) {
            return;
        }
        valid = false;
        report(kind, span, std::move(message));
    };
    auto is_const_reference_to_impl = [&](semantic_type_id type) {
        auto const* reference = std::get_if<reference_type>(&result.types.get(type));
        return reference != nullptr
            and reference->reference_kind == reference_type::kind::regular
            and reference->is_const
            and read_type(reference->pointee) == read_type(impl_type);
    };

    expect(
        function.default_marker and ast_source.slice(*function.default_marker) == "default",
        diagnostic_kind::invalid_operator,
        function.full_span,
        "expected '= default;' operator declaration"
    );
    expect(
        function.overload_operator == overload_operator_kind::spaceship,
        diagnostic_kind::invalid_operator,
        function.full_span,
        "only operator <=> can be defaulted"
    );
    expect(
        static_cast<bool>(struct_index),
        diagnostic_kind::invalid_operator,
        function.full_span,
        "defaulted comparison is only valid in a struct impl"
    );
    expect(
        not is_generic and not is_dependent_type(impl_type),
        diagnostic_kind::invalid_operator,
        function.full_span,
        "defaulted comparison is not supported for generic impls"
    );
    expect(
        function.parameters.size() == 2uz and parameter_types.size() == 2uz,
        diagnostic_kind::invalid_operator,
        function.full_span,
        "defaulted comparison requires self const& and rhs: this const&"
    );
    if(function.parameters.size() >= 1uz and parameter_types.size() >= 1uz) {
        auto const& self = function.parameters.front();
        expect(
            self.is_self_receiver
                and self.self_is_reference
                and self.is_const
                and is_const_reference_to_impl(parameter_types.front()),
            diagnostic_kind::invalid_self_parameter,
            self.full_span,
            "defaulted comparison must start with self const&"
        );
    }
    if(function.parameters.size() >= 2uz and parameter_types.size() >= 2uz) {
        expect(
            is_const_reference_to_impl(parameter_types[1uz]),
            diagnostic_kind::invalid_operator,
            function.parameters[1uz].full_span,
            "defaulted comparison rhs must be this const&"
        );
    }

    auto weak_type = weak_ordering_type();
    expect(
        static_cast<bool>(weak_type),
        diagnostic_kind::invalid_operator,
        function.full_span,
        "defaulted comparison requires weak_ordering"
    );
    expect(
        function.return_type and weak_type and can_implicitly_convert(return_type, *weak_type),
        diagnostic_kind::invalid_operator,
        function.full_span,
        "defaulted comparison must return weak_ordering"
    );
    if(not valid or not struct_index) {
        return std::nullopt;
    }

    return std::vector<semantic_default_compare_field>{};
}

auto semantic_analyzer::check_default_compare_operators() -> void
{
    for(auto index = 0uz; index < result.symbols.size(); ++index) {
        auto const& symbol = result.symbols[index];
        if(not semantic_function_body_is_defaulted_compare(symbol.body_kind)) {
            continue;
        }
        if(symbol.struct_index == std::numeric_limits<std::uint32_t>::max()) {
            continue;
        }

        auto old_unit = active_unit_index;
        active_unit_index = symbol.unit_index;
        auto fields = std::vector<semantic_default_compare_field>{};
        auto valid = true;
        auto const& item = result.structs[symbol.struct_index];
        fields.reserve(item.fields.size());
        for(auto field_index = 0uz; field_index < item.fields.size(); ++field_index) {
            auto field = default_compare_field(
                static_cast<std::uint32_t>(field_index),
                item.fields[field_index].type,
                item.fields[field_index].span
            );
            if(not field) {
                valid = false;
                continue;
            }
            fields.emplace_back(*field);
        }
        active_unit_index = old_unit;
        if(valid) {
            result.default_compare_fields[symbol_id{ static_cast<std::uint32_t>(index) }] = std::move(fields);
        }
    }
}

auto semantic_analyzer::collect_impl_operator(std::size_t unit_index, ast_arena const& ast, impl_syntax const& impl, semantic_type_id impl_type, std::vector<semantic_generic_parameter> const& impl_generic_parameters, function_id id) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    auto struct_index = struct_index_of(impl_type);
    auto variant_index = variant_index_of(impl_type);
    auto target_builtin = as_builtin(read_type(impl_type));
    auto builtin_impl_allowed = target_builtin;
    if(not function.overload_operator or (not struct_index and not variant_index and not builtin_impl_allowed)) {
        return;
    }

    auto impl_generic_parameter_names = generic_parameter_names(impl_generic_parameters);
    auto all_generic_parameters = implicit_function_generic_names(impl_generic_parameter_names, function);
    for(auto const& parameter : function.generic_parameters) {
        all_generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }
    implicit_function_generic_parameters[function_key(unit_index, id)] = implicit_function_generic_names(impl_generic_parameter_names, function);
    if(not impl_generic_parameters.empty()) {
        implicit_function_generic_parameter_kinds[function_key(unit_index, id)] = generic_parameter_kinds(impl_generic_parameters);
    }
    function_impl_target_patterns[function_key(unit_index, id)] = impl_type;
    if(impl.requires_clause) {
        function_impl_requires[function_key(unit_index, id)] = *impl.requires_clause;
    }
    validate_function_pack_shape(unit_index, id);
    validate_parameter_defaults(unit_index, id);
    record_function_parameter_defaults(0uz, unit_index, id);
    auto body_kind = semantic_body_kind(function);

    auto old_self = active_self_type;
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    active_self_type = impl_type;
    bind_active_function_generic_parameters(unit_index, id);
    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        parameter_types.emplace_back(lower_function_parameter_type(ast, function, index, impl_type));
    }
    auto return_type = semantic_type_ids::unit;
    auto inferred_return = false;
    if(function.return_type) {
        return_type = lower_return_type(ast, *function.return_type);
    } else if(semantic_function_body_has_source(body_kind)) {
        return_type = semantic_type_ids::inferred;
        inferred_return = true;
    }
    active_self_type = old_self;
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);

    auto has_current_type_parameter = false;
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        auto const& parameter = function.parameters[index];
        if(parameter.is_self_receiver) {
            has_current_type_parameter = true;
            if(index != 0uz) {
                report(diagnostic_kind::invalid_self_parameter, parameter.full_span, "self receiver must be the first operator parameter");
            }
            continue;
        }
        if(index < parameter_types.size() and read_type(parameter_types[index]) == impl_type) {
            has_current_type_parameter = true;
        }
    }
    if(not has_current_type_parameter) {
        report(diagnostic_kind::invalid_operator, function.full_span, "impl operator requires a self or this parameter");
    }

    auto default_compare_fields = std::optional<std::vector<semantic_default_compare_field>>{};
    if(function.defaulted) {
        default_compare_fields = validate_default_compare_operator(
            function,
            impl_type,
            struct_index,
            parameter_types,
            return_type,
            not all_generic_parameters.empty()
        );
        if(not default_compare_fields) {
            return;
        }
        body_kind = semantic_function_body_kind::defaulted_compare;
    }

    auto is_assignment = (
        *function.overload_operator == overload_operator_kind::equal
        or compound_assignment_operator(operator_token(*function.overload_operator))
    );
    if(is_assignment) {
        auto const first_is_writable_self = (
            not function.parameters.empty()
            and function.parameters.front().is_self_receiver
            and function.parameters.front().self_is_reference
            and not function.parameters.front().is_const
        );
        if(not first_is_writable_self) {
            report(diagnostic_kind::invalid_operator, function.full_span, "member assignment operator must start with self&");
        }
    }

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
        .inferred_return = inferred_return,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);
    if(not all_generic_parameters.empty()) {
        result.function_generic_parameter_counts.emplace(semantic_node_key{unit_index, id}, all_generic_parameters.size());
    }

    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto function_kind = (
        not function.parameters.empty() and function.parameters.front().is_self_receiver
            ? semantic_function_kind::member_function
            : semantic_function_kind::associated_function
    );
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = std::format("operator.{}.{}", std::to_underlying(*function.overload_operator), id.value),
        .span = function.name,
        .type = function_type_id,
        .body_kind = body_kind,
        .unit_index = unit_index,
        .function = id,
        .struct_index = struct_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .variant_index = variant_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .owner_type = builtin_impl_allowed ? impl_type : semantic_type_id{},
        .function_kind = function_kind,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);
    if(struct_index) {
        result.structs[*struct_index].operators[*function.overload_operator].emplace_back(symbol);
    } else if(variant_index) {
        result.variants[*variant_index].operators[*function.overload_operator].emplace_back(symbol);
    } else {
        auto const& state = units[unit_index];
        module_extension_operators[state.module_key][impl_type][*function.overload_operator].emplace_back(symbol);
        if(state.named_module) {
            module_extension_operator_exports[state.module_name][impl_type][*function.overload_operator].emplace_back(symbol);
        }
    }
}

auto semantic_analyzer::collect_concept_impl_declarations(std::size_t unit_index, ast_arena const& ast, concept_impl_id id) -> void
{
    active_unit_index = unit_index;
    auto const& impl = ast.node(id);
    validate_non_function_generic_parameter_packs(impl.generic_parameters, "concept impl");
    auto concept_name = ast_source.identifier(impl.concept_name.name);
    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
    if(not concept_symbol) {
        report(
            diagnostic_kind::unknown_concept,
            impl.concept_name.name,
            std::format("unknown concept '{}'", concept_name)
        );
        return;
    }

    auto impl_generic_parameters = impl.generic_parameters.empty()
        ? collect_type_pattern_parameters(ast, impl.target_type)
        : (
            impl.generic_parameters
            | std::views::transform([&](generic_parameter_syntax const& parameter) {
                return semantic_generic_parameter{ std::string{ ast_source.identifier(parameter.name) }, parameter.parameter_kind };
            }) | std::ranges::to<std::vector<semantic_generic_parameter>>()
        );
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    if(impl.generic_parameters.empty()) {
        bind_active_generic_parameters(impl_generic_parameters);
    } else {
        bind_active_generic_parameters(impl.generic_parameters);
    }
    auto target_type = lower_type(ast, impl.target_type);
    auto concept_arguments = lower_concept_arguments(unit_index, ast, impl.concept_name, *concept_symbol, target_type);
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
    auto target_builtin = as_builtin(read_type(target_type));
    auto target_array = std::holds_alternative<array_type>(result.types.get(read_type(target_type)));
    if(not struct_index_of(target_type) and not variant_index_of(target_type) and not target_builtin and not target_array) {
        report(diagnostic_kind::unknown_type, ast.span(impl.target_type), "concept impl target must be a struct, variant, array type, or builtin type");
        return;
    }
    auto key = std::tuple{ *concept_symbol, concept_arguments, target_type };
    if(concept_impl_index.contains(key)) {
        report(
            diagnostic_kind::duplicate_concept_impl,
            impl.concept_name.name,
            "duplicate concept implementation for target type"
        );
        return;
    }

    auto concept_impl = semantic_concept_impl {
        .concept_symbol = *concept_symbol,
        .concept_arguments = concept_arguments,
        .target_type = target_type,
        .unit_index = unit_index,
        .syntax = id,
    };

    auto& aliases = associated_types[target_type];
    for(auto const& alias : impl.type_aliases) {
        auto name = std::string{ ast_source.identifier(alias.name) };
        if(aliases.contains(name)) {
            report(
                diagnostic_kind::duplicate_symbol,
                alias.name,
                std::format("duplicate associated type '{}'", name)
            );
            continue;
        }
        if(not alias.value) {
            report(diagnostic_kind::expected_type, alias.full_span, "concept impl associated type requires a value");
            continue;
        }
        auto old_parameters_for_alias = std::move(active_generic_parameters);
        auto old_packs_for_alias = std::move(active_generic_parameter_packs);
        if(impl.generic_parameters.empty()) {
            bind_active_generic_parameters(impl_generic_parameters);
        } else {
            bind_active_generic_parameters(impl.generic_parameters);
        }
        auto type = lower_type(ast, *alias.value);
        active_generic_parameters = std::move(old_parameters_for_alias);
        active_generic_parameter_packs = std::move(old_packs_for_alias);
        aliases.emplace(name, type);
        concept_impl.associated_types.emplace(std::move(name), type);
    }

    auto const& concept_value = result.concepts[result.symbols[concept_symbol->value].concept_index];
    materialize_concept_default_associated_types(concept_impl, concept_value);

    for(auto function_id : impl.functions) {
        auto const& function = ast.node(function_id);
        auto impl_generic_parameter_names = generic_parameter_names(impl_generic_parameters);
        implicit_function_generic_parameters[function_key(unit_index, function_id)] = implicit_function_generic_names(impl_generic_parameter_names, function);
        if(not impl_generic_parameters.empty()) {
            implicit_function_generic_parameter_kinds[function_key(unit_index, function_id)] = generic_parameter_kinds(impl_generic_parameters);
        }
        function_impl_target_patterns[function_key(unit_index, function_id)] = target_type;
        if(impl.requires_clause) {
            function_impl_requires[function_key(unit_index, function_id)] = *impl.requires_clause;
        }
        auto symbol = collect_concept_impl_function(unit_index, ast, impl, target_type, function_id);
        if(not symbol) {
            continue;
        }
        concept_impl.functions[std::string{ ast_source.identifier(function.name) }].emplace_back(*symbol);
    }

    auto index = result.concept_impls.size();
    result.concept_impls.emplace_back(std::move(concept_impl));
    concept_impl_index.emplace(key, index);
    if(not impl_generic_parameters.empty()) {
        generic_concept_impl_indices.emplace_back(index);
        concept_impl_generic_parameters.emplace(index, std::move(impl_generic_parameters));
    }
    if(impl.requires_clause) {
        concept_impl_requires.emplace(index, *impl.requires_clause);
    }
    validate_concept_impl(result.concept_impls.back(), impl.full_span);
}

auto semantic_analyzer::collect_concept_impl_function(std::size_t unit_index, ast_arena const& ast, concept_impl_syntax const& impl, semantic_type_id target_type, function_id id) -> std::optional<symbol_id>
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    auto struct_index = struct_index_of(target_type);
    auto variant_index = variant_index_of(target_type);
    auto target_builtin = as_builtin(read_type(target_type));
    auto target_array = std::holds_alternative<array_type>(result.types.get(read_type(target_type)));

    auto name = std::string{ ast_source.identifier(function.name) };
    validate_function_pack_shape(unit_index, id);
    validate_parameter_defaults(unit_index, id);
    record_function_parameter_defaults(0uz, unit_index, id);
    auto body_kind = semantic_body_kind(function);
    auto function_kind = semantic_function_kind::associated_function;
    if(not function.parameters.empty() and ast_source.slice(function.parameters.front().name) == "self") {
        function_kind = semantic_function_kind::member_function;
        if(not function.parameters.front().is_self_receiver) {
            report(
                diagnostic_kind::invalid_self_parameter,
                function.parameters.front().full_span,
                "self parameter must use receiver syntax"
            );
        }
        auto old_self = active_self_type;
        auto old_parameters = std::move(active_generic_parameters);
        auto old_packs = std::move(active_generic_parameter_packs);
        active_self_type = target_type;
        bind_active_function_generic_parameters(unit_index, id);
        auto lowered_self = lower_parameter_type(ast, function.parameters.front(), target_type);
        active_self_type = old_self;
        active_generic_parameters = std::move(old_parameters);
        active_generic_parameter_packs = std::move(old_packs);
        if(read_type(lowered_self) != target_type) {
            report(
                diagnostic_kind::invalid_self_parameter,
                function.parameters.front().full_span,
                "self parameter must be the concept impl target type or reference"
            );
        }
        auto duplicate = associated_types[target_type].contains(name);
        if(duplicate) {
            report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate member '{}'", name));
            return std::nullopt;
        }
    } else {
        if(target_builtin or target_array) {
            report(diagnostic_kind::invalid_self_parameter, function.full_span, "array and builtin concept impl functions must use self receiver");
            return std::nullopt;
        }
        auto duplicate = false;
        if(struct_index) {
            duplicate = result.structs[*struct_index].associated_functions.contains(name);
        } else if(variant_index) {
            duplicate = result.variants[*variant_index].associated_functions.contains(name);
        }
        duplicate = duplicate or associated_types[target_type].contains(name);
        if(duplicate) {
            report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate associated function '{}'", name));
            return std::nullopt;
        }
    }

    auto old_aliases = active_type_aliases;
    auto old_self = active_self_type;
    auto old_parameters = std::move(active_generic_parameters);
    auto old_packs = std::move(active_generic_parameter_packs);
    active_type_aliases = &associated_types[target_type];
    active_self_type = target_type;
    auto all_generic_parameters = implicit_function_generic_parameters[function_key(unit_index, id)];
    for(auto const& parameter : function.generic_parameters) {
        all_generic_parameters.emplace_back(ast_source.identifier(parameter.name));
    }
    bind_active_function_generic_parameters(unit_index, id);
    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto index = 0uz; index < function.parameters.size(); ++index) {
        parameter_types.emplace_back(lower_function_parameter_type(ast, function, index, target_type));
    }
    auto return_type = function.return_type ? lower_return_type(ast, *function.return_type) : semantic_type_ids::unit;
    active_generic_parameters = std::move(old_parameters);
    active_generic_parameter_packs = std::move(old_packs);
    active_self_type = old_self;
    active_type_aliases = old_aliases;

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);
    if(not all_generic_parameters.empty()) {
        result.function_generic_parameter_counts.emplace(semantic_node_key{unit_index, id}, all_generic_parameters.size());
    }

    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .body_kind = body_kind,
        .unit_index = unit_index,
        .function = id,
        .struct_index = struct_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .variant_index = variant_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .owner_type = (target_builtin or target_array) ? target_type : semantic_type_id{},
        .function_kind = function_kind,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    if(function_kind == semantic_function_kind::member_function) {
        if(struct_index) {
            result.structs[*struct_index].methods[name].emplace_back(symbol);
        } else if(variant_index) {
            result.variants[*variant_index].methods[name].emplace_back(symbol);
        } else {
            auto const& state = units[unit_index];
            module_extension_methods[state.module_key][target_type][name].emplace_back(symbol);
            if(state.named_module) {
                module_extension_method_exports[state.module_name][target_type][name].emplace_back(symbol);
            }
        }
    } else {
        if(struct_index) {
            result.structs[*struct_index].associated_functions.emplace(name, symbol);
        } else if(variant_index) {
            result.variants[*variant_index].associated_functions.emplace(name, symbol);
        }
    }
    static_cast<void>(impl);
    return symbol;
}

auto semantic_analyzer::concept_substitution_map(semantic_concept const& concept_value, std::span<semantic_type_id const> concept_arguments) -> std::map<std::string, semantic_type_id>
{
    auto const& ast = units[concept_value.unit_index].ast;
    auto const& syntax = ast.node(concept_value.syntax);
    auto substitutions = std::map<std::string, semantic_type_id>{};
    auto count = std::min(syntax.generic_parameters.size(), concept_arguments.size());
    for(auto index = 0uz; index < count; ++index) {
        substitutions.emplace(std::string{ ast_source.identifier(syntax.generic_parameters[index].name) }, concept_arguments[index]);
    }
    return substitutions;
}

auto semantic_analyzer::materialize_concept_default_associated_types(semantic_concept_impl& impl, semantic_concept const& concept_value) -> void
{
    auto substitutions = concept_substitution_map(concept_value, impl.concept_arguments);
    auto old_substitutions = active_type_substitutions;
    active_type_substitutions = &substitutions;
    for(auto const& [name, requirement] : concept_value.associated_types) {
        if(impl.associated_types.contains(name) or not requirement.default_type) {
            continue;
        }
        auto& aliases = associated_types[impl.target_type];
        if(aliases.contains(name)) {
            report(diagnostic_kind::duplicate_symbol, requirement.span, std::format("duplicate associated type '{}'", name));
            continue;
        }
        auto const& ast = units[requirement.unit_index].ast;
        auto type = requirement_type(requirement.unit_index, ast, *requirement.default_type, impl.target_type);
        aliases.emplace(name, type);
        impl.associated_types.emplace(name, type);
    }
    active_type_substitutions = old_substitutions;
}

auto semantic_analyzer::materialize_concept_default_function(semantic_concept_impl const& impl, semantic_concept const& concept_value, std::string const& name, semantic_concept_function_requirement const& requirement) -> std::optional<symbol_id>
{
    if(not requirement.default_function) {
        return std::nullopt;
    }

    auto const& ast = units[requirement.unit_index].ast;
    auto const& function = ast.node(*requirement.default_function);
    validate_function_pack_shape(requirement.unit_index, *requirement.default_function);
    validate_parameter_defaults(requirement.unit_index, *requirement.default_function);

    auto struct_index = struct_index_of(impl.target_type);
    auto variant_index = variant_index_of(impl.target_type);
    auto target_builtin = as_builtin(read_type(impl.target_type));
    auto target_array = std::holds_alternative<array_type>(result.types.get(read_type(impl.target_type)));
    auto function_kind = semantic_function_kind::associated_function;
    if(not function.parameters.empty() and ast_source.slice(function.parameters.front().name) == "self") {
        function_kind = semantic_function_kind::member_function;
    } else if(target_builtin or target_array) {
        report(diagnostic_kind::invalid_self_parameter, function.full_span, "array and builtin concept default functions must use self receiver");
        return std::nullopt;
    }

    auto duplicate = associated_types[impl.target_type].contains(name);
    if(function_kind == semantic_function_kind::member_function) {
        if(struct_index) {
            duplicate = duplicate or result.structs[*struct_index].methods.contains(name);
        } else if(variant_index) {
            duplicate = duplicate or result.variants[*variant_index].methods.contains(name);
        } else {
            auto const& state = units[impl.unit_index];
            duplicate = duplicate or module_extension_methods[state.module_key][impl.target_type].contains(name);
        }
    } else {
        if(struct_index) {
            duplicate = duplicate or result.structs[*struct_index].associated_functions.contains(name);
        } else if(variant_index) {
            duplicate = duplicate or result.variants[*variant_index].associated_functions.contains(name);
        }
    }
    if(duplicate) {
        report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate default concept function '{}'", name));
        return std::nullopt;
    }

    auto substitutions = concept_substitution_map(concept_value, impl.concept_arguments);
    auto old_unit = active_unit_index;
    auto old_self = active_self_type;
    auto old_aliases = active_type_aliases;
    auto old_substitutions = active_type_substitutions;
    active_unit_index = requirement.unit_index;
    active_self_type = impl.target_type;
    active_type_aliases = &associated_types[impl.target_type];
    active_type_substitutions = &substitutions;

    auto parameter_types = std::vector<semantic_type_id>{};
    parameter_types.reserve(function.parameters.size());
    for(auto const& parameter : function.parameters) {
        parameter_types.emplace_back(lower_parameter_type(ast, parameter, impl.target_type));
    }
    auto return_type = function.return_type ? lower_return_type(ast, *function.return_type) : semantic_type_ids::unit;

    active_type_substitutions = old_substitutions;
    active_type_aliases = old_aliases;
    active_self_type = old_self;
    active_unit_index = old_unit;

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .body_kind = semantic_body_kind(function),
        .unit_index = requirement.unit_index,
        .function = *requirement.default_function,
        .struct_index = struct_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .variant_index = variant_index.value_or(std::numeric_limits<std::uint32_t>::max()),
        .owner_type = (target_builtin or target_array) ? impl.target_type : semantic_type_id{},
        .function_kind = function_kind,
    });

    auto context_index = next_context_index++;
    result.function_signatures.emplace(semantic_node_key{context_index, requirement.unit_index, *requirement.default_function}, signature_id);
    result.function_symbols.emplace(semantic_node_key{context_index, requirement.unit_index, *requirement.default_function}, symbol);
    record_function_parameter_defaults(context_index, requirement.unit_index, *requirement.default_function);

    if(function_kind == semantic_function_kind::member_function) {
        if(struct_index) {
            result.structs[*struct_index].methods[name].emplace_back(symbol);
        } else if(variant_index) {
            result.variants[*variant_index].methods[name].emplace_back(symbol);
        } else {
            auto const& state = units[impl.unit_index];
            module_extension_methods[state.module_key][impl.target_type][name].emplace_back(symbol);
            if(state.named_module) {
                module_extension_method_exports[state.module_name][impl.target_type][name].emplace_back(symbol);
            }
        }
    } else if(struct_index) {
        result.structs[*struct_index].associated_functions.emplace(name, symbol);
    } else if(variant_index) {
        result.variants[*variant_index].associated_functions.emplace(name, symbol);
    }

    auto key_arguments = std::vector<semantic_type_id>{ impl.target_type };
    key_arguments.insert(key_arguments.end(), impl.concept_arguments.begin(), impl.concept_arguments.end());
    auto key = semantic_function_instance_key{ requirement.unit_index, *requirement.default_function, std::move(key_arguments) };
    auto instance_index = result.function_instances.size();
    result.function_instances.emplace_back(key, context_index, symbol, signature_id, std::move(substitutions), std::map<std::string, std::vector<semantic_type_id>>{});
    result.function_instance_indices.emplace(result.function_instances.back().key, instance_index);
    result.function_instance_by_symbol.emplace(symbol, instance_index);
    concept_default_function_instance_indices.emplace_back(instance_index);
    return symbol;
}

auto semantic_analyzer::validate_concept_impl(semantic_concept_impl const& impl, source_span span) -> void
{
    auto const& concept_value = result.concepts[result.symbols[impl.concept_symbol.value].concept_index];
    auto const& concept_ast = units[concept_value.unit_index].ast;
    auto const& concept_syntax = concept_ast.node(concept_value.syntax);
    auto substitutions = std::map<std::string, semantic_type_id>{};
    auto substitution_count = std::min(concept_syntax.generic_parameters.size(), impl.concept_arguments.size());
    for(auto index = 0uz; index < substitution_count; ++index) {
        substitutions.emplace(
            std::string{ ast_source.identifier(concept_syntax.generic_parameters[index].name) },
            impl.concept_arguments[index]
        );
    }
    auto old_substitutions = active_type_substitutions;
    active_type_substitutions = &substitutions;

    validate_parent_concepts(impl, concept_value, span);

    for(auto const& bound : concept_value.type_bounds) {
        auto const& ast = units[bound.unit_index].ast;
        auto target = requirement_type(bound.unit_index, ast, bound.type, impl.target_type);
        for(auto const& concept_ref : bound.concepts) {
            if(is_dependent_type(target) or target_implements(bound.unit_index, ast, concept_ref, target)) {
                continue;
            }
            report(
                diagnostic_kind::missing_concept_item,
                bound.span,
                std::format("required type does not implement concept '{}'", ast_source.identifier(concept_ref.name))
            );
        }
    }

    for(auto const& equality : concept_value.type_equalities) {
        auto const& ast = units[equality.unit_index].ast;
        auto left = requirement_type(equality.unit_index, ast, equality.left, impl.target_type);
        auto right = requirement_type(equality.unit_index, ast, equality.right, impl.target_type);
        if(is_dependent_type(left) or is_dependent_type(right) or read_type(left) == read_type(right)) {
            continue;
        }
        report(diagnostic_kind::type_mismatch, equality.span, "concept requires equal associated types");
    }

    for(auto const& [name, requirement] : concept_value.associated_types) {
        if(impl.associated_types.contains(name)) {
            continue;
        }
        if(requirement.default_type) {
            auto const& ast = units[requirement.unit_index].ast;
            associated_types[impl.target_type].emplace(
                name,
                requirement_type(requirement.unit_index, ast, *requirement.default_type, impl.target_type)
            );
            continue;
        }
        report(
            diagnostic_kind::missing_concept_item,
            requirement.span,
            std::format("concept implementation is missing associated type '{}'", name)
        );
    }

    for(auto const& [name, requirement] : concept_value.functions) {
        auto actual_symbols = std::vector<symbol_id>{};
        if(auto found = impl.functions.find(name); found != impl.functions.end()) {
            actual_symbols.insert(actual_symbols.end(), found->second.begin(), found->second.end());
        } else if(auto struct_index = struct_index_of(impl.target_type)) {
            auto const& target = result.structs[*struct_index];
            if(auto method = target.methods.find(name); method != target.methods.end()) {
                actual_symbols.insert(actual_symbols.end(), method->second.begin(), method->second.end());
            } else if(auto function = target.associated_functions.find(name); function != target.associated_functions.end()) {
                actual_symbols.emplace_back(function->second);
            }
        }

        if(actual_symbols.empty()) {
            if(requirement.default_function) {
                materialize_concept_default_function(impl, concept_value, name, requirement);
            } else {
                report(
                    diagnostic_kind::missing_concept_item,
                    requirement.span,
                    std::format("concept implementation is missing function '{}'", name)
                );
            }
            continue;
        }

        auto matched = false;
        for(auto actual_symbol : actual_symbols) {
            auto const* actual = std::get_if<function_type>(&result.types.get(result.symbols[actual_symbol.value].type));
            if(actual == nullptr) {
                continue;
            }
            matched = signatures_match(function_signature{ .parameters = actual->parameters, .returns = actual->returns }, requirement, impl.target_type);
            if(matched) {
                break;
            }
        }
        if(not matched) {
            report(
                diagnostic_kind::type_mismatch,
                result.symbols[actual_symbols.front().value].span,
                std::format("concept function '{}' signature does not match requirement", name)
            );
        }
    }

    active_type_substitutions = old_substitutions;
}

auto semantic_analyzer::lower_concept_arguments(std::size_t unit_index, ast_arena const& ast, concept_id_syntax const& concept_ref, symbol_id concept_symbol, semantic_type_id self_type) -> std::vector<semantic_type_id>
{
    auto const& concept_value = result.concepts[result.symbols[concept_symbol.value].concept_index];
    auto const& concept_ast = units[concept_value.unit_index].ast;
    auto const& concept_syntax = concept_ast.node(concept_value.syntax);
    auto const has_pack = (
        not concept_syntax.generic_parameters.empty()
        and concept_syntax.generic_parameters.back().is_pack
    );
    auto minimum_count = 0uz;
    for(auto index = 0uz; index < concept_syntax.generic_parameters.size(); ++index) {
        auto const& parameter = concept_syntax.generic_parameters[index];
        if(parameter.is_pack) {
            break;
        }
        if(not parameter.default_argument) {
            minimum_count = index + 1uz;
        }
    }
    if(concept_ref.arguments.size() < minimum_count or (not has_pack and concept_ref.arguments.size() > concept_syntax.generic_parameters.size())) {
        report(
            diagnostic_kind::invalid_type_argument,
            concept_ref.full_span,
            std::format("concept '{}' expects {} type argument(s)", concept_value.name, concept_syntax.generic_parameters.size())
        );
        return {};
    }

    auto old_unit_index = active_unit_index;
    auto old_self = active_self_type;
    auto old_substitutions = active_type_substitutions;
    active_unit_index = unit_index;
    active_self_type = self_type;

    auto substitutions = std::map<std::string, semantic_type_id>{};
    auto arguments = std::vector<semantic_type_id>{};
    for(auto index = 0uz; index < concept_syntax.generic_parameters.size(); ++index) {
        auto const& parameter = concept_syntax.generic_parameters[index];
        if(parameter.is_pack) {
            for(auto argument_index = index; argument_index < concept_ref.arguments.size(); ++argument_index) {
                auto const& argument = concept_ref.arguments[argument_index];
                if(auto const* type_argument = std::get_if<type_argument_type_syntax>(&argument); type_argument != nullptr and type_argument->is_pack_expansion) {
                    append_type_pack_expansion_argument(ast, *type_argument, concept_ref.full_span, arguments);
                } else {
                    arguments.emplace_back(lower_generic_type_argument(
                        ast,
                        argument,
                        parameter.parameter_kind,
                        concept_ref.full_span
                    ));
                }
            }
            break;
        }

        auto argument = semantic_type_id{};
        if(index < concept_ref.arguments.size()) {
            if(auto const* type_argument = std::get_if<type_argument_type_syntax>(&concept_ref.arguments[index]); type_argument != nullptr and type_argument->is_pack_expansion) {
                report(diagnostic_kind::invalid_type_argument, type_argument->full_span, "type argument pack expansion requires a concept parameter pack");
                argument = semantic_type_ids::error;
                arguments.emplace_back(argument);
                substitutions.emplace(std::string{ ast_source.identifier(parameter.name) }, argument);
                continue;
            }
            argument = lower_generic_type_argument(ast, concept_ref.arguments[index], parameter.parameter_kind, concept_ref.full_span);
        } else if(parameter.default_argument) {
            active_unit_index = concept_value.unit_index;
            active_type_substitutions = &substitutions;
            argument = lower_generic_type_argument(concept_ast, *parameter.default_argument, parameter.parameter_kind, concept_ref.full_span);
            active_unit_index = unit_index;
        } else {
            report(
                diagnostic_kind::invalid_type_argument,
                concept_ref.full_span,
                std::format("concept '{}' expects {} type argument(s)", concept_value.name, concept_syntax.generic_parameters.size())
            );
            argument = semantic_type_ids::error;
        }
        arguments.emplace_back(argument);
        substitutions.emplace(std::string{ ast_source.identifier(parameter.name) }, argument);
    }

    active_type_substitutions = old_substitutions;
    active_self_type = old_self;
    active_unit_index = old_unit_index;
    return arguments;
}

auto semantic_analyzer::validate_parent_concepts(semantic_concept_impl const& impl, semantic_concept const& concept_value, source_span span) -> void
{
    for(auto parent : concept_value.parents) {
        if(target_implements(concept_value.unit_index, units[concept_value.unit_index].ast, parent, impl.target_type)) {
            continue;
        }
        report(
            diagnostic_kind::missing_concept_item,
            span,
            std::format("target type does not implement parent concept '{}'", ast_source.identifier(parent.name))
        );
    }
}

auto semantic_analyzer::concept_impl_applies(semantic_concept_impl const& impl, semantic_type_id target_type)
    -> std::optional<std::vector<semantic_type_id>>
{
    auto arguments = lower_concept_arguments(impl.unit_index, units[impl.unit_index].ast, concept_id_syntax{
        .full_span = result.concepts[result.symbols[impl.concept_symbol.value].concept_index].span,
        .name = result.concepts[result.symbols[impl.concept_symbol.value].concept_index].span,
    }, impl.concept_symbol, target_type);
    return concept_impl_applies(impl, arguments, target_type);
}

auto semantic_analyzer::concept_impl_applies(semantic_concept_impl const& impl, std::vector<semantic_type_id> const& concept_arguments, semantic_type_id target_type)
    -> std::optional<std::vector<semantic_type_id>>
{
    auto inferred = std::map<std::uint32_t, semantic_type_id>{};
    if(not infer_type_argument(impl.target_type, target_type, inferred)) {
        return std::nullopt;
    }
    if(impl.concept_arguments.size() != concept_arguments.size()) {
        return std::nullopt;
    }
    for(auto index = 0uz; index < impl.concept_arguments.size(); ++index) {
        if(not infer_type_argument(impl.concept_arguments[index], concept_arguments[index], inferred)) {
            return std::nullopt;
        }
    }

    auto type_arguments = std::vector<semantic_type_id>{};
    for(auto const& [index, value] : inferred) {
        if(index >= type_arguments.size()) {
            type_arguments.resize(index + 1uz, semantic_type_ids::error);
        }
        type_arguments[index] = value;
    }

    auto impl_index = static_cast<std::size_t>(&impl - result.concept_impls.data());
    if(auto requirement_clause = concept_impl_requires.find(impl_index); requirement_clause != concept_impl_requires.end()) {
        auto substitutions = std::map<std::string, semantic_type_id>{};
        if(auto names = concept_impl_generic_parameters.find(impl_index); names != concept_impl_generic_parameters.end()) {
            auto count = std::min(names->second.size(), type_arguments.size());
            for(auto index = 0uz; index < count; ++index) {
                substitutions.emplace(names->second[index].name, type_arguments[index]);
            }
        }
        auto const& ast = units[impl.unit_index].ast;
        if(not requires_clause_satisfied(impl.unit_index, ast, requirement_clause->second, substitutions, target_type)) {
            return std::nullopt;
        }
    }
    return type_arguments;
}

auto semantic_analyzer::target_implements(symbol_id concept_symbol, semantic_type_id target_type) -> bool
{
    auto arguments = lower_concept_arguments(
        result.concepts[result.symbols[concept_symbol.value].concept_index].unit_index,
        units[result.concepts[result.symbols[concept_symbol.value].concept_index].unit_index].ast,
        concept_id_syntax{ .full_span = result.symbols[concept_symbol.value].span, .name = result.symbols[concept_symbol.value].span },
        concept_symbol,
        target_type
    );
    return target_implements(concept_symbol, arguments, target_type);
}

auto semantic_analyzer::target_implements(symbol_id concept_symbol, std::vector<semantic_type_id> const& concept_arguments, semantic_type_id target_type) -> bool
{
    auto const& concept_value = result.concepts[result.symbols[concept_symbol.value].concept_index];
    if(auto builtin = target_implements_builtin_concept(concept_symbol, concept_arguments, target_type)) {
        return *builtin;
    }
    if(concept_impl_index.contains(std::tuple{ concept_symbol, concept_arguments, target_type })) {
        return true;
    }

    for(auto index : generic_concept_impl_indices) {
        auto const& impl = result.concept_impls[index];
        if(impl.concept_symbol != concept_symbol) {
            continue;
        }
        if(concept_impl_applies(impl, concept_arguments, target_type)) {
            return true;
        }
    }
    return false;
}

auto semantic_analyzer::target_implements_builtin_concept(symbol_id concept_symbol, std::vector<semantic_type_id> const& concept_arguments, semantic_type_id target_type) -> std::optional<bool>
{
    auto const& concept_value = result.concepts[result.symbols[concept_symbol.value].concept_index];
    auto const& concept_name = concept_value.name;
    if(concept_name == "mutable_object") {
        return is_mutable_object_type(target_type);
    }
    if(concept_name == "ordering") {
        if(concept_arguments.size() != 1uz) {
            return false;
        }
        return is_ordering_type(target_type, concept_arguments.front(), source_span{});
    }
    if(concept_name == "equality_comparable") {
        if(concept_arguments.size() != 1uz) {
            return false;
        }
        return is_equality_comparable_type(target_type, concept_arguments.front(), source_span{});
    }
    if(concept_name == "three_way_comparable") {
        if(concept_arguments.size() != 2uz) {
            return false;
        }
        return is_three_way_comparable_type(target_type, concept_arguments[0uz], concept_arguments[1uz], source_span{});
    }
    if(concept_name == "incrementable") {
        if(not concept_arguments.empty()) {
            return false;
        }
        return is_incrementable_type(target_type, source_span{});
    }
    if(concept_name == "callable") {
        if(concept_value.unit_index >= units.size() or units[concept_value.unit_index].module_name != "std.meta") {
            return std::nullopt;
        }
        return is_callable_type(target_type, concept_arguments, source_span{});
    }
    if(
        concept_name == "is_lvalue_reference"
        or concept_name == "is_const_lvalue_reference"
        or concept_name == "is_move_reference"
    ) {
        if(concept_value.unit_index >= units.size() or units[concept_value.unit_index].module_name != "std.meta") {
            return std::nullopt;
        }
        if(not concept_arguments.empty()) {
            return false;
        }
        auto const* reference = std::get_if<reference_type>(&result.types.get(target_type));
        if(concept_name == "is_lvalue_reference") {
            return reference != nullptr and reference->reference_kind == reference_type::kind::regular;
        }
        if(concept_name == "is_const_lvalue_reference") {
            return reference != nullptr and reference->reference_kind == reference_type::kind::regular and reference->is_const;
        }
        return reference != nullptr and reference->reference_kind == reference_type::kind::move;
    }
    return std::nullopt;
}

auto semantic_analyzer::is_mutable_object_type(semantic_type_id type) const -> bool
{
    auto const& direct = result.types.get(type);
    if(std::holds_alternative<reference_type>(direct) or std::holds_alternative<function_type>(direct)) {
        return false;
    }
    auto value = read_type(type);
    auto const& kind = result.types.get(value);
    return std::visit (
        overloaded {
            [](unit_type const&) { return false; },
            [](error_type const&) { return true; },
            [](inferred_type const&) { return false; },
            [](never_type const&) { return false; },
            [](builtin_type const&) { return true; },
            [](array_type const&) { return true; },
            [](storage_type const&) { return true; },
            [](tuple_type const&) { return true; },
            [](reference_type const&) { return false; },
            [](pointer_type const&) { return true; },
            [](function_type const&) { return false; },
            [](generic_parameter_type const&) { return false; },
            [](type_pack_expansion const&) { return false; },
            [](associated_type_ref const&) { return false; },
            [](meta_type_query const&) { return false; },
            [](integer_constant_type const&) { return false; },
            [](generic_integer_parameter_type const&) { return false; },
            [](struct_type const&) { return true; },
            [](enum_type const&) { return true; },
            [](opaque_type const&) { return true; },
            [](variant_type const&) { return true; },
        },
        kind
    );
}

auto semantic_analyzer::weak_ordering_type() const -> std::optional<semantic_type_id>
{
    for(auto const& variant : result.variants) {
        if(variant.name == "weak_ordering") {
            return variant.type;
        }
    }
    return std::nullopt;
}

auto semantic_analyzer::is_ordering_type(semantic_type_id type, semantic_type_id value_type, source_span span) -> bool
{
    auto argument_type = result.types.intern(reference_type{ read_type(value_type), true });
    auto result_type = weak_ordering_type();
    if(not result_type) {
        return false;
    }
    auto arguments = std::vector<expression_info> {
        expression_info {
            .type = result.types.intern(reference_type{ read_type(type), true }),
            .is_lvalue = true,
            .is_const = true,
        },
        expression_info {
            .type = argument_type,
            .is_lvalue = true,
            .is_const = true,
        },
        expression_info {
            .type = argument_type,
            .is_lvalue = true,
            .is_const = true,
        },
    };

    if(auto closure = result.lambda_of_closure(read_type(type)); closure.valid()) {
        auto const& callable = closure.callable;
        return (
            callable.parameters.size() == 2uz
            and can_implicitly_convert(arguments[1uz], callable.parameters[0uz])
            and can_implicitly_convert(arguments[2uz], callable.parameters[1uz])
            and can_implicitly_convert(callable.returns, *result_type)
        );
    }

    if(auto const* callable = callable_type(type)) {
        return (
            callable->parameters.size() == 2uz
            and can_implicitly_convert(arguments[1uz], callable->parameters[0uz])
            and can_implicitly_convert(arguments[2uz], callable->parameters[1uz])
            and can_implicitly_convert(callable->returns, *result_type)
        );
    }

    if(auto struct_index = struct_index_of(read_type(type))) {
        auto found = result.structs[*struct_index].operators.find(overload_operator_kind::call);
        if(found != result.structs[*struct_index].operators.end()) {
            auto symbol = choose_operator(found->second, arguments, read_type(type), span);
            if(not symbol) {
                return false;
            }
            auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol->value].type));
            return callable != nullptr and can_implicitly_convert(callable->returns, *result_type);
        }
    }
    if(auto variant_index = variant_index_of(read_type(type))) {
        auto found = result.variants[*variant_index].operators.find(overload_operator_kind::call);
        if(found != result.variants[*variant_index].operators.end()) {
            auto symbol = choose_operator(found->second, arguments, read_type(type), span);
            if(not symbol) {
                return false;
            }
            auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol->value].type));
            return callable != nullptr and can_implicitly_convert(callable->returns, *result_type);
        }
    }
    return false;
}

auto semantic_analyzer::is_equality_comparable_type(semantic_type_id type, semantic_type_id rhs_type, source_span span) -> bool
{
    auto left_value = read_type(type);
    auto right_value = read_type(rhs_type);
    if(try_builtin_binary_operator(token_kind::equal_equal, left_value, right_value)) {
        return true;
    }

    auto left_argument = result.types.intern(reference_type{ left_value, true });
    auto right_argument = result.types.intern(reference_type{ right_value, true });
    auto arguments = std::vector<expression_info> {
        expression_info {
            .type = left_argument,
            .is_lvalue = true,
            .is_const = true,
        },
        expression_info {
            .type = right_argument,
            .is_lvalue = true,
            .is_const = true,
        },
    };
    auto owners = std::array{ left_value, right_value };
    auto symbol = resolve_operator(overload_operator_kind::equal_equal, owners, arguments, span);
    if(not symbol) {
        return false;
    }
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol->value].type));
    return callable != nullptr and can_implicitly_convert(callable->returns, semantic_type_ids::bool_);
}

auto semantic_analyzer::is_three_way_comparable_type(semantic_type_id type, semantic_type_id rhs_type, semantic_type_id category_type, source_span span) -> bool
{
    auto left_value = read_type(type);
    auto right_value = read_type(rhs_type);
    if(auto builtin = try_builtin_binary_operator(token_kind::spaceship, left_value, right_value)) {
        return can_implicitly_convert(builtin->type, category_type);
    }
    auto left_argument = result.types.intern(reference_type{ left_value, true });
    auto right_argument = result.types.intern(reference_type{ right_value, true });
    auto arguments = std::vector<expression_info> {
        expression_info {
            .type = left_argument,
            .is_lvalue = true,
            .is_const = true,
        },
        expression_info {
            .type = right_argument,
            .is_lvalue = true,
            .is_const = true,
        },
    };
    auto owners = std::array{ left_value, right_value };
    auto symbol = resolve_operator(overload_operator_kind::spaceship, owners, arguments, span);
    if(not symbol) {
        return false;
    }
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol->value].type));
    return callable != nullptr and can_implicitly_convert(callable->returns, category_type);
}

auto semantic_analyzer::is_incrementable_type(semantic_type_id type, source_span span) -> bool
{
    auto value = read_type(type);
    if(auto builtin = as_builtin(value); builtin and is_integer(*builtin)) {
        return true;
    }

    auto self_type = result.types.intern(reference_type{ value });
    auto arguments = std::vector<expression_info> {
        expression_info {
            .type = self_type,
            .is_lvalue = true,
        },
    };
    auto owners = std::array{ value };
    auto symbol = resolve_operator(overload_operator_kind::prefix_plus_plus, owners, arguments, span);
    if(not symbol) {
        return false;
    }
    auto const* callable = std::get_if<function_type>(&result.types.get(result.symbols[symbol->value].type));
    return callable != nullptr and can_implicitly_convert(callable->returns, self_type);
}

auto semantic_analyzer::target_implements(std::size_t unit_index, ast_arena const& ast, concept_id_syntax const& concept_ref, semantic_type_id target_type) -> bool
{
    auto concept_name = ast_source.identifier(concept_ref.name);
    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
    if(not concept_symbol) {
        return false;
    }
    return target_implements(*concept_symbol, lower_concept_arguments(unit_index, ast, concept_ref, *concept_symbol, target_type), target_type);
}

auto semantic_analyzer::function_has_source_body(symbol_id symbol) const -> bool
{
    if(not symbol.valid()) {
        return false;
    }
    auto const& value = result.symbols[symbol.value];
    if(value.kind != symbol_kind::function) {
        return false;
    }
    return semantic_function_body_has_source(value.body_kind);
}

auto semantic_analyzer::function_is_deleted(symbol_id symbol) const -> bool
{
    if(not symbol.valid()) {
        return false;
    }
    auto const& value = result.symbols[symbol.value];
    if(value.kind != symbol_kind::function) {
        return false;
    }
    return semantic_function_body_is_deleted(value.body_kind);
}

auto semantic_analyzer::requires_clause_satisfied(std::size_t unit_index, ast_arena const& ast, concept_requires_syntax const& clause, std::map<std::string, semantic_type_id> const& substitutions, std::optional<semantic_type_id> self_type) -> bool
{
    auto old_unit_index = active_unit_index;
    auto old_substitutions = active_type_substitutions;
    auto old_pack_substitutions = active_type_pack_substitutions;
    auto old_self = active_self_type;
    active_unit_index = unit_index;
    active_type_substitutions = &substitutions;
    if(self_type) {
        active_self_type = *self_type;
    }

    auto satisfied = true;
    for(auto const& constraint : clause.constraints) {
        std::visit (
            overloaded {
                [&](concept_parent_constraint_syntax const& value) {
                    auto concept_name = ast_source.identifier(value.parent.name);
                    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                    if(not concept_symbol or not self_type or not target_implements(unit_index, ast, value.parent, *self_type)) {
                        satisfied = false;
                    }
                },
                [&](concept_type_bound_constraint_syntax const& value) {
                    if(value.is_pack) {
                        auto name = std::string{ ast_source.identifier(ast.node(value.type).name) };
                        if(active_type_pack_substitutions == nullptr) {
                            satisfied = false;
                            return;
                        }
                        auto pack = active_type_pack_substitutions->find(name);
                        if(pack == active_type_pack_substitutions->end()) {
                            satisfied = false;
                            return;
                        }
                        for(auto target : pack->second) {
                            for(auto bound : value.concept_bounds) {
                                auto concept_name = ast_source.identifier(bound.name);
                                auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                                if(not concept_symbol or not target_implements(unit_index, ast, bound, target)) {
                                    satisfied = false;
                                }
                            }
                        }
                        return;
                    }
                    auto target = lower_type(ast, value.type);
                    for(auto bound : value.concept_bounds) {
                        auto concept_name = ast_source.identifier(bound.name);
                        auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                        if(not concept_symbol or not target_implements(unit_index, ast, bound, target)) {
                            satisfied = false;
                        }
                    }
                },
                [&](concept_type_equality_constraint_syntax const& value) {
                    auto left = lower_type(ast, value.left);
                    auto right = lower_type(ast, value.right);
                    if(read_type(left) != read_type(right)) {
                        satisfied = false;
                    }
                },
            },
            constraint
        );
    }

    active_self_type = old_self;
    active_type_pack_substitutions = old_pack_substitutions;
    active_type_substitutions = old_substitutions;
    active_unit_index = old_unit_index;
    return satisfied;
}

auto semantic_analyzer::validate_requires_clauses() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto old_unit_index = active_unit_index;
        active_unit_index = unit_index;
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            auto const& function = ast.node(function_id);
            if(function.requires_clause) {
                auto old_parameters = std::move(active_generic_parameters);
                auto old_packs = std::move(active_generic_parameter_packs);
                bind_active_function_generic_parameters(unit_index, function_id);
                validate_requires_clause(unit_index, ast, *function.requires_clause);
                active_generic_parameters = std::move(old_parameters);
                active_generic_parameter_packs = std::move(old_packs);
            }
        }
        for(auto impl_id : syntax.impls) {
            auto const& impl = ast.node(impl_id);
            if(not impl.requires_clause) {
                continue;
            }
            auto old_parameters = std::move(active_generic_parameters);
            auto old_packs = std::move(active_generic_parameter_packs);
            if(impl.generic_parameters.empty()) {
                bind_active_generic_parameters(collect_type_pattern_parameters(ast, impl.type));
            } else {
                bind_active_generic_parameters(impl.generic_parameters);
            }
            auto target = lower_type(ast, impl.type);
            validate_requires_clause(unit_index, ast, *impl.requires_clause, target);
            active_generic_parameters = std::move(old_parameters);
            active_generic_parameter_packs = std::move(old_packs);
        }
        for(auto impl_id : syntax.concept_impls) {
            auto const& impl = ast.node(impl_id);
            if(not impl.requires_clause) {
                continue;
            }
            auto old_parameters = std::move(active_generic_parameters);
            auto old_packs = std::move(active_generic_parameter_packs);
            if(impl.generic_parameters.empty()) {
                bind_active_generic_parameters(collect_type_pattern_parameters(ast, impl.target_type));
            } else {
                bind_active_generic_parameters(impl.generic_parameters);
            }
            auto target = lower_type(ast, impl.target_type);
            validate_requires_clause(unit_index, ast, *impl.requires_clause, target);
            active_generic_parameters = std::move(old_parameters);
            active_generic_parameter_packs = std::move(old_packs);
        }
        active_unit_index = old_unit_index;
    }
}

auto semantic_analyzer::validate_requires_clause(std::size_t unit_index, ast_arena const& ast, concept_requires_syntax const& clause, std::optional<semantic_type_id> self_type) -> void
{
    active_unit_index = unit_index;
    for(auto const& constraint : clause.constraints) {
        std::visit (
            overloaded {
                [&](concept_parent_constraint_syntax const& value) {
                    auto concept_name = ast_source.identifier(value.parent.name);
                    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                    if(not concept_symbol) {
                        report(
                            diagnostic_kind::unknown_concept,
                            value.parent.name,
                            std::format("unknown concept '{}'", concept_name)
                        );
                        return;
                    }
                    if(not self_type or is_dependent_type(*self_type) or target_implements(unit_index, ast, value.parent, *self_type)) {
                        return;
                    }
                    report(
                        diagnostic_kind::missing_concept_item,
                        value.parent.full_span,
                        std::format("required type does not implement concept '{}'", concept_name)
                    );
                },
                [&](concept_type_bound_constraint_syntax const& value) {
                    if(value.is_pack) {
                        auto name = std::string{ ast_source.identifier(ast.node(value.type).name) };
                        if(active_type_pack_substitutions == nullptr) {
                            return;
                        }
                        auto pack = active_type_pack_substitutions->find(name);
                        if(pack == active_type_pack_substitutions->end()) {
                            report(
                                diagnostic_kind::invalid_type_argument,
                                value.full_span,
                                "requires clause references an unknown type parameter pack"
                            );
                            return;
                        }
                        for(auto target : pack->second) {
                            for(auto bound : value.concept_bounds) {
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
                                if(target_implements(unit_index, ast, bound, target)) {
                                    continue;
                                }
                                report(
                                    diagnostic_kind::missing_concept_item,
                                    value.full_span,
                                    std::format("required type does not implement concept '{}'", concept_name)
                                );
                            }
                        }
                        return;
                    }
                    auto target = lower_type(ast, value.type);
                    for(auto bound : value.concept_bounds) {
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
                        if(is_dependent_type(target) or target_implements(unit_index, ast, bound, target)) {
                            continue;
                        }
                        report(
                            diagnostic_kind::missing_concept_item,
                            value.full_span,
                            std::format("required type does not implement concept '{}'", concept_name)
                        );
                    }
                },
                [&](concept_type_equality_constraint_syntax const& value) {
                    auto left = lower_type(ast, value.left);
                    auto right = lower_type(ast, value.right);
                    if(is_dependent_type(left) or is_dependent_type(right) or read_type(left) == read_type(right)) {
                        return;
                    }
                    report(diagnostic_kind::type_mismatch, value.full_span, "requires clause expects equal types");
                },
            },
            constraint
        );
    }
}

auto semantic_analyzer::requirement_type(std::size_t unit_index, ast_arena const& ast, type_id id, semantic_type_id self_type) -> semantic_type_id
{
    auto old_unit = active_unit_index;
    auto old_self = active_self_type;
    auto old_aliases = active_type_aliases;
    active_unit_index = unit_index;
    active_self_type = self_type;
    active_type_aliases = &associated_types[self_type];
    auto result_type = lower_type(ast, id);
    active_unit_index = old_unit;
    active_self_type = old_self;
    active_type_aliases = old_aliases;
    return result_type;
}

auto semantic_analyzer::signatures_match(function_signature const& actual, semantic_concept_function_requirement const& expected, semantic_type_id self_type) -> bool
{
    if(actual.parameters.size() != expected.parameters.size()) {
        return false;
    }

    auto const& ast = units[expected.unit_index].ast;
    for(auto index = 0uz; index < expected.parameters.size(); ++index) {
        auto expected_type = semantic_type_id{};
        if(expected.parameters[index].is_self_receiver) {
            expected_type = lower_parameter_type(ast, expected.parameters[index], self_type);
        } else if(expected.parameters[index].type) {
            expected_type = requirement_type(expected.unit_index, ast, *expected.parameters[index].type, self_type);
        } else {
            return false;
        }
        if(actual.parameters[index] != expected_type) {
            return false;
        }
    }

    auto expected_return = expected.return_type
        ? requirement_type(expected.unit_index, ast, *expected.return_type, self_type)
        : semantic_type_ids::unit;
    return actual.returns == expected_return;
}
