export module parser.ast.stmt;

import std;
import source;
import parser.ast.ids;
import parser.ast.item;

export struct block_statement_syntax
{
    auto constexpr operator==(block_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<stmt_id> statements{};
};

export struct declaration_statement_syntax
{
    auto constexpr operator==(declaration_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    bool is_static{};
    bool is_ref{};
    source_span name{};
    std::vector<source_span> binding_names{};
    std::optional<type_id> declared_type{};
    expr_id initializer{};
};

export struct type_alias_statement_syntax
{
    auto constexpr operator==(type_alias_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    type_id type{};
};

export struct if_statement_syntax
{
    auto constexpr operator==(if_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id condition{};
    stmt_id then_branch{};
    std::optional<stmt_id> else_branch{};
};

export struct while_statement_syntax
{
    auto constexpr operator==(while_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id condition{};
    stmt_id body{};
};

export struct do_while_statement_syntax
{
    auto constexpr operator==(do_while_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    stmt_id body{};
    expr_id condition{};
};

export struct for_statement_syntax
{
    auto constexpr operator==(for_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    bool is_ref{};
    source_span name{};
    std::optional<source_span> label{};
    expr_id range{};
    stmt_id body{};
};

export enum class template_for_binding_kind : std::uint8_t
{
    let_binding,
    const_binding,
    type_binding,
};

export struct template_for_statement_syntax
{
    auto constexpr operator==(template_for_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    template_for_binding_kind binding_kind{};
    bool is_ref{};
    source_span name{};
    source_span pack_name{};
    stmt_id body{};
};

export enum class template_if_condition_kind : std::uint8_t
{
    expression,
    type_equality,
    concept_bound,
    not_,
    and_,
    or_,
};

export struct template_if_condition_syntax
{
    auto constexpr operator==(template_if_condition_syntax const& other) const -> bool = default;

    source_span full_span{};
    template_if_condition_kind kind{};
    std::optional<expr_id> expression{};
    std::optional<type_id> left_type{};
    std::optional<type_id> right_type{};
    std::optional<concept_id_syntax> concept_bound{};
    std::uint32_t left_condition{};
    std::uint32_t right_condition{};
};

export struct template_if_branch_syntax
{
    auto constexpr operator==(template_if_branch_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::uint32_t condition{};
    stmt_id body{};
};

export struct template_if_statement_syntax
{
    auto constexpr operator==(template_if_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<template_if_condition_syntax> conditions{};
    std::vector<template_if_branch_syntax> branches{};
    std::optional<stmt_id> else_branch{};
};

export struct break_statement_syntax
{
    auto constexpr operator==(break_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<source_span> label{};
};

export struct continue_statement_syntax
{
    auto constexpr operator==(continue_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<source_span> label{};
};

export struct return_statement_syntax
{
    auto constexpr operator==(return_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<expr_id> value{};
};

export struct expression_statement_syntax
{
    auto constexpr operator==(expression_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id expression{};
};

export using statement_syntax = std::variant<
    block_statement_syntax,
    declaration_statement_syntax,
    type_alias_statement_syntax,
    if_statement_syntax,
    while_statement_syntax,
    do_while_statement_syntax,
    for_statement_syntax,
    template_for_statement_syntax,
    template_if_statement_syntax,
    break_statement_syntax,
    continue_statement_syntax,
    return_statement_syntax,
    expression_statement_syntax>;
