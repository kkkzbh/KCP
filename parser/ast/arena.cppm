export module parser.ast.arena;

import std;
import source;
import parser.ast.ids;
import parser.ast.type;
import parser.ast.expr;
import parser.ast.stmt;
import parser.ast.item;

export struct ast_arena
{
    auto add(expr_syntax node) -> expr_id
    {
        auto id = expr_id{static_cast<std::uint32_t>(expressions.size())};
        expressions.emplace_back(std::move(node));
        return id;
    }

    auto add(statement_syntax node) -> stmt_id
    {
        auto id = stmt_id{static_cast<std::uint32_t>(statements.size())};
        statements.emplace_back(std::move(node));
        return id;
    }

    auto add(type_syntax node) -> type_id
    {
        auto id = type_id{static_cast<std::uint32_t>(types.size())};
        types.emplace_back(std::move(node));
        return id;
    }

    auto add(function_syntax node) -> function_id
    {
        auto id = function_id{static_cast<std::uint32_t>(functions.size())};
        functions.emplace_back(std::move(node));
        return id;
    }

    auto expression(expr_id id) -> expr_syntax&
    {
        return expressions[id.value];
    }

    auto expression(expr_id id) const -> expr_syntax const&
    {
        return expressions[id.value];
    }

    auto statement(stmt_id id) -> statement_syntax&
    {
        return statements[id.value];
    }

    auto statement(stmt_id id) const -> statement_syntax const&
    {
        return statements[id.value];
    }

    auto type(type_id id) -> type_syntax&
    {
        return types[id.value];
    }

    auto type(type_id id) const -> type_syntax const&
    {
        return types[id.value];
    }

    auto function(function_id id) -> function_syntax&
    {
        return functions[id.value];
    }

    auto function(function_id id) const -> function_syntax const&
    {
        return functions[id.value];
    }

    auto span(type_id id) const -> source_span
    {
        return type(id).full_span;
    }

    auto span(expr_id id) const -> source_span;

    auto span(stmt_id id) const -> source_span;

    auto span(function_id id) const -> source_span
    {
        return function(id).full_span;
    }

    std::vector<expr_syntax> expressions{};
    std::vector<statement_syntax> statements{};
    std::vector<type_syntax> types{};
    std::vector<function_syntax> functions{};
};

auto ast_arena::span(expr_id id) const -> source_span
{
    return std::visit([](auto const& node) { return node.full_span; }, expression(id));
}

auto ast_arena::span(stmt_id id) const -> source_span
{
    return std::visit([](auto const& node) { return node.full_span; }, statement(id));
}
