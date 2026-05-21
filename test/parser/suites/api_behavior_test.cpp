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

    auto const never_source = sources.add_source (
        "api_never.cp",
        R"(panic(message: str) -> !
{
}

fail() -> !
{
    panic("x");
}

main()
{
    let value: i32 = match optional<i32>::none {
        .some(item) => item,
        .none => panic("none"),
    };
})");
    auto never_parsed = parse_source(sources, never_source);
    test_parser::assert_true(never_parsed.accepted, "never type source should parse");
    auto const& panic_decl = first_function(never_parsed);
    test_parser::assert_true(static_cast<bool>(panic_decl.return_type), "panic declaration should keep return type");
    test_parser::assert_true(
        never_parsed.ast.node(*panic_decl.return_type).is_never_type,
        "panic return type should be parsed as never");

    auto const enum_source = sources.add_source (
        "api_enum_opaque.cp",
        R"(export module fs;

export enum open_flag : u8 {
    read = 1 << 0;
    write = 1 << 1;
}

export type file_handle = opaque u8*;

main()
{
    let flag = open_flag::read;
})");
    auto enum_parsed = parse_source(sources, enum_source);
    test_parser::assert_true(enum_parsed.accepted, "enum and opaque type source should parse");
    test_parser::assert_true(enum_parsed.root->enums.size() == 1, "translation unit should keep enum declarations");
    auto const& enum_decl = enum_parsed.ast.node(enum_parsed.root->enums.front());
    test_parser::assert_true(ast_source.identifier(enum_decl.name) == "open_flag", "enum should preserve its name");
    test_parser::assert_true(enum_decl.exported, "export enum should preserve export marker");
    test_parser::assert_true(enum_decl.cases.size() == 2, "enum should preserve cases");
    test_parser::assert_true(enum_parsed.root->type_aliases.size() == 1, "translation unit should keep top-level type aliases");
    auto const& opaque_alias = enum_parsed.ast.node(enum_parsed.root->type_aliases.front());
    test_parser::assert_true(opaque_alias.exported, "export type should preserve export marker");
    test_parser::assert_true(opaque_alias.opaque, "opaque type alias should preserve opaque marker");

    auto const extern_source = sources.add_source (
        "api_extern.cp",
        R"(extern "C" putchar(ch: i32) -> i32;

export extern "C" answer() -> i32
{
    return 42;
})");
    auto extern_parsed = parse_source(sources, extern_source);
    test_parser::assert_true(extern_parsed.accepted, "extern source should parse");
    test_parser::assert_true(extern_parsed.root->functions.size() == 2, "extern source should keep both functions");
    auto const& extern_decl = extern_parsed.ast.node(extern_parsed.root->functions.front());
    test_parser::assert_true(extern_decl.extern_abi != std::nullopt, "extern declaration should keep ABI marker");
    test_parser::assert_true(not extern_decl.has_body, "extern declaration should not require a body");
    test_parser::assert_true(
        ast_source.identifier(extern_decl.name) == "putchar",
        "extern declaration should preserve its function name");
    auto const& extern_def = extern_parsed.ast.node(extern_parsed.root->functions.back());
    test_parser::assert_true(extern_def.extern_abi != std::nullopt, "extern definition should keep ABI marker");
    test_parser::assert_true(extern_def.exported, "export extern definition should keep export marker");
    test_parser::assert_true(extern_def.has_body, "extern definition should keep its body");

	    auto const concept_source = sources.add_source (
	        "api_concept.cp",
	        R"(variant optional<T> {
	    none;
	    some(T);
	}

	concept iterator {
	    type item;
	    next(self&) -> item;
	}

struct range_iter {
    value: i32;
}

impl iterator for range_iter {
    type item = i32;

    next(self&) -> i32
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

    auto const generic_concept_source = sources.add_source (
        "api_generic_concept.cp",
        R"(concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}

struct box<T> {
    value: T;
}

impl<T> partial_eq for box<T>
requires T: partial_eq
{
    equals(self const&, rhs: box<T> const&) -> bool
    {
        return value.equals(rhs.value);
    }
}

impl partial_eq<str> for string_like {
}
)"
    );
    auto generic_concept_parsed = parse_source(sources, generic_concept_source);
    test_parser::assert_true(generic_concept_parsed.accepted, "generic concept source should parse");
    auto const& generic_concept = generic_concept_parsed.ast.node(generic_concept_parsed.root->concepts.front());
    test_parser::assert_true(generic_concept.generic_parameters.size() == 1, "concept should preserve generic parameters");
    test_parser::assert_true(
        generic_concept.generic_parameters.front().default_argument != std::nullopt,
        "concept generic parameter should preserve its default argument");
    auto const& generic_impl = generic_concept_parsed.ast.node(generic_concept_parsed.root->concept_impls.front());
    test_parser::assert_true(generic_impl.generic_parameters.size() == 1, "concept impl should preserve explicit generic parameters");
    test_parser::assert_true(generic_impl.concept_name.arguments.empty(), "concept impl should preserve omitted concept arguments");
    auto const& explicit_impl = generic_concept_parsed.ast.node(generic_concept_parsed.root->concept_impls.back());
    test_parser::assert_true(explicit_impl.concept_name.arguments.size() == 1, "concept impl should preserve explicit concept arguments");
    test_parser::assert_true(explicit_impl.generic_parameters.empty(), "non-generic concept impl should allow an omitted impl parameter list");

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
    let rows: [[i32; 3]; 2] = [[1, 2, 3], [4, 5, 6]];
    let pair: (i32, f64) = (1, 2.0);
    let single: (i32,) = (1,);
    let value = (1 + 2) as i32;
    return value + pair.0 + single.0;
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
    test_parser::assert_true(body.statements.size() == 5, "function body should contain five statements");

    auto const& rows = declaration(parsed, body.statements.front());
    test_parser::assert_true(rows.declared_type != std::nullopt, "first declaration should preserve its explicit type");
    auto const& rows_type = parsed.ast.node(*rows.declared_type);
    test_parser::assert_true (
        rows_type.is_array_type,
        "first declaration type should be a modern array type");
    test_parser::assert_true (
        parsed.ast.node(rows_type.array_element).is_array_type,
        "outer array element should be nested array type");
    test_parser::assert_true (
        is<type_argument_literal_syntax>(rows_type.array_length),
        "outer array length should be literal argument");

    auto const& pair = declaration(parsed, body.statements[1]);
    test_parser::assert_true(pair.declared_type != std::nullopt, "tuple declaration should preserve type");
    auto const& pair_type = parsed.ast.node(*pair.declared_type);
    test_parser::assert_true(pair_type.is_tuple_type, "pair type should be a tuple type");
    test_parser::assert_true(pair_type.tuple_elements.size() == 2, "pair tuple should contain two elements");

    auto const& single = declaration(parsed, body.statements[2]);
    test_parser::assert_true(single.declared_type != std::nullopt, "single tuple declaration should preserve type");
    auto const& single_type = parsed.ast.node(*single.declared_type);
    test_parser::assert_true(single_type.is_tuple_type, "single type should be a tuple type");
    test_parser::assert_true(single_type.tuple_elements.size() == 1, "single tuple should contain one element");

    auto const grouped_source = sources.add_source (
        "api_grouped_type.cp",
        R"(main(input: (i32)) -> i32
{
    return input;
})");
    auto grouped = parse_source(sources, grouped_source);
    test_parser::assert_true(grouped.accepted, "grouped type source should parse");
    auto const& grouped_parameter = first_function(grouped).parameters.front();
    test_parser::assert_true(grouped_parameter.type != std::nullopt, "grouped parameter should preserve its type");
    auto const& grouped_type = grouped.ast.node(*grouped_parameter.type);
    test_parser::assert_true(grouped_type.is_grouped_type, "(T) should parse as a grouped type");

    auto const inferred_parameter_source = sources.add_source (
        "api_inferred_parameter_type.cp",
        R"(id(value) -> i32
{
    return value;
}

borrow(value&) -> i32
{
    return value;
}

read(value const&) -> i32
{
    return value;
}

take(value move&) -> i32
{
    return value;
})");
    auto inferred_parameters = parse_source(sources, inferred_parameter_source);
    test_parser::assert_true(inferred_parameters.accepted, "inferred parameter type source should parse");
    test_parser::assert_true(inferred_parameters.root->functions.size() == 4, "inferred parameter type source should keep functions");
    auto const& inferred_value = inferred_parameters.ast.node(inferred_parameters.root->functions[0]).parameters.front();
    test_parser::assert_true(inferred_value.type == std::nullopt, "plain inferred parameter should omit its type node");
    test_parser::assert_true(not inferred_value.inferred_is_reference, "plain inferred parameter should not record a reference suffix");
    auto const& inferred_borrow = inferred_parameters.ast.node(inferred_parameters.root->functions[1]).parameters.front();
    test_parser::assert_true(
        inferred_borrow.type == std::nullopt and inferred_borrow.inferred_is_reference,
        "inferred reference parameter should omit its type node and record '&'");
    auto const& inferred_read = inferred_parameters.ast.node(inferred_parameters.root->functions[2]).parameters.front();
    test_parser::assert_true(
        inferred_read.inferred_is_reference and inferred_read.inferred_reference_is_const,
        "inferred const reference parameter should record 'const&'");
    auto const& inferred_take = inferred_parameters.ast.node(inferred_parameters.root->functions[3]).parameters.front();
    test_parser::assert_true(
        inferred_take.inferred_is_reference and inferred_take.inferred_reference_is_move,
        "inferred move reference parameter should record 'move&'");

    auto mixed_inferred_type = parse_source(
        sources,
        sources.add_source (
            "api_mixed_inferred_parameter_type.cp",
            "bad(value&: i32) -> i32 { return value; }"
        )
    );
    test_parser::assert_true(not mixed_inferred_type.accepted, "mixed inferred suffix and explicit type should be rejected");

    auto const shapes = sources.add_source (
        "api_shapes.cp",
        R"(main()
{
    let pointer: i32 const**& = value;
    let forwarded: i32 like**& = value;
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
    test_parser::assert_true(shaped_body.statements.size() == 7, "shape-focused source should contain seven statements");

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

    auto const& forwarded = declaration(shaped, shaped_body.statements[1]);
    test_parser::assert_true(forwarded.declared_type != std::nullopt, "like pointer declaration should keep explicit type");
    auto const& forwarded_type = shaped.ast.node(*forwarded.declared_type);
    test_parser::assert_true(forwarded_type.is_like, "like pointer declaration should keep receiver-const forwarding");
    test_parser::assert_true (
        forwarded_type.suffix_operators.size() == 3
            and forwarded_type.suffix_operators.front() == token_kind::star
            and forwarded_type.suffix_operators[1] == token_kind::star
            and forwarded_type.suffix_operators.back() == token_kind::amp,
        "like pointer declaration should record pointer-reference suffixes");

    auto const& nested = declaration(shaped, shaped_body.statements[2]);
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
        f(x: i32) => x + 1;
    let identity = f<T>(value: T) -> T {
        value
    };
})");
    auto function_type_parsed = parse_source(sources, function_type_source);
    test_parser::assert_true(function_type_parsed.accepted, "function type source should parse");
    test_parser::assert_true(function_type_parsed.root->functions.size() == 4, "lambdas should add synthetic functions");
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
    auto const& generic_lambda_decl = declaration(function_type_parsed, function_type_body.statements[8]);
    auto const& generic_lambda = as<lambda_expr_syntax>(function_type_parsed.ast.node(generic_lambda_decl.initializer));
    auto const& generic_lambda_function = function_type_parsed.ast.node(generic_lambda.function);
    test_parser::assert_true(
        generic_lambda_function.generic_parameters.size() == 1,
        "generic lambda should preserve generic parameters");

    auto const ambiguity_source = sources.add_source (
        "api_comparison_generic_ambiguity.cp",
        R"(id<T>(value: T) -> T
{
    return value;
}

main()
{
    let value = -1;
    if(value < 0) {
        let fixed = 0 as i32;
    }
    if((value < 10) and (value >= 0)) {
        let bounded = id<i32>(value);
    }
    while(value < 3) {
        value = value + 1;
    }
    let before_generic_call = value < id<i32>(4);
    let after = id<i32>(value);
})");
    auto ambiguity = parse_source(sources, ambiguity_source);
    test_parser::assert_true(ambiguity.accepted, "comparison and generic-call ambiguity source should parse");
    auto const& ambiguity_main = ambiguity.ast.node(ambiguity.root->functions[1]);
    auto const& ambiguity_body = function_body(ambiguity, ambiguity_main);
    auto const& first_if = as<if_statement_syntax>(ambiguity.ast.node(ambiguity_body.statements[1]));
    auto const& first_condition = as<binary_expr_syntax>(ambiguity.ast.node(first_if.condition));
    test_parser::assert_true(
        first_condition.operator_kind == token_kind::less,
        "comparison before a later type call should parse as a less-than binary expression");
    auto const& second_if = as<if_statement_syntax>(ambiguity.ast.node(ambiguity_body.statements[2]));
    auto const& second_condition = as<binary_expr_syntax>(ambiguity.ast.node(second_if.condition));
    test_parser::assert_true(
        second_condition.operator_kind == token_kind::kw_and,
        "parenthesized comparisons should stay inside the logical expression");
    auto const& before_generic_decl = declaration(ambiguity, ambiguity_body.statements[4]);
    auto const& before_generic = as<binary_expr_syntax>(ambiguity.ast.node(before_generic_decl.initializer));
    test_parser::assert_true(
        before_generic.operator_kind == token_kind::less
            and is<call_expr_syntax>(ambiguity.ast.node(before_generic.right)),
        "comparison immediately before a generic call should keep the call on the right operand");
    auto const& after_decl = declaration(ambiguity, ambiguity_body.statements[5]);
    auto const& after_call = as<call_expr_syntax>(ambiguity.ast.node(after_decl.initializer));
    test_parser::assert_true(
        after_call.type_arguments.size() == 1,
        "generic call parsing should still preserve explicit type arguments");

    auto const all_operator_source = sources.add_source (
        "api_all_operator_expressions.cp",
        R"(id<T>(value: T) -> T
{
    return value;
}

main()
{
    let value = 16;
    let pointer: i32* = &value;
    let bool_or = true or id<bool>(false);
    let bool_and = true and id<bool>(true);
    let bit_or = value | id<i32>(1);
    let bit_xor = value ^ id<i32>(2);
    let bit_and = value & id<i32>(3);
    let equal = value == id<i32>(16);
    let not_equal = value != id<i32>(0);
    let less = value < id<i32>(20);
    let less_equal = value <= id<i32>(16);
    let ordering = value <=> id<i32>(16);
    let greater = value > id<i32>(4);
    let greater_equal = value >= id<i32>(16);
    let shift_left = value << id<i32>(1);
    let shift_right = value >> id<i32>(1);
    let add = value + id<i32>(1);
    let sub = value - id<i32>(1);
    let mul = value * id<i32>(2);
    let div = value / id<i32>(2);
    let rem = value % id<i32>(3);
    let casted = id<i32>(1) as i64;

    value = id<i32>(1);
    value += id<i32>(1);
    value -= id<i32>(1);
    value *= id<i32>(2);
    value /= id<i32>(2);
    value %= id<i32>(2);
    value &= id<i32>(1);
    value |= id<i32>(1);
    value ^= id<i32>(1);
    value <<= id<i32>(1);
    value >>= id<i32>(1);

    let positive = +value;
    let negative = -value;
    let logical = not true;
    let inverted = ~value;
    let address = &value;
    let loaded = *pointer;
    let alias = ref value;
    let moved = move value;
    let const_alias = const ref value;
    ++value;
    --value;
    value++;
    value--;
})");
    auto all_operators = parse_source(sources, all_operator_source);
    test_parser::assert_true(all_operators.accepted, "all operator expression source should parse");
    auto const& all_operator_main = all_operators.ast.node(all_operators.root->functions[1]);
    auto const& all_operator_body = function_body(all_operators, all_operator_main);
    auto const binary_expectations = std::to_array<std::pair<std::size_t, token_kind>>({
        { 2uz, token_kind::kw_or },
        { 3uz, token_kind::kw_and },
        { 4uz, token_kind::pipe },
        { 5uz, token_kind::caret },
        { 6uz, token_kind::amp },
        { 7uz, token_kind::equal_equal },
        { 8uz, token_kind::bang_equal },
        { 9uz, token_kind::less },
        { 10uz, token_kind::less_equal },
        { 11uz, token_kind::spaceship },
        { 12uz, token_kind::greater },
        { 13uz, token_kind::greater_equal },
        { 14uz, token_kind::less_less },
        { 15uz, token_kind::greater_greater },
        { 16uz, token_kind::plus },
        { 17uz, token_kind::minus },
        { 18uz, token_kind::star },
        { 19uz, token_kind::slash },
        { 20uz, token_kind::percent },
    });
    for(auto [index, kind] : binary_expectations) {
        auto const& declaration_node = declaration(all_operators, all_operator_body.statements[index]);
        auto const& binary = as<binary_expr_syntax>(all_operators.ast.node(declaration_node.initializer));
        test_parser::assert_true(binary.operator_kind == kind, "binary operator declaration should preserve operator kind");
        test_parser::assert_true(
            is<call_expr_syntax>(all_operators.ast.node(binary.right)),
            "binary operator should keep generic call as its right operand");
    }
    auto const& cast_declaration = declaration(all_operators, all_operator_body.statements[21]);
    test_parser::assert_true(
        is<cast_expr_syntax>(all_operators.ast.node(cast_declaration.initializer)),
        "as expression should parse as a cast expression");

    auto const assignment_expectations = std::to_array<std::pair<std::size_t, token_kind>>({
        { 22uz, token_kind::equal },
        { 23uz, token_kind::plus_equal },
        { 24uz, token_kind::minus_equal },
        { 25uz, token_kind::star_equal },
        { 26uz, token_kind::slash_equal },
        { 27uz, token_kind::percent_equal },
        { 28uz, token_kind::amp_equal },
        { 29uz, token_kind::pipe_equal },
        { 30uz, token_kind::caret_equal },
        { 31uz, token_kind::less_less_equal },
        { 32uz, token_kind::greater_greater_equal },
    });
    for(auto [index, kind] : assignment_expectations) {
        auto const& statement = expression_statement(all_operators, all_operator_body.statements[index]);
        auto const& assignment = as<assignment_expr_syntax>(all_operators.ast.node(statement.expression));
        test_parser::assert_true(assignment.operator_kind == kind, "assignment expression should preserve operator kind");
        test_parser::assert_true(
            is<call_expr_syntax>(all_operators.ast.node(assignment.right)),
            "assignment expression should keep generic call as its right operand");
    }

    auto const unary_expectations = std::to_array<std::tuple<std::size_t, token_kind, unary_position>>({
        { 33uz, token_kind::plus, unary_position::prefix },
        { 34uz, token_kind::minus, unary_position::prefix },
        { 35uz, token_kind::kw_not, unary_position::prefix },
        { 36uz, token_kind::tilde, unary_position::prefix },
        { 37uz, token_kind::amp, unary_position::prefix },
        { 38uz, token_kind::star, unary_position::prefix },
        { 39uz, token_kind::kw_ref, unary_position::prefix },
        { 40uz, token_kind::kw_move, unary_position::prefix },
        { 41uz, token_kind::kw_const, unary_position::prefix },
    });
    for(auto [index, kind, position] : unary_expectations) {
        auto const& declaration_node = declaration(all_operators, all_operator_body.statements[index]);
        auto const& unary = as<unary_expr_syntax>(all_operators.ast.node(declaration_node.initializer));
        test_parser::assert_true(
            unary.operator_kind == kind and unary.position == position,
            "unary declaration should preserve operator kind and position");
    }
    auto const update_expectations = std::to_array<std::tuple<std::size_t, token_kind, unary_position>>({
        { 42uz, token_kind::plus_plus, unary_position::prefix },
        { 43uz, token_kind::minus_minus, unary_position::prefix },
        { 44uz, token_kind::plus_plus, unary_position::postfix },
        { 45uz, token_kind::minus_minus, unary_position::postfix },
    });
    for(auto [index, kind, position] : update_expectations) {
        auto const& statement = expression_statement(all_operators, all_operator_body.statements[index]);
        auto const& unary = as<unary_expr_syntax>(all_operators.ast.node(statement.expression));
        test_parser::assert_true(
            unary.operator_kind == kind and unary.position == position,
            "update expression should preserve operator kind and position");
    }

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

    auto const& empty_array = declaration(shaped, shaped_body.statements[3]);
    auto const& empty_array_expr = shaped.ast.node(empty_array.initializer);
    test_parser::assert_true (
        is<array_literal_expr_syntax>(empty_array_expr)
            and as<array_literal_expr_syntax>(empty_array_expr).elements.empty(),
        "empty array literal should parse as an array literal with no operands");

    auto const& increment_stmt = expression_statement(shaped, shaped_body.statements[4]);
    auto const& increment = as<unary_expr_syntax>(shaped.ast.node(increment_stmt.expression));
    test_parser::assert_true (
        increment.operator_kind == token_kind::plus_plus and increment.position == unary_position::postfix,
        "postfix increment should remain an expression statement with unary ++");

    auto const& decrement_stmt = expression_statement(shaped, shaped_body.statements[5]);
    auto const& decrement = as<unary_expr_syntax>(shaped.ast.node(decrement_stmt.expression));
    test_parser::assert_true (
        decrement.operator_kind == token_kind::minus_minus and decrement.position == unary_position::postfix,
        "postfix decrement should remain an expression statement with unary --");

    auto const& if_stmt = as<if_statement_syntax>(shaped.ast.node(shaped_body.statements[6]));
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
    let rows: [[i32; 3]; 2] = [[1, 2, 3], [4, 5, 6]];
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

    auto const operator_source = sources.add_source (
        "api_operator.cp",
        R"(struct vec2 {
    x: i32;
    y: i32;
}

operator +(left: vec2 const&, right: vec2 const&) -> vec2
{
    return vec2{ .x = left.x + right.x, .y = left.y + right.y };
}

impl vec2 {
    operator [](self&, index: i32) -> i32&
    {
        return x;
    }

    operator ()(self const&, scale: i32 = 1) -> i32
    {
        return x * scale + y;
    }

    operator prefix ++(self&) -> this&
    {
        return ref self;
    }

    operator postfix ++(self&) -> this
    {
        return self;
    }

    operator prefix --(self&) -> this&
    {
        return ref self;
    }

    operator postfix --(self&) -> this
    {
        return self;
    }
})");
    auto operators = parse_source(sources, operator_source);
    test_parser::assert_true(operators.accepted, "operator declaration source should parse");
    test_parser::assert_true(operators.root->functions.size() == 1, "top-level operator should stay in function list");
    auto const& top_operator = operators.ast.node(operators.root->functions.front());
    test_parser::assert_true (
        top_operator.overload_operator == overload_operator_kind::plus,
        "top-level operator should record its overload kind");
    auto const& impl_operator = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions.front());
    test_parser::assert_true (
        impl_operator.overload_operator == overload_operator_kind::subscript,
        "impl operator [] should record subscript overload kind");
    auto const& call_operator = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions[1]);
    test_parser::assert_true (
        call_operator.overload_operator == overload_operator_kind::call,
        "impl operator () should record call overload kind");
    test_parser::assert_true (
        static_cast<bool>(call_operator.parameters[1].default_value),
        "operator parameters should record default expressions");
    auto const& prefix_increment = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions[2]);
    test_parser::assert_true (
        prefix_increment.overload_operator == overload_operator_kind::prefix_plus_plus,
        "impl operator prefix ++ should record prefix increment overload kind");
    auto const& postfix_increment = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions[3]);
    test_parser::assert_true (
        postfix_increment.overload_operator == overload_operator_kind::postfix_plus_plus,
        "impl operator postfix ++ should record postfix increment overload kind");
    auto const& prefix_decrement = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions[4]);
    test_parser::assert_true (
        prefix_decrement.overload_operator == overload_operator_kind::prefix_minus_minus,
        "impl operator prefix -- should record prefix decrement overload kind");
    auto const& postfix_decrement = operators.ast.node(operators.ast.node(operators.root->impls.front()).functions[5]);
    test_parser::assert_true (
        postfix_decrement.overload_operator == overload_operator_kind::postfix_minus_minus,
        "impl operator postfix -- should record postfix decrement overload kind");

    auto const bare_update_operator_source = sources.add_source (
        "api_bare_update_operator.cp",
        R"(operator ++(value: i32) -> i32
{
    return value;
}

operator --(value: i32) -> i32
{
    return value;
})");
    auto bare_update_operators = parse_source(sources, bare_update_operator_source);
    test_parser::assert_true(
        not bare_update_operators.accepted,
        "bare operator ++ and operator -- declarations should be rejected");
    test_parser::assert_true(
        not bare_update_operators.diagnostics.empty(),
        "bare update operator declarations should emit parser diagnostics");

    auto const default_parameter_source = sources.add_source (
        "api_default_parameter.cp",
        R"(sort<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{})
{
    sort(values);
    sort(values, greater<i32>{});
}

concept strict_weak_order<T> {
    operator ()(self const&, left: T const&, right: T const&) -> bool;
    operator prefix ++(self&) -> this&;
    operator postfix ++(self&) -> this;
    operator prefix --(self&) -> this&;
    operator postfix --(self&) -> this;
})");
    auto default_parameters = parse_source(sources, default_parameter_source);
    test_parser::assert_true(default_parameters.accepted, "default parameter and call operator source should parse");
    auto const& sort_function = default_parameters.ast.node(default_parameters.root->functions.front());
    test_parser::assert_true(static_cast<bool>(sort_function.parameters[1].default_value), "function parameter should record default expression");
    auto const& concept_item = default_parameters.ast.node(default_parameters.root->concepts.front());
    auto const& requirement = as<concept_function_requirement_syntax>(concept_item.items.front());
    test_parser::assert_true (
        requirement.overload_operator == overload_operator_kind::call,
        "concept function requirement should record operator ()");
    auto const& increment_requirement = as<concept_function_requirement_syntax>(concept_item.items[1]);
    test_parser::assert_true (
        increment_requirement.overload_operator == overload_operator_kind::prefix_plus_plus,
        "concept function requirement should record operator prefix ++");
    auto const& postfix_increment_requirement = as<concept_function_requirement_syntax>(concept_item.items[2]);
    test_parser::assert_true (
        postfix_increment_requirement.overload_operator == overload_operator_kind::postfix_plus_plus,
        "concept function requirement should record operator postfix ++");
    auto const& decrement_requirement = as<concept_function_requirement_syntax>(concept_item.items[3]);
    test_parser::assert_true (
        decrement_requirement.overload_operator == overload_operator_kind::prefix_minus_minus,
        "concept function requirement should record operator prefix --");
    auto const& postfix_decrement_requirement = as<concept_function_requirement_syntax>(concept_item.items[4]);
    test_parser::assert_true (
        postfix_decrement_requirement.overload_operator == overload_operator_kind::postfix_minus_minus,
        "concept function requirement should record operator postfix --");

    auto const ownership_source = sources.add_source (
        "api_ownership.cp",
        R"(struct box {
    value: i32;
}

impl box {
    get(self like&) -> i32 like&
    {
        return ref value;
    }

    take(self move&) -> i32
    {
        return value;
    }

    reset(self&) = delete;
}

main()
{
    let box_value = box{ 1 };
    let ref value = box_value.get();
    let moved = move box_value;
})");
    auto ownership = parse_source(sources, ownership_source);
    test_parser::assert_true(ownership.accepted, "ownership syntax source should parse");
    auto const& ownership_impl = ownership.ast.node(ownership.root->impls.front());
    auto const& get_function = ownership.ast.node(ownership_impl.functions[0]);
    test_parser::assert_true(get_function.parameters.front().self_is_like, "self like& should be recorded on the receiver");
    auto const& take_function = ownership.ast.node(ownership_impl.functions[1]);
    test_parser::assert_true(take_function.parameters.front().self_is_move, "self move& should be recorded on the receiver");
    auto const& reset_function = ownership.ast.node(ownership_impl.functions[2]);
    test_parser::assert_true(reset_function.deleted and not reset_function.has_body, "= delete should produce a bodyless deleted function");

    auto const memory_syntax_source = sources.add_source (
        "api_new_delete.cp",
        R"(main()
{
    let pointer = new i32{1};
    delete pointer;
    delete nullptr;
    let array = new [i32; 2]{1, 2};
    delete array;
})");
    auto memory_syntax = parse_source(sources, memory_syntax_source);
    test_parser::assert_true(memory_syntax.accepted, "new/delete syntax source should parse");
    auto const& memory_body = function_body(memory_syntax, first_function(memory_syntax));
    auto const& pointer_new = declaration(memory_syntax, memory_body.statements[0]);
    test_parser::assert_true(
        is<new_expr_syntax>(memory_syntax.ast.node(pointer_new.initializer)),
        "new expression should be recorded as a dedicated expression");
    auto const& pointer_delete = expression_statement(memory_syntax, memory_body.statements[1]);
    auto const& delete_expr = as<unary_expr_syntax>(memory_syntax.ast.node(pointer_delete.expression));
    test_parser::assert_true(
        delete_expr.operator_kind == token_kind::kw_delete and delete_expr.position == unary_position::prefix,
        "delete statement should parse as a prefix delete expression");
    auto const& array_new = declaration(memory_syntax, memory_body.statements[3]);
    auto const& array_new_expr = as<new_expr_syntax>(memory_syntax.ast.node(array_new.initializer));
    test_parser::assert_true(
        memory_syntax.ast.node(array_new_expr.type).is_array_type,
        "new [T; N] should preserve the array object type");

    return 0;
}
