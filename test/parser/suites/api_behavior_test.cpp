import std;
import preprocessor;
import lexer;
import parser;

#include "assert.hpp"

namespace {
[[nodiscard]]
auto first_function(parse_result const& parsed) -> function_syntax const&
{
    auto const& unit = *parsed.root;
    return parsed.ast.node(unit.functions.front());
}

[[nodiscard]]
auto function_body(parse_result const& parsed, function_syntax const& function)
    -> block_statement_syntax const&
{
    return as<block_statement_syntax>(parsed.ast.node(function.body));
}

[[nodiscard]]
auto declaration(parse_result const& parsed, stmt_id statement)
    -> declaration_statement_syntax const&
{
    return as<declaration_statement_syntax>(parsed.ast.node(statement));
}

[[nodiscard]]
auto expression_statement(parse_result const& parsed, stmt_id statement)
    -> expression_statement_syntax const&
{
    return as<expression_statement_syntax>(parsed.ast.node(statement));
}

[[nodiscard]]
auto type_argument_type(type_argument_syntax const& argument) -> type_id
{
    return as<type_argument_type_syntax>(argument).type;
}

auto parse_source(source_manager& sources, file_id file) -> parse_result
{
    auto preprocessed = preprocess(sources, file);
    test_parser::assert_true(preprocessed.diagnostics.empty(), "test source should not emit preprocessor diagnostics");

    auto lexical = lex(preprocessed);
    test_parser::assert_true(lexical.diagnostics.empty(), "test source should not emit lexer diagnostics");

    return parse_translation_unit(std::move(lexical.tokens));
}
} // namespace

auto main() -> int
{
    auto sources = source_manager{};
    auto const ast_source = ast_source_view{ sources };

    auto const module_source = sources.add_source (
        "api_module.cp",
        R"(export module math.core;
import std.io;

main()
{
    return;
})");
    auto module_parsed = parse_source(sources, module_source);
    test_parser::assert_true(module_parsed.accepted, "module source should parse");
    test_parser::assert_true(module_parsed.root != std::nullopt, "module source should produce syntax tree");
    test_parser::assert_true(module_parsed.root->module_header != std::nullopt, "module source should keep module header");
    test_parser::assert_true (
        ast_source.module_name(module_parsed.root->module_header->name) == "math.core",
        "ast_source_view should normalize module names");
    test_parser::assert_true(module_parsed.root->imports.size() == 1, "module source should keep import declaration");
    test_parser::assert_true (
        ast_source.module_name(module_parsed.root->imports.front().name) == "std.io",
        "ast_source_view should normalize import names");

    auto const valid = sources.add_source (
        "api_valid.cp",
        R"(main()
{
    let rows: array<array<i32,3>,2> = [[1, 2, 3], [4, 5, 6]];
    let value = i32(1 + 2) as i32;
    return value;
})");

    auto parsed = parse_source(sources, valid);

    test_parser::assert_true(parsed.accepted, "valid source should parse");
    test_parser::assert_true(parsed.root != std::nullopt, "valid source should produce syntax tree");
    test_parser::assert_true(parsed.diagnostics.empty(), "valid source should not emit parser diagnostics");
    test_parser::assert_true (
        parsed.root->functions.size() == 1,
        "valid source should contain one function");

    auto const& function = first_function(parsed);
    auto const& body = function_body(parsed, function);
    test_parser::assert_true(ast_source.slice(function.name) == "main", "function name should be main");
    test_parser::assert_true(function.body.valid(), "function should have a body");
    test_parser::assert_true(body.statements.size() == 3, "function body should contain three statements");

    auto const& rows = declaration(parsed, body.statements.front());
    test_parser::assert_true(rows.declared_type != std::nullopt, "first declaration should preserve its explicit type");
    auto const& rows_type = parsed.ast.node(*rows.declared_type);
    test_parser::assert_true (
        ast_source.identifier(rows_type.name) == "array",
        "first declaration type should be array");
    test_parser::assert_true(rows_type.arguments.size() == 2, "outer array type should contain two arguments");
    test_parser::assert_true (
        is<type_argument_type_syntax>(rows_type.arguments.front()),
        "outer array first argument should be nested type");
    test_parser::assert_true (
        is<type_argument_literal_syntax>(rows_type.arguments.back()),
        "outer array second argument should be literal argument");

    auto const shapes = sources.add_source (
        "api_shapes.cp",
        R"(main()
{
    let pointer: i32 const**& = value;
    let nested: outer<inner<i32>> = seed;
    let empty_array = [];
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

    auto shaped = parse_source(sources, shapes);
    test_parser::assert_true(shaped.accepted, "shape-focused source should parse");
    test_parser::assert_true(shaped.root != std::nullopt, "shape-focused source should produce syntax tree");
    test_parser::assert_true(shaped.diagnostics.empty(), "shape-focused source should not emit parser diagnostics");

    auto const& shaped_function = first_function(shaped);
    auto const& shaped_body = function_body(shaped, shaped_function);
    test_parser::assert_true(shaped_body.statements.size() == 6, "shape-focused source should contain six statements");

    auto const& pointer = declaration(shaped, shaped_body.statements[0]);
    test_parser::assert_true(pointer.declared_type != std::nullopt, "pointer declaration should keep explicit type");
    auto const& pointer_type = shaped.ast.node(*pointer.declared_type);
    test_parser::assert_true(pointer_type.is_const, "pointer declaration should keep final pointee const information");
    test_parser::assert_true (
        pointer_type.suffix_operators.size() == 3
            and pointer_type.suffix_operators.front() == token_kind::star
            and pointer_type.suffix_operators[1] == token_kind::star
            and pointer_type.suffix_operators.back() == token_kind::amp,
        "pointer declaration should record pointer-reference suffixes");

    auto const& nested = declaration(shaped, shaped_body.statements[1]);
    test_parser::assert_true(nested.declared_type != std::nullopt, "nested generic declaration should keep explicit type");
    auto const& outer_type = shaped.ast.node(*nested.declared_type);
    test_parser::assert_true (
        ast_source.identifier(outer_type.name) == "outer",
        "nested generic declaration should preserve the outer type name");
    auto const inner_type_id = type_argument_type(outer_type.arguments.front());
    auto const& inner_type = shaped.ast.node(inner_type_id);
    test_parser::assert_true (
        outer_type.arguments.size() == 1 and inner_type.arguments.size() == 1,
        "nested generic declaration should preserve inner type arguments split from >>");

    auto const recovered_source = sources.add_source (
        "api_recovery.cp",
        R"(main()
{
    let before = 1;
    module;
    let after = 2;
})");
    auto recovered = parse_source(sources, recovered_source);
    test_parser::assert_true(not recovered.accepted, "recovery-focused source should be rejected");
    test_parser::assert_true(recovered.root != std::nullopt, "recovery-focused source should still produce syntax tree");
    test_parser::assert_true (
        not recovered.diagnostics.empty(),
        "recovery-focused source should emit parser diagnostics");
    auto const& recovered_function = first_function(recovered);
    auto const& recovered_body = function_body(recovered, recovered_function);
    test_parser::assert_true (
        recovered_body.statements.size() == 2,
        "statement recovery should continue after a bad token inside a block");

    auto const unclosed_source = sources.add_source (
        "api_unclosed_recovery.cp",
        R"(main()
{
    let before = 1;
    let broken = call(;
    let after = 2;
})");
    auto unclosed = parse_source(sources, unclosed_source);
    test_parser::assert_true(not unclosed.accepted, "unclosed recovery source should be rejected");
    test_parser::assert_true(unclosed.root != std::nullopt, "unclosed recovery source should still produce syntax tree");
    test_parser::assert_true (
        not unclosed.diagnostics.empty(),
        "unclosed recovery source should emit parser diagnostics");
    auto const& unclosed_function = first_function(unclosed);
    auto const& unclosed_body = function_body(unclosed, unclosed_function);
    test_parser::assert_true (
        unclosed_body.statements.size() == 2,
        "statement recovery should stop at semicolon even after an unclosed parenthesis");

    auto const& empty_array = declaration(shaped, shaped_body.statements[2]);
    auto const& empty_array_expr = shaped.ast.node(empty_array.initializer);
    test_parser::assert_true (
        is<array_literal_expr_syntax>(empty_array_expr)
            and as<array_literal_expr_syntax>(empty_array_expr).elements.empty(),
        "empty array literal should parse as an array literal with no operands");

    auto const& increment_stmt = expression_statement(shaped, shaped_body.statements[3]);
    auto const& increment = as<unary_expr_syntax>(shaped.ast.node(increment_stmt.expression));
    test_parser::assert_true (
        increment.operator_kind == token_kind::plus_plus and increment.position == unary_position::postfix,
        "postfix increment should remain an expression statement with unary ++");

    auto const& decrement_stmt = expression_statement(shaped, shaped_body.statements[4]);
    auto const& decrement = as<unary_expr_syntax>(shaped.ast.node(decrement_stmt.expression));
    test_parser::assert_true (
        decrement.operator_kind == token_kind::minus_minus and decrement.position == unary_position::postfix,
        "postfix decrement should remain an expression statement with unary --");

    auto const& if_stmt = as<if_statement_syntax>(shaped.ast.node(shaped_body.statements[5]));
    test_parser::assert_true(if_stmt.else_branch != std::nullopt, "if statement should keep else-if branch");
    auto const& else_if = as<if_statement_syntax>(shaped.ast.node(*if_stmt.else_branch));
    test_parser::assert_true (
        else_if.else_branch
            and is<block_statement_syntax>(shaped.ast.node(*else_if.else_branch)),
        "else-if chains should keep the trailing else block");

    return 0;
}
