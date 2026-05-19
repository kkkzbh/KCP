export module diagnostic:catalog;

import std;
import :record;

using namespace std::string_view_literals;

export auto spec(diagnostic_kind kind) -> diagnostic_spec
{
    using enum diagnostic_kind;
    using enum diagnostic_stage;
    using enum diagnostic_severity;
    switch(kind) {
        case unterminated_block_comment:
            return { preprocessor, error, "unterminated_block_comment"sv, "unterminated block comment"sv };
        case invalid_character:
            return { lexer, error, "invalid_character"sv, "invalid character"sv };
        case unterminated_string_literal:
            return { lexer, error, "unterminated_string_literal"sv, "unterminated string literal"sv };
        case unterminated_char_literal:
            return { lexer, error, "unterminated_char_literal"sv, "unterminated character literal"sv };
        case invalid_char_literal:
            return { lexer, error, "invalid_char_literal"sv, "invalid character literal"sv };
        case invalid_escape_sequence:
            return { lexer, error, "invalid_escape_sequence"sv, "invalid escape sequence"sv };
        case invalid_number_suffix:
            return { lexer, error, "invalid_number_suffix"sv, "invalid number suffix"sv };
        case unexpected_token:
            return { parser, error, "unexpected_token"sv, "unexpected token"sv };
        case expected_token:
            return { parser, error, "expected_token"sv, "expected token"sv };
        case expected_identifier:
            return { parser, error, "expected_identifier"sv, "expected identifier"sv };
        case expected_expression:
            return { parser, error, "expected_expression"sv, "expected expression"sv };
        case expected_statement:
            return { parser, error, "expected_statement"sv, "expected statement"sv };
        case expected_type:
            return { parser, error, "expected_type"sv, "expected type"sv };
        case unknown_type:
            return { semantic, error, "unknown_type"sv, "unknown type"sv };
        case invalid_type_argument:
            return { semantic, error, "invalid_type_argument"sv, "invalid type argument"sv };
        case duplicate_symbol:
            return { semantic, error, "duplicate_symbol"sv, "duplicate symbol"sv };
        case unknown_name:
            return { semantic, error, "unknown_name"sv, "unknown name"sv };
        case unknown_module:
            return { semantic, error, "unknown_module"sv, "unknown module"sv };
        case import_conflict:
            return { semantic, error, "import_conflict"sv, "import conflict"sv };
        case export_requires_module:
            return { semantic, error, "export_requires_module"sv, "export requires module"sv };
        case not_callable:
            return { semantic, error, "not_callable"sv, "not callable"sv };
        case argument_count_mismatch:
            return { semantic, error, "argument_count_mismatch"sv, "argument count mismatch"sv };
        case type_mismatch:
            return { semantic, error, "type_mismatch"sv, "type mismatch"sv };
        case condition_not_bool:
            return { semantic, error, "condition_not_bool"sv, "condition is not bool"sv };
        case invalid_assignment_target:
            return { semantic, error, "invalid_assignment_target"sv, "invalid assignment target"sv };
        case assign_to_const:
            return { semantic, error, "assign_to_const"sv, "cannot assign to const binding"sv };
        case invalid_break:
            return { semantic, error, "invalid_break"sv, "invalid break statement"sv };
        case invalid_continue:
            return { semantic, error, "invalid_continue"sv, "invalid continue statement"sv };
        case unknown_label:
            return { semantic, error, "unknown_label"sv, "unknown label"sv };
        case invalid_range:
            return { semantic, error, "invalid_range"sv, "invalid range"sv };
        case empty_aggregate_without_context:
            return { semantic, error, "empty_aggregate_without_context"sv, "empty aggregate requires context"sv };
        case aggregate_length_mismatch:
            return { semantic, error, "aggregate_length_mismatch"sv, "aggregate length mismatch"sv };
        case heterogeneous_aggregate:
            return { semantic, error, "heterogeneous_aggregate"sv, "heterogeneous aggregate"sv };
        case invalid_operator:
            return { semantic, error, "invalid_operator"sv, "invalid operator"sv };
        case invalid_cast:
            return { semantic, error, "invalid_cast"sv, "invalid cast"sv };
        case return_type_mismatch:
            return { semantic, error, "return_type_mismatch"sv, "return type mismatch"sv };
        case cannot_infer_return_type:
            return { semantic, error, "cannot_infer_return_type"sv, "cannot infer return type"sv };
        case duplicate_field:
            return { semantic, error, "duplicate_field"sv, "duplicate field"sv };
        case unknown_field:
            return { semantic, error, "unknown_field"sv, "unknown field"sv };
        case duplicate_field_initializer:
            return { semantic, error, "duplicate_field_initializer"sv, "duplicate field initializer"sv };
        case default_initialization_failure:
            return { semantic, error, "default_initialization_failure"sv, "default initialization failure"sv };
        case unknown_member:
            return { semantic, error, "unknown_member"sv, "unknown member"sv };
        case invalid_self_parameter:
            return { semantic, error, "invalid_self_parameter"sv, "invalid self parameter"sv };
        case invalid_constructor:
            return { semantic, error, "invalid_constructor"sv, "invalid constructor"sv };
        case invalid_destructor:
            return { semantic, error, "invalid_destructor"sv, "invalid destructor"sv };
        case ambiguous_constructor:
            return { semantic, error, "ambiguous_constructor"sv, "ambiguous constructor"sv };
        case unknown_concept:
            return { semantic, error, "unknown_concept"sv, "unknown concept"sv };
        case duplicate_concept_impl:
            return { semantic, error, "duplicate_concept_impl"sv, "duplicate concept implementation"sv };
        case missing_concept_item:
            return { semantic, error, "missing_concept_item"sv, "missing concept item"sv };
        case unknown_variant_case:
            return { semantic, error, "unknown_variant_case"sv, "unknown variant case"sv };
        case duplicate_variant_case:
            return { semantic, error, "duplicate_variant_case"sv, "duplicate variant case"sv };
        case non_exhaustive_match:
            return { semantic, error, "non_exhaustive_match"sv, "non-exhaustive match"sv };
    }

    std::unreachable();
}
