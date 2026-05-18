export module semantic:result;

import std;
import source;
import parser.ast;
import diagnostic;
import semantic.type;
import semantic.symbol;

export template<typename Id>
concept semantic_node_key_id = (
    std::same_as<Id, expr_id>
    or std::same_as<Id, stmt_id>
    or std::same_as<Id, function_id>
);

export struct semantic_node_key
{
    constexpr semantic_node_key() = default;

    template<semantic_node_key_id Id>
    explicit constexpr semantic_node_key(std::size_t unit, Id id) :
        unit_index(unit),
        syntax_id_value(id.value) {}

    auto constexpr operator==(semantic_node_key const& other) const -> bool
    {
        return unit_index == other.unit_index and syntax_id_value == other.syntax_id_value;
    }

    auto constexpr operator<=>(semantic_node_key const& other) const -> std::strong_ordering
    {
        if(auto unit_order = unit_index <=> other.unit_index; unit_order != 0) {
            return unit_order;
        }
        return syntax_id_value <=> other.syntax_id_value;
    }

    std::size_t unit_index{};
    std::uint32_t syntax_id_value{};
};

export struct semantic_parameter_key
{
    constexpr semantic_parameter_key() = default;

    explicit constexpr semantic_parameter_key(std::size_t unit, byte_pos position) :
        unit_index(unit),
        name_start(position) {}

    auto constexpr operator==(semantic_parameter_key const& other) const -> bool
    {
        return unit_index == other.unit_index and name_start == other.name_start;
    }

    auto constexpr operator<=>(semantic_parameter_key const& other) const -> std::strong_ordering
    {
        if(auto unit_order = unit_index <=> other.unit_index; unit_order != 0) {
            return unit_order;
        }
        return name_start <=> other.name_start;
    }

    std::size_t unit_index{};
    byte_pos name_start{};
};

export struct semantic_expression_info
{
    auto constexpr operator==(semantic_expression_info const&) const -> bool = default;

    semantic_type_id type{};
    semantic_type_id read_type{};
    bool is_lvalue{};
    bool is_const{};
};

export struct semantic_field_access
{
    auto constexpr operator==(semantic_field_access const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return struct_index != invalid_index;
    }

    auto constexpr static invalid_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t struct_index{ invalid_index };
    std::uint32_t field_index{};
    bool implicit_self{};
};

export struct semantic_variant_case_access
{
    auto constexpr operator==(semantic_variant_case_access const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return variant_index != invalid_index;
    }

    auto constexpr static invalid_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t variant_index{ invalid_index };
    std::uint32_t case_index{};
};

export struct semantic_literal_value
{
    auto constexpr operator==(semantic_literal_value const&) const -> bool = default;

    std::variant<std::monostate, bool, std::int64_t, double, char, std::string> value{};
};

export enum class semantic_builtin_call_kind : std::uint8_t
{
    alloc,
    free,
    construct_at,
    destroy_at,
};

export struct semantic_builtin_call
{
    auto constexpr operator==(semantic_builtin_call const&) const -> bool = default;

    semantic_builtin_call_kind kind{};
    semantic_type_id type{};
};

export struct semantic_lambda_capture
{
    semantic_lambda_capture() = default;

    semantic_lambda_capture(
        symbol_id captured_symbol,
        std::string captured_name,
        source_span captured_span,
        semantic_type_id captured_type,
        bool captured_const
    ) :
        symbol(captured_symbol),
        name(std::move(captured_name)),
        span(captured_span),
        type(captured_type),
        is_const(captured_const) {}

    auto constexpr operator==(semantic_lambda_capture const&) const -> bool = default;

    symbol_id symbol{};
    std::string name{};
    source_span span{};
    semantic_type_id type{};
    bool is_const{};
};

export struct semantic_lambda_info
{
    auto constexpr operator==(semantic_lambda_info const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return function_symbol.valid();
    }

    function_id function{};
    symbol_id function_symbol{};
    semantic_type_id closure_type{};
    std::uint32_t closure_struct_index{ std::numeric_limits<std::uint32_t>::max() };
    symbol_id env_symbol{};
    std::vector<semantic_lambda_capture> captures{};
    function_type callable{};
};

export struct semantic_lambda_capture_access
{
    auto constexpr operator==(semantic_lambda_capture_access const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return function.valid();
    }

    function_id function{};
    std::uint32_t field_index{};
};

template<typename Map, semantic_node_key_id Id>
auto lookup_result_entry(Map const& entries, std::size_t unit, Id id) -> Map::mapped_type
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

    auto local_binding_of(std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = local_bindings.find(semantic_parameter_key{unit, name.start});
        if(found == local_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto function_symbol_of(function_id id) const -> symbol_id
    {
        return function_symbol_of(0uz, id);
    }

    auto function_symbol_of(std::size_t unit, function_id id) const -> symbol_id
    {
        return lookup_result_entry(function_symbols, unit, id);
    }

    auto parameter_binding_of(std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = parameter_bindings.find(semantic_parameter_key{unit, name.start});
        if(found == parameter_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto info_of(expr_id id) const -> semantic_expression_info
    {
        return info_of(0uz, id);
    }

    auto info_of(std::size_t unit, expr_id id) const -> semantic_expression_info
    {
        return lookup_result_entry(expression_infos, unit, id);
    }

    auto conversion_of(expr_id id) const -> semantic_type_id
    {
        return conversion_of(0uz, id);
    }

    auto conversion_of(std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_conversions, unit, id);
    }

    auto literal_of(expr_id id) const -> semantic_literal_value
    {
        return literal_of(0uz, id);
    }

    auto literal_of(std::size_t unit, expr_id id) const -> semantic_literal_value
    {
        return lookup_result_entry(literal_values, unit, id);
    }

    auto builtin_call_of(std::size_t unit, expr_id id) const -> semantic_builtin_call
    {
        return lookup_result_entry(builtin_calls, unit, id);
    }

    auto field_access_of(std::size_t unit, expr_id id) const -> semantic_field_access
    {
        return lookup_result_entry(expression_fields, unit, id);
    }

    auto variant_case_of(std::size_t unit, expr_id id) const -> semantic_variant_case_access
    {
        return lookup_result_entry(expression_variant_cases, unit, id);
    }

    auto lambda_of(std::size_t unit, function_id id) const -> semantic_lambda_info
    {
        return lookup_result_entry(lambda_infos, unit, id);
    }

    auto lambda_capture_of(std::size_t unit, expr_id id) const -> semantic_lambda_capture_access
    {
        return lookup_result_entry(lambda_capture_accesses, unit, id);
    }

    auto lambda_of_closure(semantic_type_id type) const -> semantic_lambda_info
    {
        if(not type.valid()) {
            return {};
        }
        auto const* closure = std::get_if<struct_type>(&types.get(type));
        if(closure == nullptr) {
            return {};
        }
        auto found = closure_lambda_infos.find(closure->index);
        if(found == closure_lambda_infos.end()) {
            return {};
        }
        auto lambda = lambda_infos.find(found->second);
        if(lambda == lambda_infos.end()) {
            return {};
        }
        return lambda->second;
    }

    auto pattern_binding_of(std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = pattern_bindings.find(semantic_parameter_key{unit, name.start});
        if(found == pattern_bindings.end()) {
            return {};
        }
        return found->second;
    }

    type_arena types{};
    std::vector<semantic_symbol> symbols{};
    std::vector<semantic_struct> structs{};
    std::vector<semantic_variant> variants{};
    std::vector<semantic_concept> concepts{};
    std::vector<semantic_concept_impl> concept_impls{};
    std::vector<function_signature> signatures{};
    std::vector<diagnostic> diagnostics{};
    std::map<semantic_node_key, semantic_type_id> expression_types{};
    std::map<semantic_node_key, symbol_id> expression_symbols{};
    std::map<semantic_node_key, function_signature_id> function_signatures{};
    std::map<semantic_node_key, symbol_id> statement_bindings{};
    std::map<semantic_parameter_key, symbol_id> local_bindings{};
    std::map<semantic_node_key, symbol_id> function_symbols{};
    std::map<semantic_parameter_key, symbol_id> parameter_bindings{};
    std::map<semantic_node_key, semantic_expression_info> expression_infos{};
    std::map<semantic_node_key, semantic_type_id> expression_conversions{};
    std::map<semantic_node_key, semantic_literal_value> literal_values{};
    std::map<semantic_node_key, semantic_builtin_call> builtin_calls{};
    std::map<semantic_node_key, semantic_field_access> expression_fields{};
    std::map<semantic_node_key, semantic_variant_case_access> expression_variant_cases{};
    std::map<semantic_node_key, semantic_lambda_info> lambda_infos{};
    std::map<std::uint32_t, semantic_node_key> closure_lambda_infos{};
    std::map<semantic_node_key, semantic_lambda_capture_access> lambda_capture_accesses{};
    std::map<semantic_parameter_key, symbol_id> pattern_bindings{};
};
