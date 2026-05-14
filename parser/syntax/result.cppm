export module parser.syntax.result;

import std;
import lexer.diagnostic;
import parser.ast;
import parser.diagnostic;

export struct [[nodiscard]] parse_result
{
    bool accepted{};
    ast_arena ast{};
    std::optional<translation_unit_syntax> root{};
    std::vector<parser_diagnostic> parser_diagnostics{};
    std::vector<lexer_diagnostic> lexer_diagnostics{};
};
