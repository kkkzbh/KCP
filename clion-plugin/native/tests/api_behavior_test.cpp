import std;
import cp_lexer_helper;

using namespace std::literals;

namespace {

auto contains(std::string_view haystack, std::string_view needle) -> bool
{
    return haystack.find(needle) != std::string_view::npos;
}

auto has_diagnostic(
    cp_lexer_helper::inspect_result const& result,
    std::string_view stage,
    std::string_view code
) -> bool
{
    return std::ranges::any_of(result.diagnostics, [&](auto const& diagnostic) {
        return diagnostic.stage == stage and diagnostic.code == code;
    });
}

auto has_highlight(
    cp_lexer_helper::inspect_result const& result,
    std::string_view category
) -> bool
{
    return std::ranges::any_of(result.highlights, [&](auto const& highlight) {
        return highlight.category == category;
    });
}

} // namespace

auto main() -> int
{
    auto const assert_true = [](bool condition, std::string_view message) {
        if(condition) {
            return;
        }

        std::cerr << "assertion failed: " << message << '\n';
        std::exit(1);
    };

    auto const valid_diagnostics = cp_lexer_helper::analyze (
        "valid.cp",
        "let value = 1;\nreturn value;\n");
    assert_true(valid_diagnostics.empty(), "valid input should not produce diagnostics");

    auto const invalid_diagnostics = std::array {
        std::pair{ "invalid_character", "let value = 1; @"sv },
        std::pair{ "unterminated_string_literal", "\"unterminated\n"sv },
        std::pair{ "unterminated_char_literal", "'a\n"sv },
        std::pair{ "unterminated_block_comment", "let value = 1; /* comment"sv },
        std::pair{ "invalid_char_literal", "'ab'"sv },
        std::pair{ "invalid_escape_sequence", "\"\\q\""sv },
        std::pair{ "invalid_number_suffix", "123suffix"sv },
    };

    for(auto const& [expected_kind, source] : invalid_diagnostics) {
        auto const diagnostics = cp_lexer_helper::analyze("invalid.cp", source);
        assert_true(diagnostics.size() == 1, "each invalid sample should produce one diagnostic");
        assert_true(diagnostics.front().code == expected_kind, "diagnostic kind should match");
        assert_true(diagnostics.front().severity == "error", "severity should be error");
        assert_true(diagnostics.front().line == 1, "single-line invalid samples should map to line 1");
        assert_true(diagnostics.front().column >= 1, "columns should be one-based");
        assert_true(diagnostics.front().end_offset >= diagnostics.front().start_offset,
            "diagnostic span should be non-negative");
    }

    auto const positioned = cp_lexer_helper::analyze (
        "position.cp",
        "let ok = 1;\nlet bad = @;\n");
    assert_true(positioned.size() == 1, "position sample should produce one diagnostic");
    assert_true(positioned.front().line == 2, "diagnostic line should map through source manager");
    assert_true(positioned.front().column == 11, "diagnostic column should point at invalid character");

    auto const tokens = cp_lexer_helper::tokenize("tokens.cp", "let value = 42;\n");
    assert_true(tokens.size() >= 5, "token stream should contain source tokens and eof");
    assert_true(tokens.front().kind == "kw_let", "first token should be kw_let");
    assert_true(tokens.front().start_offset == 0, "first token should start at 0");
    assert_true(tokens.front().start_of_line, "first token should mark start of line");
    assert_true(tokens[1].kind == "identifier", "second token should be identifier");
    assert_true(tokens[1].lexeme == "value", "identifier lexeme should match");
    assert_true(tokens[1].leading_space, "identifier should record leading space");
    assert_true(tokens.back().kind == "eof", "helper token mode should include eof");

    auto const diagnostics_json = cp_lexer_helper::diagnostics_to_json(positioned);
    assert_true(contains(diagnostics_json, "\"code\":\"invalid_character\""),
        "diagnostic json should contain diagnostic kind");
    assert_true(contains(diagnostics_json, "\"startOffset\":22"),
        "diagnostic json should contain start offset");
    assert_true(contains(diagnostics_json, "\"column\":11"),
        "diagnostic json should contain column");

    auto const tokens_json = cp_lexer_helper::tokens_to_json(tokens);
    assert_true(contains(tokens_json, "\"kind\":\"kw_let\""),
        "token json should contain token kind");
    assert_true(contains(tokens_json, "\"lexeme\":\"value\""),
        "token json should contain token lexeme");
    assert_true(contains(tokens_json, "\"startOfLine\":true"),
        "token json should include token flags");

    auto const multi_file = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "main.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "math.cp",
                R"(export module math;

export add(a: i32, b: i32) -> i32
{
    return a + b;
})",
            },
            cp_lexer_helper::source_file_record {
                "main.cp",
                R"(import math;

main() -> i32
{
    return add(1, 2);
})",
            },
        },
    });
    assert_true(multi_file.accepted, "multi-file inspect should pass import/export semantic analysis");
    assert_true(multi_file.diagnostics.empty(), "valid multi-file inspect should not return diagnostics");
    assert_true(has_highlight(multi_file, "import.name"), "inspect should highlight import module names");
    assert_true(has_highlight(multi_file, "function.call"), "inspect should highlight function calls");
    assert_true(has_highlight(multi_file, "type"), "inspect should highlight type names");

    auto const unknown_module = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "unknown.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "unknown.cp",
                "import missing.core;\nmain() { return; }\n",
            },
        },
    });
    assert_true(not unknown_module.accepted, "unknown module inspect should fail semantic analysis");
    assert_true(has_diagnostic(unknown_module, "semantic", "unknown_module"),
        "unknown module inspect should report a semantic diagnostic");

    auto const parser_error = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "parser_error.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "parser_error.cp",
                "main() { let value = ; return missing; }\n",
            },
        },
    });
    assert_true(not parser_error.accepted, "parser error inspect should fail");
    assert_true(has_diagnostic(parser_error, "parser", "expected_expression"),
        "parser error inspect should report parser diagnostics");
    assert_true(not has_diagnostic(parser_error, "semantic", "unknown_name"),
        "parser error inspect should not run semantic analysis");

    auto const active_only = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "active.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "active.cp",
                "main() { return; }\n",
            },
            cp_lexer_helper::source_file_record {
                "broken.cp",
                "main() { return @; }\n",
            },
        },
    });
    assert_true(not active_only.accepted, "inactive file syntax errors should make project inspect fail");
    assert_true(active_only.diagnostics.empty(), "inspect should return only active-file diagnostics");

    auto analyze_in = std::istringstream{"let bad = @;"};
    auto analyze_out = std::ostringstream{};
    auto analyze_err = std::ostringstream{};
    auto const analyze_args = std::array {
        "analyze"sv, "--stdin"sv, "--filename"sv, "cli.cp"sv, "--format"sv, "json"sv,
    };
    auto const analyze_code = cp_lexer_helper::run_cli (
        analyze_args,
        analyze_in,
        analyze_out,
        analyze_err);
    assert_true(analyze_code == 0, "analyze cli should succeed");
    assert_true(analyze_err.view().empty(), "analyze cli should not write stderr on success");
    assert_true(contains(analyze_out.view(), "\"code\":\"invalid_character\""),
        "analyze cli should emit diagnostic json");

    auto inspect_in = std::istringstream{
        R"({"activeFile":"cli.cp","files":[{"path":"cli.cp","text":"main() { return; }\n"}]})"
    };
    auto inspect_out = std::ostringstream{};
    auto inspect_err = std::ostringstream{};
    auto const inspect_args = std::array {
        "inspect"sv, "--stdin"sv, "--format"sv, "json"sv,
    };
    auto const inspect_code = cp_lexer_helper::run_cli (
        inspect_args,
        inspect_in,
        inspect_out,
        inspect_err);
    assert_true(inspect_code == 0, "inspect cli should succeed");
    assert_true(inspect_err.view().empty(), "inspect cli should not write stderr on success");
    assert_true(contains(inspect_out.view(), "\"accepted\":true"),
        "inspect cli should emit inspection json");
    assert_true(contains(inspect_out.view(), "\"highlights\""),
        "inspect cli should emit highlight json");

    auto tokens_in = std::istringstream{"let value = 0;"};
    auto tokens_out = std::ostringstream{};
    auto tokens_err = std::ostringstream{};
    auto const tokens_args = std::array {
        "tokens"sv, "--stdin"sv, "--filename"sv, "cli.cp"sv, "--format"sv, "json"sv,
    };
    auto const tokens_code = cp_lexer_helper::run_cli (
        tokens_args,
        tokens_in,
        tokens_out,
        tokens_err);
    assert_true(tokens_code == 0, "tokens cli should succeed");
    assert_true(tokens_err.view().empty(), "tokens cli should not write stderr on success");
    assert_true(contains(tokens_out.view(), "\"kind\":\"kw_let\""),
        "tokens cli should emit token json");

    auto invalid_in = std::istringstream{"ignored"};
    auto invalid_out = std::ostringstream{};
    auto invalid_err = std::ostringstream{};
    auto const invalid_args = std::array {
        "analyze"sv, "--filename"sv, "missing.cp"sv, "--format"sv, "json"sv,
    };
    auto const invalid_code = cp_lexer_helper::run_cli (
        invalid_args,
        invalid_in,
        invalid_out,
        invalid_err);
    assert_true(invalid_code == 2, "invalid cli arguments should fail");
    assert_true(contains(invalid_err.view(), "usage: cp-lexer-helper"),
        "invalid cli arguments should print usage");

    return 0;
}
