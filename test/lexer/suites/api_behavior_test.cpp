import std;
import source;
import preprocessor;
import lexer;

#include "assert.hpp"

auto main() -> int
{
    auto sources = source_manager{};
    auto const first = sources.add_source("peek.lex", "let value = 1;");
    auto const second = sources.add_source("reset.lex", "return value;");

    auto first_preprocessed = preprocess(sources, first);
    auto first_result = lex(first_preprocessed);
    auto const first_token = first_result.tokens[0];
    test_lexer::assert_true(first_token.kind == token_kind::kw_let, "first token should be let");
    test_lexer::assert_true(has_flag(first_token.flags, token_flags::start_of_line),
        "first token should be marked start_of_line");

    auto const second_token = first_result.tokens[1];
    test_lexer::assert_true(second_token.kind == token_kind::identifier, "second token should be identifier");
    test_lexer::assert_true(std::string{sources.slice(second_token.span)} == "value",
        "identifier lexeme should match");
    test_lexer::assert_true(has_flag(second_token.flags, token_flags::leading_space),
        "identifier after whitespace should record leading_space");
    test_lexer::assert_true(first_result.diagnostics.empty(), "valid first file should not emit diagnostics");

    auto second_preprocessed = preprocess(sources, second);
    auto second_result = lex(second_preprocessed);
    auto const second_file_first = second_result.tokens[0];
    test_lexer::assert_true(second_file_first.kind == token_kind::kw_return,
        "second file should start from its own first token");
    test_lexer::assert_true(has_flag(second_file_first.flags, token_flags::start_of_line),
        "independent lex call should restore line-start state");
    test_lexer::assert_true(std::string{sources.slice(second_file_first.span)} == "return",
        "second file token span should point to its own source text");
    test_lexer::assert_true(second_result.diagnostics.empty(), "valid second file should not emit diagnostics");

    auto dangling_escape_text = std::string{};
    dangling_escape_text.push_back('\'');
    dangling_escape_text.push_back('\\');
    auto const dangling_escape = sources.add_source("dangling_escape_runtime.lex", dangling_escape_text);
    auto dangling_preprocessed = preprocess(sources, dangling_escape);
    auto dangling_result = lex(dangling_preprocessed);

    auto const dangling_token = dangling_result.tokens[0];
    test_lexer::assert_true(dangling_token.kind == token_kind::invalid,
        "dangling escape should produce invalid token");
    test_lexer::assert_true(has_flag(dangling_token.flags, token_flags::unterminated),
        "dangling escape should be unterminated");
    test_lexer::assert_true (
        dangling_result.diagnostics.back().kind == diagnostic_kind::unterminated_char_literal,
        "dangling escape should diagnose unterminated char");

    auto const unterminated_block = sources.add_source("unterminated_block_runtime.lex", "/* block\ncomment");
    auto block_preprocessed = preprocess(sources, unterminated_block);
    test_lexer::assert_true(block_preprocessed.diagnostics.size() == 1,
        "unterminated block comment should produce preprocessor diagnostic");
    auto block_result = lex(block_preprocessed);
    test_lexer::assert_true(block_result.diagnostics.empty(),
        "unterminated block comment should not produce lexer diagnostic");
    test_lexer::assert_true(block_result.tokens.front().kind == token_kind::eof,
        "preprocessed unterminated block comment should leave only eof for lexer");

    auto const comment_span = sources.add_source("comment_span.lex", "let/* inner */value;");
    auto comment_preprocessed = preprocess(sources, comment_span);
    auto comment_result = lex(comment_preprocessed);

    auto const first_after_comment = comment_result.tokens[0];
    auto const second_after_comment = comment_result.tokens[1];
    test_lexer::assert_true(first_after_comment.kind == token_kind::kw_let,
        "token before block comment should still lex correctly");
    test_lexer::assert_true(second_after_comment.kind == token_kind::identifier,
        "token after block comment should still lex correctly");
    test_lexer::assert_true(std::string{sources.slice(second_after_comment.span)} == "value",
        "token span after block comment should slice original source text");
    test_lexer::assert_true(has_flag(second_after_comment.flags, token_flags::leading_space),
        "token after block comment should still observe synthesized separating whitespace");

    test_lexer::assert_true(comment_result.diagnostics.empty(), "valid comment input should not emit diagnostics");

    auto const bang_source = sources.add_source("bang.lex", "! !=");
    auto bang_preprocessed = preprocess(sources, bang_source);
    auto bang_result = lex(bang_preprocessed);
    test_lexer::assert_true(bang_result.diagnostics.empty(), "bang tokens should lex without diagnostics");
    test_lexer::assert_true(bang_result.tokens[0].kind == token_kind::bang, "standalone ! should lex as bang");
    test_lexer::assert_true(bang_result.tokens[1].kind == token_kind::bang_equal, "!= should keep bang_equal token");
    return 0;
}
