import std;
import source;
import preprocessor;

#include "assert.hpp"

using namespace std::literals;

auto main() -> int
{
    auto const diagnostic_kind_names = std::array {
        std::pair{preprocess_diagnostic_kind::unterminated_block_comment, "unterminated_block_comment"sv},
    };

    for (auto const [kind, expected_name] : diagnostic_kind_names) {
        test_preprocessor::assert_true(to_string(kind) == expected_name,
            "preprocess diagnostic kind should have a stable string name");
    }

    auto sources = source_manager{};

    auto const valid_text = "let s = \"\\\"/* //\\\"\"; /*x*/\nreturn value / other;"sv;
    auto const valid_file = sources.add_source("valid_runtime.cp", std::string{valid_text});
    auto const valid_result = preprocess(sources, valid_file);

    test_preprocessor::assert_true(valid_result.issues.empty(), "valid input should not produce issues");
    test_preprocessor::assert_true(valid_result.normalized_text.size() == valid_text.size(),
        "valid input should preserve source length");

    auto const invalid_text = "let value = 1; /* unterminated\ncomment"sv;
    auto const invalid_file = sources.add_source("unterminated_runtime.cp", std::string{invalid_text});
    auto const invalid_result = preprocess(sources, invalid_file);
    auto const comment_start = invalid_text.find("/*");

    test_preprocessor::assert_true(invalid_result.normalized_text.size() == invalid_text.size(),
        "invalid input should preserve source length");
    test_preprocessor::assert_true(invalid_result.issues.size() == 1,
        "unterminated block comment should produce one issue");

    auto const* issue = invalid_result.issue_at(comment_start);
    test_preprocessor::assert_true(issue != nullptr, "issue_at should find issue at comment start");
    test_preprocessor::assert_true(invalid_result.issue_at(comment_start + 1) == nullptr,
        "issue_at should not match inside an issue span");
    test_preprocessor::assert_true(invalid_result.issue_at(invalid_text.size()) == nullptr,
        "issue_at should return nullptr at eof");
    auto const invalid_start = sources.file_start(invalid_file);
    test_preprocessor::assert_true (
        issue->span == source_span{
            .start = invalid_start + static_cast<byte_pos>(comment_start),
            .end = invalid_start + static_cast<byte_pos>(invalid_text.size()),
        },
        "issue span should cover unterminated block comment");
    test_preprocessor::assert_true(std::string{sources.slice(issue->span)}
            == std::string{invalid_text.substr(comment_start)},
        "issue span should slice original unterminated comment text");

    return 0;
}
