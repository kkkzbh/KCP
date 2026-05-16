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
        for(auto function_id : syntax.functions) {
            collect_function_declaration(unit_index, ast, function_id);
        }
    }
}

auto semantic_analyzer::resolve_imports() -> void
{
    for(auto& state : units) {
        auto const& syntax = state.root;
        if(auto local = module_functions.find(state.module_key); local != module_functions.end()) {
            state.visible_functions.insert_range(local->second);
        }

        for(auto const& import : syntax.imports) {
            auto module_name = ast_source.module_name(import.name);
            auto found = module_exports.find(module_name);
            if(found == module_exports.end()) {
                report(
                    diagnostic_kind::unknown_module,
                    import.full_span,
                    std::format("unknown imported module '{}'", module_name)
                );
                continue;
            }

            for(auto const& [name, symbol] : found->second) {
                if(state.visible_functions.contains(name)) {
                    report(
                        diagnostic_kind::import_conflict,
                        import.full_span,
                        std::format("imported symbol '{}' conflicts with an existing visible symbol", name)
                    );
                    continue;
                }
                state.visible_functions.emplace(name, symbol);
            }
        }
    }
}

auto semantic_analyzer::collect_function_declaration(
    std::size_t unit_index,
    ast_arena const& ast,
    function_id id
) -> void
{
    auto const& function = ast.node(id);
    auto name = std::string{ ast_source.identifier(function.name) };
    auto const& state = units[unit_index];
    auto& local_names = module_functions[state.module_key];
    if(local_names.contains(name)) {
        report(
            diagnostic_kind::duplicate_symbol,
            function.name,
            std::format("duplicate function '{}'", name)
        );
        return;
    }

    auto parameter_types = (
        function.parameters
        | std::views::transform([&](parameter_syntax const& parameter) {
            return lower_type(ast, parameter.type);
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
    local_names.emplace(std::move(name), symbol);
}
