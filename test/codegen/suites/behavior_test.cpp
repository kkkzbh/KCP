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

auto check_direct_array_initializer_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "direct_array_initializer.cp",
        R"(answer() -> i32
{
    let values = [1, 2, 3];
    return values[1];
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "direct array initializer source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("element_address"), "MIR dump should initialize array elements in place");
    test_parser::assert_true(not text.contains("aggregate_undef"), "direct array initializer should avoid aggregate temporary construction");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}

auto check_extern_c_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "extern_c.cp",
        R"(extern "C" abs(value: i32) -> i32;
extern "C" tick();
extern "C" add_default(value: i32, delta: i32 = 1) -> i32;
extern "C" choose(callback: f*(i32) -> i32) -> f*(i32) -> i32;

answer() -> i32
{
    tick();
    let callback = choose(abs);
    return callback(add_default(-42));
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "extern C source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func abs("), "MIR dump should contain extern C declaration");
    test_parser::assert_true(text.contains("func answer("), "MIR dump should contain caller");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("declare i32 @abs(i32)"), "LLVM IR should declare the C symbol");
    test_parser::assert_true(emitted.ir.contains("declare void @tick()"), "LLVM IR should declare unit-return extern C as void");
    test_parser::assert_true(emitted.ir.contains("declare i32 @add_default(i32, i32)"), "LLVM IR should keep default arguments out of the C ABI signature");
    test_parser::assert_true(emitted.ir.contains("declare ptr @choose(ptr)"), "LLVM IR should lower extern C function pointer types to pointers");
    test_parser::assert_true(emitted.ir.contains("call void @tick"), "LLVM IR should call unit-return extern C");
    test_parser::assert_true(emitted.ir.contains("call i32 @add_default"), "LLVM IR should call extern C with materialized default argument");
    test_parser::assert_true(emitted.ir.contains("call ptr @choose"), "LLVM IR should pass function pointers across extern C");
}

auto check_named_module_main_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "named_module_main.cp",
        R"(export module app;

export main() -> i32
{
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "named module main source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.app.main("), "MIR dump should preserve the named module function name");
    test_parser::assert_true(not text.contains("func main("), "named module main should not become the executable entry symbol");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define i32 @cp.app.main()"), "LLVM IR should define the module-qualified main");
    test_parser::assert_true(not emitted.ir.contains("define i32 @main("), "LLVM IR should not define i32 executable main for a named module");
    test_parser::assert_true(not emitted.ir.contains("define void @main("), "LLVM IR should not define void executable main for a named module");
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

auto check_short_circuit_control_flow() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "short_circuit.cp",
        R"(answer(flag: bool, pointer: i32*) -> bool
{
    return (flag and *pointer == 1) or flag;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "short-circuit source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("cond_branch"), "MIR dump should lower short-circuit operators with branches");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("br i1"), "LLVM IR should branch for short-circuit operators");
}

auto check_aggregate_literals() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "aggregates.cp",
        R"(answer() -> i32
{
    let data: [i32; 3] = [4, 5, 6];
    let pair: (i32, f64) = (1, 2.0);
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "aggregate source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("element_address"), "MIR dump should directly initialize array elements");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should directly initialize tuple fields");
    test_parser::assert_true(not text.contains("aggregate_undef"), "declaration aggregate literals should avoid aggregate temporaries");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("[3 x i32]"), "LLVM IR should contain array aggregate values");
    test_parser::assert_true(emitted.ir.contains("{ i32, double }"), "LLVM IR should contain tuple aggregate values");
}

auto check_array_index_operations() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "array_index.cp",
        R"(answer() -> i32
{
    let data = [4, 5, 6];
    data[1] = 8;
    return data[1] + [1, 2, 3][2];
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "array index source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("element_address"), "MIR dump should contain element_address");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("getelementptr"), "LLVM IR should compute array element addresses");
}

auto check_range_for_control_flow() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "range_for.cp",
        R"(sum(values: [i32; 4]) -> i32
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
    test_parser::assert_true(text.contains("element_address"), "MIR dump should address range elements for binding initialization");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("getelementptr"), "LLVM IR should address range elements");
}

auto check_labeled_loop_jumps() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "labeled_for.cp",
        R"(contains(rows: [[i32; 3]; 2], target: i32) -> bool
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

auto check_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "variant_match.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

answer() -> i32
{
    let value = optional<i32>::some(42);
    return match value {
        .some(item) => item,
        .none => 0,
    };
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "variant match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("extract_value"), "MIR dump should inspect the variant tag and payload");
    test_parser::assert_true(text.contains("match.arm.0"), "MIR dump should contain match arms");
    test_parser::assert_true(text.contains("match.unreachable"), "MIR dump should contain an explicit invalid-tag fallback");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("extractvalue"), "LLVM IR should extract variant payloads");
}

auto check_variant_match_wildcard_payload_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "variant_match_wildcard_payload.cp",
        R"(variant result {
    none;
    pair(i32, i32);
    flag(bool);
}

main() -> i32
{
    let value = result::pair(20, 22);
    return match value {
        .pair(left, right) => left + right,
        _ => 0,
    };
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "wildcard multi-payload variant match should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm.1"), "MIR dump should contain the wildcard match arm");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should bind multi-payload case fields");
    test_parser::assert_true(not text.contains("match.unreachable"), "wildcard match should not need an invalid-tag fallback block");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("extractvalue"), "LLVM IR should extract multi-payload variant fields");
}

auto check_variant_match_wildcard_before_specific_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "variant_match_wildcard_before_specific.cp",
        R"(variant result {
    none;
    pair(i32, i32);
}

main() -> i32
{
    let value = result::pair(20, 22);
    return match value {
        _ => 40,
        .pair(left, right) => left + right,
    } + 2;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "wildcard-before-specific match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm.0"), "MIR dump should contain the leading wildcard match arm");
    test_parser::assert_true(text.contains("match.arm.1"), "MIR dump should still lower the later case arm");
    test_parser::assert_true(not text.contains("match.unreachable"), "wildcard-first match should not need an invalid-tag fallback block");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}

auto check_unit_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "variant_match_unit.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

answer() -> i32
{
    let total = 0;
    let value = optional<i32>::some(42);
    match value {
        .some(item) => {
            total = item;
        },
        .none => {
            total = 0;
        },
    };
    return total;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "unit variant match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should contain unit match arms");
    test_parser::assert_true(not text.contains("match.result"), "unit match should not allocate a result slot");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("extractvalue"), "LLVM IR should still extract the matched variant payload");
}

auto check_contextual_and_unit_return_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "contextual_unit_match.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

fail() -> !
{
    return panic("missing");
}

pick(value: optional<i32>) -> i64
{
    let widened: i64 = match value {
        .some(item) => item,
        .none => fail(),
    };
    return widened;
}

touch(value: optional<i32>)
{
    return match value {
        .some(item) => {},
        .none => {},
    };
}

main() -> i32
{
    touch(optional<i32>::none);
    return pick(optional<i32>::some(42)) as i32;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "contextual and unit-return match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm."), "contextual match should lower to match arms");
    test_parser::assert_true(text.contains("cast"), "contextual match arm should materialize an integer conversion");
    test_parser::assert_true(text.contains("func touch("), "unit-return match function should be emitted");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("i64 @pick"), "LLVM IR should keep the contextual i64 match result");
    test_parser::assert_true(emitted.ir.contains("void @touch"), "LLVM IR should infer unit return for unit match");
}

auto check_all_never_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "all_never_variant_match.cp",
        R"(variant abort {
    stop;
    again;
}

fail(value: abort)
{
    return match value {
        .stop => panic("stop"),
        .again => unreachable(),
    };
}

main() -> i32
{
    if(false) {
        return fail(abort::stop);
    }
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "all-never variant match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should contain all-never match arms");
    test_parser::assert_true(not text.contains("match.result"), "all-never match should not allocate a result slot");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}

auto check_match_arm_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "match_arm_return_codegen.cp",
        R"(variant choice {
    left(i32);
    right(i32);
}

pick(value: choice)
{
    match value {
        .left(item) => {
            return item;
        },
        .right(item) => {
            return item + 1;
        },
    };
}

main() -> i32
{
    return pick(choice::right(41));
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "match arms with direct returns should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should contain match return arms");
    test_parser::assert_true(not text.contains("match.result"), "all-return match arms should not allocate a result slot");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("i32 @pick"), "LLVM IR should infer and emit an i32 pick function");
}

auto check_function_pointer_and_memory_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "function_pointer_memory.cp",
        R"(add(x: i32, y: i32) -> i32
{
    return x + y;
}

answer() -> i32
{
    let callback: f(i32, i32) -> i32 = add;
    let raw: f*(i32, i32) -> i32 = add;
    let pointer = alloc<i32>(2);
    construct_at(pointer, callback(20, 1));
    construct_at(pointer + 1, raw(20, 1));
    let result = *pointer + *(pointer + 1);
    destroy_at(pointer + 1);
    destroy_at(pointer);
    free(pointer);
    return result;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "function pointer memory source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("alloc_raw"), "MIR dump should contain raw allocation");
    test_parser::assert_true(text.contains("free_raw"), "MIR dump should contain raw free");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("declare ptr @cp_alloc"), "LLVM IR should declare runtime cp_alloc");
    test_parser::assert_true(emitted.ir.contains("declare void @cp_free"), "LLVM IR should declare runtime cp_free");
    test_parser::assert_true(not emitted.ir.contains("@malloc"), "LLVM IR should not lower allocation directly to malloc");
    test_parser::assert_true(emitted.ir.contains("getelementptr i32"), "LLVM IR should use typed pointer arithmetic");
}

auto check_new_delete_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "new_delete.cp",
        R"(struct guard {
    total: i32*;
    value: i32;
}

impl guard {
    ~guard()
    {
        *total += value;
    }
}

answer() -> i32
{
    let total = 0;
    let item = new guard{ &total, 3 };
    let values = new [guard; 2]{ guard{ &total, 5 }, guard{ &total, 7 } };
    delete item;
    delete values;
    return total;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "new/delete source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("alloc_raw"), "MIR dump should contain new allocation");
    test_parser::assert_true(text.contains("free_raw"), "MIR dump should contain delete free");
    test_parser::assert_true(text.contains("element_address"), "MIR dump should destroy array elements through element addresses");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("declare ptr @cp_alloc"), "LLVM IR should declare runtime cp_alloc for new");
    test_parser::assert_true(emitted.ir.contains("declare void @cp_free"), "LLVM IR should declare runtime cp_free for delete");
}

auto check_generic_function_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_function.cp",
        R"(id<T>(input: T) -> T
{
    return input;
}

choose<T>(left: T, right: T, flag: bool) -> T
{
    if(flag) {
        return left;
    }
    return right;
}

main() -> i32
{
    let first = id<i32>(20);
    let second = id(22);
    let ok = id<bool>(true);
    if(ok) {
        return choose(first, second, false);
    }
    return 1;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic function source should pass semantic analysis");
    test_parser::assert_true(checked.function_instances.size() == 3, "generic codegen should keep one instance per concrete signature");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func id.mono."), "MIR dump should contain monomorphized id functions");
    test_parser::assert_true(text.contains("func choose.mono."), "MIR dump should contain monomorphized choose function");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @id.mono."), "LLVM IR should define i32 id instance");
    test_parser::assert_true(emitted.ir.contains("define internal i8 @id.mono."), "LLVM IR should define bool id instance");
    test_parser::assert_true(emitted.ir.contains("call i32 @choose.mono."), "LLVM IR should call concrete choose instance");
}

auto check_parameter_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "parameter_pack.cp",
        R"(sum<T...>(values: T...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value;
    }
    return total;
}

type_count<T...>() -> i32
{
    let total = 0;
    template fo)" "r" R"( (type U : T...) {
        type current = U;
        total = total + 1;
    }
    return total;
}

main() -> i32
{
    return sum(10, 20, 9) + type_count<i32, bool, i32>();
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "parameter pack source should pass semantic analysis");
    test_parser::assert_true(checked.function_instances.size() == 2, "parameter pack codegen should instantiate pack functions");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("values.0"), "MIR dump should contain expanded pack parameter");
    test_parser::assert_true(text.contains("func sum.mono."), "MIR dump should contain monomorphized pack sum");
    test_parser::assert_true(text.contains("func type_count.mono."), "MIR dump should contain monomorphized type pack function");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @sum.mono."), "LLVM IR should define sum pack instance");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @type_count.mono."), "LLVM IR should define type pack instance");
}

auto check_empty_pack_unit_inferred_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "empty_pack_unit_return.cp",
        R"(touch<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
    }
}

main() -> i32
{
    touch();
    touch(1, 2);
    return 42;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "empty-pack unit inferred return source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func touch.mono."), "MIR dump should contain monomorphized unit pack function");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal void @touch.mono."), "LLVM IR should define void pack instances");
    test_parser::assert_true(emitted.ir.contains("call void @touch.mono."), "LLVM IR should call void pack instances");
}

auto check_forward_parameter_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "forward_parameter_pack_codegen.cp",
        R"(read(value: i32) -> i32
{
    return value;
}

sum_forward<T...>(values: T forward&...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + read(forward value);
    }
    return total;
}

sum_forward_explicit<T...>(values: T forward&...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + read(forward value);
    }
    return total;
}

main() -> i32
{
    let first = 10;
    const second = 20;
    let third = 1;
    return sum_forward(first, second, 12) + sum_forward_explicit<i32, i32>(move third, 0) - 1;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "forward parameter-pack source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("values.0"), "MIR dump should contain expanded forward pack parameters");
    test_parser::assert_true(text.contains("func sum_forward.mono."), "MIR dump should contain inferred forward-pack instance");
    test_parser::assert_true(text.contains("func sum_forward_explicit.mono."), "MIR dump should contain explicit forward-pack instance");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @sum_forward.mono."), "LLVM IR should define inferred forward-pack instance");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @sum_forward_explicit.mono."), "LLVM IR should define explicit forward-pack instance");
}

auto check_generic_lambda_variant_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_lambda_variant_pack_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

main() -> i32
{
    let callback = f<T...>(values: optional<T>...) -> i32 {
        let total = 0;
        template fo)" "r" R"( (let value : values...) {
            total = total + match value {
                .some(item) => item,
                .none => 0,
            };
        }
        return total;
    };
    let value = callback(optional<i32>::some(20), optional<i32>::some(22));
    return value;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic lambda variant-pack source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("values.0"), "MIR dump should contain expanded lambda pack parameters");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside generic lambda pack expansion");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}

auto check_default_initialization_and_operator_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "default_init_ops.cp",
        R"(struct inner {
    value: i32 = 4;
}

struct outer {
    items: [inner; 2];
    single: inner;
    plain: i32;
}

integer_ops(left: i32, right: i32) -> i32
{
    let shifted = (left << 2) | (right >> 1);
    let masked = shifted & 31;
    return (+masked) ^ (~right);
}

float_ops(value: f64) -> f64
{
    return -value;
}

main() -> i32
{
    let value: outer = outer{};
    return value.items[0].value + value.items[1].value + value.single.value + value.plain + integer_ops(3, 1) + (float_ops(1.0) as i32) - 23;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "default initialization and operator source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("element_address"), "MIR dump should initialize defaulted array elements");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should initialize defaulted struct fields");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("shl"), "LLVM IR should lower left shift");
    test_parser::assert_true(emitted.ir.contains("ashr") or emitted.ir.contains("lshr"), "LLVM IR should lower right shift");
    test_parser::assert_true(emitted.ir.contains("xor"), "LLVM IR should lower xor and bitwise not");
    test_parser::assert_true(emitted.ir.contains("fneg"), "LLVM IR should lower floating negation");
}

auto check_default_value_expression_and_str_field_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "default_value_expression_str_field.cp",
        R"(struct inner {
    value: i32 = 4;
}

struct outer {
    items: [inner; 2];
    single: inner;
    plain: i32;
}

make_outer() -> outer
{
    return outer{};
}

text_len() -> i32
{
    let text: str = "abc";
    let same = str{ .ptr = text.ptr, .len = text.len };
    return same.len as i32;
}

main() -> i32
{
    let value = make_outer();
    return value.items[0].value + value.items[1].value + value.single.value + value.plain + text_len();
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "default-value expression and str field source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("insert_value"), "MIR dump should build defaulted aggregate values in expression context");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should address str fields");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal { [2 x { i32 }], { i32 }, i32 } @make_outer()"), "LLVM IR should return the defaulted aggregate value");
    test_parser::assert_true(emitted.ir.contains("getelementptr inbounds nuw { ptr, i64 }"), "LLVM IR should address str field values");
}

auto check_variant_match_type_pack_inferred_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "variant_pack_return.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

first_i32<T...>()
{
    template fo)" "r" R"( (type U : T...) {
        template if(U == i32) {
            return 20;
        }
    }
    return 0;
}

first_some<T...>(values: optional<T>...)
{
    template fo)" "r" R"( (let value : values...) {
        let current = match value {
            .some(item) => item,
            .none => 0,
        };
        if(current != 0) {
            return current;
        }
    }
    return 0;
}

main() -> i32
{
    return first_i32<bool, i32>() + first_some(optional<i32>::none, optional<i32>::some(22));
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "variant match type-pack inferred return source should pass semantic analysis");
    test_parser::assert_true(checked.function_instances.size() == 2, "type and value pack functions should instantiate for codegen");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func first_i32.mono."), "MIR dump should contain monomorphized type-pack return function");
    test_parser::assert_true(text.contains("func first_some.mono."), "MIR dump should contain monomorphized variant pack function");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower variant match arms inside the pack expansion");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_i32.mono."), "LLVM IR should define type-pack return instance");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_some.mono."), "LLVM IR should define variant pack return instance");
}

auto check_type_pack_variant_match_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "type_pack_variant_match_return_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

first_i32<T...>()
{
    template fo)" "r" R"( (type U : T...) {
        template if(U == i32) {
            let value = optional<i32>::some(42);
            return match value {
                .some(item) => item,
                .none => 0,
            };
        }
    }
    return 0;
}

main() -> i32
{
    return first_i32<bool, i32>();
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "type-pack variant match return source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func first_i32.mono."), "MIR dump should contain the type-pack match return instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower variant match arms under the type-pack branch");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_i32.mono."), "LLVM IR should define the type-pack match return instance");
}

auto check_direct_match_return_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "direct_match_return_pack_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

first_present<T...>(values: optional<T>...)
{
    template fo)" "r" R"( (let value : values...) {
        return match value {
            .some(item) => item,
            .none => 0,
        };
    }
    return 0;
}

main() -> i32
{
    return first_present(optional<i32>::some(42));
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "direct match return inside pack expansion should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func first_present.mono."), "MIR dump should contain the direct match-return pack instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms under direct return");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_present.mono."), "LLVM IR should define direct match-return pack instance");
}

auto check_module_variant_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto units = std::vector<parse_result> {
        parse_source(
            sources,
            "base.cp",
            R"(export module base;

export variant optional<T> {
    none;
    some(T);
}

export first_some<T...>(values: optional<T>...)
{
    template fo)" "r" R"( (let value : values...) {
        let current = match value {
            .some(item) => item,
            .none => 0,
        };
        if(current != 0) {
            return current;
        }
    }
    return 0;
})"),
        parse_source(
            sources,
            "wrap.cp",
            R"(export module wrap;

export import base;)"),
        parse_source(
            sources,
            "main.cp",
            R"(import wrap;

main() -> i32
{
    return first_some(optional<i32>::none, optional<i32>::some(42));
})")
    };
    auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
    test_parser::assert_true(checked.accepted(), "module re-exported variant pack source should pass semantic analysis");

    auto ir = emit_ir(sources, std::span<parse_result const>{ units }, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.base.first_some.mono."), "MIR dump should contain the module-qualified pack instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms in the imported pack instance");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.base.first_some.mono."), "LLVM IR should define the module-qualified pack instance");
}

auto check_nested_parameter_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "nested_parameter_pack_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

inc(value: i32) -> i32
{
    return value + 1;
}

same_bool(value: bool) -> bool
{
    return value;
}

count_callbacks<T...>(callbacks: (f(T) -> T)...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let callback : callbacks...) {
        total = total + 1;
    }
    return total;
}

count_options<T...>(values: optional<T>...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        match value {
            .some(item) => {
                total = total + 1;
            },
            .none => {},
        };
    }
    return total;
}

main() -> i32
{
    return count_callbacks(inc, same_bool)
        + count_options(optional<i32>::some(1), optional<bool>::none)
        + 39;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "nested parameter-pack source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func count_callbacks.mono."), "MIR dump should contain function-type pack instance");
    test_parser::assert_true(text.contains("func count_options.mono."), "MIR dump should contain variant pack instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside a mixed variant pack");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @count_callbacks.mono."), "LLVM IR should define function-type pack instance");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @count_options.mono."), "LLVM IR should define variant pack instance");
}

auto check_length_prefixed_variant_pack_codegen() -> void
{
    auto sources = source_manager{};
    auto units = std::vector<parse_result> {
        parse_source(
            sources,
            "base.cp",
            R"(export module base;

export variant optional<T> {
    none;
    some(T);
}

export count_present<N: usize, T...>(values: [optional<T>; N]...)
{
    let total = 0;
    template fo)" "r" R"( (let bucket : values...) {
        let current = bucket[0];
        total = total + match current {
            .some(item) => 1,
            .none => 0,
        };
    }
    return total;
}

export first_immediate<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        return value;
    }
    return 0;
})"),
        parse_source(
            sources,
            "wrap.cp",
            R"(export module wrap;

export import base;)"),
        parse_source(
            sources,
            "main.cp",
            R"(import wrap;

main() -> i32
{
    return count_present(
        [optional<i32>::some(1), optional<i32>::none],
        [optional<bool>::some(true), optional<bool>::none]
    ) + first_immediate(40, 99);
})")
    };
    auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
    test_parser::assert_true(
        checked.accepted(),
        "module re-exported length-prefixed variant pack source should pass semantic analysis");

    auto ir = emit_ir(sources, std::span<parse_result const>{ units }, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.base.count_present.mono."), "MIR dump should contain the length-prefixed pack instance");
    test_parser::assert_true(text.contains("func cp.base.first_immediate.mono."), "MIR dump should contain the early-return pack instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside length-prefixed pack expansion");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.base.count_present.mono."), "LLVM IR should define the length-prefixed pack instance");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.base.first_immediate.mono."), "LLVM IR should define the early-return pack instance");
}

auto check_generic_variant_pack_payload_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_variant_pack_payload_codegen.cp",
        R"(variant packet<T> {
    empty;
    pair(T, T);
}

count_pairs<T...>(values: packet<T>...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + match value {
            .pair(left, right) => 1,
            _ => 0,
        };
    }
    return total;
}

main() -> i32
{
    return count_pairs(packet<i32>::pair(1, 2), packet<bool>::pair(true, false)) + 40;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic variant pack payload source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func count_pairs.mono."), "MIR dump should contain the generic variant pack instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside generic variant pack expansion");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should bind substituted multi-payload fields");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @count_pairs.mono."), "LLVM IR should define the generic variant pack instance");
}

auto check_empty_variant_pack_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "empty_variant_pack_match_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

count_present<T...>(values: optional<T>...)
{
    let total = 0;
    template for(let value : values...) {
        total = total + match value {
            .some(item) => 1,
            _ => 0,
        };
    }
    return total;
}

first_or<T...>(values: optional<T>...)
{
    template for(let value : values...) {
        return match value {
            .some(item) => item,
            .none => 0,
        };
    }
    return 0;
}

main() -> i32
{
    return count_present()
        + count_present(optional<i32>::some(1), optional<bool>::none)
        + first_or();
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "empty variant-pack match source should pass semantic analysis");
    test_parser::assert_true(
        checked.function_instances.size() == 3uz,
        "empty and non-empty variant-pack calls should create distinct function instances");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func count_present.mono."), "MIR dump should contain variant-pack count instances");
    test_parser::assert_true(text.contains("func first_or.mono."), "MIR dump should contain empty-pack fallback instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms for the non-empty variant-pack instance");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @count_present.mono."), "LLVM IR should define variant-pack count instances");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_or.mono."), "LLVM IR should define the empty-pack fallback instance");
}

auto check_non_struct_impl_inferred_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "non_struct_impl_inferred_return_codegen.cp",
        R"(variant token {
    empty;
    ready(i32);
}

impl token {
    score(self const&)
    {
        return match self {
            .ready(value) => value,
            .empty => 0,
        };
    }
}

impl i32 {
    double(self const&)
    {
        return self + self;
    }
}

impl [i32; 2] {
    first(self const&)
    {
        return self[0];
    }
}

main() -> i32
{
    let value = token::ready(20);
    let numbers = [1, 2];
    return value.score() + (10 as i32).double() + numbers.first() + match token::ready(1) {
        .ready(item) => item,
        .empty => 0,
    };
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "non-struct impl inferred-return source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.local.token.score."), "MIR dump should contain inferred variant method");
    test_parser::assert_true(text.contains("func cp.local.i32.double."), "MIR dump should contain inferred builtin method");
    test_parser::assert_true(text.contains("func cp.local.array.first."), "MIR dump should contain inferred array method");
    test_parser::assert_true(text.contains("aggregate_undef"), "MIR dump should build the temporary matched variant");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should bind matched variant payload fields");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms for non-struct impl returns");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.local.token.score."), "LLVM IR should define inferred variant method as i32");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.local.i32.double."), "LLVM IR should define inferred builtin method as i32");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.local.array.first."), "LLVM IR should define inferred array method as i32");
}

auto check_generic_impl_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_impl_variant_match_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

struct box<T> {
    value: T;
}

impl box<T> {
    pick(self const&, fallback: T)
    {
        return match optional<T>::some(value) {
            .some(item) => item,
            .none => fallback,
        };
    }
}

answer() -> i32
{
    let item = box<i32>{ 42 };
    return item.pick(0);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic impl variant-match source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.local.box.pick.mono."), "MIR dump should contain monomorphized generic impl method");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside the generic impl method");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.local.box.pick.mono."), "LLVM IR should define generic impl method as i32");
}

auto check_implicit_impl_const_argument_variant_match_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "implicit_impl_const_argument_variant_match_codegen.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

struct bucket<T, N: usize> {
    values: [T; N];
}

impl bucket<T, N> {
    pick(self const&, index: usize, fallback: T)
    {
        let current = optional<T>::some(values[index]);
        return match current {
            .some(item) => item,
            .none => fallback,
        };
    }
}

main() -> i32
{
    let values = bucket<i32, 2>{ [20, 22] };
    return values.pick(1, 0);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "implicit impl const-argument source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func cp.local.bucket.pick.mono."), "MIR dump should contain the implicit generic impl instance");
    test_parser::assert_true(text.contains("match.arm."), "MIR dump should lower match arms inside the implicit generic impl");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @cp.local.bucket.pick.mono."), "LLVM IR should define implicit generic impl method as i32");
}

auto check_generic_block_expression_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_block_expression_return_codegen.cp",
        R"(id<T>(input: T)
{
    return {
        return input;
    };
}

from_initializer<T>(input: T)
{
    let ignored: T = {
        return input;
    };
}

first_present<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        let ignored = {
            return value;
        };
    }
    panic("empty");
}

main() -> i32
{
    return id(10) + from_initializer(11) + first_present(21);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic block-expression return source should pass semantic analysis");
    auto i32_instance_count = std::ranges::count_if(
        checked.function_instances,
        [&](semantic_function_instance const& instance) {
            return instance.signature.valid()
                   and checked.signatures[instance.signature.value].returns == semantic_type_ids::i32;
        }
    );
    test_parser::assert_true(
        i32_instance_count == 3,
        "block-expression returns should infer i32 for each generic instance before IR emission");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func id.mono."), "MIR dump should contain the generic direct block-return instance");
    test_parser::assert_true(text.contains("func from_initializer.mono."), "MIR dump should contain the initializer block-return instance");
    test_parser::assert_true(text.contains("func first_present.mono."), "MIR dump should contain the pack block-return instance");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @id.mono."), "LLVM IR should define direct block-return instance as i32");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @from_initializer.mono."), "LLVM IR should define initializer block-return instance as i32");
    test_parser::assert_true(emitted.ir.contains("define internal i32 @first_present.mono."), "LLVM IR should define pack block-return instance as i32");
}

auto check_template_if_block_expression_return_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "template_if_block_expression_return_codegen.cp",
        R"(from_block<T>(value: T)
{
    let ignored: i32 = {
        template if(T == i32) {
            return value;
        } else {
            return 7;
        }
        0
    };
    return ignored;
}

main() -> i32
{
    return from_block(42) + from_block(true) - 7;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "template-if block-expression return source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func from_block.mono."), "MIR dump should contain template-if block-return instances");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @from_block.mono."), "LLVM IR should define template-if block-return instances as i32");
}

auto check_concept_default_extension_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "concept_default_extension_codegen.cp",
        R"(concept measured {
    measure(self const&) -> i32
    {
        return 1;
    }
}

variant token {
    ready;
}

impl measured for i32 {
}

impl measured for [i32; 2] {
}

impl measured for token {
}

main() -> i32
{
    let values = [1, 2];
    let value = token::ready;
    return (10 as i32).measure() + values.measure() + value.measure() + 39;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "concept default extension source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("call"), "MIR dump should call materialized concept default extension functions");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
}

auto check_destructure_template_ref_and_empty_for_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "destructure_template_ref_empty_for.cp",
        R"(sum_ref<T...>(values: T const&...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (const ref value : values...) {
        total = total + value;
    }
    return total;
}

main() -> i32
{
    let pair = (20, 1);
    const ref (left, right) = pair;
    let total = left + right;
    let empty: [i32; 0] = [];
    for(let value : empty) {
        total = total + value;
    }
    return total + sum_ref(left, right);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "destructuring, template ref pack, and empty for source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("values.0"), "MIR dump should contain ref pack expansion bindings");
    test_parser::assert_true(text.contains("field_address"), "MIR dump should address destructured tuple fields");
    test_parser::assert_true(text.contains("for.end"), "MIR dump should lower the empty range-for to an end block");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal i32 @sum_ref.mono."), "LLVM IR should define ref-pack instance");
}

auto check_generic_function_tuple_storage_substitution_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_function_tuple_storage_substitution.cp",
        R"(inc(value: i32) -> i32
{
    return value + 1;
}

identity_fn<T>(callback: f(T) -> T) -> f(T) -> T
{
    return callback;
}

duplicate<T>(value: T) -> (T, T)
{
    return (value, value);
}

make_storage<T, N: usize>() -> storage [T; N]
{
    return storage [T; N]{};
}

main() -> i32
{
    let callback = identity_fn(inc);
    let pair = duplicate(10);
    let slots = make_storage<i32, 2>();
    return callback(pair.0) + pair.1 + 21;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic function, tuple, and storage substitution source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("func identity_fn.mono."), "MIR dump should contain function-type generic instance");
    test_parser::assert_true(text.contains("func duplicate.mono."), "MIR dump should contain tuple-return generic instance");
    test_parser::assert_true(text.contains("func make_storage.mono."), "MIR dump should contain storage-return generic instance");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("define internal ptr @identity_fn.mono."), "LLVM IR should define function-type instance");
    test_parser::assert_true(emitted.ir.contains("define internal { i32, i32 } @duplicate.mono."), "LLVM IR should define tuple instance");
}

auto check_generic_struct_field_substitution_codegen() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "generic_struct_field_substitution.cp",
        R"(inc(value: i32) -> i32
{
    return value + 1;
}

struct holder<T> {
    pair: (T, T);
    callback: f(T) -> T;
    slots: storage [T; 2];
}

main() -> i32
{
    let item = holder<i32>{ (10, 11), inc, storage [i32; 2]{} };
    let callback = item.callback;
    return item.pair.0 + callback(item.pair.1) + 20;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "generic struct field substitution source should pass semantic analysis");

    auto ir = emit_ir(sources, parsed, checked);
    test_parser::assert_true(ir.accepted, ir.error.empty() ? "IR emission should pass" : ir.error);
    auto text = dump_ir(ir.module);
    test_parser::assert_true(text.contains("field_address"), "MIR dump should address substituted generic fields");
    test_parser::assert_true(text.contains("call"), "MIR dump should call through substituted function field");

    auto emitted = emit_llvm_ir(ir.module);
    test_parser::assert_true(emitted.verified, emitted.error.empty() ? "LLVM module should verify" : emitted.error);
    test_parser::assert_true(emitted.ir.contains("call i32"), "LLVM IR should call through the substituted function field");
}

auto check_rejects_unterminated_ir_block() -> void
{
    auto module = ir_module{};
    module.functions.emplace_back("bad", ir_linkage::external, semantic_type_ids::i32, symbol_id{0});
    module.functions.back().blocks.emplace_back("entry");

    auto emitted = emit_llvm_ir(module);
    test_parser::assert_true(not emitted.verified, "LLVM lowering should reject unterminated MIR blocks");
    test_parser::assert_true(emitted.error.contains("unterminated MIR block"), "unterminated MIR block should report a clear error");
}
} // namespace

auto main() -> int
{
    check_return_literal();
    check_locals_assignment_and_call();
    check_direct_array_initializer_codegen();
    check_extern_c_codegen();
    check_named_module_main_codegen();
    check_if_control_flow();
    check_short_circuit_control_flow();
    check_aggregate_literals();
    check_array_index_operations();
    check_range_for_control_flow();
    check_labeled_loop_jumps();
    check_variant_match_codegen();
    check_variant_match_wildcard_payload_codegen();
    check_variant_match_wildcard_before_specific_codegen();
    check_unit_variant_match_codegen();
    check_contextual_and_unit_return_variant_match_codegen();
    check_all_never_variant_match_codegen();
    check_match_arm_return_codegen();
    check_function_pointer_and_memory_codegen();
    check_new_delete_codegen();
    check_generic_function_codegen();
    check_parameter_pack_codegen();
    check_empty_pack_unit_inferred_return_codegen();
    check_forward_parameter_pack_codegen();
    check_generic_lambda_variant_pack_codegen();
    check_default_initialization_and_operator_codegen();
    check_default_value_expression_and_str_field_codegen();
    check_variant_match_type_pack_inferred_return_codegen();
    check_type_pack_variant_match_return_codegen();
    check_direct_match_return_pack_codegen();
    check_module_variant_pack_codegen();
    check_nested_parameter_pack_codegen();
    check_length_prefixed_variant_pack_codegen();
    check_generic_variant_pack_payload_codegen();
    check_empty_variant_pack_match_codegen();
    check_non_struct_impl_inferred_return_codegen();
    check_generic_impl_variant_match_codegen();
    check_implicit_impl_const_argument_variant_match_codegen();
    check_generic_block_expression_return_codegen();
    check_template_if_block_expression_return_codegen();
    check_concept_default_extension_codegen();
    check_destructure_template_ref_and_empty_for_codegen();
    check_generic_function_tuple_storage_substitution_codegen();
    check_generic_struct_field_substitution_codegen();
    check_rejects_unterminated_ir_block();
    return 0;
}
