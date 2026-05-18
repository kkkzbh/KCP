export module semantic.symbol;

import std;
import source;
import parser.ast;
import semantic.type;

export struct [[nodiscard]] symbol_id
{
    constexpr symbol_id() = default;

    explicit constexpr symbol_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(symbol_id const&) const -> bool = default;
    auto constexpr operator<=>(symbol_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export struct [[nodiscard]] function_signature_id
{
    constexpr function_signature_id() = default;

    explicit constexpr function_signature_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(function_signature_id const&) const -> bool = default;
    auto constexpr operator<=>(function_signature_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export enum class symbol_kind : std::uint8_t
{
    function,
    type,
    concept_,
    parameter,
    local,
};

export enum class semantic_function_kind : std::uint8_t
{
    free_function,
    constructor,
    destructor,
    member_function,
    associated_function,
};

export struct semantic_symbol
{
    auto constexpr operator==(semantic_symbol const&) const -> bool = default;

    symbol_kind kind{};
    std::string name{};
    source_span span{};
    semantic_type_id type{};
    bool exported{};
    bool is_const{};
    std::size_t unit_index{};
    function_id function{};
    std::uint32_t struct_index{ std::numeric_limits<std::uint32_t>::max() };
    std::uint32_t variant_index{ std::numeric_limits<std::uint32_t>::max() };
    std::uint32_t concept_index{ std::numeric_limits<std::uint32_t>::max() };
    semantic_function_kind function_kind{ semantic_function_kind::free_function };
};

export struct function_signature
{
    auto constexpr operator==(function_signature const&) const -> bool = default;

    std::vector<semantic_type_id> parameters{};
    semantic_type_id returns{};
    bool inferred_return{};
};

export struct semantic_struct_field
{
    semantic_struct_field() = default;

    semantic_struct_field(std::string field_name, source_span field_span, semantic_type_id field_type) :
        name(std::move(field_name)),
        span(field_span),
        type(field_type) {}

    auto constexpr operator==(semantic_struct_field const&) const -> bool = default;

    std::string name{};
    source_span span{};
    semantic_type_id type{};
};

export struct semantic_struct
{
    semantic_struct() = default;

    semantic_struct(
        std::string struct_name,
        source_span struct_span,
        semantic_type_id struct_type_id,
        bool is_exported,
        std::size_t source_unit,
        struct_id struct_syntax,
        symbol_id struct_symbol
    ) :
        name(std::move(struct_name)),
        span(struct_span),
        type(struct_type_id),
        exported(is_exported),
        unit_index(source_unit),
        syntax(struct_syntax),
        symbol(struct_symbol) {}

    auto constexpr operator==(semantic_struct const&) const -> bool = default;

    std::string name{};
    source_span span{};
    semantic_type_id type{};
    bool exported{};
    std::size_t unit_index{};
    struct_id syntax{};
    symbol_id symbol{};
    std::vector<std::string> generic_parameters{};
    std::vector<semantic_struct_field> fields{};
    std::vector<symbol_id> constructors{};
    symbol_id destructor{};
    std::map<std::string, symbol_id> methods{};
    std::map<std::string, symbol_id> associated_functions{};
};

export struct semantic_variant_case
{
    semantic_variant_case() = default;

    semantic_variant_case(
        std::string case_name,
        source_span case_span,
        std::vector<semantic_type_id> case_payload_types
    ) :
        name(std::move(case_name)),
        span(case_span),
        payload_types(std::move(case_payload_types)) {}

    auto constexpr operator==(semantic_variant_case const&) const -> bool = default;

    std::string name{};
    source_span span{};
    std::vector<semantic_type_id> payload_types{};
};

export struct semantic_variant
{
    semantic_variant() = default;

    semantic_variant(
        std::string variant_name,
        source_span variant_span,
        semantic_type_id variant_type_id,
        bool is_exported,
        std::size_t source_unit,
        variant_id variant_syntax,
        symbol_id variant_symbol
    ) :
        name(std::move(variant_name)),
        span(variant_span),
        type(variant_type_id),
        exported(is_exported),
        unit_index(source_unit),
        syntax(variant_syntax),
        symbol(variant_symbol) {}

    auto constexpr operator==(semantic_variant const&) const -> bool = default;

    std::string name{};
    source_span span{};
    semantic_type_id type{};
    bool exported{};
    std::size_t unit_index{};
    variant_id syntax{};
    symbol_id symbol{};
    std::vector<std::string> generic_parameters{};
    std::vector<semantic_variant_case> cases{};
    std::map<std::string, std::uint32_t> case_indices{};
    std::map<std::string, symbol_id> methods{};
    std::map<std::string, symbol_id> associated_functions{};
};

export struct semantic_concept_associated_type
{
    auto constexpr operator==(semantic_concept_associated_type const&) const -> bool = default;

    std::string name{};
    source_span span{};
    std::size_t unit_index{};
    std::optional<type_id> default_type{};
};

export struct semantic_concept_function_requirement
{
    auto constexpr operator==(semantic_concept_function_requirement const&) const -> bool = default;

    std::string name{};
    source_span span{};
    std::size_t unit_index{};
    std::vector<parameter_syntax> parameters{};
    std::optional<type_id> return_type{};
    std::optional<function_id> default_function{};
};

export struct semantic_concept_type_bound
{
    semantic_concept_type_bound() = default;

    semantic_concept_type_bound(
        std::size_t source_unit,
        type_id bound_type,
        std::vector<symbol_id> bound_concepts,
        source_span bound_span
    ) :
        unit_index(source_unit),
        type(bound_type),
        concepts(std::move(bound_concepts)),
        span(bound_span) {}

    auto constexpr operator==(semantic_concept_type_bound const&) const -> bool = default;

    std::size_t unit_index{};
    type_id type{};
    std::vector<symbol_id> concepts{};
    source_span span{};
};

export struct semantic_concept_type_equality
{
    semantic_concept_type_equality() = default;

    semantic_concept_type_equality(
        std::size_t source_unit,
        type_id left_type,
        type_id right_type,
        source_span equality_span
    ) :
        unit_index(source_unit),
        left(left_type),
        right(right_type),
        span(equality_span) {}

    auto constexpr operator==(semantic_concept_type_equality const&) const -> bool = default;

    std::size_t unit_index{};
    type_id left{};
    type_id right{};
    source_span span{};
};

export struct semantic_concept
{
    semantic_concept() = default;

    semantic_concept(
        std::string concept_name,
        source_span concept_span,
        bool is_exported,
        std::size_t source_unit,
        concept_id concept_syntax,
        symbol_id concept_symbol
    ) :
        name(std::move(concept_name)),
        span(concept_span),
        exported(is_exported),
        unit_index(source_unit),
        syntax(concept_syntax),
        symbol(concept_symbol) {}

    auto constexpr operator==(semantic_concept const&) const -> bool = default;

    std::string name{};
    source_span span{};
    bool exported{};
    std::size_t unit_index{};
    concept_id syntax{};
    symbol_id symbol{};
    std::vector<symbol_id> parents{};
    std::vector<semantic_concept_type_bound> type_bounds{};
    std::vector<semantic_concept_type_equality> type_equalities{};
    std::map<std::string, semantic_concept_associated_type> associated_types{};
    std::map<std::string, semantic_concept_function_requirement> functions{};
};

export struct semantic_concept_impl
{
    auto constexpr operator==(semantic_concept_impl const&) const -> bool = default;

    symbol_id concept_symbol{};
    semantic_type_id target_type{};
    std::size_t unit_index{};
    concept_impl_id syntax{};
    std::map<std::string, semantic_type_id> associated_types{};
    std::map<std::string, symbol_id> functions{};
};
