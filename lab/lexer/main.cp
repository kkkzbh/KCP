import std;
import preprocessor;
import lexer;

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
            panic("failed to open lexer input file");
        },
    };
}

main() -> i32
{
    let source = read_source("lab/test.c");
    let file = source_file{"lab/test.c", source.as_str()};
    let preprocessed = preprocess(file);
    println("mini c lexer input bytes: {}", source.size());
    println("mini c preprocessor diagnostics: {}", preprocessed.diagnostics.size());
    if(preprocessed.diagnostics.size() != 0) {
        println("mini c lexer failed: preprocessor diagnostics were produced");
        return 1;
    }
    let result = lex(preprocessed);
    println("mini c lexer token count: {}", result.tokens.size());
    println("mini c lexer diagnostics: {}", result.diagnostics.size());
    if(result.diagnostics.size() != 0) {
        println("mini c lexer failed: diagnostics were produced");
        return 1;
    }
    if(result.tokens.size() < 180) {
        println("mini c lexer failed: token count is too small");
        return 2;
    }
    if(result.tokens[0 as usize].kind != token_kind::kw_void) {
        println("mini c lexer failed: first token is not void");
        return 3;
    }
    if(result.tokens[result.tokens.size() - 1].kind != token_kind::eof) {
        println("mini c lexer failed: last token is not eof");
        return 4;
    }
    let saw_if = false;
    let saw_for = false;
    let saw_break = false;
    let saw_continue = false;
    let saw_return = false;
    for(const ref token : result.tokens) {
        let kind = token.kind;
        if(kind == token_kind::kw_if) {
            saw_if = true;
        }
        if(kind == token_kind::kw_for) {
            saw_for = true;
        }
        if(kind == token_kind::kw_break) {
            saw_break = true;
        }
        if(kind == token_kind::kw_continue) {
            saw_continue = true;
        }
        if(kind == token_kind::kw_return) {
            saw_return = true;
        }
    }
    println("mini c lexer saw if: {}", saw_if);
    println("mini c lexer saw for: {}", saw_for);
    println("mini c lexer saw break: {}", saw_break);
    println("mini c lexer saw continue: {}", saw_continue);
    println("mini c lexer saw return: {}", saw_return);
    if(not saw_if or not saw_for or not saw_break or not saw_continue or not saw_return) {
        println("mini c lexer failed: required control-flow keyword is missing");
        return 5;
    }
    println("mini c lexer ok");
    return 0;
}
