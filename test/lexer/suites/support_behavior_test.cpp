import std;
import lexer;

#include "assert.hpp"
#include "case_types.hpp"

using namespace std::literals;

namespace {
struct recording_lexer_diagnostic_sink
{
    auto report(lexer_diagnostic value) -> void
    {
        diagnostics_.push_back(std::move(value));
    }

    [[nodiscard]]
    auto diagnostics() const -> std::vector<lexer_diagnostic> const&
    {
        return diagnostics_;
    }

private:
    std::vector<lexer_diagnostic> diagnostics_{};
};

static_assert(lexer_diagnostic_sink<std::vector<lexer_diagnostic>>);
static_assert(lexer_diagnostic_sink<recording_lexer_diagnostic_sink>);
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

    auto const alpha_start = sources.file_start(alpha);
    auto const to_global = [&](std::size_t local) { return alpha_start + static_cast<byte_pos>(local); };

    test_lexer::assert_true (
        sources.position(to_global(0)) == source_position{.line = 1, .column = 1},
        "position should report first character");
    test_lexer::assert_true (
        sources.position(to_global(return_offset)) == source_position{.line = 2, .column = 3},
        "position should track multi-line offsets");
    test_lexer::assert_true (
        sources.position(to_global(x_offset)) == source_position{.line = 2, .column = 10},
        "position should report later columns");
    test_lexer::assert_true (
        sources.position(to_global(file_text.size())) == source_position{.line = 3, .column = 1},
        "position should resolve eof sentinel back to its owning file");

    auto const return_span = source_span{
        .start = to_global(return_offset),
        .end = to_global(return_offset + "return"sv.size()),
    };
    test_lexer::assert_true(sources.slice(return_span) == "return", "slice should extract substrings");

    auto default_diagnostic = lexer_diagnostic{};
    test_lexer::assert_true(default_diagnostic.message.empty(), "diagnostic message should default construct");

    auto sink = std::vector<lexer_diagnostic>{};
    sink.emplace_back (
        lexer_diagnostic_severity::error,
        lexer_diagnostic_code::invalid_character,
        "invalid character",
        return_span);

    test_lexer::assert_true(sink.size() == 1, "diagnostic sink should collect entries");
    sink.clear();
    test_lexer::assert_true(sink.empty(), "clear should remove diagnostics");

    auto custom_sink = recording_lexer_diagnostic_sink{};
    auto const invalid = sources.add_source("invalid.lex", "@");
    auto custom_lexer = lexer{ sources, invalid, custom_sink };
    static_assert(std::same_as<decltype(custom_lexer), lexer>);

    auto const invalid_token = custom_lexer.next();
    test_lexer::assert_true(invalid_token.kind == token_kind::invalid,
        "custom diagnostic sink should allow lexer construction");
    test_lexer::assert_true(custom_sink.diagnostics().size() == 1,
        "custom diagnostic sink should receive diagnostics");
    test_lexer::assert_true(custom_sink.diagnostics().front().code == lexer_diagnostic_code::invalid_character,
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
    return 0;
}
