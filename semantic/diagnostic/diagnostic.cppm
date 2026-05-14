export module semantic.diagnostic;

import std;
import source;

export enum class semantic_diagnostic_code : std::uint8_t
{
    unknown_type,
    invalid_type_argument,
    duplicate_symbol,
    unknown_name,
    unknown_module,
    import_conflict,
    not_callable,
    argument_count_mismatch,
    type_mismatch,
    condition_not_bool,
    invalid_assignment_target,
    assign_to_const,
    invalid_break,
    invalid_continue,
    unknown_label,
    invalid_range,
    empty_aggregate_without_context,
    aggregate_length_mismatch,
    heterogeneous_aggregate,
    invalid_operator,
    invalid_cast,
    return_type_mismatch,
    cannot_infer_return_type,
};

export struct semantic_diagnostic
{
    auto constexpr operator==(semantic_diagnostic const&) const -> bool = default;

    semantic_diagnostic_code code{};
    source_span primary_span{};
    std::string message{};
};

export auto diagnostic_name(semantic_diagnostic_code code) -> std::string_view
{
    using enum semantic_diagnostic_code;
    switch(code) {
        case unknown_type: return "unknown_type";
        case invalid_type_argument: return "invalid_type_argument";
        case duplicate_symbol: return "duplicate_symbol";
        case unknown_name: return "unknown_name";
        case unknown_module: return "unknown_module";
        case import_conflict: return "import_conflict";
        case not_callable: return "not_callable";
        case argument_count_mismatch: return "argument_count_mismatch";
        case type_mismatch: return "type_mismatch";
        case condition_not_bool: return "condition_not_bool";
        case invalid_assignment_target: return "invalid_assignment_target";
        case assign_to_const: return "assign_to_const";
        case invalid_break: return "invalid_break";
        case invalid_continue: return "invalid_continue";
        case unknown_label: return "unknown_label";
        case invalid_range: return "invalid_range";
        case empty_aggregate_without_context: return "empty_aggregate_without_context";
        case aggregate_length_mismatch: return "aggregate_length_mismatch";
        case heterogeneous_aggregate: return "heterogeneous_aggregate";
        case invalid_operator: return "invalid_operator";
        case invalid_cast: return "invalid_cast";
        case return_type_mismatch: return "return_type_mismatch";
        case cannot_infer_return_type: return "cannot_infer_return_type";
    }
    std::unreachable();
}
