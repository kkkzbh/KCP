export module parser;

import std;
export import lexer.token;
export import diagnostic;
export import parser.ast;
export import parser.trace;
import parser.state;
import parser.program;

export struct parse_result {
    accepted: bool;
    ast: ast_arena;
    root: program_id;
    diagnostics: vector<diagnostic>;
    trace: vector<trace_record>;
}

export parse_with_options(tokens: vector<token>, options: parse_options) -> parse_result
{
    let state = parser{move tokens, options};
    let root = state.parse_program();
    let current = state.peek();
    let accepted = state.diagnostics.empty() and current.kind == token_kind::eof;
    if(accepted) {
        state.trace_accept();
    }
    return parse_result{
        .accepted = accepted,
        .ast = move state.arena,
        .root = root,
        .diagnostics = state.diagnostics.take(),
        .trace = move state.trace
    };
}

export parse(tokens: vector<token>) -> parse_result
{
    return parse_with_options(move tokens, parse_options{});
}
