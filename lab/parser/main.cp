import std;
import preprocessor;
import lexer;
import parser;
import parser.table;

read_source(path: str) -> string
{
    match file::open(path, open_options{}.read()) {
        .value(input) => {
            let storage = raw_buffer<u8>{4096 as usize};
            let read = input.read(span<u8>{storage.data(), storage.capacity()});
            let count = read.value_or(0 as usize);
            return string{str{ .ptr = storage.data() as char const*, .len = count }};
        },
        .unexpected(error) => {
            panic("failed to open parser input file");
        },
    };
}

usize_digits(value: usize) -> usize
{
    let count: usize = 1;
    let current = value;
    while(current >= 10 as usize) {
        current = current / 10 as usize;
        count += 1;
    }
    return count;
}

print_padding(width: usize, used: usize) -> void
{
    let cursor = used;
    while(cursor < width) {
        print(" ");
        cursor += 1;
    }
}

print_text_cell(text: str, width: usize) -> void
{
    print("{}", text);
    print_padding(width, text.size());
}

print_usize_cell(value: usize, width: usize) -> void
{
    print("{}", value);
    print_padding(width, usize_digits(value));
}

print_trace_indent(depth: usize) -> void
{
    let cursor: usize = 0;
    while(cursor < depth) {
        print("  ");
        cursor += 1;
    }
}

print_trace_token(kind: token_kind, text: str) -> void
{
    print("{}(\"{}\")", token_kind_name(kind), text);
}

print_grammar_symbol(symbol: grammar_symbol const&) -> void
{
    match symbol.data {
        .terminal(kind) => {
            print("{}", token_kind_name(kind));
        },
        .nonterminal(kind) => {
            print("{}", nonterminal_kind_name(kind));
        },
        .epsilon => {
            print("epsilon");
        },
    };
}

print_production(rule: production const&) -> void
{
    print("{}", nonterminal_kind_name(rule.lhs));
    print(" ->");
    if(rule.rhs.size() == 0 as usize) {
        print(" epsilon");
        return;
    }

    let index: usize = 0;
    while(index < rule.rhs.size()) {
        print(" ");
        print_grammar_symbol(rule.rhs[index]);
        index += 1;
    }
}

print_trace_prefix(step: usize, depth: usize) -> void
{
    print_usize_cell(step, 4 as usize);
    print(" ");
    print_trace_indent(depth);
}

print_action_key(state: usize, kind: token_kind) -> void
{
    print("ACTION[{}, {}]", state, token_kind_name(kind));
}

print_tree_trace_row(tables: parser_tables const&, item: trace_record const&, visible_step: usize) -> usize
{
    if(item.action.as_str() == "exit" and item.detail.as_str() == "ok") {
        return visible_step;
    }

    print_trace_prefix(visible_step, item.depth);
    if(item.action.as_str() == "enter") {
        print("{}", item.subject.as_str());
        print("    lookahead ");
        print_trace_token(item.lookahead_kind, item.lookahead_text.as_str());
        println("");
        return visible_step + 1;
    }
    if(item.action.as_str() == "select") {
        print("choose {}", item.detail.as_str());
        print("    lookahead ");
        print_trace_token(item.lookahead_kind, item.lookahead_text.as_str());
        println("");
        return visible_step + 1;
    }
    if(item.action.as_str() == "match") {
        print_action_key(item.state, item.lookahead_kind);
        print(" = shift {}; match ", item.target_state);
        print_trace_token(item.lookahead_kind, item.lookahead_text.as_str());
        println("");
        return visible_step + 1;
    }
    if(item.action.as_str() == "reduce") {
        const ref rule = tables.grammar.productions[item.production];
        print_action_key(item.state, item.lookahead_kind);
        print(" = reduce p{}: ", item.production);
        print_production(rule);
        print("; pop {}", item.pop_count);
        print("; GOTO[{}, {}] = {}", item.goto_state, nonterminal_kind_name(rule.lhs), item.goto_target);
        print("; lookahead ");
        print_trace_token(item.lookahead_kind, item.lookahead_text.as_str());
        println("");
        return visible_step + 1;
    }
    if(item.action.as_str() == "error") {
        print("error {}: {}", item.subject.as_str(), item.detail.as_str());
        print("    lookahead ");
        print_trace_token(item.lookahead_kind, item.lookahead_text.as_str());
        println("");
        return visible_step + 1;
    }
    if(item.action.as_str() == "accept") {
        print_action_key(item.state, item.lookahead_kind);
        println(" = accept {}", item.subject.as_str());
        return visible_step + 1;
    }

    println("{} {}", item.action.as_str(), item.detail.as_str());
    return visible_step + 1;
}

main() -> i32
{
    let source = read_source("lab/test.c");
    let file = source_file{"lab/test.c", source.as_str()};
    let preprocessed = preprocess(file);
    println("mini c LR(1) parser input bytes: {}", source.size());
    println("mini c preprocessor diagnostics: {}", preprocessed.diagnostics.size());
    if(preprocessed.diagnostics.size() != 0) {
        println("mini c parser failed: preprocessor diagnostics were produced");
        return 1;
    }

    let lexical = lex(preprocessed);
    println("mini c lexer token count: {}", lexical.tokens.size());
    println("mini c lexer diagnostics: {}", lexical.diagnostics.size());
    if(lexical.diagnostics.size() != 0) {
        println("mini c parser failed: lexer diagnostics were produced");
        return 2;
    }

    let tables = build_parser_tables();
    println("mini c LR(1) state count: {}", tables.states.size());
    println("mini c LR(1) action count: {}", tables.action_table.size());
    println("mini c LR(1) goto count: {}", tables.goto_table.size());
    println("mini c LR(1) conflict count: {}", tables.conflicts.size());
    if(tables.conflicts.size() != 0) {
        println("mini c parser failed: LR(1) table has conflicts");
        return 3;
    }

    let result = parse(move lexical.tokens, parse_options{ .trace_enabled = true });
    const ref program = result.ast.programs[result.root.value];
    println("mini c parser accepted: {}", result.accepted);
    println("mini c parser diagnostics: {}", result.diagnostics.size());
    println("mini c parser function count: {}", program.functions.size());
    println("mini c parser statement count: {}", result.ast.statements.size());
    println("mini c parser expression count: {}", result.ast.expressions.size());
    println("mini c parser trace:");
    let visible_step: usize = 1;
    for(const ref item : result.trace) {
        visible_step = print_tree_trace_row(tables, item, visible_step);
    }

    if(not result.accepted or result.diagnostics.size() != 0) {
        println("mini c parser failed: parser diagnostics were produced");
        return 4;
    }
    if(program.functions.size() != 4) {
        println("mini c parser failed: expected selection_sort, count_passing, checksum and main functions");
        return 5;
    }

    const ref sort_function = result.ast.functions[program.functions[0 as usize].value];
    const ref count_function = result.ast.functions[program.functions[1 as usize].value];
    const ref checksum_function = result.ast.functions[program.functions[2 as usize].value];
    const ref main_function = result.ast.functions[program.functions[3 as usize].value];
    if(file.slice(sort_function.name) != "selection_sort") {
        println("mini c parser failed: first function is not selection_sort");
        return 6;
    }
    if(file.slice(count_function.name) != "count_passing") {
        println("mini c parser failed: second function is not count_passing");
        return 10;
    }
    if(file.slice(checksum_function.name) != "checksum") {
        println("mini c parser failed: third function is not checksum");
        return 11;
    }
    if(file.slice(main_function.name) != "main") {
        println("mini c parser failed: fourth function is not main");
        return 7;
    }
    if(result.ast.statements.size() < 30) {
        println("mini c parser failed: statement tree is too small");
        return 8;
    }
    if(result.ast.expressions.size() < 45) {
        println("mini c parser failed: expression tree is too small");
        return 9;
    }

    println("mini c parser ok");
    return 0;
}
