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
    or std::same_as<std::remove_cvref_t<Node>, type_alias_syntax>
    or std::same_as<std::remove_cvref_t<Node>, struct_syntax>
    or std::same_as<std::remove_cvref_t<Node>, enum_syntax>
    or std::same_as<std::remove_cvref_t<Node>, variant_syntax>
    or std::same_as<std::remove_cvref_t<Node>, impl_syntax>
    or std::same_as<std::remove_cvref_t<Node>, concept_syntax>
    or std::same_as<std::remove_cvref_t<Node>, concept_impl_syntax>
);

export template<typename Id>
concept ast_arena_id = (
    std::same_as<std::remove_cvref_t<Id>, expr_id>
    or std::same_as<std::remove_cvref_t<Id>, stmt_id>
    or std::same_as<std::remove_cvref_t<Id>, type_id>
    or std::same_as<std::remove_cvref_t<Id>, function_id>
    or std::same_as<std::remove_cvref_t<Id>, type_alias_id>
    or std::same_as<std::remove_cvref_t<Id>, struct_id>
    or std::same_as<std::remove_cvref_t<Id>, enum_id>
    or std::same_as<std::remove_cvref_t<Id>, variant_id>
    or std::same_as<std::remove_cvref_t<Id>, impl_id>
    or std::same_as<std::remove_cvref_t<Id>, concept_id>
    or std::same_as<std::remove_cvref_t<Id>, concept_impl_id>
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
        } else if constexpr(std::same_as<syntax_node, function_syntax>) {
            auto id = function_id{static_cast<std::uint32_t>(functions.size())};
            functions.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, type_alias_syntax>) {
            auto id = type_alias_id{static_cast<std::uint32_t>(type_aliases.size())};
            type_aliases.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, struct_syntax>) {
            auto id = struct_id{static_cast<std::uint32_t>(structs.size())};
            structs.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, enum_syntax>) {
            auto id = enum_id{static_cast<std::uint32_t>(enums.size())};
            enums.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, variant_syntax>) {
            auto id = variant_id{static_cast<std::uint32_t>(variants.size())};
            variants.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, impl_syntax>) {
            auto id = impl_id{static_cast<std::uint32_t>(impls.size())};
            impls.emplace_back(std::move(node));
            return id;
        } else if constexpr(std::same_as<syntax_node, concept_syntax>) {
            auto id = concept_id{static_cast<std::uint32_t>(concepts.size())};
            concepts.emplace_back(std::move(node));
            return id;
        } else {
            auto id = concept_impl_id{static_cast<std::uint32_t>(concept_impls.size())};
            concept_impls.emplace_back(std::move(node));
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
        } else if constexpr(std::same_as<id_type, function_id>) {
            return (self.functions[id.value]);
        } else if constexpr(std::same_as<id_type, type_alias_id>) {
            return (self.type_aliases[id.value]);
        } else if constexpr(std::same_as<id_type, struct_id>) {
            return (self.structs[id.value]);
        } else if constexpr(std::same_as<id_type, enum_id>) {
            return (self.enums[id.value]);
        } else if constexpr(std::same_as<id_type, variant_id>) {
            return (self.variants[id.value]);
        } else if constexpr(std::same_as<id_type, impl_id>) {
            return (self.impls[id.value]);
        } else if constexpr(std::same_as<id_type, concept_id>) {
            return (self.concepts[id.value]);
        } else {
            return (self.concept_impls[id.value]);
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
    std::vector<type_alias_syntax> type_aliases{};
    std::vector<struct_syntax> structs{};
    std::vector<enum_syntax> enums{};
    std::vector<variant_syntax> variants{};
    std::vector<impl_syntax> impls{};
    std::vector<concept_syntax> concepts{};
    std::vector<concept_impl_syntax> concept_impls{};
};
