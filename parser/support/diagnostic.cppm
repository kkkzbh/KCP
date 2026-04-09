export module parser.diagnostic;

import std;
import lexer.source;

export enum class parser_diagnostic_severity
{
    error,
    warning,
};

export enum class parser_diagnostic_code
{
    unexpected_token,
    expected_token,
    expected_identifier,
    expected_expression,
    expected_statement,
    expected_type,
    unsupported_construct,
    lexical_failure,
};

export struct parser_diagnostic
{
    parser_diagnostic_severity severity{ parser_diagnostic_severity::error };
    parser_diagnostic_code code{};
    std::string message;
    span primary_span{};
};

export [[nodiscard]] auto to_string(parser_diagnostic_code code) -> std::string_view;

auto to_string(parser_diagnostic_code code) -> std::string_view
{
    using enum parser_diagnostic_code;

    switch(code) {
    case unexpected_token: return "unexpected_token";
    case expected_token: return "expected_token";
    case expected_identifier: return "expected_identifier";
    case expected_expression: return "expected_expression";
    case expected_statement: return "expected_statement";
    case expected_type: return "expected_type";
    case unsupported_construct: return "unsupported_construct";
    case lexical_failure: return "lexical_failure";
    }

    return "unknown_parser_diagnostic";
}
