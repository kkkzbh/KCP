module semantic:modules;

import semantic;

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
    for(auto& state : units) {
        auto const& syntax = state.root;
        if(auto local = module_functions.find(state.module_key); local != module_functions.end()) {
            state.visible_functions.insert_range(local->second);
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
            auto found_types = module_type_exports.find(module_name);
            auto found_concepts = module_concept_exports.find(module_name);
            if (
                found_functions == module_exports.end()
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
                    if(state.visible_functions.contains(name)) {
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
                    if(state.visible_types.contains(name)) {
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
                if(state.visible_concepts.contains(name)) {
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

auto semantic_analyzer::collect_concept_declaration(
    std::size_t unit_index,
    ast_arena const& ast,
    concept_id id
) -> void
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

auto semantic_analyzer::collect_concept_items(
    std::size_t unit_index,
    ast_arena const& ast,
    concept_id id
) -> void
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
                                    auto parent_name = ast_source.identifier(parent_value.name);
                                    auto parent = resolve_concept_symbol(unit_index, parent_name);
                                    if(not parent) {
                                        report(
                                            diagnostic_kind::unknown_concept,
                                            parent_value.name,
                                            std::format("unknown parent concept '{}'", parent_name)
                                        );
                                        return;
                                    }
                                    concept_value.parents.emplace_back(*parent);
                                },
                                [&](concept_type_bound_constraint_syntax const& bound_value) {
                                    auto concepts = std::vector<symbol_id>{};
                                    for(auto bound : bound_value.concept_bounds) {
                                        auto concept_name = ast_source.identifier(bound);
                                        auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                                        if(not concept_symbol) {
                                            report(
                                                diagnostic_kind::unknown_concept,
                                                bound,
                                                std::format("unknown concept '{}'", concept_name)
                                            );
                                            continue;
                                        }
                                        concepts.emplace_back(*concept_symbol);
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
                    concept_value.functions.emplace(
                        name,
                        semantic_concept_function_requirement {
                            .name = name,
                            .span = value.name,
                            .unit_index = unit_index,
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

auto semantic_analyzer::collect_struct_declaration(
    std::size_t unit_index,
    ast_arena const& ast,
    struct_id id
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
        item.generic_parameters.emplace_back(ast_source.identifier(parameter.name));
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

auto semantic_analyzer::collect_struct_fields(
    std::size_t unit_index,
    ast_arena const& ast,
    struct_id id
) -> void
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
    active_generic_parameters.clear();
    for(auto index = 0uz; index < syntax.generic_parameters.size(); ++index) {
        active_generic_parameters.emplace(
            std::string{ ast_source.identifier(syntax.generic_parameters[index].name) },
            result.types.intern(generic_parameter_type{ .index = static_cast<std::uint32_t>(index) })
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
        item.fields.emplace_back(std::move(field_name), field.name, lower_type(ast, field.type));
    }
    active_generic_parameters = std::move(old_parameters);
}

auto semantic_analyzer::collect_function_declaration(
    std::size_t unit_index,
    ast_arena const& ast,
    function_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
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

    auto parameter_types = (
        function.parameters
        | std::views::transform([&](parameter_syntax const& parameter) {
            return parameter.type
                ? lower_type(ast, *parameter.type)
                : semantic_type_ids::inferred;
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
    auto inferred_return = not function.return_type;
    auto return_type = semantic_type_ids::inferred;
    if(function.return_type) {
        return_type = lower_type(ast, *function.return_type);
    }

    auto signature_id = (
        add_signature (function_signature {
            .parameters = parameter_types,
            .returns = return_type,
            .inferred_return = inferred_return,
        })
    );
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);

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

auto semantic_analyzer::collect_impl_declarations(
    std::size_t unit_index,
    ast_arena const& ast,
    impl_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& impl = ast.node(id);
    auto type = lower_type(ast, impl.type);
    if(not struct_index_of(type)) {
        report(
            diagnostic_kind::unknown_type,
            ast.span(impl.type),
            "impl target must be a struct type"
        );
        return;
    }
    for(auto function_id : impl.functions) {
        collect_impl_function(unit_index, ast, impl, function_id);
    }
}

auto semantic_analyzer::collect_impl_function(
    std::size_t unit_index,
    ast_arena const& ast,
    impl_syntax const& impl,
    function_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    auto impl_type = lower_type(ast, impl.type);
    auto struct_index = struct_index_of(impl_type);
    if(not struct_index) {
        return;
    }
    auto& item = result.structs[*struct_index];
    auto name = std::string{ ast_source.identifier(function.name) };
    auto is_constructor = function.defaulted or name == item.name;
    auto is_destructor = function.kind == function_syntax_kind::destructor;

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
        if(name != item.name) {
            report(diagnostic_kind::invalid_destructor, function.name, "destructor name must match impl type");
        }
        if(not function.parameters.empty()) {
            report(diagnostic_kind::invalid_destructor, function.full_span, "destructor cannot have parameters");
        }
        if(function.return_type) {
            report(diagnostic_kind::invalid_destructor, function.full_span, "destructor cannot declare a return type");
        }
        if(item.destructor.valid()) {
            report(diagnostic_kind::duplicate_symbol, function.name, "struct already has a destructor");
        }
    } else if(is_constructor) {
        function_kind = semantic_function_kind::constructor;
        return_type = item.type;
        if(function.return_type) {
            report(diagnostic_kind::invalid_constructor, function.full_span, "constructor cannot declare a return type");
        }
    } else {
        if(function.return_type) {
            return_type = lower_type(ast, *function.return_type);
        } else {
            return_type = semantic_type_ids::inferred;
            inferred_return = true;
        }
        if(not function.parameters.empty() and ast_source.slice(function.parameters.front().name) == "self") {
            function_kind = semantic_function_kind::member_function;
            auto self_type = function.parameters.front().type;
            auto lowered_self = self_type ? lower_type(ast, *self_type) : semantic_type_ids::error;
            auto target = read_type(lowered_self);
            if(target != item.type) {
                report(
                    diagnostic_kind::invalid_self_parameter,
                    function.parameters.front().full_span,
                    "self parameter must be the current struct type or reference"
                );
            }
            if(item.methods.contains(name)) {
                report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate member '{}'", name));
            }
        } else if(item.associated_functions.contains(name)) {
            report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate associated function '{}'", name));
        }
    }

    auto parameter_types = (
        function.parameters
        | std::views::transform([&](parameter_syntax const& parameter) {
            return parameter.type
                ? lower_type(ast, *parameter.type)
                : semantic_type_ids::inferred;
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
    if(is_destructor) {
        parameter_types.emplace_back(result.types.intern(reference_type{ item.type }));
    }

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
        .inferred_return = inferred_return,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);

    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .unit_index = unit_index,
        .function = id,
        .struct_index = *struct_index,
        .function_kind = function_kind,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    if(function_kind == semantic_function_kind::constructor) {
        item.constructors.emplace_back(symbol);
    } else if(function_kind == semantic_function_kind::destructor) {
        item.destructor = symbol;
    } else if(function_kind == semantic_function_kind::member_function) {
        item.methods.emplace(name, symbol);
    } else {
        item.associated_functions.emplace(name, symbol);
    }
}

auto semantic_analyzer::collect_concept_impl_declarations(
    std::size_t unit_index,
    ast_arena const& ast,
    concept_impl_id id
) -> void
{
    active_unit_index = unit_index;
    auto const& impl = ast.node(id);
    auto concept_name = ast_source.identifier(impl.concept_name);
    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
    if(not concept_symbol) {
        report(
            diagnostic_kind::unknown_concept,
            impl.concept_name,
            std::format("unknown concept '{}'", concept_name)
        );
        return;
    }

    auto target_type = lower_type(ast, impl.target_type);
    if(not struct_index_of(target_type)) {
        report(diagnostic_kind::unknown_type, ast.span(impl.target_type), "concept impl target must be a struct type");
        return;
    }
    auto key = std::pair{ *concept_symbol, target_type };
    if(concept_impl_index.contains(key)) {
        report(
            diagnostic_kind::duplicate_concept_impl,
            impl.concept_name,
            "duplicate concept implementation for target type"
        );
        return;
    }

    auto concept_impl = semantic_concept_impl {
        .concept_symbol = *concept_symbol,
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
        auto type = lower_type(ast, *alias.value);
        aliases.emplace(name, type);
        concept_impl.associated_types.emplace(std::move(name), type);
    }

    for(auto function_id : impl.functions) {
        auto symbol = collect_concept_impl_function(unit_index, ast, impl, target_type, function_id);
        if(not symbol) {
            continue;
        }
        auto const& function = ast.node(function_id);
        concept_impl.functions.emplace(std::string{ ast_source.identifier(function.name) }, *symbol);
    }

    auto index = result.concept_impls.size();
    result.concept_impls.emplace_back(std::move(concept_impl));
    concept_impl_index.emplace(key, index);
    validate_concept_impl(result.concept_impls.back(), impl.full_span);
}

auto semantic_analyzer::collect_concept_impl_function(
    std::size_t unit_index,
    ast_arena const& ast,
    concept_impl_syntax const& impl,
    semantic_type_id target_type,
    function_id id
) -> std::optional<symbol_id>
{
    active_unit_index = unit_index;
    auto const& function = ast.node(id);
    auto struct_index = struct_index_of(target_type);
    if(not struct_index) {
        return std::nullopt;
    }

    auto& item = result.structs[*struct_index];
    auto name = std::string{ ast_source.identifier(function.name) };
    auto function_kind = semantic_function_kind::associated_function;
    if(not function.parameters.empty() and ast_source.slice(function.parameters.front().name) == "self") {
        function_kind = semantic_function_kind::member_function;
        auto old_self = active_self_type;
        active_self_type = target_type;
        auto self_type = function.parameters.front().type;
        auto lowered_self = self_type ? lower_type(ast, *self_type) : semantic_type_ids::error;
        active_self_type = old_self;
        if(read_type(lowered_self) != target_type) {
            report(
                diagnostic_kind::invalid_self_parameter,
                function.parameters.front().full_span,
                "self parameter must be the concept impl target type or reference"
            );
        }
        if(item.methods.contains(name)) {
            report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate member '{}'", name));
            return std::nullopt;
        }
    } else if(item.associated_functions.contains(name)) {
        report(diagnostic_kind::duplicate_symbol, function.name, std::format("duplicate associated function '{}'", name));
        return std::nullopt;
    }

    auto old_aliases = active_type_aliases;
    auto old_self = active_self_type;
    active_type_aliases = &associated_types[target_type];
    active_self_type = target_type;
    auto parameter_types = (
        function.parameters
        | std::views::transform([&](parameter_syntax const& parameter) {
            return parameter.type
                ? lower_type(ast, *parameter.type)
                : semantic_type_ids::inferred;
        }) | std::ranges::to<std::vector<semantic_type_id>>()
    );
    auto return_type = function.return_type ? lower_type(ast, *function.return_type) : semantic_type_ids::unit;
    active_self_type = old_self;
    active_type_aliases = old_aliases;

    auto signature_id = add_signature(function_signature {
        .parameters = parameter_types,
        .returns = return_type,
    });
    result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);

    auto function_type_id = intern_type(function_type {
        .parameters = parameter_types,
        .returns = return_type,
    });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::function,
        .name = name,
        .span = function.name,
        .type = function_type_id,
        .unit_index = unit_index,
        .function = id,
        .struct_index = *struct_index,
        .function_kind = function_kind,
    });
    result.function_symbols.emplace(semantic_node_key{unit_index, id}, symbol);

    if(function_kind == semantic_function_kind::member_function) {
        item.methods.emplace(name, symbol);
    } else {
        item.associated_functions.emplace(name, symbol);
    }
    static_cast<void>(impl);
    return symbol;
}

auto semantic_analyzer::validate_concept_impl(semantic_concept_impl const& impl, source_span span) -> void
{
    auto const& concept_value = result.concepts[result.symbols[impl.concept_symbol.value].concept_index];
    validate_parent_concepts(impl, concept_value, span);

    for(auto const& bound : concept_value.type_bounds) {
        auto const& ast = units[bound.unit_index].ast;
        auto target = requirement_type(bound.unit_index, ast, bound.type, impl.target_type);
        for(auto concept_symbol : bound.concepts) {
            if(target_implements(concept_symbol, target)) {
                continue;
            }
            report(
                diagnostic_kind::missing_concept_item,
                bound.span,
                std::format("required type does not implement concept '{}'", result.symbols[concept_symbol.value].name)
            );
        }
    }

    for(auto const& equality : concept_value.type_equalities) {
        auto const& ast = units[equality.unit_index].ast;
        auto left = requirement_type(equality.unit_index, ast, equality.left, impl.target_type);
        auto right = requirement_type(equality.unit_index, ast, equality.right, impl.target_type);
        if(left == right) {
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
        auto actual_symbol = std::optional<symbol_id>{};
        if(auto found = impl.functions.find(name); found != impl.functions.end()) {
            actual_symbol = found->second;
        } else if(auto struct_index = struct_index_of(impl.target_type)) {
            auto const& target = result.structs[*struct_index];
            if(auto method = target.methods.find(name); method != target.methods.end()) {
                actual_symbol = method->second;
            } else if(auto function = target.associated_functions.find(name); function != target.associated_functions.end()) {
                actual_symbol = function->second;
            }
        }

        if(not actual_symbol) {
            if(not requirement.default_function) {
                report(
                    diagnostic_kind::missing_concept_item,
                    requirement.span,
                    std::format("concept implementation is missing function '{}'", name)
                );
            }
            continue;
        }

        auto const* actual = std::get_if<function_type>(&result.types.get(result.symbols[actual_symbol->value].type));
        if(not actual) {
            report(diagnostic_kind::type_mismatch, result.symbols[actual_symbol->value].span, "concept function type mismatch");
            continue;
        }
        if(not signatures_match(function_signature{ .parameters = actual->parameters, .returns = actual->returns }, requirement, impl.target_type)) {
            report(
                diagnostic_kind::type_mismatch,
                result.symbols[actual_symbol->value].span,
                std::format("concept function '{}' signature does not match requirement", name)
            );
        }
    }
}

auto semantic_analyzer::validate_parent_concepts(
    semantic_concept_impl const& impl,
    semantic_concept const& concept_value,
    source_span span
) -> void
{
    for(auto parent : concept_value.parents) {
        if(target_implements(parent, impl.target_type)) {
            continue;
        }
        report(
            diagnostic_kind::missing_concept_item,
            span,
            std::format("target type does not implement parent concept '{}'", result.symbols[parent.value].name)
        );
    }
}

auto semantic_analyzer::target_implements(symbol_id concept_symbol, semantic_type_id target_type) const -> bool
{
    return concept_impl_index.contains(std::pair{ concept_symbol, target_type });
}

auto semantic_analyzer::validate_requires_clauses() -> void
{
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& unit = units[unit_index];
        auto const& ast = unit.ast;
        auto const& syntax = unit.root;
        for(auto function_id : syntax.functions) {
            auto const& function = ast.node(function_id);
            if(function.requires_clause) {
                validate_requires_clause(unit_index, ast, *function.requires_clause);
            }
        }
        for(auto impl_id : syntax.impls) {
            auto const& impl = ast.node(impl_id);
            if(not impl.requires_clause) {
                continue;
            }
            auto target = lower_type(ast, impl.type);
            validate_requires_clause(unit_index, ast, *impl.requires_clause, target);
        }
        for(auto impl_id : syntax.concept_impls) {
            auto const& impl = ast.node(impl_id);
            if(not impl.requires_clause) {
                continue;
            }
            auto target = lower_type(ast, impl.target_type);
            validate_requires_clause(unit_index, ast, *impl.requires_clause, target);
        }
    }
}

auto semantic_analyzer::validate_requires_clause(
    std::size_t unit_index,
    ast_arena const& ast,
    concept_requires_syntax const& clause,
    std::optional<semantic_type_id> self_type
) -> void
{
    active_unit_index = unit_index;
    for(auto const& constraint : clause.constraints) {
        std::visit (
            overloaded {
                [&](concept_parent_constraint_syntax const& value) {
                    auto concept_name = ast_source.identifier(value.name);
                    auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                    if(not concept_symbol) {
                        report(
                            diagnostic_kind::unknown_concept,
                            value.name,
                            std::format("unknown concept '{}'", concept_name)
                        );
                        return;
                    }
                    if(not self_type or is_dependent_type(*self_type) or target_implements(*concept_symbol, *self_type)) {
                        return;
                    }
                    report(
                        diagnostic_kind::missing_concept_item,
                        value.full_span,
                        std::format("required type does not implement concept '{}'", concept_name)
                    );
                },
                [&](concept_type_bound_constraint_syntax const& value) {
                    auto target = lower_type(ast, value.type);
                    for(auto bound : value.concept_bounds) {
                        auto concept_name = ast_source.identifier(bound);
                        auto concept_symbol = resolve_concept_symbol(unit_index, concept_name);
                        if(not concept_symbol) {
                            report(
                                diagnostic_kind::unknown_concept,
                                bound,
                                std::format("unknown concept '{}'", concept_name)
                            );
                            continue;
                        }
                        if(is_dependent_type(target) or target_implements(*concept_symbol, target)) {
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

auto semantic_analyzer::requirement_type(
    std::size_t unit_index,
    ast_arena const& ast,
    type_id id,
    semantic_type_id self_type
) -> semantic_type_id
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

auto semantic_analyzer::signatures_match(
    function_signature const& actual,
    semantic_concept_function_requirement const& expected,
    semantic_type_id self_type
) -> bool
{
    if(actual.parameters.size() != expected.parameters.size()) {
        return false;
    }

    auto const& ast = units[expected.unit_index].ast;
    for(auto index = 0uz; index < expected.parameters.size(); ++index) {
        if(not expected.parameters[index].type) {
            return false;
        }
        auto expected_type = requirement_type(expected.unit_index, ast, *expected.parameters[index].type, self_type);
        if(actual.parameters[index] != expected_type) {
            return false;
        }
    }

    auto expected_return = expected.return_type
        ? requirement_type(expected.unit_index, ast, *expected.return_type, self_type)
        : semantic_type_ids::unit;
    return actual.returns == expected_return;
}
