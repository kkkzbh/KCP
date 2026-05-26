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
    let tables = build_parser_tables();
    return parse_with_tables(move tokens, tables, options);
}

export parse_with_tables(tokens: vector<token>, tables: parser_tables const&, options: parse_options = parse_options{}) -> parse_result
{
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
