import std;
import parser;

#include "assert.hpp"

auto main() -> int
{
    auto const grammar = cp_ll1_grammar();
    auto const analysis = analyze_grammar(grammar);
    auto conflicts = std::string{};
    for(auto const& conflict : analysis.conflicts) {
        if(not conflicts.empty()) {
            conflicts += '\n';
        }
        conflicts += conflict;
    }

    test_parser::assert_true(
        analysis.select_sets.size() == grammar.productions.size(),
        "every production should have a select set");
    if(not analysis.conflicts.empty()) {
        std::cerr << conflicts << '\n';
    }
    test_parser::assert_true(
        analysis.conflicts.empty(),
        conflicts.empty() ? "cp LL(1) grammar should not contain select conflicts" : conflicts);
    test_parser::assert_true(
        analysis.first_sets.at("Statement").contains("kw_if"),
        "FIRST(Statement) should contain kw_if");
    test_parser::assert_true(
        analysis.first_sets.at("Statement").contains("identifier"),
        "FIRST(Statement) should contain identifier");
    test_parser::assert_true(
        analysis.follow_sets.at("Expression").contains("semicolon"),
        "FOLLOW(Expression) should contain semicolon");
    test_parser::assert_true(
        analysis.follow_sets.at("Expression").contains("r_paren"),
        "FOLLOW(Expression) should contain r_paren");
    test_parser::assert_true(
        analysis.follow_sets.at("Type").contains("equal"),
        "FOLLOW(Type) should contain equal");

    auto const ambiguous = grammar_definition{
        .start_symbol = "S",
        .productions = {
            grammar_production{ "S", { "a" } },
            grammar_production{ "S", { "a", "b" } },
        },
    };
    auto const ambiguous_analysis = analyze_grammar(ambiguous);
    test_parser::assert_true(
        ambiguous_analysis.conflicts.size() == 1,
        "ambiguous grammar should report exactly one select conflict");
    test_parser::assert_true(
        ambiguous_analysis.conflicts.front() == "S conflict between production 0 and 1 on a",
        "conflict message should include the overlapping terminal set");

    return 0;
}
