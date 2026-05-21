export module diagnostic.catalog;

import diagnostic.kind;
import diagnostic.stage;
import diagnostic.severity;
import diagnostic.record;

export spec(kind: diagnostic_kind) -> diagnostic_spec
{
    if(kind == diagnostic_kind::invalid_character) {
        return diagnostic_spec{
            .stage = diagnostic_stage::lexer,
            .severity = diagnostic_severity::error,
            .name = "invalid_character",
            .message = "invalid character"
        };
    }
    if(kind == diagnostic_kind::invalid_number_suffix) {
        return diagnostic_spec{
            .stage = diagnostic_stage::lexer,
            .severity = diagnostic_severity::error,
            .name = "invalid_number_suffix",
            .message = "numeric literal cannot be followed by identifier characters"
        };
    }
    if(kind == diagnostic_kind::leading_zero_integer) {
        return diagnostic_spec{
            .stage = diagnostic_stage::lexer,
            .severity = diagnostic_severity::error,
            .name = "leading_zero_integer",
            .message = "integer literal cannot have leading zeroes"
        };
    }
    if(kind == diagnostic_kind::unterminated_block_comment) {
        return diagnostic_spec{
            .stage = diagnostic_stage::lexer,
            .severity = diagnostic_severity::error,
            .name = "unterminated_block_comment",
            .message = "unterminated block comment"
        };
    }
    if(kind == diagnostic_kind::expected_token) {
        return diagnostic_spec{
            .stage = diagnostic_stage::parser,
            .severity = diagnostic_severity::error,
            .name = "expected_token",
            .message = "expected token"
        };
    }
    if(kind == diagnostic_kind::expected_identifier) {
        return diagnostic_spec{
            .stage = diagnostic_stage::parser,
            .severity = diagnostic_severity::error,
            .name = "expected_identifier",
            .message = "expected identifier"
        };
    }
    if(kind == diagnostic_kind::expected_expression) {
        return diagnostic_spec{
            .stage = diagnostic_stage::parser,
            .severity = diagnostic_severity::error,
            .name = "expected_expression",
            .message = "expected expression"
        };
    }
    if(kind == diagnostic_kind::expected_statement) {
        return diagnostic_spec{
            .stage = diagnostic_stage::parser,
            .severity = diagnostic_severity::error,
            .name = "expected_statement",
            .message = "expected statement"
        };
    }
    if(kind == diagnostic_kind::duplicate_function) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "duplicate_function",
            .message = "function is already defined"
        };
    }
    if(kind == diagnostic_kind::missing_main) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "missing_main",
            .message = "program must define main"
        };
    }
    if(kind == diagnostic_kind::duplicate_parameter) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "duplicate_parameter",
            .message = "parameter is already defined"
        };
    }
    if(kind == diagnostic_kind::duplicate_local) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "duplicate_local",
            .message = "local variable is already defined in this scope"
        };
    }
    if(kind == diagnostic_kind::undeclared_variable) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "undeclared_variable",
            .message = "variable is not declared"
        };
    }
    if(kind == diagnostic_kind::undefined_function) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "undefined_function",
            .message = "function is not defined"
        };
    }
    if(kind == diagnostic_kind::argument_count_mismatch) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "argument_count_mismatch",
            .message = "function argument count does not match parameter count"
        };
    }
    if(kind == diagnostic_kind::void_value) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "void_value",
            .message = "void function call cannot be used as an expression value"
        };
    }
    if(kind == diagnostic_kind::int_return_value_missing) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "int_return_value_missing",
            .message = "int function must return a value"
        };
    }
    if(kind == diagnostic_kind::void_return_value) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "void_return_value",
            .message = "void function cannot return a value"
        };
    }
    if(kind == diagnostic_kind::constant_divide_by_zero) {
        return diagnostic_spec{
            .stage = diagnostic_stage::semantic,
            .severity = diagnostic_severity::error,
            .name = "constant_divide_by_zero",
            .message = "constant expression divides by zero"
        };
    }
    return diagnostic_spec{
        .stage = diagnostic_stage::parser,
        .severity = diagnostic_severity::error,
        .name = "unexpected_token",
        .message = "unexpected token"
    };
}
