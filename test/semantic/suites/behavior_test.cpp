import std;
import preprocessor;
import lexer;
import parser;
import semantic;

#include "assert.hpp"

namespace {
auto read_design_example(std::string_view name) -> std::string
{
    auto path = std::filesystem::path{ TEST_SEMANTIC_DESIGN_DIR } / name;
    auto input = std::ifstream{ path };
    test_parser::assert_true(input.good(), std::format("{} should be readable", path.string()));
    auto buffer = std::ostringstream{};
    buffer << input.rdbuf();
    return buffer.str();
}

auto parse_source(source_manager& sources, std::string_view name, std::string text) -> parse_result
{
    auto file = sources.add_source(std::string{name}, std::move(text));
    auto preprocessed = preprocess(sources, file);
    test_parser::assert_true(preprocessed.diagnostics.empty(),
        std::format("{} should preprocess before semantic analysis", name));
    auto lexical = lex(preprocessed);
    test_parser::assert_true(lexical.diagnostics.empty(), std::format("{} should lex before semantic analysis", name));
    auto parsed = parse_translation_unit(std::move(lexical.tokens));
    test_parser::assert_true(parsed.accepted, std::format("{} should parse before semantic analysis", name));
    return parsed;
}

auto parse_rejected_source(source_manager& sources, std::string_view name, std::string text) -> parse_result
{
    auto file = sources.add_source(std::string{name}, std::move(text));
    auto preprocessed = preprocess(sources, file);
    test_parser::assert_true(preprocessed.diagnostics.empty(),
        std::format("{} should preprocess before semantic rejection", name));
    auto lexical = lex(preprocessed);
    test_parser::assert_true(lexical.diagnostics.empty(), std::format("{} should lex before semantic rejection", name));
    auto parsed = parse_translation_unit(std::move(lexical.tokens));
    test_parser::assert_true(parsed.accepted, std::format("{} should parse before semantic rejection", name));
    return parsed;
}

auto has_diagnostic(semantic_result const& result, diagnostic_kind kind) -> bool
{
    return std::ranges::contains(result.diagnostics, kind, &diagnostic::kind);
}

auto analyze_single(source_manager const& sources, parse_result const& parsed) -> semantic_result
{
    return analyze_semantics(sources, std::span<parse_result const>{ &parsed, 1uz });
}

auto analyze_one(std::string_view name, std::string text) -> semantic_result
{
    auto sources = source_manager{};
    auto parsed = parse_rejected_source(sources, name, std::move(text));
    return analyze_single(sources, parsed);
}

auto expect_diagnostic(std::string_view name, std::string text, diagnostic_kind kind) -> void
{
    auto result = analyze_one(name, std::move(text));
    test_parser::assert_true(
        has_diagnostic(result, kind),
        std::format("{} should report {}", name, spec(kind).code));
}

auto function_return_type(
    source_manager const& sources,
    parse_result const& parsed,
    semantic_result const& checked,
    std::string_view name
) -> semantic_type_id
{
    auto source = ast_source_view{ sources };
    auto const& unit = *parsed.root;
    for(auto id : unit.functions) {
        auto const& function = parsed.ast.node(id);
        if(source.identifier(function.name) == name) {
            auto signature = checked.signature_of(id);
            test_parser::assert_true(signature.valid(), std::format("{} should have a signature", name));
            return checked.signatures[signature.value].returns;
        }
    }
    test_parser::assert_true(false, std::format("{} should exist", name));
    return {};
}

auto check_design_examples() -> void
{
    auto sources = source_manager{};
    auto units = std::array {
        parse_source(sources, "math.cp", read_design_example("math.cp")),
        parse_source(sources, "types_demo.cp", read_design_example("types_demo.cp")),
        parse_source(sources, "flow_demo.cp", read_design_example("flow_demo.cp")),
        parse_source(sources, "main.cp", read_design_example("main.cp")),
    };

    auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
    test_parser::assert_true(checked.accepted(), "design examples should pass semantic analysis together");
}

auto check_side_tables() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "side_tables.cp",
        R"(main()
{
    let data = [1, 2, 3];
    let value: i64 = 1;
    return value;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "side table source should pass semantic analysis");

    auto const& unit = *parsed.root;
    auto const& function = parsed.ast.node(unit.functions.front());
    auto const& body = as<block_statement_syntax>(parsed.ast.node(function.body));
    auto const& data = as<declaration_statement_syntax>(parsed.ast.node(body.statements.front()));
    auto data_type = checked.type_of(data.initializer);
    test_parser::assert_true(data_type.valid(), "array literal should have a semantic type");
    test_parser::assert_true(
        std::holds_alternative<array_type>(checked.types.get(data_type)),
        "[] without context should infer array<T,N>");

    auto signature = checked.signature_of(unit.functions.front());
    test_parser::assert_true(signature.valid(), "function should have a semantic signature");
    test_parser::assert_true(
        checked.signatures[signature.value].returns.valid(),
        "inferred function return should be recorded");
}

auto check_fixed_type_ids() -> void
{
    static_assert(semantic_type_ids::bool_ == semantic_type_ids::builtin(builtin_type_kind::bool_));

    test_parser::assert_true(is_error(semantic_type_ids::error), "fixed error id should be recognized");
    test_parser::assert_true(is_inferred(semantic_type_ids::inferred), "fixed inferred id should be recognized");
    test_parser::assert_true(not is_error(semantic_type_ids::i32), "builtin id should not be error");
    test_parser::assert_true(
        semantic_type_ids::bool_.value == semantic_type_ids::builtin_offset,
        "builtin ids should start at builtin_offset");

    auto builtin = as_builtin(semantic_type_ids::i32);
    test_parser::assert_true(builtin == builtin_type_kind::i32, "fixed i32 id should decode to builtin i32");

    auto types = type_arena{};
    test_parser::assert_true(
        types.intern(unit_type{}) == semantic_type_ids::unit,
        "unit intern should return its fixed id");
    test_parser::assert_true(
        types.intern(error_type{}) == semantic_type_ids::error,
        "error intern should return its fixed id");
    test_parser::assert_true(
        types.intern(inferred_type{}) == semantic_type_ids::inferred,
        "inferred intern should return its fixed id");
    test_parser::assert_true(
        types.intern(builtin_type{ builtin_type_kind::i32 }) == semantic_type_ids::i32,
        "builtin intern should return its fixed id");

    auto pointer = types.intern(pointer_type{ semantic_type_ids::i32 });
    test_parser::assert_true(
        pointer.value >= semantic_type_ids::fixed_count,
        "dynamic type ids should start after fixed type ids");

    auto const& kind = types.get(pointer);
    auto const* lowered = std::get_if<pointer_type>(&kind);
    test_parser::assert_true(lowered != nullptr, "dynamic pointer type should round-trip through type_arena");
    test_parser::assert_true(
        lowered->pointee == semantic_type_ids::i32,
        "dynamic pointer type should keep its fixed pointee id");
}

auto check_inferred_return_types() -> void
{
    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "forward_inferred_return.cp",
            R"(main() -> i32
{
    return f();
}

f()
{
    return 1;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "forward inferred return call should pass");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "f") == semantic_type_ids::i32,
            "forward callee should infer i32");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "ordered_inferred_return.cp",
            R"(f()
{
    return 1;
}

main()
{
    return f();
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "ordered inferred return call should pass");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "main") == semantic_type_ids::i32,
            "caller should infer callee return type");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(sources, "unit_inferred_return.cp", "f() { return; }");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "empty return should infer unit");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "f") == semantic_type_ids::unit,
            "empty return should infer unit");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "multi_inferred_return.cp",
            R"(f()
{
    if(true) {
        return 1;
    }
    return 2;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "same-family returns should infer one return type");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "f") == semantic_type_ids::i32,
            "same integer returns should infer i32");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(sources, "explicit_recursive_return.cp", "f() -> i32 { return f(); }");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "explicit recursive return type should not need inference");
    }
}

auto check_contiguous_unit_batch() -> void
{
    auto sources = source_manager{};
    auto units = std::array {
        parse_source(sources, "first.cp", "first() { return; }"),
        parse_source(sources, "second.cp", "second() { return; }"),
    };

    auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
    test_parser::assert_true(checked.accepted(), "contiguous parse_result batch should pass semantic analysis");
}

auto check_anonymous_modules() -> void
{
    {
        auto sources = source_manager{};
        auto units = std::array {
            parse_source(sources, "first.cp", "helper() { return 1; }"),
            parse_source(sources, "second.cp", "helper() { return true; }"),
        };

        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(checked.accepted(), "anonymous units should keep independent local functions");
    }

    {
        auto sources = source_manager{};
        auto units = std::array {
            parse_source(sources, "helper.cp", "helper() { return 1; }"),
            parse_source(sources, "main.cp", "main() { return helper(); }"),
        };

        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::unknown_name),
            "anonymous units should not see each other's local functions");
    }

    expect_diagnostic(
        "anonymous_export.cp",
        "export helper() { return; }",
        diagnostic_kind::export_requires_module);
}

auto check_reference_pointer_types() -> void
{
    {
        auto result = analyze_one(
            "const_double_pointer_reference.cp",
            R"(main() -> i32
{
    const first: i32 = 7;
    const second: i32 = 11;
    let p: i32 const* = &first;
    let q: i32 const* = &second;
    let pp: i32 const** = &p;
    let qq: i32 const** = &q;
    let rr: i32 const**& = pp;
    rr = qq;
    return **pp;
})"
        );
        test_parser::assert_true(result.accepted(), "i32 const**& alias source should pass semantic analysis");
    }

    {
        auto result = analyze_one(
            "i64_triple_pointer_write.cp",
            R"(main() -> i32
{
    let value: i64 = 9;
    let p: i64* = &value;
    let pp: i64** = &p;
    let ppp: i64*** = &pp;
    ***ppp = 33;
    ***ppp += 4;
    return i32(value);
})"
        );
        test_parser::assert_true(result.accepted(), "i64*** read/write source should pass semantic analysis");
    }

    {
        auto result = analyze_one(
            "i64_pointer_reference_alias.cp",
            R"(main() -> i32
{
    let first: i64 = 5;
    let second: i64 = 11;
    let pointer: i64* = &first;
    let alias: i64*& = pointer;
    alias = &second;
    *alias = 20;
    *pointer += 2;
    return i32(second);
})"
        );
        test_parser::assert_true(result.accepted(), "i64*& alias source should pass semantic analysis");
    }
}

auto check_negative_cases() -> void
{
    using enum diagnostic_kind;
    expect_diagnostic("mixed_array.cp", "main() { let value = [1, 2.0]; }", heterogeneous_aggregate);
    expect_diagnostic("empty_array.cp", "main() { let value = []; }", empty_aggregate_without_context);
    expect_diagnostic("short_array.cp", "main() { let value: array<i32,2> = [1]; }", aggregate_length_mismatch);
    expect_diagnostic("bad_element.cp", "main() { let value: array<i32,2> = [1, true]; }", type_mismatch);
    expect_diagnostic("unknown_name.cp", "main() { let value = missing; }", unknown_name);
    expect_diagnostic("duplicate.cp", "main() { let value = 1; let value = 2; }", duplicate_symbol);
    expect_diagnostic("const_assign.cp", "main() { const value = 1; value = 2; }", assign_to_const);
    expect_diagnostic (
        "const_pointer_assign.cp",
        "main() { const value = 1; let pointer: i32 const* = &value; *pointer = 2; }",
        assign_to_const
    );
    expect_diagnostic (
        "const_double_pointer_reference_assign.cp",
        "main() { const value = 1; let p: i32 const* = &value; let pp: i32 const** = &p; let rr: i32 const**& = pp; **rr = 2; }",
        assign_to_const
    );
    expect_diagnostic (
        "const_pointer_reference_compound_assign.cp",
        "main() { const value: i64 = 1; let pointer: i64 const* = &value; let alias: i64 const*& = pointer; *alias += 2; }",
        assign_to_const
    );
    expect_diagnostic (
        "pointer_reference_compound_assign.cp",
        "main() { let first: i64 = 1; let second: i64 = 2; let pointer: i64* = &first; let other: i64* = &second; let alias: i64*& = pointer; alias += other; }",
        invalid_operator
    );
    expect_diagnostic("float_remainder.cp", "main() { let value = 5.0 % 2.0; }", invalid_operator);
    expect_diagnostic("float_remainder_assign.cp", "main() { let value = 5.0; value %= 2.0; }", invalid_operator);
    expect_diagnostic("bad_condition.cp", "main() { if(1) { return; } }", condition_not_bool);
    expect_diagnostic("bad_return.cp", "main() -> i32 { return true; }", return_type_mismatch);
    expect_diagnostic (
        "mixed_inferred_return.cp",
        "f() { if(true) { return 1; } return true; }",
        cannot_infer_return_type
    );
    expect_diagnostic("recursive_inferred_return.cp", "f() { return f(); }", cannot_infer_return_type);
    expect_diagnostic (
        "mutual_recursive_inferred_return.cp",
        "f() { return g(); } g() { return f(); }",
        cannot_infer_return_type
    );
    expect_diagnostic("bad_break.cp", "main() { break; }", invalid_break);
    expect_diagnostic("bad_label.cp", "main() { for(let x : [1]) { continue outer; } }", unknown_label);
    expect_diagnostic("bad_builtin_arg.cp", "main() { let value: i32<u8> = 1; }", invalid_type_argument);
}

} // namespace

auto main() -> int
{
    check_design_examples();
    check_side_tables();
    check_fixed_type_ids();
    check_inferred_return_types();
    check_contiguous_unit_batch();
    check_anonymous_modules();
    check_reference_pointer_types();
    check_negative_cases();
    return 0;
}
