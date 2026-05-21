export module parser;

import std;
export import lexer.token;
export import diagnostic;
export import parser.ast;
export import parser.trace;
import parser.state;

export struct parse_result {
    accepted: bool;
    ast: ast_arena;
    root: program_id;
    diagnostics: vector<diagnostic>;
    trace: vector<trace_record>;
}

export parse_with_options(tokens: vector<token>, options: parse_options) -> parse_result
{
    let result = parse_lr(move tokens, options);
    return parse_result{
        .accepted = result.accepted,
        .ast = move result.ast,
        .root = result.root,
        .diagnostics = move result.diagnostics,
        .trace = move result.trace
    };
}

export parse(tokens: vector<token>) -> parse_result
{
    return parse_with_options(move tokens, parse_options{});
}
