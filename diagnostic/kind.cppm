export module diagnostic:kind;

import std;

export enum class diagnostic_kind : std::uint16_t
{
    // Preprocessor diagnostics.
    unterminated_block_comment,

    // Lexer diagnostics.
    invalid_character,
    unterminated_string_literal,
    unterminated_char_literal,
    invalid_char_literal,
    invalid_escape_sequence,
    invalid_number_suffix,
    missing_exponent_digits,
    leading_zero_integer,

    // Parser diagnostics.
    unexpected_token,
    expected_token,
    expected_identifier,
    expected_expression,
    expected_statement,
    expected_type,
    empty_statement,

    // Semantic name, module, and symbol diagnostics.
    unknown_type,
    invalid_type_argument,
    duplicate_symbol,
    unknown_name,
    unexported_name,
    unknown_module,
    import_conflict,
    export_requires_module,

    // Semantic call and type diagnostics.
    not_callable,
    argument_count_mismatch,
    type_mismatch,
    condition_not_bool,
    invalid_assignment_target,
    assign_to_const,

    // Semantic control-flow diagnostics.
    invalid_break,
    invalid_continue,
    unknown_label,
    invalid_range,

    // Semantic aggregate and operator diagnostics.
    empty_aggregate_without_context,
    aggregate_length_mismatch,
    heterogeneous_aggregate,
    invalid_operator,
    invalid_cast,

    // Semantic return diagnostics.
    return_type_mismatch,
    missing_return,
    cannot_infer_return_type,
    independent_closure_capture,

    // Semantic member and object diagnostics.
    duplicate_field,
    unknown_field,
    duplicate_field_initializer,
    default_initialization_failure,
    unknown_member,
    invalid_self_parameter,
    invalid_constructor,
    invalid_destructor,
    ambiguous_constructor,

    // Semantic concept diagnostics.
    unknown_concept,
    duplicate_concept_impl,
    missing_concept_item,

    // Semantic variant diagnostics.
    unknown_variant_case,
    duplicate_variant_case,
    non_exhaustive_match,
};
