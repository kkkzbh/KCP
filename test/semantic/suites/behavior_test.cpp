import std;
import preprocessor;
import lexer;
import parser;
import semantic;

#include "assert.hpp"

namespace {
auto read_text_file(std::filesystem::path const& path) -> std::string
{
    auto input = std::ifstream{ path };
    test_parser::assert_true(input.good(), std::format("{} should be readable", path.string()));
    auto buffer = std::ostringstream{};
    buffer << input.rdbuf();
    return buffer.str();
}

auto read_fixture_example(std::string_view name) -> std::string
{
    return read_text_file(std::filesystem::path{ TEST_SEMANTIC_FIXTURE_EXAMPLES_DIR } / name);
}

auto read_std_module(std::string_view name) -> std::string
{
    return read_text_file(std::filesystem::path{ TEST_SEMANTIC_STD_DIR } / name);
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

auto constexpr std_io_modules = {
    std::string_view{ "option.cp" },
    std::string_view{ "expected.cp" },
    std::string_view{ "iter.cp" },
    std::string_view{ "detail/runtime.cp" },
    std::string_view{ "span.cp" },
    std::string_view{ "buffer.cp" },
    std::string_view{ "vector.cp" },
    std::string_view{ "str.cp" },
    std::string_view{ "string.cp" },
    std::string_view{ "ranges/iota.cp" },
    std::string_view{ "ranges.cp" },
    std::string_view{ "io/raw.cp" },
    std::string_view{ "io/format.cp" },
    std::string_view{ "io.cp" },
    std::string_view{ "std.cp" },
};

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

auto check_fixture_example_group(std::initializer_list<std::string_view> names, std::initializer_list<std::string_view> std_modules = {}) -> void
{
    auto sources = source_manager{};
    auto units = std::vector<parse_result>{};
    for(auto name : std_modules) {
        units.emplace_back(parse_source(sources, std::format("std/{}", name), read_std_module(name)));
    }
    for(auto name : names) {
        units.emplace_back(parse_source(sources, name, read_fixture_example(name)));
    }

    auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
    test_parser::assert_true(checked.accepted(), "fixture example group should pass semantic analysis");
}

auto analyze_with_std_io(std::string_view name, std::string text) -> semantic_result
{
    auto sources = source_manager{};
    auto units = std::vector<parse_result>{};
    for(auto module : std_io_modules) {
        units.emplace_back(parse_source(sources, std::format("std/{}", module), read_std_module(module)));
    }
    units.emplace_back(parse_source(sources, name, std::move(text)));
    return analyze_semantics(sources, std::span<parse_result const>{ units });
}

auto function_return_type(source_manager const& sources, parse_result const& parsed, semantic_result const& checked, std::string_view name) -> semantic_type_id
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

auto check_fixture_examples() -> void
{
    check_fixture_example_group({ "basics/main.cp" });
    check_fixture_example_group({ "modules/math.cp", "modules/main.cp" });
    check_fixture_example_group({ "types/main.cp" });
    check_fixture_example_group({ "flow/main.cp" });
    check_fixture_example_group({ "structs/main.cp" });
    check_fixture_example_group({ "concepts/main.cp" }, { "option.cp", "iter.cp" });
    check_fixture_example_group({ "generics/main.cp" });
    check_fixture_example_group({ "variants/main.cp" }, { "option.cp" });
    check_fixture_example_group({ "lambdas/main.cp" });
    check_fixture_example_group({ "memory/main.cp" });
    check_fixture_example_group({ "operators/main.cp" });
    check_fixture_example_group({ "ownership/main.cp" });
    check_fixture_example_group({ "std/main.cp" }, std_io_modules);
}

auto check_io_semantics() -> void
{
    auto accepted = analyze_with_std_io(
        "io_ok.cp",
        R"(import std;

main() -> i32
{
    println("x = {}, label = {}", 1, "cp");
    return 0;
})"
    );
    test_parser::assert_true(accepted.accepted(), "std.io should accept builtin display concept impls");

    auto nested = analyze_with_std_io(
        "io_nested_generic_call_argument.cp",
        R"(import std;

saw_result(result: print_result) -> bool
{
    return true;
}

main() -> i32
{
    let observed: bool = saw_result(println("{}", 1));
    return 0;
})"
    );
    test_parser::assert_true(nested.accepted(), "std.io result should remain typed through nested generic calls");

    auto str_checked = analyze_with_std_io(
        "str_view_ok.cp",
        R"(import std;

main() -> i32
{
    let text: str = "a\0b";
    let ptr = text.ptr;
    let len = text.len;
    let same_ptr = text.data();
    let same_len = text.size();
    let view = str{ .ptr = same_ptr, .len = 2 };
    for(let ch : "a\0b") {
        let item: char = ch;
    }
    if(len == same_len and view.len == 2) {
        return 0;
    }
    return 1;
})"
    );
    test_parser::assert_true(str_checked.accepted(), "str should expose ptr/len, methods, construction, and char iteration");

    auto rejected = analyze_with_std_io(
        "io_missing_display.cp",
        R"(import std;

struct box {
    value: i32;
}

main() -> i32
{
    println("{}", box{ 1 });
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(rejected, diagnostic_kind::missing_concept_item),
        "std.io should reject values without display"
    );
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
        "[] without context should infer [T; N]");

    auto signature = checked.signature_of(unit.functions.front());
    test_parser::assert_true(signature.valid(), "function should have a semantic signature");
    test_parser::assert_true(
        checked.signatures[signature.value].returns.valid(),
        "inferred function return should be recorded");
}

auto check_array_index_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "array_index.cp",
        R"(read()
{
    let data: [i32; 3] = [4, 5, 6];
    return data[1];
}

write_nested() -> i32
{
    let rows: [[i32; 3]; 2] = [[1, 2, 3], [4, 5, 6]];
    rows[1][2] = 9;
    return rows[1][2];
}

rvalue_index()
{
    return [4, 5, 6][2];
}

dynamic_index(offset: i32)
{
    let data: [i32; 3] = [4, 5, 6];
    return data[offset];
}

const_ref_read(values: [i32; 3] const&)
{
    return values[1];
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "array index source should pass semantic analysis");

    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "read") == semantic_type_ids::i32,
        "array index read should infer the element type");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "rvalue_index") == semantic_type_ids::i32,
        "array rvalue index should infer the element type");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "dynamic_index") == semantic_type_ids::i32,
        "dynamic array index should infer the element type");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "const_ref_read") == semantic_type_ids::i32,
        "const reference array index read should infer the element type");
}

auto check_tuple_member_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "tuple_member.cp",
        R"(main() -> i32
{
    let pair: (i32, i32) = (10, 20);
    pair.0 = 22;
    let single: (i32,) = (20,);
    return pair.0 + single.0;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "tuple member source should pass semantic analysis");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "main") == semantic_type_ids::i32,
        "tuple member access should infer selected element type");
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

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "base.cp",
                R"(export module base;

export concept marker {
})"),
            parse_source(
                sources,
                "wrap.cp",
                R"(export module wrap;

export import base;)"),
            parse_source(
                sources,
                "all.cp",
                R"(export module all;

import base;
export import wrap;)"),
            parse_source(
                sources,
                "main.cp",
                R"(import all;

struct value {
}

impl marker for value {
}

main()
{
    return;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(checked.accepted(), "same re-exported concept should be imported only once");
    }
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
    return value as i32;
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
    return second as i32;
})"
        );
        test_parser::assert_true(result.accepted(), "i64*& alias source should pass semantic analysis");
    }

    {
        auto result = analyze_one(
            "pointer_const_qualification.cp",
            R"(read_pointer(value: i32 const*) -> i32
{
    return *value;
}

read_double_pointer(value: i32 const**) -> i32
{
    return **value;
}

read_pointer_ref(value: i32 const*&) -> i32
{
    return *value;
}

main() -> i32
{
    let value = 7;
    let pointer: i32* = &value;
    let pointer_pointer: i32** = &pointer;
    let first = read_pointer(pointer);
    let second = read_double_pointer(pointer_pointer);
    let third = read_pointer_ref(ref pointer);
    return first + second + third;
})"
        );
        test_parser::assert_true(result.accepted(), "pointer qualification conversion source should pass semantic analysis");
    }
}

auto check_struct_impl_semantics() -> void
{
    auto result = analyze_one(
        "struct_impl.cp",
        R"(struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    vec2(x: i32, y: i32)
    {
        return vec2{ .x = x, .y = y };
    }

    sum(self const&) -> i32
    {
        return x + self.y;
    }

    add(self&, value: i32) -> void
    {
        x += value;
    }

    add_to_y(self&) -> void
    {
        add(y);
    }

    total(self const&) -> i32
    {
        return sum();
    }

    zero() -> vec2
    {
        return vec2{};
    }

    ~vec2()
    {
    }
}

main() -> i32
{
    let value = vec2{ 1, 2 };
    value.add_to_y();
    let empty = vec2::zero();
    return value.total() + empty.x;
})"
    );
    test_parser::assert_true(result.accepted(), "struct impl source should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic("duplicate_field.cp", "struct point { x: i32; x: i32; }", duplicate_field);
    expect_diagnostic("unknown_init_field.cp", "struct point { x: i32; } main() { let p = point{ .y = 1 }; }", unknown_field);
    expect_diagnostic (
        "duplicate_init_field.cp",
        "struct point { x: i32; } main() { let p = point{ .x = 1, .x = 2 }; }",
        duplicate_field_initializer
    );
    expect_diagnostic (
        "invalid_self.cp",
        "struct a { x: i32; } struct b { x: i32; } impl a { bad(self: b) { return; } }",
        invalid_self_parameter
    );
    expect_diagnostic (
        "reference_default_field.cp",
        "struct holder { value: i32&; } main() { let holder_value = holder{}; }",
        default_initialization_failure
    );
    expect_diagnostic (
        "implicit_self_call_blocks_first_arg_ufcs.cp",
        "struct a { } struct b { } impl a { foo(self&, value: i32) -> void { } test(self&, item: b) -> void { foo(item); } } impl b { foo(self&) -> void { } }",
        type_mismatch
    );
    expect_diagnostic (
        "local_name_blocks_implicit_self_call.cp",
        "struct a { } impl a { foo(self&) -> void { } test(self&) -> void { let foo = 1; foo(); } }",
        not_callable
    );
}

auto check_generic_struct_semantics() -> void
{
    auto result = analyze_one(
        "generic_struct.cp",
        R"(struct vector<T> {
    item: T;
    size: usize;
}

main() -> i32
{
    let values: vector<i32> = vector<i32>{ .item = 40, .size = 2 };
    return values.item + (values.size as i32);
})"
    );
    test_parser::assert_true(result.accepted(), "generic struct source should pass semantic analysis");

    auto const_result = analyze_one(
        "const_generic_struct.cp",
        R"(struct buffer<T, N: usize> {
    data: [T; N];
}

main() -> i32
{
    let values: buffer<i32, 4> = buffer<i32, 4>{ .data = [40, 1, 2, 3] };
    return values.data[0] + values.data[1];
})"
    );
    test_parser::assert_true(const_result.accepted(), "const generic struct source should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic(
        "generic_struct_missing_argument.cp",
        "struct vector<T> { item: T; } main() { let values = vector{}; }",
        invalid_type_argument
    );
    expect_diagnostic(
        "generic_struct_extra_argument.cp",
        "struct vector<T> { item: T; } main() { let values = vector<i32, i32>{ .item = 1 }; }",
        invalid_type_argument
    );
}

auto check_generic_function_semantics() -> void
{
    auto result = analyze_one(
        "generic_function.cp",
        R"(concept marker {
}

struct value {
    item: i32;
}

impl marker for value {
}

id<T>(input: T) -> T
{
    return input;
}

accept<T: marker>(input: T) -> i32
{
    return 1;
}

use<T>(input: T) -> i32
requires T: marker
{
    return 2;
}

zero<T>(values: [T; 0]) -> i32
{
    return 0;
}

head<T, N: usize>(values: [T; N]) -> T
{
    return values[0];
}

struct box<T> {
    value: T;
}

impl box<T> {
    make(seed: usize) -> box<T>
    {
        return box<T>{};
    }

    get(self const&) -> T
    {
        return value;
    }
}

main() -> i32
{
    let first = id<i32>(20);
    let second = id(21);
    let ok = id<bool>(true);
    let item = value{ .item = 1 };
    let boxed = box<i32>::make(1);
    if(ok) {
        return first + second + accept(item) + use(item) + zero<i32>([]) + boxed.get() + head([2, 3, 4]) - 4;
    }
    return 0;
})"
    );
    test_parser::assert_true(result.accepted(), "generic function source should pass semantic analysis");
    test_parser::assert_true(result.function_instances.size() == 8, "generic calls should create concrete function instances");
    test_parser::assert_true(
        std::ranges::all_of(result.function_instances, [](semantic_function_instance const& instance) {
            return instance.context_index != 0uz and instance.symbol.valid() and instance.signature.valid();
        }),
        "generic function instances should expose context, symbol, and signature metadata");

    auto void_result = analyze_one(
        "generic_void_return.cp",
        "touch<T>(value: T) -> void { return; } main() -> i32 { touch(1); return 0; }"
    );
    test_parser::assert_true(void_result.accepted(), "generic functions should allow explicit void return");

    using enum diagnostic_kind;
    expect_diagnostic(
        "generic_function_too_many_type_args.cp",
        "id<T>(input: T) -> T { return input; } main() -> i32 { return id<i32,bool>(1); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "generic_function_cannot_infer_type_arg.cp",
        "make<T>() -> T { return 1; } main() -> i32 { return make(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "generic_function_argument_mismatch.cp",
        "id<T>(input: T) -> T { return input; } main() -> i32 { return id<bool>(1); }",
        type_mismatch
    );
    expect_diagnostic(
        "generic_function_missing_bound.cp",
        "concept marker { } accept<T: marker>(input: T) -> i32 { return 1; } main() -> i32 { return accept(1); }",
        missing_concept_item
    );
    expect_diagnostic(
        "generic_function_inferred_return.cp",
        "id<T>(input: T) { return input; } main() -> i32 { return id(1); }",
        cannot_infer_return_type
    );
}

auto check_parameter_pack_semantics() -> void
{
    auto result = analyze_one(
        "parameter_pack.cp",
        R"(concept mark {
}

struct value {
    item: i32;
}

impl mark for value {
}

sum<T...: mark>(values: T...) -> i32
requires T...: mark
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value.item;
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

empty<T...>(values: T...) -> i32
{
    let total = 3;
    template fo)" "r" R"( (let value : values...) {
        total = total + value.item;
    }
    return total;
}

main() -> i32
{
    return sum(value{ .item = 20 }, value{ .item = 19 }) + type_count<value, value, value>() + empty();
})"
    );
    test_parser::assert_true(result.accepted(), "parameter pack source should pass semantic analysis");
    test_parser::assert_true(result.function_instances.size() == 3, "parameter pack calls should create concrete function instances");
    auto expansion_count = 0uz;
    for(auto const& [key, expansions] : result.template_for_expansions) {
        static_cast<void>(key);
        expansion_count += expansions.size();
    }
    test_parser::assert_true(expansion_count == 5, "template for should record value/type pack expansions");

    using enum diagnostic_kind;
    expect_diagnostic(
        "parameter_pack_missing_bound.cp",
        "concept mark { } sum<T...: mark>(values: T...) -> i32 { return 0; } main() -> i32 { return sum(1); }",
        missing_concept_item
    );
    expect_diagnostic(
        "parameter_pack_unknown_value_pack.cp",
        "bad<T...>(values: T...) -> i32 { template fo" "r (let value : missing...) { } return 0; } main() -> i32 { return bad(1); }",
        invalid_range
    );
    expect_diagnostic(
        "parameter_pack_direct_type_use.cp",
        "bad<T...>(values: T...) -> T { return 0; } main() -> i32 { return bad(1); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "parameter_pack_non_final_value_pack.cp",
        "bad<T...>(values: T..., last: i32) -> i32 { return last; } main() -> i32 { return bad(1, 2); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "template_for_break_outer_loop.cp",
        "bad<T...>(values: T...) -> i32 { while(true) { template fo" "r (let value : values...) { break; } } return 0; } main() -> i32 { return bad(1); }",
        invalid_break
    );
}

auto check_concept_semantics() -> void
{
    auto result = analyze_one(
        "concept_impl.cp",
        R"(concept iterator {
    type item;
    next(self&) -> item;
}

concept sized_iterator {
    requires iterator;
    remaining(self const&) -> i32;
}

struct range_iter {
    value: i32;
    remaining_value: i32;
}

impl iterator for range_iter {
    type item = i32;

    next(self&) -> i32
    {
        return value;
    }
}

impl sized_iterator for range_iter {
    remaining(self const&) -> i32
    {
        return remaining_value;
    }
}

main() -> i32
{
    type value_type = range_iter::item;
    let value: value_type = 1;
    return value;
})"
    );
    test_parser::assert_true(result.accepted(), "concrete concept impl source should pass semantic analysis");
    test_parser::assert_true(result.concepts.size() == 2, "semantic result should expose concepts");
    test_parser::assert_true(result.concept_impls.size() == 2, "semantic result should expose concept impl facts");

    auto this_result = analyze_one(
        "concept_this_type.cp",
        R"(concept comparable {
    same(self const&, rhs: this const&) -> i32;
}

struct value {
    x: i32;
}

impl comparable for value {
    same(self const&, rhs: this const&) -> i32
    {
        return x + rhs.x;
    }
}

main() -> i32
{
    let value = value{ .x = 1 };
    return value.same(value);
})"
    );
    test_parser::assert_true(this_result.accepted(), "this type in concept signatures should pass semantic analysis");

	    auto local_alias_result = analyze_one(
	        "local_type_alias.cp",
	        "main() { type value_type = i32; let value: value_type = 1; return value; }"
	    );
	    test_parser::assert_true(local_alias_result.accepted(), "local type alias source should pass semantic analysis");

    auto variant_result = analyze_one(
        "variant_match.cp",
        R"(variant optional<T> {
	    none;
	    some(T);
	}

	main() -> i32
	{
	    let value = optional<i32>::some(42);
	    return match value {
	        .some(item) => item,
	        .none => 0,
	    };
	})"
	    );
	    test_parser::assert_true(variant_result.accepted(), "variant match source should pass semantic analysis");
	    test_parser::assert_true(variant_result.variants.size() == 1, "semantic result should expose variants");

    auto requires_result = analyze_one(
        "requires_clause.cp",
        R"(concept marker {
}

struct value {
}

impl marker for value {
}

use() -> i32
requires value: marker
{
    return 42;
}

main() -> i32
{
    return use();
})"
    );
    test_parser::assert_true(requires_result.accepted(), "requires clause source should pass semantic analysis");

	    using enum diagnostic_kind;
    expect_diagnostic (
        "unknown_concept.cp",
        "struct value { x: i32; } impl missing for value { }",
        unknown_concept
    );
    expect_diagnostic (
        "missing_assoc_type.cp",
        "concept c { type item; } struct value { x: i32; } impl c for value { }",
        missing_concept_item
    );
    expect_diagnostic (
        "missing_concept_function.cp",
        "concept c { call(self&) -> i32; } struct value { x: i32; } impl c for value { }",
        missing_concept_item
    );
    expect_diagnostic (
        "concept_signature_mismatch.cp",
        "concept c { call(self&) -> i32; } struct value { x: i32; } impl c for value { call(self&) -> bool { return true; } }",
        type_mismatch
    );
    expect_diagnostic (
        "duplicate_concept_impl.cp",
        "concept c { } struct value { x: i32; } impl c for value { } impl c for value { }",
        duplicate_concept_impl
    );
    expect_diagnostic (
        "missing_parent_concept.cp",
        "concept a { } concept b { requires a; } struct value { x: i32; } impl b for value { }",
        missing_concept_item
    );
    expect_diagnostic (
        "anonymous_export_concept.cp",
        "export concept c { }",
        export_requires_module
    );
	    expect_diagnostic (
	        "duplicate_local_type_alias.cp",
	        "main() { type value = i32; type value = bool; }",
	        duplicate_symbol
	    );
	    expect_diagnostic (
	        "unknown_variant_case.cp",
	        "variant option { none; } main() { let value = option::none; let out = match value { .some(item) => item, }; }",
	        unknown_variant_case
	    );
	    expect_diagnostic (
	        "non_exhaustive_match.cp",
	        "variant option { none; some(i32); } main() { let value = option::none; let out = match value { .none => 0, }; }",
	        non_exhaustive_match
	    );
    expect_diagnostic (
        "requires_missing_impl.cp",
        "concept marker { } struct value { } use() -> i32 requires value: marker { return 1; }",
        missing_concept_item
    );
    expect_diagnostic (
        "requires_unknown_concept.cp",
        "struct value { } use() -> i32 requires value: missing { return 1; }",
        unknown_concept
    );
	}

auto check_function_type_and_memory_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "function_type_memory.cp",
        R"(add(x: i32, y: i32) -> i32
{
    return x + y;
}

main() -> i32
{
    let callback: f(left: i32, right: i32) -> i32 = add;
    let raw: f*(i32, i32) -> i32 = add;
    let first = callback(20, 1);
    let second = raw(20, 1);
    let pointer = alloc<i32>(2);
    construct_at(pointer, first + second);
    construct_at(pointer + 1, 1);
    let result = *pointer + *(pointer + 1);
    destroy_at(pointer + 1);
    destroy_at(pointer);
    free(pointer);
    return result;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "function type and memory source should pass semantic analysis");

    auto const& unit = *parsed.root;
    auto const& main_function = parsed.ast.node(unit.functions[1]);
    auto const& body = as<block_statement_syntax>(parsed.ast.node(main_function.body));
    auto const& callback_decl = as<declaration_statement_syntax>(parsed.ast.node(body.statements.front()));
    auto callback_type = checked.binding_of(0uz, body.statements.front());
    test_parser::assert_true(callback_type.valid(), "callback declaration should bind a symbol");
    test_parser::assert_true(
        std::holds_alternative<function_type>(checked.types.get(checked.symbols[callback_type.value].type)),
        "f(...) declaration should lower to function_type");
    static_cast<void>(callback_decl);

    auto const& raw_decl = as<declaration_statement_syntax>(parsed.ast.node(body.statements[1]));
    auto raw_symbol = checked.binding_of(0uz, body.statements[1]);
    test_parser::assert_true(raw_symbol.valid(), "raw function pointer declaration should bind a symbol");
    test_parser::assert_true(
        std::holds_alternative<pointer_type>(checked.types.get(checked.symbols[raw_symbol.value].type)),
        "f*(...) declaration should lower to pointer_type");
    static_cast<void>(raw_decl);

    auto const& pointer_decl = as<declaration_statement_syntax>(parsed.ast.node(body.statements[4]));
    auto alloc_builtin = checked.builtin_call_of(0uz, pointer_decl.initializer);
    test_parser::assert_true(
        alloc_builtin.kind == semantic_builtin_call_kind::alloc and alloc_builtin.type == semantic_type_ids::i32,
        "alloc<T> should record builtin call metadata");

    auto new_parsed = parse_source(
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

main() -> i32
{
    let total = 0;
    let item = new guard{ &total, 3 };
    let values = new [i32; 2]{ 1, 2 };
    delete item;
    delete values;
    delete nullptr;
    return total;
})");
    auto new_checked = analyze_single(sources, new_parsed);
    test_parser::assert_true(new_checked.accepted(), "new/delete source should pass semantic analysis");
    auto const& new_body = as<block_statement_syntax>(new_parsed.ast.node(new_parsed.ast.node(new_parsed.root->functions.front()).body));
    auto const& item_decl = as<declaration_statement_syntax>(new_parsed.ast.node(new_body.statements[1]));
    auto new_builtin = new_checked.builtin_call_of(0uz, item_decl.initializer);
    test_parser::assert_true(
        new_builtin.kind == semantic_builtin_call_kind::new_object,
        "new expression should record builtin metadata");
    auto const& delete_statement = as<expression_statement_syntax>(new_parsed.ast.node(new_body.statements[3]));
    auto delete_builtin = new_checked.builtin_call_of(0uz, delete_statement.expression);
    test_parser::assert_true(
        delete_builtin.kind == semantic_builtin_call_kind::delete_object,
        "delete expression should record builtin metadata");

    auto null_parsed = parse_source(
        sources,
        "nullptr.cp",
        R"(set(pointer: i32*) { }

main() -> i32
{
    let pointer: i32* = nullptr;
    set(nullptr);
    if(pointer == nullptr) {
        return 1;
    }
    if(nullptr == pointer) {
        return 2;
    }
    return 3;
})");
    auto null_checked = analyze_single(sources, null_parsed);
    test_parser::assert_true(null_checked.accepted(), "nullptr should convert to contextual pointer types");

    expect_diagnostic("bad_delete_value.cp", "main() { let value = 1; delete value; }", diagnostic_kind::type_mismatch);
    expect_diagnostic("bad_delete_const_pointer.cp", "main() { let value = 1; let pointer: i32 const* = &value; delete pointer; }", diagnostic_kind::type_mismatch);
}

auto check_extern_c_semantics() -> void
{
    using enum diagnostic_kind;

    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "extern_c.cp",
        R"(export module c.wrap;

extern "C" abs(value: i32) -> i32;

export extern "C" answer() -> i32
{
    return abs(-42);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "extern C source should pass semantic analysis");

    auto const& unit = *parsed.root;
    auto const& abs_function = parsed.ast.node(unit.functions.front());
    auto abs_signature = checked.signature_of(unit.functions.front());
    auto abs_symbol = checked.function_symbol_of(unit.functions.front());
    auto answer_symbol = checked.function_symbol_of(unit.functions.back());
    test_parser::assert_true(abs_signature.valid(), "extern C declaration should have a signature");
    test_parser::assert_true(abs_symbol.valid(), "extern C declaration should have a symbol");
    test_parser::assert_true(answer_symbol.valid(), "extern C definition should have a symbol");
    test_parser::assert_true(not abs_function.has_body, "extern C declaration should not have a body");
    test_parser::assert_true(
        checked.symbols[abs_symbol.value].body_kind == semantic_function_body_kind::extern_declaration,
        "extern C declaration should be classified as an extern declaration");
    test_parser::assert_true(
        checked.symbols[answer_symbol.value].body_kind == semantic_function_body_kind::source_body,
        "extern C definition should be classified as a source body");
    test_parser::assert_true(
        checked.signatures[abs_signature.value].returns == semantic_type_ids::i32,
        "extern C declaration should preserve its return type");

    expect_diagnostic(
        "extern_c_bad_abi.cp",
        "extern \"Rust\" item(value: i32) -> i32;",
        invalid_type_argument
    );
    expect_diagnostic(
        "extern_c_bad_return.cp",
        "extern \"C\" text() -> str;",
        invalid_type_argument
    );
    expect_diagnostic(
        "extern_c_generic.cp",
        "extern \"C\" id<T>(value: T) -> T;",
        invalid_type_argument
    );
}

auto check_string_index_semantics() -> void
{
    auto checked = analyze_one(
        "string_index.cp",
R"(main() -> i32
{
    let title: str = "cp";
    if(title[0] == 'c') {
        return 42;
    }
    return 1;
})"
    );
    test_parser::assert_true(checked.accepted(), "string index source should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic("string_dynamic_index_type.cp", "main() { let text: str = \"cp\"; let value = text[true]; }", invalid_operator);
    expect_diagnostic("string_index_assign.cp", "main() { let text: str = \"cp\"; text[0] = 'x'; }", invalid_assignment_target);
}

auto check_decltype_ref_and_destructuring_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "decltype_ref_destructure.cp",
        R"(main() -> i32
{
    let pair = (10, 20);
    let ref
        (left, right) = pair;
    left = 21;
    type item = decltype(right);
    let value: item = right;
    let (copy_left, copy_right) = pair;
    return copy_left + copy_right + value - 19;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "decltype/ref/destructuring source should pass semantic analysis");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "main") == semantic_type_ids::i32,
        "decltype/ref/destructuring main should return i32");
}

auto check_ownership_frontend_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "ownership_frontend.cp",
        R"(by_value(value: i32) -> i32
{
    return value;
}

by_ref(value: i32&) -> i32
{
    value = value + 1;
    return value;
}

by_const(value: i32 const&) -> i32
{
    return value;
}

take_move(value: i32 move&) -> i32
{
    return value;
}

main() -> i32
{
    let value = 1;
    type value_ref = decltype(ref value);
    let ref alias: value_ref = value;
    let a = by_ref(ref value);
    let b = by_const(const ref value);
    let c = take_move(move value);
    let d = by_value(move value);
    return a + b + c + d + alias - 8;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "ownership frontend source should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic("ref_to_value.cp", "by_value(value: i32) { } main() { let x = 1; by_value(ref x); }", type_mismatch);
    expect_diagnostic("const_ref_to_value.cp", "by_value(value: i32) { } main() { let x = 1; by_value(const ref x); }", type_mismatch);
    expect_diagnostic("const_ref_to_mut.cp", "by_ref(value: i32&) { } main() { let x = 1; by_ref(const ref x); }", type_mismatch);
    expect_diagnostic("move_to_ref.cp", "by_ref(value: i32&) { } main() { let x = 1; by_ref(move x); }", type_mismatch);
    expect_diagnostic("lvalue_to_move_ref.cp", "take_move(value: i32 move&) { } main() { let x = 1; take_move(x); }", type_mismatch);
    expect_diagnostic("const_move.cp", "take_move(value: i32 move&) { } main() { const x = 1; take_move(move x); }", invalid_assignment_target);
}

auto check_like_receiver_and_delete_semantics() -> void
{
    auto checked = analyze_one(
        "like_receiver.cp",
        R"(struct box {
    value: i32;
}

impl box {
    get(self like&) -> i32 like&
    {
        return ref value;
    }
}

main() -> i32
{
    let item = box{ 40 };
    const fixed = box{ 2 };
    let ref value = item.get();
    return value + fixed.get();
})");
    test_parser::assert_true(checked.accepted(), "self like& receiver source should pass semantic analysis");

    auto pointer_checked = analyze_one(
        "like_pointer_receiver.cp",
R"(struct holder {
    ptr: i32*;
    ptr_ptr: i32**;
}

impl holder {
    data(self like&) -> i32 like*
    {
        return ptr;
    }

    data_ptr(self like&) -> i32 like**
    {
        return ptr_ptr;
    }

    data_ref(self like&) -> i32 like*&
    {
        return ref ptr;
    }
}

main() -> i32
{
    let value = 1;
    let pointer = &value;
    let item = holder{ &value, &pointer };
    const fixed = holder{ &value, &pointer };
    *item.data() = 2;
    *item.data_ref() = 3;
    **item.data_ptr() = 4;
    return *fixed.data() + **fixed.data_ptr();
})");
    test_parser::assert_true(pointer_checked.accepted(), "like pointer and reference returns should follow receiver constness");

    using enum diagnostic_kind;
    expect_diagnostic(
        "like_const_to_mut_ref.cp",
        "struct box { value: i32; } impl box { get(self like&) -> i32 like& { return ref value; } } main() { const fixed = box{ 1 }; fixed.get() = 2; }",
        assign_to_const
    );
    expect_diagnostic(
        "like_const_pointer_write.cp",
        "struct holder { ptr: i32*; } impl holder { data(self like&) -> i32 like* { return ptr; } } main() { let value = 1; const fixed = holder{ &value }; *fixed.data() = 3; }",
        assign_to_const
    );
    expect_diagnostic(
        "like_const_double_pointer_write.cp",
        "struct holder { ptr_ptr: i32**; } impl holder { data(self like&) -> i32 like** { return ptr_ptr; } } main() { let value = 1; let pointer = &value; const fixed = holder{ &pointer }; **fixed.data() = 3; }",
        assign_to_const
    );
    expect_diagnostic(
        "deleted_method_call.cp",
        "struct box { value: i32; } impl box { reset(self&) = delete; } main() { let item = box{ 1 }; item.reset(); }",
        not_callable
    );
    expect_diagnostic(
        "deleted_assignment_operator.cp",
        "struct box { value: i32; } impl box { operator =(self&, rhs: this const&) = delete; } main() { let left = box{ 1 }; let right = box{ 2 }; left = right; }",
        invalid_operator
    );
}

auto check_function_body_kind_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "function_body_kinds.cp",
        R"(struct box {
    value: i32;
}

impl box {
    box() = default;
    reset(self&) = delete;

    make() -> box
    {
        return box{};
    }
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "function body kind source should pass semantic analysis");

    auto const& impl = parsed.ast.node(parsed.root->impls.front());
    auto defaulted = checked.function_symbol_of(impl.functions[0]);
    auto deleted = checked.function_symbol_of(impl.functions[1]);
    auto source_body = checked.function_symbol_of(impl.functions[2]);
    test_parser::assert_true(defaulted.valid(), "defaulted constructor should have a symbol");
    test_parser::assert_true(deleted.valid(), "deleted method should have a symbol");
    test_parser::assert_true(source_body.valid(), "source body method should have a symbol");
    test_parser::assert_true(
        checked.symbols[defaulted.value].body_kind == semantic_function_body_kind::defaulted,
        "defaulted constructor should be classified as defaulted");
    test_parser::assert_true(
        checked.symbols[deleted.value].body_kind == semantic_function_body_kind::deleted,
        "deleted method should be classified as deleted");
    test_parser::assert_true(
        checked.symbols[source_body.value].body_kind == semantic_function_body_kind::source_body,
        "ordinary method should be classified as source body");
}

auto check_lambda_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "lambda.cp",
        R"(main() -> i32
{
    let inc: f(i32) -> i32 =
        f(x) => x + 1;
    let square = f(x: i32) -> i32 {
        x * x
    };
    let bias = 1;
    let add_bias = f(x: i32) -> i32 {
        x + bias
    };
    let generic_add = f<T>(x: T) -> T {
        x + bias
    };
    let generic_id = f<T>(x: T) -> T {
        x
    };
    return inc(20) + square(4) + add_bias(1) + generic_add<i32>(1) + generic_id<i32>(0);
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "lambda source should pass semantic analysis");
    test_parser::assert_true(
        function_return_type(sources, parsed, checked, "main") == semantic_type_ids::i32,
        "lambda main should return i32");
}

auto check_operator_overload_semantics() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "operator_overload.cp",
        R"(struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    operator +(self const&, rhs: this const&) -> this
    {
        return vec2{ .x = x + rhs.x, .y = y + rhs.y };
    }

    operator ==(self const&, rhs: this const&) -> bool
    {
        return x == rhs.x and y == rhs.y;
    }

    operator +=(self&, rhs: this const&)
    {
        x += rhs.x;
        y += rhs.y;
    }
}

operator -(left: vec2 const&, right: vec2 const&) -> vec2
{
    return vec2{ .x = left.x - right.x, .y = left.y - right.y };
}

struct slot {
    value: i32;
}

impl slot {
    operator [](self&, index: i32) -> i32&
    {
        return value;
    }
}

main() -> i32
{
    let a = vec2{ 1, 2 };
    let b = vec2{ 3, 4 };
    let c = a + b;
    let d = c - a;
    let expected = vec2{ 4, 6 };
    let same = c == expected;
    let holder = slot{ 5 };
    holder[0] = 9;
    a += b;
    if(same) {
        return d.x + holder[0];
    }
    return 0;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "operator overload source should pass semantic analysis");
    test_parser::assert_true(checked.expression_operators.size() >= 5uz, "operator expressions should record selected overloads");
}

auto check_operator_import_semantics() -> void
{
    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "ops.cp",
                R"(export module ops;

export struct vec2 {
    x: i32;
    y: i32;
}

export operator +(left: vec2 const&, right: vec2 const&) -> vec2
{
    return vec2{ .x = left.x + right.x, .y = left.y + right.y };
})"),
            parse_source(
                sources,
                "main.cp",
                R"(import ops;

main() -> i32
{
    let a = vec2{ 1, 2 };
    let b = vec2{ 3, 4 };
    let c = a + b;
    return c.x;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(checked.accepted(), "exported top-level operator should be visible after import");
        test_parser::assert_true(not checked.expression_operators.empty(), "imported operator expression should record selected overload");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "hidden.cp",
                R"(export module hidden;

export struct box {
    value: i32;
}

operator +(left: box const&, right: box const&) -> box
{
    return box{ left.value + right.value };
})"),
            parse_source(
                sources,
                "main.cp",
                R"(import hidden;

main()
{
    let a = box{ 1 };
    let b = box{ 2 };
    let c = a + b;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::invalid_operator),
            "non-exported top-level operator should not be visible after import");
    }
}

auto check_operator_negative_semantics() -> void
{
    using enum diagnostic_kind;
    expect_diagnostic (
        "operator_ambiguous.cp",
        R"(struct marker {
    value: i32;
}

operator +(left: marker const&, right: marker const&) -> marker
{
    return marker{ left.value + right.value };
}

operator +(left: marker const&, right: marker const&) -> marker
{
    return marker{ left.value - right.value };
}

main()
{
    let a = marker{ 1 };
    let b = marker{ 2 };
    let c = a + b;
})",
        invalid_operator
    );
    expect_diagnostic (
        "operator_layer_no_fallback.cp",
        R"(struct marker {
    value: i32;
}

impl marker {
    operator +(self const&, rhs: bool) -> this
    {
        return marker{ value };
    }
}

operator +(left: marker const&, right: marker const&) -> marker
{
    return marker{ left.value + right.value };
}

main()
{
    let a = marker{ 1 };
    let b = marker{ 2 };
    let c = a + b;
})",
        invalid_operator
    );
    expect_diagnostic (
        "operator_const_index_assign.cp",
        R"(struct cell {
    value: i32;
}

impl cell {
    operator [](self const&, index: i32) -> i32 const&
    {
        return value;
    }
}

main()
{
    let item = cell{ 1 };
    item[0] = 2;
})",
        assign_to_const
    );
}

auto check_negative_cases() -> void
{
    using enum diagnostic_kind;
    expect_diagnostic("mixed_array.cp", "main() { let value = [1, 2.0]; }", heterogeneous_aggregate);
    expect_diagnostic("empty_array.cp", "main() { let value = []; }", empty_aggregate_without_context);
    expect_diagnostic("short_array.cp", "main() { let value: [i32; 2] = [1]; }", aggregate_length_mismatch);
    expect_diagnostic("bad_element.cp", "main() { let value: [i32; 2] = [1, true]; }", type_mismatch);
    expect_diagnostic("index_non_array.cp", "main() { let value = 1; let item = value[0]; }", invalid_operator);
    expect_diagnostic("index_non_integer.cp", "main() { let data = [1, 2]; let item = data[true]; }", invalid_operator);
    expect_diagnostic("index_negative.cp", "main() { let data = [1, 2]; let item = data[-1]; }", invalid_operator);
    expect_diagnostic("index_too_large.cp", "main() { let data: [i32; 2] = [1, 2]; let item = data[2]; }", invalid_operator);
    expect_diagnostic("tuple_subscript.cp", "main() { let pair = (1, 2); return pair[0]; }", invalid_operator);
    expect_diagnostic("tuple_member_too_large.cp", "main() { let pair = (1, 2); return pair.2; }", invalid_operator);
    expect_diagnostic("index_const_assign.cp", "main() { const data = [1, 2]; data[0] = 3; }", assign_to_const);
    expect_diagnostic (
        "index_const_ref_assign.cp",
        "mutate(values: [i32; 2] const&) { values[0] = 3; }",
        assign_to_const
    );
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
    expect_diagnostic("float_bitwise_not.cp", "main() { let value = 1.0; let next = ~value; }", invalid_operator);
    expect_diagnostic("float_increment.cp", "main() { let value = 1.0; value++; }", invalid_operator);
    expect_diagnostic("bad_condition.cp", "main() { if(1) { return; } }", condition_not_bool);
    expect_diagnostic("bad_return.cp", "main() -> i32 { return true; }", return_type_mismatch);
    expect_diagnostic("bad_void_value.cp", "main() { let value: void = {}; }", unknown_type);
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
    expect_diagnostic("bad_alloc_count.cp", "main() { let pointer = alloc<i32>(true); }", type_mismatch);
    expect_diagnostic("bad_construct_value.cp", "main() { let pointer = alloc<i32>(1); construct_at(pointer, true); }", type_mismatch);
    expect_diagnostic("bad_nullptr_inference.cp", "main() { let pointer = nullptr; }", type_mismatch);
    expect_diagnostic("bad_ref_initializer.cp", "main() { let ref value = 1 + 2; }", invalid_assignment_target);
    expect_diagnostic("bad_destructure_initializer.cp", "main() { let (a, b) = 1; }", type_mismatch);
    expect_diagnostic("lambda_parameter_infer.cp", "main() { let f = f(x) => x; }", type_mismatch);
    expect_diagnostic (
        "operator_builtin_only.cp",
        "operator +(left: i32, right: i32) -> i32 { return left + right; }",
        invalid_operator
    );
    expect_diagnostic (
        "operator_bad_assign_self.cp",
        "struct value { item: i32; } impl value { operator =(self const&, rhs: this const&) { } }",
        invalid_operator
    );
    expect_diagnostic (
        "operator_value_index_assign.cp",
        "struct slot { value: i32; } impl slot { operator [](self&, index: i32) -> i32 { return value; } } main() { let holder = slot{ 1 }; holder[0] = 2; }",
        invalid_assignment_target
    );
}

} // namespace

auto main() -> int
{
    check_fixture_examples();
    check_io_semantics();
    check_side_tables();
    check_array_index_semantics();
    check_tuple_member_semantics();
    check_fixed_type_ids();
    check_inferred_return_types();
    check_contiguous_unit_batch();
    check_anonymous_modules();
    check_reference_pointer_types();
    check_struct_impl_semantics();
    check_generic_struct_semantics();
    check_generic_function_semantics();
    check_parameter_pack_semantics();
    check_concept_semantics();
    check_function_type_and_memory_semantics();
    check_extern_c_semantics();
    check_function_body_kind_semantics();
    check_string_index_semantics();
    check_decltype_ref_and_destructuring_semantics();
    check_ownership_frontend_semantics();
    check_like_receiver_and_delete_semantics();
    check_lambda_semantics();
    check_operator_overload_semantics();
    check_operator_import_semantics();
    check_operator_negative_semantics();
    check_negative_cases();
    return 0;
}
