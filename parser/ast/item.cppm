export module parser.ast.item;

import std;
import source;
import parser.ast.ids;
import parser.ast.name;

export struct parameter_syntax
{
    auto constexpr operator==(parameter_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    bool is_pack{};
    bool is_self_receiver{};
    bool self_is_reference{};
    source_span name{};
    std::optional<type_id> type{};
};

export struct generic_parameter_syntax
{
    auto constexpr operator==(generic_parameter_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    bool is_pack{};
    std::vector<source_span> concept_bounds{};
};

export struct concept_parent_constraint_syntax
{
    auto constexpr operator==(concept_parent_constraint_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
};

export struct concept_type_bound_constraint_syntax
{
    auto constexpr operator==(concept_type_bound_constraint_syntax const& other) const -> bool = default;

    source_span full_span{};
    type_id type{};
    bool is_pack{};
    std::vector<source_span> concept_bounds{};
};

export struct concept_type_equality_constraint_syntax
{
    auto constexpr operator==(concept_type_equality_constraint_syntax const& other) const -> bool = default;

    source_span full_span{};
    type_id left{};
    type_id right{};
};

export using concept_requires_constraint_syntax = std::variant<
    concept_parent_constraint_syntax,
    concept_type_bound_constraint_syntax,
    concept_type_equality_constraint_syntax>;

export struct concept_requires_syntax
{
    auto constexpr operator==(concept_requires_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<concept_requires_constraint_syntax> constraints{};
};

export enum class function_syntax_kind : std::uint8_t
{
    free_function,
    constructor,
    destructor,
    impl_function,
    lambda,
};

export struct function_syntax
{
    auto constexpr operator==(function_syntax const& other) const -> bool = default;

    source_span full_span{};
    function_syntax_kind kind{ function_syntax_kind::free_function };
    bool exported{};
    bool defaulted{};
    bool has_body{ true };
    std::optional<source_span> extern_abi{};
    source_span name{};
    std::vector<generic_parameter_syntax> generic_parameters{};
    std::vector<parameter_syntax> parameters{};
    std::optional<type_id> return_type{};
    std::optional<concept_requires_syntax> requires_clause{};
    std::optional<source_span> default_marker{};
    stmt_id body{};
};

export struct import_syntax
{
    auto constexpr operator==(import_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    module_name_syntax name{};
};

export struct module_header_syntax
{
    auto constexpr operator==(module_header_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    module_name_syntax name{};
};

export struct struct_field_syntax
{
    auto constexpr operator==(struct_field_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    type_id type{};
};

export struct struct_syntax
{
    auto constexpr operator==(struct_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    source_span name{};
    std::vector<generic_parameter_syntax> generic_parameters{};
    std::vector<struct_field_syntax> fields{};
};

export struct variant_case_syntax
{
    auto constexpr operator==(variant_case_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    std::vector<type_id> payloads{};
};

export struct variant_syntax
{
    auto constexpr operator==(variant_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    source_span name{};
    std::vector<generic_parameter_syntax> generic_parameters{};
    std::vector<variant_case_syntax> cases{};
};

export struct type_alias_syntax
{
    auto constexpr operator==(type_alias_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    std::optional<type_id> value{};
};

export struct impl_syntax
{
    auto constexpr operator==(impl_syntax const& other) const -> bool = default;

    source_span full_span{};
    type_id type{};
    std::optional<concept_requires_syntax> requires_clause{};
    std::vector<type_alias_syntax> type_aliases{};
    std::vector<function_id> functions{};
};

export struct concept_function_requirement_syntax
{
    auto constexpr operator==(concept_function_requirement_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    std::vector<parameter_syntax> parameters{};
    std::optional<type_id> return_type{};
    std::optional<function_id> default_function{};
};

export using concept_item_syntax = std::variant<
    concept_requires_syntax,
    type_alias_syntax,
    concept_function_requirement_syntax>;

export struct concept_syntax
{
    auto constexpr operator==(concept_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    source_span name{};
    std::vector<concept_item_syntax> items{};
};

export struct concept_impl_syntax
{
    auto constexpr operator==(concept_impl_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span concept_name{};
    type_id target_type{};
    std::optional<concept_requires_syntax> requires_clause{};
    std::vector<type_alias_syntax> type_aliases{};
    std::vector<function_id> functions{};
};

export struct translation_unit_syntax
{
    auto constexpr operator==(translation_unit_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<module_header_syntax> module_header{};
    std::vector<import_syntax> imports{};
    std::vector<concept_id> concepts{};
    std::vector<struct_id> structs{};
    std::vector<variant_id> variants{};
    std::vector<impl_id> impls{};
    std::vector<concept_impl_id> concept_impls{};
    std::vector<function_id> functions{};
};
