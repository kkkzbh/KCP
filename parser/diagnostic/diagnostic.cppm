export module parser.diagnostic;

import std;
import source;
import source.diagnostic;

export enum class parser_diagnostic_severity {
    error,
    warning,
};

export enum class parser_diagnostic_code {
    unexpected_token,
    expected_token,
    expected_identifier,
    expected_expression,
    expected_statement,
    expected_type,
    lexical_failure,
};

export using parser_diagnostic = diagnostic<parser_diagnostic_severity, parser_diagnostic_code>;

export auto to_string(parser_diagnostic_code code) -> std::string_view
{
    using enum parser_diagnostic_code;

    switch(code) {
        case unexpected_token:
            return "unexpected_token";
        case expected_token:
            return "expected_token";
        case expected_identifier:
            return "expected_identifier";
        case expected_expression:
            return "expected_expression";
        case expected_statement:
            return "expected_statement";
        case expected_type:
            return "expected_type";
        case lexical_failure:
            return "lexical_failure";
    }

    std::unreachable();
}
