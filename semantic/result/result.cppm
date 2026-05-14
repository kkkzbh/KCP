export module semantic.result;

export import semantic.result.keys;

import std;
import parser.ast;
import semantic.ty;
import semantic.symbol;
import semantic.diagnostic;

template<typename Map, typename Id>
auto lookup_result_entry(Map const& entries, std::size_t unit, Id id) -> typename Map::mapped_type
{
    auto found = entries.find(semantic_node_key{unit, id});
    if(found == entries.end()) {
        return {};
    }
    return found->second;
}

export struct semantic_result
{
    auto accepted() const -> bool
    {
        return diagnostics.empty();
    }

    auto type_of(expr_id id) const -> semantic_type_id
    {
        return type_of(0uz, id);
    }

    auto type_of(std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_types, unit, id);
    }

    auto resolved_name(expr_id id) const -> symbol_id
    {
        return resolved_name(0uz, id);
    }

    auto resolved_name(std::size_t unit, expr_id id) const -> symbol_id
    {
        return lookup_result_entry(expression_symbols, unit, id);
    }

    auto signature_of(function_id id) const -> function_signature_id
    {
        return signature_of(0uz, id);
    }

    auto signature_of(std::size_t unit, function_id id) const -> function_signature_id
    {
        return lookup_result_entry(function_signatures, unit, id);
    }

    auto binding_of(stmt_id id) const -> symbol_id
    {
        return binding_of(0uz, id);
    }

    auto binding_of(std::size_t unit, stmt_id id) const -> symbol_id
    {
        return lookup_result_entry(statement_bindings, unit, id);
    }

    type_arena types{};
    std::vector<semantic_symbol> symbols{};
    std::vector<function_signature> signatures{};
    std::vector<semantic_diagnostic> diagnostics{};
    std::map<semantic_node_key, semantic_type_id> expression_types{};
    std::map<semantic_node_key, symbol_id> expression_symbols{};
    std::map<semantic_node_key, function_signature_id> function_signatures{};
    std::map<semantic_node_key, symbol_id> statement_bindings{};
};
