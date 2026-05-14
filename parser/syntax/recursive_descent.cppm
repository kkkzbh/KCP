export module parser.syntax.recursive_descent;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import parser.syntax.result;
import :context;
import :program;

export auto parse_translation_unit(source_manager const& sources, file_id file) -> parse_result
{
    auto lexer_sink = std::vector<lexer_diagnostic>{};
    auto lex = lexer{ sources, file, lexer_sink };
    auto tokens = lex.tokenize_all();

    auto result = parse_result {
        .accepted = false,
        .lexer_diagnostics = std::move(lexer_sink),
    };

    if(not result.lexer_diagnostics.empty()) {
        result.parser_diagnostics.emplace_back(
            parser_diagnostic_severity::error,
            parser_diagnostic_code::lexical_failure,
            "lexical analysis reported errors; parser did not continue",
            result.lexer_diagnostics.front().primary_span
        );
        return result;
    }

    auto context = parser_context{ std::move(tokens) };
    auto root = program_parser{ context }.parse_translation_unit_node();
    auto accepted = context.diagnostics.empty() and root and context.peek().kind == token_kind::eof;

    auto parsed = parse_result {
        .accepted = accepted,
        .ast = std::move(context.arena),
        .root = root.value_or(translation_unit_id{}),
        .parser_diagnostics = std::move(context.diagnostics),
        .lexer_diagnostics = std::move(result.lexer_diagnostics),
    };
    return parsed;
}
