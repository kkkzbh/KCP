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
auto type_alias_statement(parse_result const& parsed, stmt_id statement)
    -> type_alias_statement_syntax const&
{
    return as<type_alias_statement_syntax>(parsed.ast.node(statement));
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

	    auto const concept_source = sources.add_source (
	        "api_concept.cp",
	        R"(variant optional<T> {
	    none;
	    some(T);
	}

	concept iterator {
	    type item;
	    next(self: Self&) -> item;
	}

struct range_iter {
    value: i32;
}

impl iterator for range_iter {
    type item = i32;

    next(self: range_iter&) -> i32
    {
        return value;
    }
}

main()
{
    type value_type = range_iter::item;
	    let value: value_type = 1;
	    let next = optional<i32>::some(value);
	    let none = optional<i32>::none;
	    match next {
	        .some(item) => item,
	        .none => 0,
	    };
	})");
	    auto concept_parsed = parse_source(sources, concept_source);
	    test_parser::assert_true(concept_parsed.accepted, "concept source should parse");
	    test_parser::assert_true(concept_parsed.root->variants.size() == 1, "concept source should keep variant declarations");
	    auto const& optional_decl = concept_parsed.ast.node(concept_parsed.root->variants.front());
	    test_parser::assert_true(optional_decl.generic_parameters.size() == 1, "variant should preserve generic parameters");
	    test_parser::assert_true(optional_decl.cases.size() == 2, "variant should preserve cases");
	    test_parser::assert_true(optional_decl.cases.back().payloads.size() == 1, "variant case should preserve payload types");
	    test_parser::assert_true(concept_parsed.root->concepts.size() == 1, "concept source should keep concept declarations");
    test_parser::assert_true(
        concept_parsed.root->concept_impls.size() == 1,
        "concept source should keep concept impl declarations");
    auto const& concept_decl = concept_parsed.ast.node(concept_parsed.root->concepts.front());
    test_parser::assert_true(concept_decl.items.size() == 2, "concept declaration should preserve its items");
    auto const& concept_impl = concept_parsed.ast.node(concept_parsed.root->concept_impls.front());
    test_parser::assert_true(concept_impl.type_aliases.size() == 1, "concept impl should preserve type aliases");
    test_parser::assert_true(concept_impl.functions.size() == 1, "concept impl should preserve functions");
    auto const& concept_main = concept_parsed.ast.node(concept_parsed.root->functions.front());
    auto const& concept_body = function_body(concept_parsed, concept_main);
    auto const& alias = type_alias_statement(concept_parsed, concept_body.statements.front());
    auto const& alias_type = concept_parsed.ast.node(alias.type);
    test_parser::assert_true(
        alias_type.associated_names.size() == 1,
        "type alias statement should preserve associated type paths");
    auto const& associated_decl = declaration(concept_parsed, concept_body.statements[1]);
    auto const& associated_type = concept_parsed.ast.node(*associated_decl.declared_type);
    test_parser::assert_true(
        ast_source.identifier(associated_type.name) == "value_type",
        "declaration should preserve local type alias names");
    auto const& associated_call_decl = declaration(concept_parsed, concept_body.statements[2]);
    auto const& associated_call = as<call_expr_syntax>(concept_parsed.ast.node(associated_call_decl.initializer));
    auto const& associated_callee = as<associated_name_expr_syntax>(concept_parsed.ast.node(associated_call.callee));
    auto const& associated_callee_type = concept_parsed.ast.node(associated_callee.type);
    test_parser::assert_true(
        associated_callee_type.arguments.size() == 1,
        "generic associated name expression should preserve type arguments");
    auto const& associated_value_decl = declaration(concept_parsed, concept_body.statements[3]);
	    test_parser::assert_true(
	        is<associated_name_expr_syntax>(concept_parsed.ast.node(associated_value_decl.initializer)),
	        "generic associated value expression should parse as associated name");
	    auto const& match_statement = expression_statement(concept_parsed, concept_body.statements[4]);
	    auto const& match = as<match_expr_syntax>(concept_parsed.ast.node(match_statement.expression));
	    test_parser::assert_true(match.arms.size() == 2, "match expression should preserve arms");
    auto const& some_pattern = as<match_case_pattern_syntax>(match.arms.front().pattern);
    test_parser::assert_true(some_pattern.bindings.size() == 1, "match case pattern should preserve bindings");

    auto const pack_source = sources.add_source (
        "api_template_for_pack.cp",
        R"(sum<T...>(values: T...) -> i32
requires T...: display
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value;
    }
    template fo)" "r" R"( (type U : T...) {
        type current = U;
    }
    return total;
})");
    auto pack_parsed = parse_source(sources, pack_source);
    test_parser::assert_true(pack_parsed.accepted, "template for pack source should parse");
    auto const& pack_function = first_function(pack_parsed);
    test_parser::assert_true(pack_function.generic_parameters.size() == 1, "pack function should preserve generic parameter");
    test_parser::assert_true(pack_function.generic_parameters.front().is_pack, "generic parameter should preserve pack marker");
    test_parser::assert_true(pack_function.parameters.size() == 1, "pack function should preserve value parameter");
    test_parser::assert_true(pack_function.parameters.front().is_pack, "value parameter should preserve pack marker");
    test_parser::assert_true(pack_function.requires_clause != std::nullopt, "pack function should preserve requires clause");
    auto const& pack_constraint = pack_function.requires_clause->constraints.front();
    auto const& pack_bound = as<concept_type_bound_constraint_syntax>(pack_constraint);
    test_parser::assert_true(pack_bound.is_pack, "requires clause should preserve type pack marker");
    auto const& pack_body = function_body(pack_parsed, pack_function);
    auto const& value_template_for = as<template_for_statement_syntax>(pack_parsed.ast.node(pack_body.statements[1]));
    test_parser::assert_true(
        value_template_for.binding_kind == template_for_binding_kind::let_binding,
        "template for value binding should preserve binding kind");
    auto const& type_template_for = as<template_for_statement_syntax>(pack_parsed.ast.node(pack_body.statements[2]));
    test_parser::assert_true(
        type_template_for.binding_kind == template_for_binding_kind::type_binding,
        "template for type binding should preserve binding kind");

    auto const bad_concept_impl_source = sources.add_source (
        "api_bad_concept_impl.cp",
        "impl iterator range_iter { }");
    auto bad_concept_impl = parse_source(sources, bad_concept_impl_source);
    test_parser::assert_true(
        not bad_concept_impl.accepted,
        "concept impl without for should be rejected");

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

    auto const function_type_source = sources.add_source (
        "api_function_type.cp",
        R"(add(x: i32, y: i32) -> i32
{
    return x + y;
}

main()
{
    let callback: f(left: i32, right: i32) -> i32 = add;
    let raw: f*(i32, i32) -> i32 = add;
    let pointer = alloc<i32>(1);
    let ref alias = pointer;
    let (first, second) = (1, 2);
    const ref
        (left, right) = (first, second);
    type alias_type = decltype(alias);
    let inc: f(i32) -> i32 =
        fn(x: i32) => x + 1;
})");
    auto function_type_parsed = parse_source(sources, function_type_source);
    test_parser::assert_true(function_type_parsed.accepted, "function type source should parse");
    test_parser::assert_true(function_type_parsed.root->functions.size() == 3, "lambda should add one synthetic function");
    auto const& function_type_main = function_type_parsed.ast.node(function_type_parsed.root->functions[1]);
    auto const& function_type_body = function_body(function_type_parsed, function_type_main);
    auto const& callback_decl = declaration(function_type_parsed, function_type_body.statements.front());
    auto const& callback_type = function_type_parsed.ast.node(*callback_decl.declared_type);
    test_parser::assert_true(callback_type.is_function_type, "f(...) type should be recorded as a function type");
    test_parser::assert_true(not callback_type.is_function_pointer, "f(...) type should not be a function pointer");
    test_parser::assert_true(callback_type.function_parameters.size() == 2, "function type should preserve parameter types");
    test_parser::assert_true(
        callback_type.function_parameters.front().name != std::nullopt,
        "function type should preserve optional parameter names");
    auto const& raw_decl = declaration(function_type_parsed, function_type_body.statements[1]);
    auto const& raw_type = function_type_parsed.ast.node(*raw_decl.declared_type);
    test_parser::assert_true(raw_type.is_function_pointer, "f*(...) type should be recorded as a function pointer");
    auto const& alloc_decl = declaration(function_type_parsed, function_type_body.statements[2]);
    auto const& alloc_call = as<call_expr_syntax>(function_type_parsed.ast.node(alloc_decl.initializer));
    test_parser::assert_true(alloc_call.type_arguments.size() == 1, "generic call should preserve type arguments");
    auto const& ref_decl = declaration(function_type_parsed, function_type_body.statements[3]);
    test_parser::assert_true(ref_decl.is_ref, "ref declaration should preserve ref mode");
    auto const& destructure_decl = declaration(function_type_parsed, function_type_body.statements[4]);
    test_parser::assert_true(destructure_decl.binding_names.size() == 2, "tuple destructuring should preserve bindings");
    auto const& ref_destructure_decl = declaration(function_type_parsed, function_type_body.statements[5]);
    test_parser::assert_true(
        ref_destructure_decl.is_ref and ref_destructure_decl.binding_names.size() == 2,
        "ref tuple destructuring should preserve ref mode and bindings");
    auto const& decltype_alias = type_alias_statement(function_type_parsed, function_type_body.statements[6]);
    auto const& decltype_type = function_type_parsed.ast.node(decltype_alias.type);
    test_parser::assert_true(decltype_type.is_decltype, "decltype type expression should be preserved in AST");
    auto const& lambda_decl = declaration(function_type_parsed, function_type_body.statements[7]);
    test_parser::assert_true(
        is<lambda_expr_syntax>(function_type_parsed.ast.node(lambda_decl.initializer)),
        "lambda expression should be preserved in AST");

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

    auto const index_source = sources.add_source (
        "api_index.cp",
        R"(main()
{
    let rows: array<array<i32,3>,2> = [[1, 2, 3], [4, 5, 6]];
    let value = rows[0][1];
    rows[1][2] = value;
})");
    auto indexed = parse_source(sources, index_source);
    test_parser::assert_true(indexed.accepted, "index expression source should parse");
    auto const& indexed_function = first_function(indexed);
    auto const& indexed_body = function_body(indexed, indexed_function);
    test_parser::assert_true(indexed_body.statements.size() == 3, "index expression source should contain three statements");

    auto const& indexed_value = declaration(indexed, indexed_body.statements[1]);
    auto const& nested_index = as<index_expr_syntax>(indexed.ast.node(indexed_value.initializer));
    test_parser::assert_true (
        is<index_expr_syntax>(indexed.ast.node(nested_index.object)),
        "nested index expression should keep the inner index as its object");

    auto const& index_assignment_statement = expression_statement(indexed, indexed_body.statements[2]);
    auto const& index_assignment = as<assignment_expr_syntax>(indexed.ast.node(index_assignment_statement.expression));
    test_parser::assert_true (
        is<index_expr_syntax>(indexed.ast.node(index_assignment.left)),
        "index expression should parse as an assignment target");

    return 0;
}
