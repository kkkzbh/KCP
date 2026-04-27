import std;
import parser;

#include "assert.hpp"

auto main() -> int
{
    auto sources = source_manager{};

    auto const valid = sources.add_source(
        "api_valid.cp",
        R"(main()
{
    let rows: array<array<i32,3>,2> = [[1, 2, 3], [4, 5, 6]];
    let value = i32(1 + 2) as i32;
    return value;
})");

    auto parsed = parse_translation_unit(sources, valid, parse_options{
        .trace_enabled = true,
    });

    test_parser::assert_true(parsed.accepted, "valid source should parse");
    test_parser::assert_true(parsed.syntax_tree != nullptr, "valid source should produce syntax tree");
    test_parser::assert_true(parsed.lexer_diagnostics.empty(), "valid source should not emit lexer diagnostics");
    test_parser::assert_true(parsed.diagnostics.empty(), "valid source should not emit parser diagnostics");
    test_parser::assert_true(not parsed.trace.empty(), "trace should be populated when enabled");
    test_parser::assert_true(parsed.syntax_tree->functions.size() == 1, "valid source should contain one function");
    test_parser::assert_true(
        to_string(trace_event_kind::enter) == "enter"
            and to_string(trace_event_kind::match) == "match"
            and to_string(trace_event_kind::fail) == "fail"
            and to_string(trace_event_kind::exit) == "exit",
        "trace event strings should remain stable");
    test_parser::assert_true(
        format_trace_event(sources, parsed.trace.front()) == "enter\ttranslation_unit\tidentifier\tmain",
        "formatted trace should include the current token lexeme");
    test_parser::assert_true(
        format_trace_event(
            sources,
            trace_event{
                .kind = trace_event_kind::fail,
                .label = "eof",
                .current = token{
                    .kind = token_kind::eof,
                    .source_span = span{
                        .file = valid,
                        .start = 0,
                        .end = 0,
                    },
                },
            })
            == "fail\teof\teof\t",
        "formatted trace should omit lexeme text for empty spans");

    auto const& function = parsed.syntax_tree->functions.front();
    test_parser::assert_true(std::string(sources.slice(function.name)) == "main", "function name should be main");
    test_parser::assert_true(function.body != nullptr, "function should have a body");
    test_parser::assert_true(function.body->statements.size() == 3, "function body should contain three statements");
    test_parser::assert_true(
        function.body->statements.front()->declared_type != nullptr,
        "first declaration should preserve its explicit type");
    test_parser::assert_true(
        std::string(sources.slice(function.body->statements.front()->declared_type->name.components.front())) == "array",
        "first declaration type should be array");
    test_parser::assert_true(
        function.body->statements.front()->declared_type->type_arguments.size() == 1,
        "outer array type should contain nested type argument");
    test_parser::assert_true(
        function.body->statements.front()->declared_type->literal_arguments.size() == 1,
        "outer array type should contain one literal argument");

    auto const shapes = sources.add_source(
        "api_shapes.cp",
        R"(main()
{
    let pointer: i32 const* = &value;
    let nested: outer<inner<i32>> = seed;
    let empty_array = [];
    let empty_sequence = {};
    counter++;
    other--;
    if(flag) {
        return;
    } else if(other_flag) {
        return;
    } else {
        return;
    }
})");

    auto shaped = parse_translation_unit(sources, shapes);
    test_parser::assert_true(shaped.accepted, "shape-focused source should parse");
    test_parser::assert_true(shaped.syntax_tree != nullptr, "shape-focused source should produce syntax tree");
    test_parser::assert_true(shaped.diagnostics.empty(), "shape-focused source should not emit parser diagnostics");
    test_parser::assert_true(
        shaped.syntax_tree->functions.size() == 1 and shaped.syntax_tree->functions.front().body != nullptr,
        "shape-focused source should produce one function body");

    auto const& shaped_body = shaped.syntax_tree->functions.front().body->statements;
    test_parser::assert_true(shaped_body.size() == 7, "shape-focused source should contain seven statements");
    test_parser::assert_true(
        shaped_body[0]->declared_type != nullptr and shaped_body[0]->declared_type->const_qualified,
        "pointer declaration should keep const-qualified type information");
    test_parser::assert_true(
        shaped_body[0]->declared_type->suffix_operators.size() == 1
            and shaped_body[0]->declared_type->suffix_operators.front() == token_kind::star,
        "pointer declaration should record pointer suffix");
    test_parser::assert_true(
        shaped_body[1]->declared_type != nullptr
            and std::string(sources.slice(shaped_body[1]->declared_type->name.components.front())) == "outer",
        "nested generic declaration should preserve the outer type name");
    test_parser::assert_true(
        shaped_body[1]->declared_type->type_arguments.size() == 1
            and shaped_body[1]->declared_type->type_arguments.front()->type_arguments.size() == 1,
        "nested generic declaration should preserve inner type arguments split from >>");
    test_parser::assert_true(
        shaped_body[2]->expressions.front()->kind == expr_syntax_kind::array_literal
            and shaped_body[2]->expressions.front()->operands.empty(),
        "empty array literal should parse as an array literal with no operands");
    test_parser::assert_true(
        shaped_body[3]->expressions.front()->kind == expr_syntax_kind::sequence_literal
            and shaped_body[3]->expressions.front()->operands.empty(),
        "empty sequence literal should parse as a sequence literal with no operands");
    test_parser::assert_true(
        shaped_body[4]->kind == statement_syntax_kind::expr_stmt
            and shaped_body[4]->expressions.front()->kind == expr_syntax_kind::unary
            and shaped_body[4]->expressions.front()->operator_kind == token_kind::plus_plus,
        "postfix increment should remain an expression statement with unary ++");
    test_parser::assert_true(
        shaped_body[5]->kind == statement_syntax_kind::expr_stmt
            and shaped_body[5]->expressions.front()->kind == expr_syntax_kind::unary
            and shaped_body[5]->expressions.front()->operator_kind == token_kind::minus_minus,
        "postfix decrement should remain an expression statement with unary --");
    test_parser::assert_true(
        shaped_body[6]->kind == statement_syntax_kind::if_stmt
            and shaped_body[6]->statements.size() == 2
            and shaped_body[6]->statements[1]->kind == statement_syntax_kind::if_stmt,
        "else-if chains should stay nested as if statements");
    test_parser::assert_true(
        shaped_body[6]->statements[1]->statements.size() == 2
            and shaped_body[6]->statements[1]->statements[1]->kind == statement_syntax_kind::block,
        "else-if chains should keep the trailing else block");

    auto const invalid = sources.add_source("api_invalid.cp", "main() { let value = @; }");
    auto rejected = parse_translation_unit(sources, invalid);
    test_parser::assert_true(not rejected.accepted, "lexically invalid source should be rejected");
    test_parser::assert_true(not rejected.lexer_diagnostics.empty(), "lexically invalid source should keep lexer diagnostics");
    test_parser::assert_true(rejected.diagnostics.size() == 1, "lexically invalid source should emit parser lexical failure");
    test_parser::assert_true(
        rejected.diagnostics.front().code == parser_diagnostic_code::lexical_failure,
        "lexically invalid source should emit lexical_failure");
    test_parser::assert_true(
        std::string(sources.slice(rejected.diagnostics.front().primary_span)) == "@",
        "lexical_failure should point at the lexer-reported invalid lexeme");

    return 0;
}
