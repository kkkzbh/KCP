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

    template<semantic_node_key_id Id>
    explicit constexpr semantic_node_key(std::size_t context, std::size_t unit, Id id) :
        context_index(context),
        unit_index(unit),
        syntax_id_value(id.value) {}

    auto constexpr operator==(semantic_node_key const& other) const -> bool
    {
        return context_index == other.context_index
               and unit_index == other.unit_index
               and syntax_id_value == other.syntax_id_value;
    }

    auto constexpr operator<=>(semantic_node_key const& other) const -> std::strong_ordering
    {
        if(context_index < other.context_index) {
            return std::strong_ordering::less;
        }
        if(context_index > other.context_index) {
            return std::strong_ordering::greater;
        }
        if(unit_index < other.unit_index) {
            return std::strong_ordering::less;
        }
        if(unit_index > other.unit_index) {
            return std::strong_ordering::greater;
        }
        return syntax_id_value <=> other.syntax_id_value;
    }

    std::size_t context_index{};
    std::size_t unit_index{};
    std::uint32_t syntax_id_value{};
};

export struct semantic_parameter_key
{
    constexpr semantic_parameter_key() = default;

    explicit constexpr semantic_parameter_key(std::size_t unit, byte_pos position) :
        unit_index(unit),
        name_start(position) {}

    explicit constexpr semantic_parameter_key(std::size_t context, std::size_t unit, byte_pos position) :
        context_index(context),
        unit_index(unit),
        name_start(position) {}

    auto constexpr operator==(semantic_parameter_key const& other) const -> bool
    {
        return context_index == other.context_index
               and unit_index == other.unit_index
               and name_start == other.name_start;
    }

    auto constexpr operator<=>(semantic_parameter_key const& other) const -> std::strong_ordering
    {
        if(context_index < other.context_index) {
            return std::strong_ordering::less;
        }
        if(context_index > other.context_index) {
            return std::strong_ordering::greater;
        }
        if(unit_index < other.unit_index) {
            return std::strong_ordering::less;
        }
        if(unit_index > other.unit_index) {
            return std::strong_ordering::greater;
        }
        return name_start <=> other.name_start;
    }

    std::size_t context_index{};
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
        return struct_index != invalid_index or owner_type.valid();
    }

    auto constexpr static invalid_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t struct_index{ invalid_index };
    std::uint32_t field_index{};
    semantic_type_id owner_type{};
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

export struct semantic_enum_case_access
{
    auto constexpr operator==(semantic_enum_case_access const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return enum_index != invalid_index;
    }

    auto constexpr static invalid_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t enum_index{ invalid_index };
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
    new_object,
    delete_object,
    storage_slot,
    panic,
    assert_,
    unreachable,
};

export struct semantic_builtin_call
{
    auto constexpr operator==(semantic_builtin_call const&) const -> bool = default;

    semantic_builtin_call_kind kind{};
    semantic_type_id type{};
};

export enum class semantic_lambda_capture_mode : std::uint8_t
{
    const_ref,
    ref,
    copy,
    owned_mut_copy,
};

export auto constexpr semantic_lambda_capture_mode_name(semantic_lambda_capture_mode mode) -> std::string_view
{
    using enum semantic_lambda_capture_mode;
    switch(mode) {
        case const_ref: return "const_ref";
        case ref: return "ref";
        case copy: return "copy";
        case owned_mut_copy: return "owned_mut_copy";
    }
    std::unreachable();
}

export enum class semantic_lambda_escape_reason : std::uint8_t
{
    none,
    returned,
    stored,
    passed,
};

export auto constexpr semantic_lambda_escape_reason_name(semantic_lambda_escape_reason reason) -> std::string_view
{
    using enum semantic_lambda_escape_reason;
    switch(reason) {
        case none: return "none";
        case returned: return "returned";
        case stored: return "stored";
        case passed: return "passed";
    }
    std::unreachable();
}

export struct semantic_lambda_capture
{
    semantic_lambda_capture() = default;

    semantic_lambda_capture(symbol_id captured_symbol, std::string captured_name, source_span captured_span, semantic_type_id captured_type, bool captured_const) :
        symbol(captured_symbol),
        name(std::move(captured_name)),
        span(captured_span),
        type(captured_type),
        value_type(captured_type),
        is_const(captured_const) {}

    auto constexpr operator==(semantic_lambda_capture const&) const -> bool = default;

    symbol_id symbol{};
    std::string name{};
    source_span span{};
    semantic_type_id type{};
    semantic_type_id value_type{};
    semantic_lambda_capture_mode mode{ semantic_lambda_capture_mode::copy };
    semantic_lambda_escape_reason escape_reason{ semantic_lambda_escape_reason::none };
    bool is_const{};
    bool mutated{};
    bool escaped{};
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

export enum class semantic_for_range_kind : std::uint8_t
{
    array,
    iterator_protocol,
};

export struct semantic_for_range_info
{
    auto constexpr operator==(semantic_for_range_info const&) const -> bool = default;

    auto constexpr valid() const -> bool
    {
        return element_type.valid();
    }

    semantic_for_range_kind kind{ semantic_for_range_kind::array };
    semantic_type_id element_type{};
    semantic_type_id iterator_type{};
    symbol_id iter_symbol{};
    symbol_id next_symbol{};
    std::uint32_t optional_variant_index{ semantic_variant_case_access::invalid_index };
    std::uint32_t some_case_index{};
    std::uint32_t none_case_index{};
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

template<typename Map, semantic_node_key_id Id>
auto lookup_result_entry(Map const& entries, std::size_t context, std::size_t unit, Id id) -> Map::mapped_type
{
    auto found = entries.find(semantic_node_key{context, unit, id});
    if(found == entries.end()) {
        return {};
    }
    return found->second;
}

export struct semantic_function_instance_key
{
    constexpr semantic_function_instance_key() = default;

    semantic_function_instance_key(std::size_t source_unit, function_id source_function, std::vector<semantic_type_id> instance_type_arguments, std::vector<bool> instance_forward_rvalues = {}) :
        unit_index(source_unit),
        function_id_value(source_function.value),
        type_arguments(std::move(instance_type_arguments)),
        forward_rvalues(std::move(instance_forward_rvalues)) {}

    auto constexpr operator==(semantic_function_instance_key const&) const -> bool = default;
    auto constexpr operator<=>(semantic_function_instance_key const&) const = default;

    std::size_t unit_index{};
    std::uint32_t function_id_value{};
    std::vector<semantic_type_id> type_arguments{};
    std::vector<bool> forward_rvalues{};
};

export struct semantic_function_instance
{
    semantic_function_instance() = default;

    semantic_function_instance(semantic_function_instance_key instance_key, std::size_t instance_context, symbol_id instance_symbol, function_signature_id instance_signature, std::map<std::string, semantic_type_id> instance_substitutions, std::map<std::string, std::vector<semantic_type_id>> instance_pack_substitutions) :
        key(std::move(instance_key)),
        context_index(instance_context),
        symbol(instance_symbol),
        signature(instance_signature),
        substitutions(std::move(instance_substitutions)),
        pack_substitutions(std::move(instance_pack_substitutions)) {}

    auto constexpr operator==(semantic_function_instance const&) const -> bool = default;

    semantic_function_instance_key key{};
    std::size_t context_index{};
    symbol_id symbol{};
    function_signature_id signature{};
    std::map<std::string, semantic_type_id> substitutions{};
    std::map<std::string, std::vector<semantic_type_id>> pack_substitutions{};
};

export enum class semantic_template_for_expansion_kind : std::uint8_t
{
    value,
    type,
};

export struct semantic_template_for_expansion
{
    auto constexpr operator==(semantic_template_for_expansion const&) const -> bool = default;

    semantic_template_for_expansion_kind kind{};
    std::size_t context_index{};
    symbol_id binding_symbol{};
    symbol_id pack_symbol{};
    semantic_type_id bound_type{};
};

export struct semantic_result
{
    auto accepted() const -> bool
    {
        return not contains_error_diagnostic(std::span{ diagnostics });
    }

    auto type_of(expr_id id) const -> semantic_type_id
    {
        return type_of(0uz, id);
    }

    auto type_of(std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_types, unit, id);
    }

    auto type_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_types, context, unit, id);
    }

    auto resolved_name(expr_id id) const -> symbol_id
    {
        return resolved_name(0uz, id);
    }

    auto resolved_name(std::size_t unit, expr_id id) const -> symbol_id
    {
        return lookup_result_entry(expression_symbols, unit, id);
    }

    auto resolved_name(std::size_t context, std::size_t unit, expr_id id) const -> symbol_id
    {
        return lookup_result_entry(expression_symbols, context, unit, id);
    }

    auto selected_operator(std::size_t unit, expr_id id) const -> symbol_id
    {
        return lookup_result_entry(expression_operators, unit, id);
    }

    auto selected_operator(std::size_t context, std::size_t unit, expr_id id) const -> symbol_id
    {
        return lookup_result_entry(expression_operators, context, unit, id);
    }

    auto signature_of(function_id id) const -> function_signature_id
    {
        return signature_of(0uz, id);
    }

    auto signature_of(std::size_t unit, function_id id) const -> function_signature_id
    {
        return lookup_result_entry(function_signatures, unit, id);
    }

    auto signature_of(std::size_t context, std::size_t unit, function_id id) const -> function_signature_id
    {
        return lookup_result_entry(function_signatures, context, unit, id);
    }

    auto binding_of(stmt_id id) const -> symbol_id
    {
        return binding_of(0uz, id);
    }

    auto binding_of(std::size_t unit, stmt_id id) const -> symbol_id
    {
        return lookup_result_entry(statement_bindings, unit, id);
    }

    auto binding_of(std::size_t context, std::size_t unit, stmt_id id) const -> symbol_id
    {
        return lookup_result_entry(statement_bindings, context, unit, id);
    }

    auto local_binding_of(std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = local_bindings.find(semantic_parameter_key{unit, name.start});
        if(found == local_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto local_binding_of(std::size_t context, std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = local_bindings.find(semantic_parameter_key{context, unit, name.start});
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

    auto function_symbol_of(std::size_t context, std::size_t unit, function_id id) const -> symbol_id
    {
        return lookup_result_entry(function_symbols, context, unit, id);
    }

    auto parameter_binding_of(std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = parameter_bindings.find(semantic_parameter_key{unit, name.start});
        if(found == parameter_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto parameter_binding_of(std::size_t context, std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = parameter_bindings.find(semantic_parameter_key{context, unit, name.start});
        if(found == parameter_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto parameter_pack_bindings_of(std::size_t context, std::size_t unit, source_span name) const
        -> std::vector<symbol_id>
    {
        auto found = parameter_pack_bindings.find(semantic_parameter_key{context, unit, name.start});
        if(found == parameter_pack_bindings.end()) {
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

    auto info_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_expression_info
    {
        return lookup_result_entry(expression_infos, context, unit, id);
    }

    auto conversion_of(expr_id id) const -> semantic_type_id
    {
        return conversion_of(0uz, id);
    }

    auto conversion_of(std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_conversions, unit, id);
    }

    auto conversion_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_type_id
    {
        return lookup_result_entry(expression_conversions, context, unit, id);
    }

    auto nrvo_candidate_of(function_id id) const -> symbol_id
    {
        return nrvo_candidate_of(0uz, 0uz, id);
    }

    auto nrvo_candidate_of(std::size_t unit, function_id id) const -> symbol_id
    {
        return nrvo_candidate_of(0uz, unit, id);
    }

    auto nrvo_candidate_of(std::size_t context, std::size_t unit, function_id id) const -> symbol_id
    {
        return lookup_result_entry(function_nrvo_candidates, context, unit, id);
    }

    auto nrvo_return_of(stmt_id id) const -> symbol_id
    {
        return nrvo_return_of(0uz, 0uz, id);
    }

    auto nrvo_return_of(std::size_t unit, stmt_id id) const -> symbol_id
    {
        return nrvo_return_of(0uz, unit, id);
    }

    auto nrvo_return_of(std::size_t context, std::size_t unit, stmt_id id) const -> symbol_id
    {
        return lookup_result_entry(return_nrvo_candidates, context, unit, id);
    }

    auto direct_initializer_of(stmt_id id) const -> bool
    {
        return direct_initializer_of(0uz, 0uz, id);
    }

    auto direct_initializer_of(std::size_t unit, stmt_id id) const -> bool
    {
        return direct_initializer_of(0uz, unit, id);
    }

    auto direct_initializer_of(std::size_t context, std::size_t unit, stmt_id id) const -> bool
    {
        return lookup_result_entry(direct_initializers, context, unit, id);
    }

    auto literal_of(expr_id id) const -> semantic_literal_value
    {
        return literal_of(0uz, id);
    }

    auto literal_of(std::size_t unit, expr_id id) const -> semantic_literal_value
    {
        return lookup_result_entry(literal_values, unit, id);
    }

    auto literal_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_literal_value
    {
        return lookup_result_entry(literal_values, context, unit, id);
    }

    auto builtin_call_of(std::size_t unit, expr_id id) const -> semantic_builtin_call
    {
        return lookup_result_entry(builtin_calls, unit, id);
    }

    auto builtin_call_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_builtin_call
    {
        return lookup_result_entry(builtin_calls, context, unit, id);
    }

    auto field_access_of(std::size_t unit, expr_id id) const -> semantic_field_access
    {
        return lookup_result_entry(expression_fields, unit, id);
    }

    auto field_access_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_field_access
    {
        return lookup_result_entry(expression_fields, context, unit, id);
    }

    auto variant_case_of(std::size_t unit, expr_id id) const -> semantic_variant_case_access
    {
        return lookup_result_entry(expression_variant_cases, unit, id);
    }

    auto variant_case_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_variant_case_access
    {
        return lookup_result_entry(expression_variant_cases, context, unit, id);
    }

    auto enum_case_of(std::size_t unit, expr_id id) const -> semantic_enum_case_access
    {
        return lookup_result_entry(expression_enum_cases, unit, id);
    }

    auto enum_case_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_enum_case_access
    {
        return lookup_result_entry(expression_enum_cases, context, unit, id);
    }

    auto lambda_of(std::size_t unit, function_id id) const -> semantic_lambda_info
    {
        return lookup_result_entry(lambda_infos, unit, id);
    }

    auto lambda_of(std::size_t context, std::size_t unit, function_id id) const -> semantic_lambda_info
    {
        return lookup_result_entry(lambda_infos, context, unit, id);
    }

    auto lambda_capture_of(std::size_t unit, expr_id id) const -> semantic_lambda_capture_access
    {
        return lookup_result_entry(lambda_capture_accesses, unit, id);
    }

    auto lambda_capture_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_lambda_capture_access
    {
        return lookup_result_entry(lambda_capture_accesses, context, unit, id);
    }

    auto lambda_call_of(std::size_t unit, expr_id id) const -> semantic_lambda_info
    {
        return lookup_result_entry(lambda_call_infos, unit, id);
    }

    auto lambda_call_of(std::size_t context, std::size_t unit, expr_id id) const -> semantic_lambda_info
    {
        return lookup_result_entry(lambda_call_infos, context, unit, id);
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

    auto pattern_binding_of(std::size_t context, std::size_t unit, source_span name) const -> symbol_id
    {
        auto found = pattern_bindings.find(semantic_parameter_key{context, unit, name.start});
        if(found == pattern_bindings.end()) {
            return {};
        }
        return found->second;
    }

    auto function_instance_of(symbol_id symbol) const -> semantic_function_instance const*
    {
        auto found = function_instance_by_symbol.find(symbol);
        if(found == function_instance_by_symbol.end()) {
            return nullptr;
        }
        return &function_instances[found->second];
    }

    auto for_range_of(std::size_t unit, stmt_id id) const -> semantic_for_range_info
    {
        return lookup_result_entry(for_ranges, unit, id);
    }

    auto for_range_of(std::size_t context, std::size_t unit, stmt_id id) const -> semantic_for_range_info
    {
        return lookup_result_entry(for_ranges, context, unit, id);
    }

    auto template_for_expansions_of(std::size_t context, std::size_t unit, stmt_id id) const
        -> std::vector<semantic_template_for_expansion>
    {
        return lookup_result_entry(template_for_expansions, context, unit, id);
    }

    auto generic_parameter_count_of(std::size_t unit, function_id id) const -> std::size_t
    {
        auto found = function_generic_parameter_counts.find(semantic_node_key{unit, id});
        if(found == function_generic_parameter_counts.end()) {
            return 0uz;
        }
        return found->second;
    }

    auto parameter_defaults_of(symbol_id symbol) const -> std::vector<bool> const*
    {
        if(not symbol.valid()) {
            return nullptr;
        }
        auto const& value = symbols[symbol.value];
        auto context = 0uz;
        if(auto instance = function_instance_of(symbol)) {
            context = instance->context_index;
        }
        auto found = function_parameter_defaults.find(semantic_node_key{context, value.unit_index, value.function});
        if(found != function_parameter_defaults.end()) {
            return &found->second;
        }
        found = function_parameter_defaults.find(semantic_node_key{0uz, value.unit_index, value.function});
        if(found == function_parameter_defaults.end()) {
            return nullptr;
        }
        return &found->second;
    }

    type_arena types{};
    std::vector<semantic_symbol> symbols{};
    std::vector<semantic_struct> structs{};
    std::vector<semantic_enum> enums{};
    std::vector<semantic_opaque_alias> opaque_aliases{};
    std::vector<semantic_variant> variants{};
    std::vector<semantic_concept> concepts{};
    std::vector<semantic_concept_impl> concept_impls{};
    std::vector<semantic_function_instance> function_instances{};
    std::vector<function_signature> signatures{};
    std::vector<diagnostic> diagnostics{};
    std::map<semantic_function_instance_key, std::size_t> function_instance_indices{};
    std::map<symbol_id, std::size_t> function_instance_by_symbol{};
    std::map<semantic_node_key, std::size_t> function_generic_parameter_counts{};
    std::map<semantic_node_key, semantic_type_id> expression_types{};
    std::map<semantic_node_key, symbol_id> expression_symbols{};
    std::map<semantic_node_key, symbol_id> expression_operators{};
    std::map<semantic_node_key, function_signature_id> function_signatures{};
    std::map<semantic_node_key, std::vector<bool>> function_parameter_defaults{};
    std::map<semantic_node_key, symbol_id> statement_bindings{};
    std::map<semantic_parameter_key, symbol_id> local_bindings{};
    std::map<semantic_node_key, symbol_id> function_symbols{};
    std::map<semantic_parameter_key, symbol_id> parameter_bindings{};
    std::map<semantic_parameter_key, std::vector<symbol_id>> parameter_pack_bindings{};
    std::map<semantic_node_key, semantic_expression_info> expression_infos{};
    std::map<semantic_node_key, semantic_type_id> expression_conversions{};
    std::map<semantic_node_key, symbol_id> function_nrvo_candidates{};
    std::map<semantic_node_key, symbol_id> return_nrvo_candidates{};
    std::map<semantic_node_key, bool> direct_initializers{};
    std::map<semantic_node_key, semantic_literal_value> literal_values{};
    std::map<semantic_node_key, semantic_builtin_call> builtin_calls{};
    std::map<semantic_node_key, semantic_field_access> expression_fields{};
    std::map<semantic_node_key, semantic_variant_case_access> expression_variant_cases{};
    std::map<semantic_node_key, semantic_enum_case_access> expression_enum_cases{};
    std::map<semantic_node_key, semantic_lambda_info> lambda_infos{};
    std::map<semantic_node_key, semantic_lambda_info> lambda_call_infos{};
    std::map<std::uint32_t, semantic_node_key> closure_lambda_infos{};
    std::map<semantic_node_key, semantic_lambda_capture_access> lambda_capture_accesses{};
    std::map<semantic_parameter_key, symbol_id> pattern_bindings{};
    std::map<semantic_node_key, semantic_for_range_info> for_ranges{};
    std::map<semantic_node_key, std::vector<semantic_template_for_expansion>> template_for_expansions{};
};
