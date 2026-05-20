export module lexer;

import std;
export import source;
import preprocessor;
export import diagnostic;
import lexer.state;
export import lexer.token;

export struct lex_result {
    tokens: vector<token>;
    diagnostics: vector<diagnostic>;
}

export lex(input: preprocessed_file const&) -> lex_result
{
    let state = lexer{input};
    return lex_result{ .tokens = state.tokenize_all(), .diagnostics = state.take_diagnostics() };
}
