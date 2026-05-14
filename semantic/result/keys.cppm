export module semantic.result.keys;

import std;
import parser.ast.ids;

export struct semantic_node_key
{
    constexpr semantic_node_key() = default;

    explicit constexpr semantic_node_key(std::size_t unit, expr_id id) :
        unit_index(unit),
        syntax_id_value(id.value) {}

    explicit constexpr semantic_node_key(std::size_t unit, stmt_id id) :
        unit_index(unit),
        syntax_id_value(id.value) {}

    explicit constexpr semantic_node_key(std::size_t unit, function_id id) :
        unit_index(unit),
        syntax_id_value(id.value) {}

    auto constexpr operator==(semantic_node_key const&) const -> bool = default;
    auto constexpr operator<=>(semantic_node_key const&) const = default;

    std::size_t unit_index{};
    std::uint32_t syntax_id_value{};
};
