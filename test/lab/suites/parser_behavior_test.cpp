import std;

#include "assert.hpp"

namespace {

struct test_tools
{
    std::filesystem::path cp{};
    std::filesystem::path lab_root{};
};

auto shell_quote(std::string_view value) -> std::string
{
    auto output = std::string{ "'" };
    for(auto character : value) {
        if(character == '\'') {
            output += "'\\''";
        } else {
            output += character;
        }
    }
    output += '\'';
    return output;
}

auto shell_join(std::vector<std::string> const& arguments) -> std::string
{
    return (
        arguments
        | std::views::transform([](std::string const& value) { return shell_quote(value); })
        | std::views::join_with(' ')
        | std::ranges::to<std::string>()
    );
}

auto run_status(std::vector<std::string> const& arguments) -> int
{
    auto command = shell_join(arguments) + " >/dev/null 2>&1";
    return std::system(command.c_str());
}

auto exit_code(int status) -> int
{
    if(status < 0) {
        return status;
    }
    return status / 256;
}

auto unique_temp_dir(std::string_view name) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / std::format("cp-lab-parser-test-{}-{}", name, stamp);
    std::filesystem::create_directories(path);
    return path;
}

auto write_source(std::filesystem::path const& path, std::string_view text) -> void
{
    auto stream = std::ofstream{ path };
    test_parser::assert_true(stream.is_open(), std::format("should open {}", path.string()));
    stream << text;
    test_parser::assert_true(static_cast<bool>(stream), std::format("should write {}", path.string()));
}

auto module_sources(test_tools const& tools) -> std::vector<std::string>
{
    return {
        (tools.lab_root / "source/source.cp").string(),
        (tools.lab_root / "diagnostic/stage.cp").string(),
        (tools.lab_root / "diagnostic/kind.cp").string(),
        (tools.lab_root / "diagnostic/severity.cp").string(),
        (tools.lab_root / "diagnostic/record.cp").string(),
        (tools.lab_root / "diagnostic/catalog.cp").string(),
        (tools.lab_root / "diagnostic/collector.cp").string(),
        (tools.lab_root / "diagnostic/diagnostic.cp").string(),
        (tools.lab_root / "preprocessor/preprocessor.cp").string(),
        (tools.lab_root / "lexer/token.cp").string(),
        (tools.lab_root / "lexer/charset.cp").string(),
        (tools.lab_root / "lexer/keyword.cp").string(),
        (tools.lab_root / "lexer/state.cp").string(),
        (tools.lab_root / "lexer/lexer.cp").string(),
        (tools.lab_root / "parser/ast/ids.cp").string(),
        (tools.lab_root / "parser/ast/expr.cp").string(),
        (tools.lab_root / "parser/ast/stmt.cp").string(),
        (tools.lab_root / "parser/ast/item.cp").string(),
        (tools.lab_root / "parser/ast/arena.cp").string(),
        (tools.lab_root / "parser/ast/ast.cp").string(),
        (tools.lab_root / "parser/trace.cp").string(),
        (tools.lab_root / "parser/state.cp").string(),
        (tools.lab_root / "parser/expression.cp").string(),
        (tools.lab_root / "parser/statement.cp").string(),
        (tools.lab_root / "parser/program.cp").string(),
        (tools.lab_root / "parser/parser.cp").string()
    };
}

auto compile(test_tools const& tools, std::vector<std::string> arguments) -> int
{
    arguments.insert(arguments.begin(), tools.cp.string());
    return run_status(arguments);
}

auto compile_modules(test_tools const& tools, std::filesystem::path const& main_source, std::filesystem::path const& output) -> int
{
    auto arguments = module_sources(tools);
    arguments.push_back(main_source.string());
    arguments.push_back("-o");
    arguments.push_back(output.string());
    return compile(tools, arguments);
}

auto check_harness(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("harness");
    auto source = dir / "main.cp";
    auto app = dir / "main";
    write_source(
        source,
        R"(import std;
import preprocessor;
import lexer;
import parser;
import diagnostic;

preprocess_text(text: str) -> preprocessed_file
{
    let file = source_file{"<memory>", text};
    return preprocess(file);
}

parse_text(text: str) -> parse_result
{
    let preprocessed = preprocess_text(text);
    let lexical = lex(preprocessed);
    return parse(move lexical.tokens);
}

parse_text_with_trace(text: str) -> parse_result
{
    let preprocessed = preprocess_text(text);
    let lexical = lex(preprocessed);
    return parse_with_options(move lexical.tokens, parse_options{ .trace_enabled = true });
}

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
            panic("failed to open parser input file");
        },
    };
}

trace_contains(parsed: parse_result const&, action: str, subject: str) -> bool
{
    let index: usize = 0;
    while(index < parsed.trace.size()) {
        let item = parsed.trace[index];
        if(item.action.as_str() == action and item.subject.as_str() == subject) {
            return true;
        }
        index += 1;
    }
    return false;
}

first_body_statement(parsed: parse_result const&) -> optional<stmt_id>
{
    let program = parsed.ast.programs[parsed.root.value];
    if(program.functions.size() == 0) {
        return optional<stmt_id>::none;
    }
    let function = parsed.ast.functions[program.functions[0].value];
    match parsed.ast.statements[function.body.value] {
        .block(value) => {
            if(value.statements.size() == 0) {
                return optional<stmt_id>::none;
            }
            return optional<stmt_id>::some(value.statements[0]);
        },
        .var_decl(value) => {},
        .assign(value) => {},
        .call(value) => {},
        .if_stmt(value) => {},
        .while_stmt(value) => {},
        .return_stmt(value) => {},
    };
    return optional<stmt_id>::none;
}

first_return_value(parsed: parse_result const&) -> optional<expr_id>
{
    let statement = first_body_statement(parsed);
    if(not statement.has_value()) {
        return optional<expr_id>::none;
    }
    let statement_id = stmt_id{ .value = (*statement).value };
    match parsed.ast.statements[statement_id.value] {
        .block(value) => {},
        .var_decl(value) => {},
        .assign(value) => {},
        .call(value) => {},
        .if_stmt(value) => {},
        .while_stmt(value) => {},
        .return_stmt(value) => {
            return value.value;
        },
    };
    return optional<expr_id>::none;
}

is_binary(ast: ast_arena const&, id: expr_id, kind: token_kind) -> bool
{
    return match ast.expressions[id.value] {
        .integer(value) => false,
        .name(value) => false,
        .unary(value) => false,
        .binary(value) => value.operator_kind == kind,
        .call(value) => false,
        .grouped(value) => false,
    };
}

binary_right(ast: ast_arena const&, id: expr_id) -> optional<expr_id>
{
    return match ast.expressions[id.value] {
        .integer(value) => optional<expr_id>::none,
        .name(value) => optional<expr_id>::none,
        .unary(value) => optional<expr_id>::none,
        .binary(value) => optional<expr_id>::some(value.right),
        .call(value) => optional<expr_id>::none,
        .grouped(value) => optional<expr_id>::none,
    };
}

first_var_decl_count(parsed: parse_result const&) -> usize
{
    let statement = first_body_statement(parsed);
    if(not statement.has_value()) {
        return 0 as usize;
    }
    return match parsed.ast.statements[(*statement).value] {
        .block(value) => 0 as usize,
        .var_decl(value) => value.declarations.size(),
        .assign(value) => 0 as usize,
        .call(value) => 0 as usize,
        .if_stmt(value) => 0 as usize,
        .while_stmt(value) => 0 as usize,
        .return_stmt(value) => 0 as usize,
    };
}

main() -> i32
{
    let empty = parse_text("");
    if(not empty.accepted) { return 1; }
    if(empty.ast.programs[empty.root.value].functions.size() != 0) { return 2; }

    let basic = parse_text("int main() { return 0; }");
    if(not basic.accepted) { return 3; }
    if(basic.ast.programs[basic.root.value].functions.size() != 1) { return 4; }
    if(basic.ast.statements.size() != 2) { return 5; }
    if(basic.trace.size() != 0) { return 26; }

    let traced = parse_text_with_trace("int main() { return 0; }");
    if(not traced.accepted) { return 27; }
    if(traced.trace.size() == 0) { return 28; }
    if(not trace_contains(traced, "enter", "Program")) { return 29; }
    if(not trace_contains(traced, "match", "kw_int")) { return 30; }
    let last_trace = traced.trace[traced.trace.size() - 1];
    if(last_trace.action.as_str() != "accept") { return 31; }

    let sample_source = read_source("lab/test.c");
    let sample = parse_text(sample_source.as_str());
    if(not sample.accepted) { return 6; }
    if(sample.ast.programs[sample.root.value].functions.size() != 2) { return 7; }

    let features = parse_text("int add(int a, int b) { return a + b; } int main() { int x = add(1, 2); int y; y = x; add(y, 3); if (y >= 3) { y = y - 1; } else { y = 0; } while (y > 0) { y = y - 1; } return y; }");
    if(not features.accepted) { return 8; }
    if(features.ast.programs[features.root.value].functions.size() != 2) { return 9; }

    let declaration_list = parse_text("int main() { int a = 2, c = 0; return a + c; }");
    if(not declaration_list.accepted) { return 23; }
    if(first_var_decl_count(declaration_list) != 2 as usize) { return 24; }

    let precedence = parse_text("int main() { return 1 + 2 * 3; }");
    if(not precedence.accepted) { return 10; }
    let value = first_return_value(precedence);
    if(not value.has_value()) { return 11; }
    let value_id = expr_id{ .value = (*value).value };
    if(not is_binary(precedence.ast, value_id, token_kind::plus)) { return 12; }
    let right = binary_right(precedence.ast, value_id);
    if(not right.has_value()) { return 13; }
    let right_id = expr_id{ .value = (*right).value };
    if(not is_binary(precedence.ast, right_id, token_kind::star)) { return 14; }

    let missing_semicolon = parse_text("int main() { return 0 }");
    if(missing_semicolon.accepted) { return 15; }
    if(missing_semicolon.diagnostics.size() == 0) { return 16; }
    if(missing_semicolon.diagnostics[0].stage != diagnostic_stage::parser) { return 17; }

    let missing_paren = parse_text("int main( { return 0; }");
    if(missing_paren.accepted) { return 18; }

    let missing_body = parse_text("int main();");
    if(missing_body.accepted) { return 19; }

    let expression_statement = parse_text("int main() { 1 + 2; return 0; }");
    if(expression_statement.accepted) { return 20; }

    let void_parameter = parse_text("int bad(void value) { return 0; }");
    if(void_parameter.accepted) { return 21; }

    let identifier_tail = parse_text("int main() { value + 1; return 0; }");
    if(identifier_tail.accepted) { return 22; }

    let incomplete_declaration_list = parse_text("int main() { int a = 1, ; return 0; }");
    if(incomplete_declaration_list.accepted) { return 25; }

    return 0;
})"
    );

    auto status = compile_modules(tools, source, app);
    test_parser::assert_true(status == 0, "lab parser harness should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab parser harness should pass");
}

auto check_sample_main(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("sample");
    auto app = dir / "parser-main";
    auto status = compile_modules(tools, tools.lab_root / "parser/main.cp", app);
    test_parser::assert_true(status == 0, "lab parser sample main should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab parser sample main should pass");
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc != 3) {
        test_parser::fail("usage: lab_parser_behavior_test <cp> <lab-root>");
    }

    auto tools = test_tools{
        .cp = argv[1],
        .lab_root = argv[2]
    };

    check_harness(tools);
    check_sample_main(tools);
    return 0;
}
