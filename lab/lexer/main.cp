import std;
import lexer;

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
            panic("failed to open lexer input file");
        },
    };
}

main() -> i32
{
    let source = read_source("lab/test.c");
    let result = lex(source.as_str());
    println("mini c lexer input bytes: {}", source.size());
    println("mini c lexer token count: {}", result.tokens.size());
    println("mini c lexer diagnostics: {}", result.diagnostics.size());
    if(result.diagnostics.size() != 0) {
        println("mini c lexer failed: diagnostics were produced");
        return 1;
    }
    if(result.tokens.size() < 70) {
        println("mini c lexer failed: token count is too small");
        return 2;
    }
    if(result.tokens[0 as usize].kind != token_kind::kw_int) {
        println("mini c lexer failed: first token is not int");
        return 3;
    }
    if(result.tokens[result.tokens.size() - 1].kind != token_kind::eof) {
        println("mini c lexer failed: last token is not eof");
        return 4;
    }
    let saw_if = false;
    let saw_while = false;
    let saw_return = false;
    let index: usize = 0;
    while(index < result.tokens.size()) {
        let kind = result.tokens[index].kind;
        if(kind == token_kind::kw_if) {
            saw_if = true;
        }
        if(kind == token_kind::kw_while) {
            saw_while = true;
        }
        if(kind == token_kind::kw_return) {
            saw_return = true;
        }
        ++index;
    }
    println("mini c lexer saw if: {}", saw_if);
    println("mini c lexer saw while: {}", saw_while);
    println("mini c lexer saw return: {}", saw_return);
    if(not saw_if or not saw_while or not saw_return) {
        println("mini c lexer failed: required control-flow keyword is missing");
        return 5;
    }
    println("mini c lexer ok");
    return 0;
}
