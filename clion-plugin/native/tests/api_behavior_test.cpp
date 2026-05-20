import std;
import cp_lexer_helper;

using namespace std::literals;

namespace {

auto contains(std::string_view haystack, std::string_view needle) -> bool
{
    return haystack.find(needle) != std::string_view::npos;
}

auto has_diagnostic(cp_lexer_helper::inspect_result const& result, std::string_view stage, std::string_view code) -> bool
{
    return std::ranges::any_of(result.diagnostics, [&](auto const& diagnostic) {
        return diagnostic.stage == stage and diagnostic.code == code;
    });
}

auto has_highlight(cp_lexer_helper::inspect_result const& result, std::string_view category) -> bool
{
    return std::ranges::any_of(result.highlights, [&](auto const& highlight) {
        return highlight.category == category;
    });
}

auto highlight_count(cp_lexer_helper::inspect_result const& result, std::string_view source, std::string_view category, std::string_view text) -> std::size_t
{
    return static_cast<std::size_t>(std::ranges::count_if(result.highlights, [&](auto const& highlight) {
        return highlight.category == category
               and highlight.end_offset <= source.size()
               and source.substr(highlight.start_offset, highlight.end_offset - highlight.start_offset) == text;
    }));
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
        std::pair{ "missing_exponent_digits", "1e+"sv },
        std::pair{ "leading_zero_integer", "012"sv },
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

    auto constexpr rich_source = R"(struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    zero() -> vec2
    {
        return vec2{ .x = 0, .y = 0 };
    }

    sum(self const&) -> i32
    {
        return self.x + self.y;
    }
}

variant event {
    quit;
    resize(i32, i32);
}

score(value: event) -> i32
{
    return match value {
        .resize(width, height) => width + height,
        .quit => 0,
    };
}

main() -> i32
{
    let point = vec2::zero();
    return point.sum() + score(event::resize(5, 7));
})"sv;
    auto const rich_highlights = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "rich.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "rich.cp",
                std::string{ rich_source },
            },
        },
    });
    assert_true(rich_highlights.accepted, "rich highlight sample should pass semantic analysis");
    assert_true(highlight_count(rich_highlights, rich_source, "associated.function.call", "zero") == 1uz,
        "inspect should distinguish associated function calls");
    assert_true(highlight_count(rich_highlights, rich_source, "member.function.call", "sum") == 1uz,
        "inspect should distinguish member function calls");
    assert_true(highlight_count(rich_highlights, rich_source, "variant.case", "resize") == 3uz,
        "inspect should distinguish variant constructors from associated function calls");
    assert_true(highlight_count(rich_highlights, rich_source, "pattern.binding", "width") == 1uz,
        "inspect should highlight match payload bindings");

    auto constexpr global_source = R"(type public_int = i32;

enum state: i32 {
    ready = 0;
    done = 1;
}

concept measurable {
    type item;
    size(self const&) -> i32;
}

struct box {
    value: i32;
}

impl box {
    type scalar = i32;

    box(value: i32)
    {
        return box{ .value = value };
    }

    ~box()
    {
    }

    get(self const&) -> i32
    {
        return value;
    }

    zero() -> box
    {
        return box{ .value = 0 };
    }
}

impl measurable for box {
    type item = i32;

    size(self const&) -> i32
    {
        return value;
    }
}

main() -> i32
{
    type payload = box::item;
    let current = state::ready;
    let scalar_value: box::scalar = 2;
    const bias = 1;
    let inc: f(i32) -)"
R"(> i32 = f(value) => value + 1;
    let add_bias = f(value: i32) -)"
R"(> i32 {
        value + bias
    };
    let memory = alloc<i32>(1);
    free(memory);
    let value = box::zero();
    return value.get() + value.size() + scalar_value + (1.5 as i32) + inc(1) + add_bias(1);
})"sv;
    auto const global_highlights = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "global.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "global.cp",
                std::string{ global_source },
            },
        },
    });
    assert_true(global_highlights.accepted, "global highlight sample should pass semantic analysis");
    assert_true(highlight_count(global_highlights, global_source, "type.alias.declaration", "public_int") == 1uz,
        "inspect should distinguish top-level type aliases");
    assert_true(highlight_count(global_highlights, global_source, "enum.case", "ready") == 2uz,
        "inspect should distinguish enum case declarations and references");
    assert_true(highlight_count(global_highlights, global_source, "concept.declaration", "measurable") == 1uz,
        "inspect should distinguish concept declarations");
    assert_true(highlight_count(global_highlights, global_source, "associated.type.requirement", "item") == 1uz,
        "inspect should distinguish associated type requirements");
    assert_true(highlight_count(global_highlights, global_source, "associated.type.declaration", "item") == 1uz,
        "inspect should distinguish associated type declarations");
    assert_true(highlight_count(global_highlights, global_source, "associated.type.declaration", "scalar") == 1uz,
        "inspect should distinguish inherent associated type declarations");
    assert_true(highlight_count(global_highlights, global_source, "associated.type.reference", "item") == 1uz,
        "inspect should distinguish associated type references");
    assert_true(highlight_count(global_highlights, global_source, "associated.type.reference", "scalar") == 1uz,
        "inspect should distinguish inherent associated type references");
    assert_true(highlight_count(global_highlights, global_source, "constructor.declaration", "box") == 1uz,
        "inspect should distinguish constructors");
    assert_true(highlight_count(global_highlights, global_source, "destructor.declaration", "box") == 1uz,
        "inspect should distinguish destructors");
    assert_true(highlight_count(global_highlights, global_source, "builtin.function.call", "alloc") == 1uz,
        "inspect should distinguish builtin calls");
    assert_true(highlight_count(global_highlights, global_source, "function.type", "f") == 1uz,
        "inspect should highlight function type markers");
    assert_true(highlight_count(global_highlights, global_source, "lambda.marker", "f") == 2uz,
        "inspect should highlight lambda markers");
    assert_true(highlight_count(global_highlights, global_source, "lambda.capture.reference", "bias") == 1uz,
        "inspect should distinguish lambda captures");
    assert_true(highlight_count(global_highlights, global_source, "constant.declaration", "bias") == 1uz,
        "inspect should distinguish const local declarations");

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

    auto const partial_semantic = cp_lexer_helper::inspect(cp_lexer_helper::inspect_request {
        .active_file = "partial_semantic.cp",
        .files = {
            cp_lexer_helper::source_file_record {
                "partial_semantic.cp",
                R"(main()
{
    let value = missing_type{ .field = 1 };
    return;
})",
            },
        },
    });
    assert_true(not partial_semantic.accepted, "semantically invalid struct initialization should be rejected");
    assert_true(has_diagnostic(partial_semantic, "semantic", "unknown_type"),
        "invalid struct initialization should still report semantic diagnostics");

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
