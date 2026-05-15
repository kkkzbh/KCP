import std;
import preprocessor;
import lexer;
import parser;
import semantic;
import codegen.ir;
import codegen.llvm;

#include "assert.hpp"

namespace {
auto parse_source(source_manager& sources, std::string_view name, std::string text) -> parse_result
{
    auto file = sources.add_source(std::string{name}, std::move(text));
    auto preprocessed = preprocess(sources, file);
    test_parser::assert_true(preprocessed.diagnostics.empty(), std::format("{} should preprocess before codegen", name));
    auto lexical = lex(preprocessed);
    test_parser::assert_true(lexical.diagnostics.empty(), std::format("{} should lex before codegen", name));
    auto parsed = parse_translation_unit(std::move(lexical.tokens));
    test_parser::assert_true(parsed.accepted, std::format("{} should parse before codegen", name));
    return parsed;
}

auto analyze_single(source_manager const& sources, parse_result const& parsed) -> semantic_result
{
    return analyze_semantics(sources, std::span<parse_result const>{ &parsed, 1uz });
}

auto check_return_literal() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "return_literal.cp",
        R"(answer() -> i32
{
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "return literal source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("literal 42"), "MIR dump should preserve the integer literal");
    test_parser::assert_true(text.contains("return"), "MIR dump should contain a return instruction");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("ret i32 42"), "LLVM IR should return the real literal");
}

auto check_locals_assignment_and_call() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "locals_call.cp",
        R"(add(x: i32, y: i32) -> i32
{
    return x + y;
}

answer() -> i32
{
    let value = add(40, 1);
    value = value + 1;
    return value;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "locals and call source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("call i32 @add"), "LLVM IR should call add");
    test_parser::assert_true(emitted.ir.contains("ret i32"), "LLVM IR should return i32");
}

auto check_if_control_flow() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "if_flow.cp",
        R"(answer() -> i32
{
    if(true) {
        return 42;
    }
    return 0;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "if source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("cond_branch"), "MIR dump should contain a conditional branch");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("br i1"), "LLVM IR should contain a conditional branch");
}

auto check_aggregate_literals() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "aggregates.cp",
        R"(answer() -> i32
{
    let seq: sequence<i32,3> = {1, 2, 3};
    let data: array<i32,3> = [4, 5, 6];
    let pair: tuple<i32,f64> = (1, 2.0);
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "aggregate source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("aggregate_undef"), "MIR dump should contain aggregate_undef");
    test_parser::assert_true(text.contains("insert_value"), "MIR dump should contain insert_value");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("[3 x i32]"), "LLVM IR should contain array aggregate values");
    test_parser::assert_true(emitted.ir.contains("{ i32, double }"), "LLVM IR should contain tuple aggregate values");
}

auto check_range_for_control_flow() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "range_for.cp",
        R"(sum(values: array<i32,4>) -> i32
{
    let total: i32 = 0;
    for(let value : values) {
        if(value < 0) {
            continue;
        }
        total = total + value;
    }
    return total;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "range-for source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("for.iter.0"), "MIR dump should contain range-for iteration blocks");
    test_parser::assert_true(text.contains("extract_value"), "MIR dump should contain range element extraction");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("extractvalue"), "LLVM IR should extract range elements");
}

auto check_labeled_loop_jumps() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "labeled_for.cp",
        R"(contains(rows: array<array<i32,3>,2>, target: i32) -> bool
{
    let found: bool = false;
    for: outer(let row : rows) {
        for(const value : row) {
            if(value < 0) {
                continue outer;
            }
            if(value == target) {
                found = true;
                break outer;
            }
        }
    }
    return found;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "labeled loop source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("for.iter.1"), "MIR dump should contain expanded outer iterations");
    test_parser::assert_true(text.contains("branch ^1"), "MIR dump should contain labeled break to outer end");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}
} // namespace

auto main() -> int
{
    check_return_literal();
    check_locals_assignment_and_call();
    check_if_control_flow();
    check_aggregate_literals();
    check_range_for_control_flow();
    check_labeled_loop_jumps();
    return 0;
}
