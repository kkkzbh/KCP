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
    return diagnostic_spec{
        .stage = diagnostic_stage::parser,
        .severity = diagnostic_severity::error,
        .name = "unexpected_token",
        .message = "unexpected token"
    };
}
