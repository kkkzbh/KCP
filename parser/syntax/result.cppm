export module parser.syntax.result;

import std;
import lexer.diagnostic;
import parser.ast;
import parser.diagnostic;

export struct [[nodiscard]] parse_result
{
    auto has_root() const -> bool
    {
        return root.valid();
    }

    bool accepted{};
    ast_arena ast{};
    translation_unit_id root{};
    std::vector<parser_diagnostic> parser_diagnostics{};
    std::vector<lexer_diagnostic> lexer_diagnostics{};
};
