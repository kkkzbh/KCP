import std;
import preprocessor;
import lexer;
import parser;
import semantic.result;
import semantic.program;
import ir.result;
import ir.program;
import ir.format;

read_source(path: str) -> string
{
    match file::open(path, open_options{}.read()) {
        .value(input) => {
            let storage = buffer<u8>{8192 as usize};
            let read = input.read(span<u8>{storage.data(), storage.capacity()});
            let count = read.value_or(0 as usize);
            return string{str{ .ptr = storage.data() as char const*, .len = count }};
        },
        .unexpected(error) => {
            panic("failed to open mini c input file");
        },
    };
}

print_constant_values(semantics: semantic_result const&) -> void
{
    let index: usize = 0;
    while(index < semantics.expression_infos.size()) {
        let item = semantics.expression_infos[index];
        if(item.info.constant.has_value()) {
            println("mini c semantic expr {} value: {}", item.expression.value, *item.info.constant);
        }
        index += 1;
    }
}

main() -> i32
{
    let source = read_source("lab/test.c");
    let file = source_file{"lab/test.c", source.as_str()};
    let preprocessed = preprocess(file);
    println("mini c semantic input bytes: {}", source.size());
    println("mini c semantic preprocessor diagnostics: {}", preprocessed.diagnostics.size());
    if(preprocessed.diagnostics.size() != 0) {
        return 1;
    }

    let lexical = lex(preprocessed);
    println("mini c semantic lexer diagnostics: {}", lexical.diagnostics.size());
    if(lexical.diagnostics.size() != 0) {
        return 2;
    }

    let parsed = parse(move lexical.tokens);
    println("mini c semantic parser accepted: {}", parsed.accepted);
    println("mini c semantic parser diagnostics: {}", parsed.diagnostics.size());
    if(not parsed.accepted or parsed.diagnostics.size() != 0) {
        return 3;
    }

    let semantics = analyze_semantics(file, parsed);
    println("mini c semantic accepted: {}", semantics.accepted());
    println("mini c semantic diagnostics: {}", semantics.diagnostics.size());
    println("mini c semantic function count: {}", semantics.functions.size());
    println("mini c semantic symbol count: {}", semantics.symbols.size());
    println("mini c semantic expression info count: {}", semantics.expression_infos.size());
    print_constant_values(semantics);
    if(not semantics.accepted()) {
        return 4;
    }

    let quads = emit_quads(file, parsed, semantics);
    println("mini c quad accepted: {}", quads.accepted);
    println("mini c quad count: {}", quads.quads.size());
    let text = dump_quads(quads.quads);
    print("{}", text.as_str());
    if(not quads.accepted) {
        return 5;
    }

    return 0;
}
