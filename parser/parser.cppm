export module parser;

import std;
import :state;
export import lexer.token;
export import parser.ast;
export import diagnostic;

export struct [[nodiscard]] parse_result
{
    bool accepted{};
    ast_arena ast{};
    std::optional<translation_unit_syntax> root{};
    std::vector<diagnostic> diagnostics{};
};

export auto parse_translation_unit(std::vector<token> tokens) -> parse_result
{
    auto state = parser{ std::move(tokens) };
    auto root = state.parse_translation_unit_node();
    auto accepted = not contains_error_diagnostic(state.diagnostics.span())
        and root
        and state.peek().kind == token_kind::eof;

    return parse_result {
        .accepted = accepted,
        .ast = std::move(state.arena),
        .root = std::move(root),
        .diagnostics = state.diagnostics.take(),
    };
}
