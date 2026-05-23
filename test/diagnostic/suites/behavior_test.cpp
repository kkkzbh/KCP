import std;
import diagnostic;

auto main() -> int
{
    auto const assert_true = [](bool condition, std::string_view message) {
        if(condition) {
            return;
        }
        std::cerr << "assertion failed: " << message << '\n';
        std::exit(1);
    };

    auto diagnostics = diagnostic_collector{};
    auto const span = source_span {
        .start = 2,
        .end = 3,
    };

    diagnostics.report(diagnostic_kind::invalid_character, span);
    assert_true(diagnostics.size() == 1, "report should append one diagnostic");
    assert_true(diagnostics.span().front().kind == diagnostic_kind::invalid_character,
        "report should preserve diagnostic kind");
    assert_true(diagnostics.span().front().message == "invalid character",
        "default report should use spec message");

    diagnostics.report(diagnostic_kind::expected_token, "expected ';'", span);
    assert_true(diagnostics.size() == 2, "custom report should append one diagnostic");
    assert_true(diagnostics.span().back().message == "expected ';'",
        "custom report should preserve caller message");

    auto taken = diagnostics.take();
    assert_true(taken.size() == 2, "take should return collected diagnostics");
    assert_true(diagnostics.empty(), "take should leave collector empty");

    diagnostics.report(diagnostic_kind::expected_expression, span);
    diagnostics.clear();
    assert_true(diagnostics.empty(), "clear should remove diagnostics");

    auto const info = spec(diagnostic_kind::expected_expression);
    assert_true(info.stage == diagnostic_stage::parser, "parser diagnostic kind should report parser stage");
    assert_true(info.severity == diagnostic_severity::error, "diagnostic severity should be error");
    assert_true(info.code == "expected_expression", "diagnostic kind should be stable");

    auto const warning = spec(diagnostic_kind::empty_statement);
    assert_true(warning.stage == diagnostic_stage::parser, "empty statement should be a parser diagnostic");
    assert_true(warning.severity == diagnostic_severity::warning, "empty statement should be a warning");
    diagnostics.report(diagnostic_kind::empty_statement, span);
    assert_true(not contains_error_diagnostic(diagnostics.span()), "warnings should not count as fatal diagnostics");
    diagnostics.report(diagnostic_kind::expected_statement, span);
    assert_true(contains_error_diagnostic(diagnostics.span()), "errors should count as fatal diagnostics");

    return 0;
}
