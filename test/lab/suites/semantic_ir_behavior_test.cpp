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

auto coverage_links_gcov() -> bool
{
    auto const* value = std::getenv("CP_COMPILER_TEST_LINK_GCOV");
    return value != nullptr and std::string_view{ value } == "1";
}

auto unique_temp_dir(std::string_view name) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / std::format("cp-lab-semantic-ir-test-{}-{}", name, stamp);
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
        (tools.lab_root / "parser/symbol.cp").string(),
        (tools.lab_root / "parser/grammar.cp").string(),
        (tools.lab_root / "parser/table.cp").string(),
        (tools.lab_root / "parser/state.cp").string(),
        (tools.lab_root / "parser/parser.cp").string(),
        (tools.lab_root / "semantic/type.cp").string(),
        (tools.lab_root / "semantic/symbol.cp").string(),
        (tools.lab_root / "semantic/result.cp").string(),
        (tools.lab_root / "semantic/state.cp").string(),
        (tools.lab_root / "semantic/expression.cp").string(),
        (tools.lab_root / "semantic/statement.cp").string(),
        (tools.lab_root / "semantic/function.cp").string(),
        (tools.lab_root / "semantic/program.cp").string(),
        (tools.lab_root / "ir/quad.cp").string(),
        (tools.lab_root / "ir/result.cp").string(),
        (tools.lab_root / "ir/state.cp").string(),
        (tools.lab_root / "ir/expression.cp").string(),
        (tools.lab_root / "ir/statement.cp").string(),
        (tools.lab_root / "ir/program.cp").string(),
        (tools.lab_root / "ir/format.cp").string()
    };
}

auto compile(test_tools const& tools, std::vector<std::string> arguments) -> int
{
    arguments.insert(arguments.begin(), tools.cp.string());
    if(coverage_links_gcov()) {
        arguments.emplace_back("--link-arg");
        arguments.emplace_back("-lgcov");
    }
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
import semantic.type;
import semantic.result;
import semantic.program;
import ir.quad;
import ir.result;
import ir.program;
import ir.format;

parse_file(file: source_file const&) -> parse_result
{
    let preprocessed = preprocess(file);
    if(preprocessed.diagnostics.size() != 0) {
        panic("preprocessor diagnostics in semantic ir test");
    }
    let lexical = lex(preprocessed);
    if(lexical.diagnostics.size() != 0) {
        panic("lexer diagnostics in semantic ir test");
    }
    let parsed = parse(move lexical.tokens);
    if(not parsed.accepted) {
        panic("parser diagnostics in semantic ir test");
    }
    return parsed;
}

semantic_of(text: str) -> semantic_result
{
    let file = source_file{"<memory>", text};
    let parsed = parse_file(file);
    return analyze_semantics(file, parsed);
}

has_diag(result: semantic_result const&, kind: diagnostic_kind) -> bool
{
    let index: usize = 0;
    while(index < result.diagnostics.size()) {
        if(result.diagnostics[index].kind == kind and result.diagnostics[index].stage == diagnostic_stage::semantic) {
            return true;
        }
        index += 1;
    }
    return false;
}

expect_diag(text: str, kind: diagnostic_kind, code: i32) -> i32
{
    let result = semantic_of(text);
    if(not has_diag(result, kind)) {
        return code;
    }
    return 0;
}

quad_index(quads: vector<quad> const&, op: str) -> usize
{
    let index: usize = 0;
    while(index < quads.size()) {
        if(quads[index].op.as_str() == op) {
            return index;
        }
        index += 1;
    }
    return quads.size();
}

has_quad_op(quads: vector<quad> const&, op: str) -> bool
{
    return quad_index(quads, op) != quads.size();
}

has_call_to(quads: vector<quad> const&, name: str) -> bool
{
    let index: usize = 0;
    while(index < quads.size()) {
        if(quads[index].op.as_str() == "call" and quads[index].arg1.as_str() == name) {
            return true;
        }
        index += 1;
    }
    return false;
}

has_constant(result: semantic_result const&, value: i32) -> bool
{
    let index: usize = 0;
    while(index < result.expression_infos.size()) {
        let info = result.expression_infos[index].info;
        if(info.constant.has_value() and *info.constant == value) {
            return true;
        }
        index += 1;
    }
    return false;
}

main() -> i32
{
    let valid_text: str = "void selection_sort(int values[], int n) { for (int i = 0; i < n; i = i + 1) { int min = i; for (int j = i + 1; j < n; j = j + 1) { if (values[j] < values[min]) { min = j; } } if (min != i) { int temp = values[i]; values[i] = values[min]; values[min] = temp; } } } int count_passing(int values[], int n) { int count = 0; for (int i = 0; i < n; i = i + 1) { if (values[i] < 0) { break; } if (values[i] < 60) { continue; } count = count + 1; } return count; } int checksum(int values[], int n) { int total = 0; for (int i = 0; i < n; i = i + 1) { total = total + values[i] * (i + 1); } return total; } int main() { int scores[7] = {72, 55, 88, 91, 66, 79, -1}; selection_sort(scores, 6); int passing = count_passing(scores, 7); int total = checksum(scores, 6); return total + passing * 100 + scores[3]; }";
    let valid_file = source_file{"valid.c", valid_text};
    let valid_parsed = parse_file(valid_file);
    let valid_semantics = analyze_semantics(valid_file, valid_parsed);
    if(not valid_semantics.accepted()) { return 1; }
    if(valid_semantics.functions.size() != 4 as usize) { return 2; }
    let sort_symbol = semantic_find_function(valid_semantics, "selection_sort");
    if(not sort_symbol.valid()) { return 3; }
    if(valid_semantics.symbols[sort_symbol.index()].parameter_count != 2 as usize) { return 4; }
    if(valid_semantics.symbols[sort_symbol.index()].parameter_types[0 as usize].type != semantic_type_kind::int_array_type) { return 28; }

    let valid_quads = emit_quads(valid_file, valid_parsed, valid_semantics);
    if(not valid_quads.accepted) { return 5; }
    if(not has_quad_op(valid_quads.quads, "func")) { return 6; }
    if(not has_call_to(valid_quads.quads, "selection_sort")) { return 7; }
    if(not has_quad_op(valid_quads.quads, "ret")) { return 8; }
    if(not has_quad_op(valid_quads.quads, "jnz")) { return 9; }
    if(not has_quad_op(valid_quads.quads, "jz")) { return 10; }
    if(not has_quad_op(valid_quads.quads, "label")) { return 11; }
    if(not has_quad_op(valid_quads.quads, "array_decl")) { return 29; }
    if(not has_quad_op(valid_quads.quads, "array_load")) { return 30; }
    if(not has_quad_op(valid_quads.quads, "array_store")) { return 31; }

    let expr_text: str = "int main() { return 1 + 2 * 3; }";
    let expr_file = source_file{"expr.c", expr_text};
    let expr_parsed = parse_file(expr_file);
    let expr_semantics = analyze_semantics(expr_file, expr_parsed);
    if(not expr_semantics.accepted()) { return 12; }
    if(not has_constant(expr_semantics, 7)) { return 13; }
    let expr_quads = emit_quads(expr_file, expr_parsed, expr_semantics);
    let multiply = quad_index(expr_quads.quads, "*");
    let plus = quad_index(expr_quads.quads, "+");
    if(multiply >= plus) { return 14; }

    let call_text: str = "int add(int a, int b) { return a + b; } int main() { return add(1, 2); }";
    let call_file = source_file{"call.c", call_text};
    let call_parsed = parse_file(call_file);
    let call_semantics = analyze_semantics(call_file, call_parsed);
    if(not call_semantics.accepted()) { return 15; }
    let call_quads = emit_quads(call_file, call_parsed, call_semantics);
    if(not has_quad_op(call_quads.quads, "param")) { return 16; }
    if(not has_call_to(call_quads.quads, "add")) { return 17; }

    let code = expect_diag("int main() { return 0; } int main() { return 1; }", diagnostic_kind::duplicate_function, 18);
    if(code != 0) { return code; }
    code = expect_diag("int main(int a, int a) { return a; }", diagnostic_kind::duplicate_parameter, 19);
    if(code != 0) { return code; }
    code = expect_diag("int main() { int x; int x; return 0; }", diagnostic_kind::duplicate_local, 20);
    if(code != 0) { return code; }
    code = expect_diag("int main() { x = 1; return 0; }", diagnostic_kind::undeclared_variable, 21);
    if(code != 0) { return code; }
    code = expect_diag("int main() { return missing(); }", diagnostic_kind::undefined_function, 22);
    if(code != 0) { return code; }
    code = expect_diag("int add(int a) { return a; } int main() { return add(1, 2); }", diagnostic_kind::argument_count_mismatch, 23);
    if(code != 0) { return code; }
    code = expect_diag("void main() { return 1; }", diagnostic_kind::void_return_value, 24);
    if(code != 0) { return code; }
    code = expect_diag("int main() { return; }", diagnostic_kind::int_return_value_missing, 25);
    if(code != 0) { return code; }
    code = expect_diag("int main() { return 1 / 0; }", diagnostic_kind::constant_divide_by_zero, 26);
    if(code != 0) { return code; }
    code = expect_diag("int helper() { return 0; }", diagnostic_kind::missing_main, 27);
    if(code != 0) { return code; }
    code = expect_diag("int main() { int a[0]; return 0; }", diagnostic_kind::invalid_array_size, 32);
    if(code != 0) { return code; }
    code = expect_diag("int main() { int a[2] = {1, 2, 3}; return 0; }", diagnostic_kind::array_initializer_too_many, 33);
    if(code != 0) { return code; }
    code = expect_diag("int main() { int a[2]; return a; }", diagnostic_kind::array_value_required, 34);
    if(code != 0) { return code; }
    code = expect_diag("void take(int a[]) { return; } int main() { int value = 1; take(value); return 0; }", diagnostic_kind::argument_type_mismatch, 35);
    if(code != 0) { return code; }
    code = expect_diag("int take(int value) { return value; } int main() { int a[1]; return take(a); }", diagnostic_kind::argument_type_mismatch, 36);
    if(code != 0) { return code; }
    code = expect_diag("int main() { int value = 1; return value[0]; }", diagnostic_kind::non_array_index, 37);
    if(code != 0) { return code; }
    code = expect_diag("int main() { break; return 0; }", diagnostic_kind::break_outside_loop, 38);
    if(code != 0) { return code; }
    code = expect_diag("int main() { continue; return 0; }", diagnostic_kind::continue_outside_loop, 39);
    if(code != 0) { return code; }

    return 0;
})"
    );

    auto status = compile_modules(tools, source, app);
    test_parser::assert_true(status == 0, "lab semantic/IR harness should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab semantic/IR harness should pass");
}

auto check_sample_main(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("sample");
    auto app = dir / "semantic-ir-main";
    auto arguments = module_sources(tools);
    arguments.push_back((tools.lab_root / "ir/main.cp").string());
    arguments.push_back("-o");
    arguments.push_back(app.string());
    auto status = compile(tools, std::move(arguments));
    test_parser::assert_true(status == 0, "lab semantic/IR sample main should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab semantic/IR sample main should pass");
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc != 3) {
        test_parser::fail("usage: lab_semantic_ir_behavior_test <cp> <lab-root>");
    }

    auto tools = test_tools{
        .cp = argv[1],
        .lab_root = argv[2],
    };
    check_harness(tools);
    check_sample_main(tools);
    return 0;
}
