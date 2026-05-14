import std;
import parser;
import lexer.scanner;

#include "assert.hpp"

namespace {

[[nodiscard]]
auto run(std::vector<std::string> input) -> op_parse_result
{
    auto const grammar = cp_expression_op_grammar();
    auto table = build_op_precedence_table(grammar);
    return parse_with_op_precedence(grammar, table, input);
}

[[nodiscard]]
auto names(std::initializer_list<std::string_view> items) -> std::vector<std::string>
{
    auto out = std::vector<std::string>{};
    out.reserve(items.size());
    for(auto const& item : items) {
        out.emplace_back(item);
    }
    return out;
}

auto check_construction() -> void
{
    auto const grammar = cp_expression_op_grammar();
    auto table = build_op_precedence_table(grammar);

    test_parser::assert_true (
        table.is_operator_grammar,
        "cp_expression_op_grammar should be an operator grammar");
    test_parser::assert_true (
        table.is_operator_precedence_grammar,
        "cp_expression_op_grammar should be an operator-precedence grammar (no matrix conflicts)");
    test_parser::assert_true (
        table.conflicts.empty(),
        "no conflicts should be reported for cp_expression_op_grammar");

    auto const& start_firstvt = table.firstvt[grammar.start_symbol];
    test_parser::assert_true (
        start_firstvt.contains("plus"),
        "FIRSTVT(start) should contain plus");
    test_parser::assert_true (
        start_firstvt.contains("star"),
        "FIRSTVT(start) should contain star");
    test_parser::assert_true (
        start_firstvt.contains("l_paren"),
        "FIRSTVT(start) should contain l_paren");
    test_parser::assert_true (
        start_firstvt.contains("identifier"),
        "FIRSTVT(start) should contain identifier");
}

auto check_positive_cases() -> void
{
    auto const accepted_simple = run(names({ "identifier", "plus", "identifier", "star", "identifier" }));
    test_parser::assert_true (
        accepted_simple.accepted,
        "id + id * id should be accepted");

    auto const accepted_paren = run(names({
        "l_paren", "identifier", "plus", "identifier", "r_paren", "star", "identifier",
    }));
    test_parser::assert_true (
        accepted_paren.accepted,
        "(id + id) * id should be accepted");

    auto const accepted_chain = run(names({
        "identifier", "less", "identifier", "kw_and", "identifier", "less", "identifier",
    }));
    test_parser::assert_true (
        accepted_chain.accepted,
        "id < id and id < id should be accepted");
}

auto check_negative_cases() -> void
{
    // Two adjacent atoms have no precedence relation in the matrix, so the
    // textbook driver rejects with a diagnostic. (Note: pure operator-precedence
    // parsing cannot detect "id + + id" because the relation between '+' and '+'
    // is well-defined; only the leftmost-prime-phrase shape is checked.)
    auto const rejected = run(names({ "identifier", "identifier" }));
    test_parser::assert_true (
        not rejected.accepted,
        "id id should be rejected (no precedence relation between adjacent atoms)");
    test_parser::assert_true (
        not rejected.diagnostics.empty(),
        "rejected input should report diagnostics");
}

auto check_lexer_integration() -> void
{
    auto sources = source_manager{};
    auto const file = sources.add_source("<test>", std::string("alpha + beta * gamma"));
    auto sink = std::vector<lexer_diagnostic>{};
    auto scanner = lexer{ sources, file, sink };
    auto tokens = scanner.tokenize_all();

    auto stream = tokens_to_terminal_names(tokens);
    auto const grammar = cp_expression_op_grammar();
    auto const table = build_op_precedence_table(grammar);
    auto const result = parse_with_op_precedence(grammar, table, stream);

    test_parser::assert_true (
        result.accepted,
        "lexer-driven 'alpha + beta * gamma' should be accepted");
}

auto check_trace_steps() -> void
{
    auto const grammar = cp_expression_op_grammar();
    auto const table = build_op_precedence_table(grammar);
    auto input = names({ "identifier", "plus", "identifier" });
    auto const result = (
        parse_with_op_precedence (
            grammar,
            table,
            input,
            op_parse_options {
                .trace_enabled = true,
            }
        )
    );

    test_parser::assert_true(result.accepted, "traced operator-precedence parse should be accepted");
    test_parser::assert_true(not result.trace.empty(), "trace should contain operator-precedence steps");
    test_parser::assert_true(result.trace.front().stack == "$", "first trace step should expose initial stack");
    test_parser::assert_true(result.trace.front().action == "init", "first trace step should be init");
    test_parser::assert_true (
        format_trace_step(result.trace.front()).contains("init"),
        "formatted op trace step should include action");
}

auto check_non_operator_grammar() -> void
{
    auto const bad = grammar_definition {
        .start_symbol = "S",
        .productions = {
            grammar_production{ .lhs = "S", .rhs = { "A", "B" } },
            grammar_production{ .lhs = "A", .rhs = { "a" } },
            grammar_production{ .lhs = "B", .rhs = { "b" } },
        },
    };
    auto const table = build_op_precedence_table(bad);
    test_parser::assert_true (
        not table.is_operator_grammar,
        "S -> A B should not be classified as an operator grammar");
    test_parser::assert_true (
        not table.conflicts.empty(),
        "non-operator grammar should report a conflict");
}

auto check_matrix_conflict() -> void
{
    // E -> E + E | a is ambiguous and yields conflicting matrix entries
    // for (+, +): both <· and ·> appear via FIRSTVT/LASTVT closures.
    auto const ambiguous = grammar_definition {
        .start_symbol = "E",
        .productions = {
            grammar_production{ .lhs = "E", .rhs = { "E", "plus", "E" } },
            grammar_production{ .lhs = "E", .rhs = { "a" } },
        },
    };
    auto const table = build_op_precedence_table(ambiguous);
    test_parser::assert_true (
        table.is_operator_grammar,
        "E -> E + E | a is an operator grammar (no adjacent nonterminals nor empty rhs)");
    test_parser::assert_true (
        not table.is_operator_precedence_grammar,
        "ambiguous E -> E + E | a should not be operator-precedence (matrix conflict)");
    test_parser::assert_true (
        not table.conflicts.empty(),
        "ambiguous E -> E + E | a should report conflicts");
}

auto check_pratt_consistency() -> void
{
    // For every pair (a, b) of cp binary operators (excluding kw_as) check that
    // the auto-generated operator-precedence matrix agrees with the relation
    // implied by the Pratt binding-power table. In Pratt left-associative form,
    // after consuming operator a we recurse with min_bp = a.right_bp; the next
    // operator b is bound to the inner expression iff b.left_bp >= a.right_bp.
    // Therefore:
    //   a.right_bp >  b.left_bp  ⇒  a ·> b   (a wins, reduce a first)
    //   a.right_bp <= b.left_bp  ⇒  a <· b   (b wins, shift)
    // Equal binding-power siblings (e.g. plus vs minus) end up ·> by virtue of
    // using left=N, right=N+1 (left-associative).
    auto const grammar = cp_expression_op_grammar();
    auto const table = build_op_precedence_table(grammar);

    for(auto const& a : cp_binary_operator_table) {
        if(a.kind == token_kind::kw_as) {
            continue;
        }
        for(auto const& b : cp_binary_operator_table) {
            if(b.kind == token_kind::kw_as) {
                continue;
            }
            auto const expected = a.right_bp > b.left_bp
                ? op_precedence_relation::kind::gt
                : op_precedence_relation::kind::lt;
            auto const it = table.matrix.find({ std::string(a.terminal_name), std::string(b.terminal_name) });
            test_parser::assert_true (
                it != table.matrix.end(),
                std::format("matrix should have an entry for ({}, {})", a.terminal_name, b.terminal_name));
            test_parser::assert_true (
                it->second == expected,
                std::format (
                    "matrix relation for ({}, {}) should match Pratt: expected {}, got {}",
                    a.terminal_name,
                    b.terminal_name,
                    to_string(expected),
                    to_string(it->second)));
        }
    }
}

} // namespace

auto main() -> int
{
    check_construction();
    check_positive_cases();
    check_negative_cases();
    check_lexer_integration();
    check_trace_steps();
    check_non_operator_grammar();
    check_matrix_conflict();
    check_pratt_consistency();
    return 0;
}
