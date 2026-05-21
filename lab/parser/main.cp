import std;
import preprocessor;
import lexer;
import parser;

read_source(path: str) -> string
{
    match file::open(path, open_options{}.read()) {
        .value(input) => {
            let storage = buffer<u8>{4096 as usize};
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

print_trace_prefix(step: usize, depth: usize) -> void
{
    print_usize_cell(step, 4 as usize);
    print(" ");
    print_trace_indent(depth);
}

print_tree_trace_row(item: trace_record const&, visible_step: usize) -> usize
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
        print("match ");
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
        println("accept {}", item.subject.as_str());
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
    println("mini c parser input bytes: {}", source.size());
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

    let result = parse_with_options(move lexical.tokens, parse_options{ .trace_enabled = true });
    let program = result.ast.programs[result.root.value];
    println("mini c parser accepted: {}", result.accepted);
    println("mini c parser diagnostics: {}", result.diagnostics.size());
    println("mini c parser function count: {}", program.functions.size());
    println("mini c parser statement count: {}", result.ast.statements.size());
    println("mini c parser expression count: {}", result.ast.expressions.size());
    println("mini c parser trace:");
    let visible_step: usize = 1;
    for(let item : result.trace) {
        visible_step = print_tree_trace_row(item, visible_step);
    }

    if(not result.accepted or result.diagnostics.size() != 0) {
        println("mini c parser failed: parser diagnostics were produced");
        return 3;
    }
    if(program.functions.size() != 2) {
        println("mini c parser failed: expected add and main functions");
        return 4;
    }

    let add_function = result.ast.functions[program.functions[0 as usize].value];
    let main_function = result.ast.functions[program.functions[1 as usize].value];
    if(file.slice(add_function.name) != "add") {
        println("mini c parser failed: first function is not add");
        return 5;
    }
    if(file.slice(main_function.name) != "main") {
        println("mini c parser failed: second function is not main");
        return 6;
    }
    if(result.ast.statements.size() < 10) {
        println("mini c parser failed: statement tree is too small");
        return 7;
    }
    if(result.ast.expressions.size() < 12) {
        println("mini c parser failed: expression tree is too small");
        return 8;
    }

    println("mini c parser ok");
    return 0;
}
