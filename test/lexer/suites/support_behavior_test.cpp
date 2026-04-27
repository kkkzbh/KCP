import std;
import lexer;

#include "assert.hpp"
#include "case_types.hpp"

using namespace std::literals;

namespace {
struct recording_diagnostic_sink
{
    auto report(diagnostic value) -> void
    {
        diagnostics_.push_back(std::move(value));
    }

    [[nodiscard]]
    auto diagnostics() const -> std::vector<diagnostic> const&
    {
        return diagnostics_;
    }

private:
    std::vector<diagnostic> diagnostics_{};
};

static_assert(diagnostic_sink<vector_diagnostic_sink>);
static_assert(diagnostic_sink<recording_diagnostic_sink>);
} // namespace

auto main() -> int
{
    auto sources = source_manager{};
    auto const alpha = sources.add_source("alpha.lex", "let x = 1;\n  return x;\n");
    auto const beta = sources.add_source("beta.lex", "module beta;");

    test_lexer::assert_true(sources.name(alpha) == "alpha.lex", "source_manager::name should return file name");
    test_lexer::assert_true(sources.name(beta) == "beta.lex", "source_manager::name should work for later files");

    auto const file_text = sources.text(alpha);
    auto const return_offset = file_text.find("return");
    auto const x_offset = file_text.rfind('x');

    test_lexer::assert_true(
        sources.position(alpha, 0) == source_position{.line = 1, .column = 1},
        "position should report first character");
    test_lexer::assert_true(
        sources.position(alpha, return_offset) == source_position{.line = 2, .column = 3},
        "position should track multi-line offsets");
    test_lexer::assert_true(
        sources.position(alpha, x_offset) == source_position{.line = 2, .column = 10},
        "position should report later columns");
    test_lexer::assert_true(
        sources.position(alpha, file_text.size()) == source_position{.line = 3, .column = 1},
        "position should allow eof");
    test_lexer::assert_true(
        sources.position(alpha, file_text.size() + 4) == source_position{.line = 3, .column = 1},
        "position should clamp offsets past eof");

    auto const return_span = span{
        .file = alpha,
        .start = return_offset,
        .end = return_offset + "return"sv.size(),
    };
    test_lexer::assert_true(sources.slice(return_span) == "return", "slice should extract substrings");

    auto default_diagnostic = diagnostic{};
    test_lexer::assert_true(default_diagnostic.message.empty(), "diagnostic message should default construct");

    auto sink = vector_diagnostic_sink{};
    sink.report(diagnostic{
        .code = diagnostic_code::invalid_character,
        .message = "invalid character",
        .primary_span = return_span,
    });

    test_lexer::assert_true(sink.diagnostics().size() == 1, "diagnostic sink should collect entries");
    sink.clear();
    test_lexer::assert_true(sink.diagnostics().empty(), "clear should remove diagnostics");

    auto custom_sink = recording_diagnostic_sink{};
    auto const invalid = sources.add_source("invalid.lex", "@");
    auto custom_lexer = lexer{ sources, invalid, custom_sink };
    static_assert(std::same_as<decltype(custom_lexer), lexer<recording_diagnostic_sink>>);

    auto const invalid_token = custom_lexer.next();
    test_lexer::assert_true(invalid_token.kind == token_kind::invalid,
        "custom diagnostic sink should allow lexer ctad construction");
    test_lexer::assert_true(custom_sink.diagnostics().size() == 1,
        "custom diagnostic sink should receive diagnostics");
    test_lexer::assert_true(custom_sink.diagnostics().front().code == diagnostic_code::invalid_character,
        "custom diagnostic sink should observe invalid character diagnostics");

    test_lexer::assert_true(test_lexer::all_token_kinds.size() == 72, "token list should stay exhaustive");
    auto const to_string_ptr = &to_string;
    test_lexer::assert_true(to_string_ptr(token_kind::kw_let) == "kw_let",
        "function pointer should call to_string");
    for (auto const kind : test_lexer::all_token_kinds) {
        auto const text = to_string(kind);
        test_lexer::assert_true(text != "unknown", "known token kinds should have spellings");
    }

    test_lexer::assert_true(to_string(token_kind::kw_let) == "kw_let",
        "to_string should match keyword tokens");
    test_lexer::assert_true(to_string(token_kind::arrow) == "arrow",
        "to_string should match punctuator tokens");
    test_lexer::assert_true(to_string(static_cast<token_kind>(255)) == "unknown",
        "unknown token kinds should fall back");
    return 0;
}
