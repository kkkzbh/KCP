export module parser.ast.arena;

import std;
import source;
import parser.ast.ids;
import parser.ast.type;
import parser.ast.expr;
import parser.ast.stmt;
import parser.ast.item;

export template<typename Node>
concept ast_arena_syntax_node = (
    std::same_as<std::remove_cvref_t<Node>, expr_syntax>
    or std::same_as<std::remove_cvref_t<Node>, statement_syntax>
    or std::same_as<std::remove_cvref_t<Node>, type_syntax>
    or std::same_as<std::remove_cvref_t<Node>, function_syntax>
);

export template<typename Id>
concept ast_arena_id = (
    std::same_as<std::remove_cvref_t<Id>, expr_id>
    or std::same_as<std::remove_cvref_t<Id>, stmt_id>
    or std::same_as<std::remove_cvref_t<Id>, type_id>
    or std::same_as<std::remove_cvref_t<Id>, function_id>
);

export struct ast_arena
{
    template<ast_arena_syntax_node Node>
    auto add(Node node) -> ast_arena_id auto
    {
        using syntax_node = std::remove_cvref_t<Node>;

        if constexpr(std::same_as<syntax_node, expr_syntax>) {
            auto id = expr_id{static_cast<std::uint32_t>(expressions.size())};
            expressions.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, statement_syntax>) {
            auto id = stmt_id{static_cast<std::uint32_t>(statements.size())};
            statements.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, type_syntax>) {
            auto id = type_id{static_cast<std::uint32_t>(types.size())};
            types.emplace_back(std::move(node));
            return id;
        } else {
            auto id = function_id{static_cast<std::uint32_t>(functions.size())};
            functions.emplace_back(std::move(node));
            return id;
        }
    }

    template<ast_arena_id Id>
    auto node(this auto& self, Id id) -> decltype(auto)
    {
        using id_type = std::remove_cvref_t<Id>;

        if constexpr(std::same_as<id_type, expr_id>) {
            return (self.expressions[id.value]);
        } else if constexpr(std::same_as<id_type, stmt_id>) {
            return (self.statements[id.value]);
        } else if constexpr(std::same_as<id_type, type_id>) {
            return (self.types[id.value]);
        } else {
            return (self.functions[id.value]);
        }
    }

    template<ast_arena_id Id>
    auto span(Id id) const -> source_span
    {
        using id_type = std::remove_cvref_t<Id>;

        auto const& syntax = node(id);

        if constexpr(std::same_as<id_type, expr_id> or std::same_as<id_type, stmt_id>) {
            return std::visit([](auto const& item) { return item.full_span; }, syntax);
        } else {
            return syntax.full_span;
        }
    }

    std::vector<expr_syntax> expressions{};
    std::vector<statement_syntax> statements{};
    std::vector<type_syntax> types{};
    std::vector<function_syntax> functions{};
};
