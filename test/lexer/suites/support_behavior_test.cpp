import std;
import lexer;

#include "assert.hpp"
#include "case_types.hpp"

using namespace std::literals;

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

    auto* deleted_through_base = static_cast<diagnostic_sink*>(new vector_diagnostic_sink{});
    delete deleted_through_base;

    std::unique_ptr<diagnostic_sink> sink = std::make_unique<vector_diagnostic_sink>();
    sink->report(diagnostic{
        .code = diagnostic_code::invalid_character,
        .message = "invalid character",
        .primary_span = return_span,
    });

    auto* concrete_sink = dynamic_cast<vector_diagnostic_sink*>(sink.get());
    test_lexer::assert_true(concrete_sink != nullptr, "diagnostic sink should downcast to concrete type");
    test_lexer::assert_true(concrete_sink->diagnostics().size() == 1, "diagnostic sink should collect entries");
    concrete_sink->clear();
    test_lexer::assert_true(concrete_sink->diagnostics().empty(), "clear should remove diagnostics");

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
