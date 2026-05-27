export module parser;

import std;
export import lexer.token;
export import diagnostic;
export import parser.ast;
export import parser.trace;
import parser.state;
import parser.table;

export struct parse_result {
    accepted: bool;
    ast: ast_arena;
    root: program_id;
    diagnostics: vector<diagnostic>;
    trace: vector<trace_record>;
}

export parse(tokens: vector<token>, options: parse_options = parse_options{}) -> parse_result
{
    const static tables = build_parser_tables();
    let state = parser_state{move tokens, options};
    let result = state.run(tables);
    return parse_result{
        .accepted = result.accepted,
        .ast = move result.ast,
        .root = result.root,
        .diagnostics = move result.diagnostics,
        .trace = move result.trace
    };
}
