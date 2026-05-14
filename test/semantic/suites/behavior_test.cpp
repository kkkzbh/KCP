import std;
import parser;
import semantic;
import codegen.llvm;

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
    auto parsed = parse_translation_unit(sources, file);
    test_parser::assert_true(parsed.accepted, std::format("{} should parse before semantic analysis", name));
    return parsed;
}

auto parse_rejected_source(source_manager& sources, std::string_view name, std::string text) -> parse_result
{
    auto file = sources.add_source(std::string{name}, std::move(text));
    auto parsed = parse_translation_unit(sources, file);
    test_parser::assert_true(parsed.accepted, std::format("{} should parse before semantic rejection", name));
    return parsed;
}

auto has_diagnostic(semantic_result const& result, semantic_diagnostic_code code) -> bool
{
    return std::ranges::contains(result.diagnostics, code, &semantic_diagnostic::code);
}

auto analyze_one(std::string_view name, std::string text) -> semantic_result
{
    auto sources = source_manager{};
    auto parsed = parse_rejected_source(sources, name, std::move(text));
    return analyze_semantics(sources, parsed);
}

auto expect_diagnostic(std::string_view name, std::string text, semantic_diagnostic_code code) -> void
{
    auto result = analyze_one(name, std::move(text));
    test_parser::assert_true(
        has_diagnostic(result, code),
        std::format("{} should report {}", name, diagnostic_name(code)));
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
        auto const& function = parsed.ast.function(id);
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
    auto math = parse_source(sources, "math.cp", read_design_example("math.cp"));
    auto types = parse_source(sources, "types_demo.cp", read_design_example("types_demo.cp"));
    auto flow = parse_source(sources, "flow_demo.cp", read_design_example("flow_demo.cp"));
    auto main = parse_source(sources, "main.cp", read_design_example("main.cp"));

    auto checked = analyze_semantics(sources, math, types, flow, main);
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
    let seq = {1, 2, 3};
    let value: i64 = 1;
    return value;
})");
    auto checked = analyze_semantics(sources, parsed);
    test_parser::assert_true(checked.accepted(), "side table source should pass semantic analysis");

    auto const& unit = *parsed.root;
    auto const& function = parsed.ast.function(unit.functions.front());
    auto const& body = as<block_statement_syntax>(parsed.ast.statement(function.body));
    auto const& data = as<declaration_statement_syntax>(parsed.ast.statement(body.statements.front()));
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
        auto checked = analyze_semantics(sources, parsed);
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
        auto checked = analyze_semantics(sources, parsed);
        test_parser::assert_true(checked.accepted(), "ordered inferred return call should pass");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "main") == semantic_type_ids::i32,
            "caller should infer callee return type");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(sources, "unit_inferred_return.cp", "f() { return; }");
        auto checked = analyze_semantics(sources, parsed);
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
        auto checked = analyze_semantics(sources, parsed);
        test_parser::assert_true(checked.accepted(), "same-family returns should infer one return type");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "f") == semantic_type_ids::i32,
            "same integer returns should infer i32");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(sources, "explicit_recursive_return.cp", "f() -> i32 { return f(); }");
        auto checked = analyze_semantics(sources, parsed);
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

auto check_negative_cases() -> void
{
    using enum semantic_diagnostic_code;
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

auto check_llvm_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "llvm.cp",
        R"(answer() -> i32
{
    return 42;
})");
    auto checked = analyze_semantics(sources, parsed);
    test_parser::assert_true(checked.accepted(), "LLVM smoke source should pass semantic analysis");
    auto emitted = emit_llvm_ir(sources, parsed, checked);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("@answer"), "LLVM IR should contain the function symbol");
}
} // namespace

auto main() -> int
{
    check_design_examples();
    check_side_tables();
    check_fixed_type_ids();
    check_inferred_return_types();
    check_contiguous_unit_batch();
    check_negative_cases();
    check_llvm_codegen();
    return 0;
}
