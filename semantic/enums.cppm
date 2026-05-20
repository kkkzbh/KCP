module semantic:enums;

import semantic;

auto semantic_analyzer::constant_integer_expression(ast_arena const& ast, expr_id id) -> std::optional<std::int64_t>
{
    auto const& expression = ast.node(id);
    return std::visit (
        overloaded {
            [&](literal_expr_syntax const& literal) -> std::optional<std::int64_t> {
                auto text = ast_source.slice(literal.full_span);
                if(text.contains('.') or text.contains('e') or text.contains('E')) {
                    return std::nullopt;
                }
                return parse_integer_literal(text);
            },
            [&](unary_expr_syntax const& unary) -> std::optional<std::int64_t> {
                auto operand = constant_integer_expression(ast, unary.operand);
                if(not operand or unary.position != unary_position::prefix) {
                    return std::nullopt;
                }
                if(unary.operator_kind == token_kind::minus) {
                    return -*operand;
                }
                if(unary.operator_kind == token_kind::tilde) {
                    return ~*operand;
                }
                return std::nullopt;
            },
            [&](binary_expr_syntax const& binary) -> std::optional<std::int64_t> {
                auto left = constant_integer_expression(ast, binary.left);
                auto right = constant_integer_expression(ast, binary.right);
                if(not left or not right) {
                    return std::nullopt;
                }
                switch(binary.operator_kind) {
                    case token_kind::plus:
                        return *left + *right;
                    case token_kind::minus:
                        return *left - *right;
                    case token_kind::star:
                        return *left * *right;
                    case token_kind::slash:
                        return *right == 0 ? std::nullopt : std::optional{ *left / *right };
                    case token_kind::percent:
                        return *right == 0 ? std::nullopt : std::optional{ *left % *right };
                    case token_kind::amp:
                        return *left & *right;
                    case token_kind::pipe:
                        return *left | *right;
                    case token_kind::caret:
                        return *left ^ *right;
                    case token_kind::less_less:
                        return *right < 0 ? std::nullopt : std::optional{ *left << *right };
                    case token_kind::greater_greater:
                        return *right < 0 ? std::nullopt : std::optional{ *left >> *right };
                    default:
                        return std::nullopt;
                }
            },
            [&](grouped_expr_syntax const& grouped) {
                return constant_integer_expression(ast, grouped.expression);
            },
            [](auto const&) -> std::optional<std::int64_t> {
                return std::nullopt;
            },
        },
        expression
    );
}

auto semantic_analyzer::collect_enum_declaration(std::size_t unit_index, ast_arena const& ast, enum_id id) -> void
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
        report(diagnostic_kind::duplicate_symbol, syntax.name, std::format("duplicate top-level name '{}'", name));
        return;
    }

    auto index = static_cast<std::uint32_t>(result.enums.size());
    auto type = result.types.intern(enum_type{ .index = index });
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::type,
        .name = name,
        .span = syntax.name,
        .type = type,
        .exported = syntax.exported,
        .unit_index = unit_index,
        .enum_index = index,
    });

    result.enums.emplace_back(name, syntax.name, type, syntax.exported, unit_index, id, symbol);
    if(syntax.exported and not state.named_module) {
        report(diagnostic_kind::export_requires_module, syntax.name, "exported enum requires an export module declaration");
    }
    if(syntax.exported and state.named_module) {
        module_type_exports[state.module_name].emplace(name, symbol);
    }
    local_types.emplace(std::move(name), symbol);
}

auto semantic_analyzer::collect_type_alias_declaration(std::size_t unit_index, ast_arena const& ast, type_alias_id id) -> void
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
        report(diagnostic_kind::duplicate_symbol, syntax.name, std::format("duplicate top-level name '{}'", name));
        return;
    }
    if(not syntax.value) {
        report(diagnostic_kind::expected_type, syntax.full_span, "top-level type alias requires a value");
        return;
    }

    auto underlying = lower_type(ast, *syntax.value);
    auto index = static_cast<std::uint32_t>(result.opaque_aliases.size());
    auto type = syntax.opaque ? result.types.intern(opaque_type{ .index = index }) : underlying;
    auto symbol = add_symbol(semantic_symbol {
        .kind = symbol_kind::type,
        .name = name,
        .span = syntax.name,
        .type = type,
        .exported = syntax.exported,
        .unit_index = unit_index,
        .opaque_index = syntax.opaque ? index : std::numeric_limits<std::uint32_t>::max(),
    });

    if(syntax.opaque) {
        result.opaque_aliases.emplace_back(name, syntax.name, type, underlying, syntax.exported, unit_index, symbol);
    }
    if(syntax.exported and not state.named_module) {
        report(diagnostic_kind::export_requires_module, syntax.name, "exported type alias requires an export module declaration");
    }
    if(syntax.exported and state.named_module) {
        module_type_exports[state.module_name].emplace(name, symbol);
    }
    local_types.emplace(std::move(name), symbol);
}

auto semantic_analyzer::collect_enum_cases(std::size_t unit_index, ast_arena const& ast, enum_id id) -> void
{
    active_unit_index = unit_index;
    auto const& syntax = ast.node(id);
    auto symbol = resolve_type_symbol(unit_index, ast_source.identifier(syntax.name));
    if(not symbol) {
        return;
    }

    auto enum_index = result.symbols[symbol->value].enum_index;
    auto& item = result.enums[enum_index];
    auto underlying = lower_type(ast, syntax.underlying_type);
    auto builtin = as_builtin(read_type(underlying));
    if(not builtin or not is_integer(*builtin)) {
        report(diagnostic_kind::invalid_type_argument, ast.span(syntax.underlying_type), "enum underlying type must be an integer type");
        underlying = semantic_type_ids::error;
    }
    item.underlying_type = underlying;

    auto names = std::set<std::string>{};
    for(auto const& enum_case : syntax.cases) {
        auto name = std::string{ ast_source.identifier(enum_case.name) };
        if(names.contains(name)) {
            report(diagnostic_kind::duplicate_variant_case, enum_case.name, std::format("duplicate enum case '{}'", name));
            continue;
        }
        names.emplace(name);

        auto value = constant_integer_expression(ast, enum_case.value);
        if(not value) {
            report(diagnostic_kind::invalid_type_argument, enum_case.full_span, "enum case value must be an integer constant expression");
            continue;
        }

        auto case_index = static_cast<std::uint32_t>(item.cases.size());
        item.case_indices.emplace(name, case_index);
        item.cases.emplace_back(std::move(name), enum_case.name, *value);
    }
}

auto semantic_analyzer::enum_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* value = std::get_if<enum_type>(&kind)) {
        return value->index;
    }
    return std::nullopt;
}

auto semantic_analyzer::opaque_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* value = std::get_if<opaque_type>(&kind)) {
        return value->index;
    }
    return std::nullopt;
}

auto semantic_analyzer::underlying_type(semantic_type_id type) const -> semantic_type_id
{
    auto value = read_type(type);
    auto const& kind = result.types.get(value);
    if(auto const* enum_value = std::get_if<enum_type>(&kind)) {
        return result.enums[enum_value->index].underlying_type;
    }
    if(auto const* opaque_value = std::get_if<opaque_type>(&kind)) {
        return result.opaque_aliases[opaque_value->index].underlying_type;
    }
    return value;
}
