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
    auto path = std::filesystem::temp_directory_path() / std::format("cp-lab-lexer-test-{}-{}", name, stamp);
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
        (tools.lab_root / "lexer/lexer.cp").string()
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
	R"(import lexer;
	import preprocessor;
	import diagnostic;

	preprocess_text(text: str) -> preprocessed_file
	{
	    let file = source_file{"<memory>", text};
	    return preprocess(file);
	}

	main() -> i32
	{
	    let program = lex(preprocess_text("int main() { return 0; }"));
	    if(program.diagnostics.size() != 0) { return 1; }
	    if(program.tokens.size() != 10) { return 2; }
	    if(program.tokens[0].kind != token_kind::kw_int) { return 3; }
	    if(program.tokens[1].kind != token_kind::identifier) { return 4; }
	    if(program.tokens[6].kind != token_kind::integer_literal) { return 5; }
	    if(program.tokens[9].kind != token_kind::eof) { return 6; }

	    let words = lex(preprocess_text("int integer void if else while return for break continue"));
	    if(words.diagnostics.size() != 0) { return 7; }
	    if(words.tokens[0].kind != token_kind::kw_int) { return 8; }
	    if(words.tokens[1].kind != token_kind::identifier) { return 9; }
    if(words.tokens[2].kind != token_kind::kw_void) { return 10; }
    if(words.tokens[3].kind != token_kind::kw_if) { return 11; }
    if(words.tokens[4].kind != token_kind::kw_else) { return 12; }
	    if(words.tokens[5].kind != token_kind::kw_while) { return 13; }
	    if(words.tokens[6].kind != token_kind::kw_return) { return 14; }
	    if(words.tokens[7].kind != token_kind::kw_for) { return 58; }
	    if(words.tokens[8].kind != token_kind::kw_break) { return 59; }
	    if(words.tokens[9].kind != token_kind::kw_continue) { return 60; }

	    let symbols = lex(preprocess_text("( ) { } [ ] , ; + - * / % = == != < <= > >= && ||"));
	    if(symbols.diagnostics.size() != 0) { return 15; }
	    if(symbols.tokens[0].kind != token_kind::l_paren) { return 16; }
	    if(symbols.tokens[1].kind != token_kind::r_paren) { return 17; }
    if(symbols.tokens[2].kind != token_kind::l_brace) { return 18; }
    if(symbols.tokens[3].kind != token_kind::r_brace) { return 19; }
    if(symbols.tokens[4].kind != token_kind::l_bracket) { return 61; }
    if(symbols.tokens[5].kind != token_kind::r_bracket) { return 62; }
    if(symbols.tokens[6].kind != token_kind::comma) { return 20; }
    if(symbols.tokens[7].kind != token_kind::semicolon) { return 21; }
    if(symbols.tokens[8].kind != token_kind::plus) { return 22; }
    if(symbols.tokens[9].kind != token_kind::minus) { return 23; }
    if(symbols.tokens[10].kind != token_kind::star) { return 24; }
    if(symbols.tokens[11].kind != token_kind::slash) { return 25; }
    if(symbols.tokens[12].kind != token_kind::percent) { return 26; }
    if(symbols.tokens[13].kind != token_kind::equal) { return 27; }
    if(symbols.tokens[14].kind != token_kind::equal_equal) { return 28; }
    if(symbols.tokens[15].kind != token_kind::bang_equal) { return 29; }
    if(symbols.tokens[16].kind != token_kind::less) { return 30; }
    if(symbols.tokens[17].kind != token_kind::less_equal) { return 31; }
    if(symbols.tokens[18].kind != token_kind::greater) { return 32; }
    if(symbols.tokens[19].kind != token_kind::greater_equal) { return 33; }
	    if(symbols.tokens[20].kind != token_kind::amp_amp) { return 34; }
	    if(symbols.tokens[21].kind != token_kind::pipe_pipe) { return 35; }

	    let comments = lex(preprocess_text("int/* block */void// line\nreturn"));
	    if(comments.diagnostics.size() != 0) { return 36; }
	    if(comments.tokens.size() != 4) { return 37; }
	    if(comments.tokens[0].kind != token_kind::kw_int) { return 38; }
	    if(comments.tokens[1].kind != token_kind::kw_void) { return 39; }
	    if(comments.tokens[2].kind != token_kind::kw_return) { return 40; }

	    let comment_eof = lex(preprocess_text("int // line comment at eof"));
	    if(comment_eof.diagnostics.size() != 0) { return 48; }
	    if(comment_eof.tokens.size() != 2) { return 49; }
	    if(comment_eof.tokens[0].kind != token_kind::kw_int) { return 50; }
	    if(comment_eof.tokens[1].kind != token_kind::eof) { return 51; }

	    let position_source = source_file{"<memory>", "int\n  return\r\n\twhile"};
	    let position = lex(preprocess(position_source));
	    let return_position = position_source.position(position.tokens[1].span.start);
	    let while_position = position_source.position(position.tokens[2].span.start);
	    if(return_position.line != 2) { return 41; }
	    if(return_position.column != 3) { return 42; }
	    if(while_position.line != 3) { return 52; }
	    if(while_position.column != 2) { return 53; }

	    let error_source = source_file{"<memory>", "09 12abc @ ! & | /*"};
	    let error_preprocessed = preprocess(error_source);
	    if(error_preprocessed.diagnostics.size() != 1) { return 47; }
	    if(error_preprocessed.diagnostics[0].kind != diagnostic_kind::unterminated_block_comment) { return 57; }
	    let errors = lex(error_preprocessed);
	    if(errors.diagnostics.size() != 6) { return 43; }
	    if(errors.diagnostics[0].kind != diagnostic_kind::leading_zero_integer) { return 44; }
	    if(errors.diagnostics[1].kind != diagnostic_kind::invalid_number_suffix) { return 45; }
	    if(errors.diagnostics[2].kind != diagnostic_kind::invalid_character) { return 46; }
	    if(errors.diagnostics[3].kind != diagnostic_kind::invalid_character) { return 54; }
	    if(errors.diagnostics[4].kind != diagnostic_kind::invalid_character) { return 55; }
	    if(errors.diagnostics[5].kind != diagnostic_kind::invalid_character) { return 56; }
	    return 0;
	})"
    );

    auto status = compile_modules(tools, source, app);
    test_parser::assert_true(status == 0, "lab lexer harness should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab lexer harness should pass");
}

auto check_sample_main(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("sample");
    auto app = dir / "lexer-main";
    auto status = compile_modules(tools, tools.lab_root / "lexer/main.cp", app);
    test_parser::assert_true(status == 0, "lab lexer sample main should compile");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "lab lexer sample main should pass");
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc != 3) {
        test_parser::fail("usage: lab_lexer_behavior_test <cp> <lab-root>");
    }

    auto tools = test_tools{
        .cp = argv[1],
        .lab_root = argv[2]
    };

    check_harness(tools);
    check_sample_main(tools);
    return 0;
}
