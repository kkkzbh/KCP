import std;
import source;
import preprocessor;

#include "assert.hpp"

using namespace std::literals;

auto main() -> int
{
    auto const diagnostic_kind_names = std::array {
        std::pair{diagnostic_kind::unterminated_block_comment, "unterminated_block_comment"sv},
    };

    for (auto const [kind, expected_name] : diagnostic_kind_names) {
        test_preprocessor::assert_true(spec(kind).code == expected_name,
            "preprocess diagnostic kind should have a stable string name");
    }

    auto sources = source_manager{};

    auto const valid_text = "let s = \"\\\"/* //\\\"\"; /*x*/\nreturn value / other;"sv;
    auto const valid_file = sources.add_source("valid_runtime.cp", std::string{valid_text});
    auto const valid_result = preprocess(sources, valid_file);

    test_preprocessor::assert_true(valid_result.diagnostics.empty(),
        "valid input should not produce preprocessor diagnostics");
    test_preprocessor::assert_true(valid_result.normalized_text.size() == valid_text.size(),
        "valid input should preserve source length");

    auto const invalid_text = "let value = 1; /* unterminated\ncomment"sv;
    auto const invalid_file = sources.add_source("unterminated_runtime.cp", std::string{invalid_text});
    auto const invalid_result = preprocess(sources, invalid_file);
    auto const comment_start = invalid_text.find("/*");

    test_preprocessor::assert_true(invalid_result.normalized_text.size() == invalid_text.size(),
        "invalid input should preserve source length");
    test_preprocessor::assert_true(invalid_result.diagnostics.size() == 1,
        "unterminated block comment should produce one preprocessor diagnostic");

    auto const& diagnostic = invalid_result.diagnostics.front();
    auto const invalid_start = sources.file_start(invalid_file);
    test_preprocessor::assert_true (
        diagnostic.primary_span == source_span{
            .start = invalid_start + static_cast<byte_pos>(comment_start),
            .end = invalid_start + static_cast<byte_pos>(invalid_text.size()),
        },
        "preprocessor diagnostic span should cover unterminated block comment");
    test_preprocessor::assert_true(std::string{sources.slice(diagnostic.primary_span)}
            == std::string{invalid_text.substr(comment_start)},
        "preprocessor diagnostic span should slice original unterminated comment text");

    return 0;
}
