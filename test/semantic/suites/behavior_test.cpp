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

auto has_static_local(semantic_result const& result, std::string_view name) -> bool
{
    return std::ranges::any_of(result.symbols, [&](semantic_symbol const& symbol) {
        return symbol.kind == symbol_kind::local and symbol.name == name and symbol.is_static_local;
    });
}

auto has_capture_mode(semantic_result const& result, std::string_view name, semantic_lambda_capture_mode mode) -> bool
{
    for(auto const& [key, lambda] : result.lambda_infos) {
        static_cast<void>(key);
        for(auto const& capture : lambda.captures) {
            if(capture.name == name and capture.mode == mode) {
                return true;
            }
        }
    }
    return false;
}

auto constexpr std_io_modules = {
    std::string_view{ "core/option.cp" },
    std::string_view{ "core/expected.cp" },
    std::string_view{ "core/iter.cp" },
    std::string_view{ "detail/runtime.cp" },
    std::string_view{ "memory/raw_buffer.cp" },
    std::string_view{ "memory/span.cp" },
    std::string_view{ "collections/detail/vector_storage.cp" },
    std::string_view{ "collections/vector.cp" },
    std::string_view{ "collections/detail/btree_storage.cp" },
    std::string_view{ "collections/detail/btree.cp" },
    std::string_view{ "collections/map.cp" },
    std::string_view{ "collections/set.cp" },
    std::string_view{ "text/str.cp" },
    std::string_view{ "text/detail/string_storage.cp" },
    std::string_view{ "text/string.cp" },
    std::string_view{ "core.cp" },
    std::string_view{ "memory.cp" },
    std::string_view{ "collections.cp" },
    std::string_view{ "text.cp" },
    std::string_view{ "compare.cp" },
    std::string_view{ "algorithm/sort.cp" },
    std::string_view{ "algorithm.cp" },
    std::string_view{ "io/raw.cp" },
    std::string_view{ "io/format.cp" },
    std::string_view{ "io.cp" },
    std::string_view{ "fs/file.cp" },
    std::string_view{ "fs.cp" },
    std::string_view{ "meta.cp" },
    std::string_view{ "ranges/sources.cp" },
    std::string_view{ "ranges/iota.cp" },
    std::string_view{ "ranges/terminals.cp" },
    std::string_view{ "ranges/adapters.cp" },
    std::string_view{ "ranges.cp" },
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
    auto group_name = names | std::views::join_with(',') | std::ranges::to<std::string>();
    test_parser::assert_true(checked.accepted(), std::format("{} fixture example group should pass semantic analysis", group_name));
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

auto function_named(source_manager const& sources, parse_result const& parsed, std::string_view name) -> function_id
{
    auto source = ast_source_view{ sources };
    auto const& unit = *parsed.root;
    for(auto id : unit.functions) {
        auto const& function = parsed.ast.node(id);
        if(source.identifier(function.name) == name) {
            return id;
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
    check_fixture_example_group({ "concepts/main.cp" }, { "core/option.cp", "core/iter.cp" });
    check_fixture_example_group({ "generics/main.cp" });
    check_fixture_example_group({ "variants/main.cp" }, { "core/option.cp" });
    check_fixture_example_group({ "lambdas/main.cp" });
    check_fixture_example_group({ "memory/main.cp" });
    check_fixture_example_group({ "operators/main.cp" });
    check_fixture_example_group({ "ownership/main.cp" });
    check_fixture_example_group({ "interop/main.cp" });
    check_fixture_example_group({ "errors/main.cp" }, { "core/option.cp" });
    check_fixture_example_group({ "fs/main.cp" }, std_io_modules);
    check_fixture_example_group({ "std/main.cp" }, std_io_modules);
}

auto check_io_semantics() -> void
{
    auto accepted = analyze_with_std_io(
        "io_ok.cp",
        R"cp(import std;

struct point {
    x: i32;
    y: i32;
}

impl display for point {
    display(self const&, out: formatter&) -> display_result
    {
        return out.format("point({}, {})", x, y);
    }
}

main() -> i32
{
    let name = string{"cp"};
    println(
        "types {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
        true,
        'x',
        "cp",
        name,
        -1 as i8,
        -2 as i16,
        -3,
        -4 as i64,
        -5 as isize,
        1 as u8,
        2 as u16,
        3 as u32,
        4 as u64,
        5 as usize,
        1.5
    );
    println("custom = {}", point{ .x = 1, .y = 2 });
    return 0;
})cp"
    );
    test_parser::assert_true(accepted.accepted(), "std.io should accept builtin and string display concept impls");

    auto nested = analyze_with_std_io(
        "io_nested_generic_call_argument.cp",
        R"(import std;

saw_result(result: display_result) -> bool
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

auto check_std_layered_imports() -> void
{
    auto vector_import = analyze_with_std_io(
        "std_layered_vector_import.cp",
        R"(import std.collections.vector;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(42);
    return values.size() as i32;
})"
    );
    test_parser::assert_true(vector_import.accepted(), "std.collections.vector should be directly importable");

    auto flat_import = analyze_with_std_io(
        "std_flat_vector_import.cp",
        R"(import std.vector;

main() -> i32
{
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(flat_import, diagnostic_kind::unknown_module),
        "std.vector should not remain as a flat compatibility module"
    );
}

auto check_range_for_binding_semantics() -> void
{
    auto bindings = analyze_with_std_io(
        "range_for_bindings.cp",
        R"(import std;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(1);
    for(const range_for_value_marker : values) {
        let copy: i32 = range_for_value_marker;
    }
    for(let ref range_for_ref_marker : values) {
        range_for_ref_marker += 1;
    }
    for(const ref range_for_const_ref_marker : values) {
        let copy: i32 = range_for_const_ref_marker;
    }
    const fixed = values;
    for(const ref range_for_fixed_marker : fixed) {
        let copy: i32 = range_for_fixed_marker;
    }
    return values[0];
})"
    );
    test_parser::assert_true(bindings.accepted(), "range-for should accept value, ref, const-ref, and const iterable bindings");

    auto value_binding = std::ranges::find_if(bindings.symbols, [](semantic_symbol const& symbol) {
        return symbol.name == "range_for_value_marker";
    });
    test_parser::assert_true(value_binding != bindings.symbols.end(), "range-for value binding should create a local symbol");
    test_parser::assert_true(
        not std::holds_alternative<reference_type>(bindings.types.get(value_binding->type)),
        "for(const x : vector) should bind x by value");
    test_parser::assert_true(value_binding->is_const, "for(const x : vector) should create a const binding");

    auto ref_binding = std::ranges::find_if(bindings.symbols, [](semantic_symbol const& symbol) {
        return symbol.name == "range_for_ref_marker";
    });
    test_parser::assert_true(ref_binding != bindings.symbols.end(), "range-for ref binding should create a local symbol");
    auto const* ref_type = std::get_if<reference_type>(&bindings.types.get(ref_binding->type));
    test_parser::assert_true(ref_type != nullptr and not ref_type->is_const, "for(let ref x : vector) should bind a mutable reference");

    auto const_ref_binding = std::ranges::find_if(bindings.symbols, [](semantic_symbol const& symbol) {
        return symbol.name == "range_for_const_ref_marker";
    });
    test_parser::assert_true(const_ref_binding != bindings.symbols.end(), "range-for const ref binding should create a local symbol");
    auto const* const_ref_type = std::get_if<reference_type>(&bindings.types.get(const_ref_binding->type));
    test_parser::assert_true(const_ref_type != nullptr and const_ref_type->is_const, "for(const ref x : vector) should bind a readonly reference");

    auto const_ref_assign = analyze_with_std_io(
        "range_for_const_ref_assign_rejected.cp",
        R"(import std;

main()
{
    let values = vector<i32>{};
    values.push_back(1);
    for(const ref item : values) {
        item = 2;
    }
})"
    );
    test_parser::assert_true(has_diagnostic(const_ref_assign, diagnostic_kind::assign_to_const), "const ref range binding should reject writes");

    auto value_iterator_ref = analyze_with_std_io(
        "range_for_iterator_rejected.cp",
        R"(import std;

struct count_iter {
    value: i32;
}

impl iterator for count_iter {
    type iter_item = i32;

    next(self&) -> optional<i32>
    {
        return optional<i32>::none;
    }
}

main()
{
    let iter = count_iter{ .value = 0 };
    for(let ref item : iter) {
    }
})"
    );
    test_parser::assert_true(has_diagnostic(value_iterator_ref, diagnostic_kind::invalid_range), "range-for should reject naked iterators");

    auto source_iterator = analyze_with_std_io(
        "range_for_source_iterator_rejected.cp",
        R"(import std;

main()
{
    for(let item : iota(0, 3).iter()) {
    }
})"
    );
    test_parser::assert_true(has_diagnostic(source_iterator, diagnostic_kind::invalid_range), "range-for should reject iter() cursors");

    auto mutable_ref_const_vector = analyze_with_std_io(
        "range_for_const_vector_mut_ref_rejected.cp",
        R"(import std;

main()
{
    let source = vector<i32>{};
    const values = source;
    for(let ref item : values) {
    }
})"
    );
    test_parser::assert_true(has_diagnostic(mutable_ref_const_vector, diagnostic_kind::invalid_assignment_target), "let ref should reject const vector iteration items");
}

auto check_sort_and_callable_semantics() -> void
{
    auto checked = analyze_with_std_io(
        "sort_semantics.cp",
        R"(import std;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(1);
    values.push_back(2);
    sort(values);
    values.sort(desc<i32>{});
    sort(span<i32>{values.data(), values.size()}, f(left: i32 const&, right: i32 const&) -> weak_ordering {
        return left <=> right;
    });

    let fixed: [i32; 3] = [3, 1, 2];
    sort(fixed);
    fixed.sort(desc<i32>{});

    let text = string{"cba"};
    sort(text);
    text.sort(desc<char>{});
    return values[0];
})"
    );
    test_parser::assert_true(checked.accepted(), "std sort should accept contiguous mutable ranges and comparators");
    test_parser::assert_true(not checked.expression_operators.empty(), "call operator expressions should record selected overloads");

    auto bool_order = analyze_with_std_io(
        "sort_bool_order_rejected.cp",
        R"(import std;

main()
{
    let values = vector<i32>{};
    sort(values, f(left: i32 const&, right: i32 const&) -> bool {
        return left < right;
    });
})"
    );
    test_parser::assert_true(
        has_diagnostic(bool_order, diagnostic_kind::missing_concept_item),
        "std sort should reject bool comparators"
    );

    auto readonly_str = analyze_with_std_io(
        "sort_str_rejected.cp",
        R"(import std;

main() -> i32
{
    let text: str = "cba";
    sort(text);
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(readonly_str, diagnostic_kind::missing_concept_item),
        "std sort should reject readonly str views"
    );

    auto raw_buffer = analyze_with_std_io(
        "sort_buffer_rejected.cp",
        R"(import std;

main() -> i32
{
    let storage = raw_buffer<i32>{3};
    sort(storage);
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(raw_buffer, diagnostic_kind::missing_concept_item),
        "std sort should reject raw storage buffers"
    );

    auto const_vector = analyze_with_std_io(
        "sort_const_vector_rejected.cp",
        R"(import std;

main() -> i32
{
    const values = vector<i32>{};
    sort(values);
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(const_vector, diagnostic_kind::type_mismatch),
        "std sort should reject const vector receivers"
    );

    auto const_array = analyze_with_std_io(
        "sort_const_array_rejected.cp",
        R"(import std;

main() -> i32
{
    const values: [i32; 3] = [3, 1, 2];
    sort(values);
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(const_array, diagnostic_kind::type_mismatch),
        "std sort should reject const array receivers"
    );

    auto const_string = analyze_with_std_io(
        "sort_const_string_rejected.cp",
        R"(import std;

main() -> i32
{
    const text = string{"cba"};
    sort(text);
    return 0;
})"
    );
    test_parser::assert_true(
        has_diagnostic(const_string, diagnostic_kind::type_mismatch),
        "std sort should reject const string receivers"
    );
}

auto check_iota_semantics() -> void
{
    auto prefix_only = analyze_with_std_io(
        "iota_prefix_only_incrementable.cp",
        R"(import std;

struct counter {
    value: i32;
}

impl counter {
    operator ==(self const&, rhs: this const&) -> bool
    {
        return value == rhs.value;
    }

    operator prefix ++(self&) -> this&
    {
        value += 1;
        return ref self;
    }
}

main() -> i32
{
    let values = iota(counter{ 0 }, counter{ 3 });
    let total = 0;
    for(let item : values) {
        total += item.value;
    }
    return total;
})"
    );
    test_parser::assert_true(prefix_only.accepted(), "std.ranges.iota should accept equality comparable prefix-only incrementable types");

    auto postfix_only = analyze_with_std_io(
        "iota_postfix_only_incrementable.cp",
        R"(import std;

struct counter {
    value: i32;
}

impl counter {
    operator ==(self const&, rhs: this const&) -> bool
    {
        return value == rhs.value;
    }

    operator postfix ++(self&) -> this
    {
        let old: counter = self;
        value += 1;
        return old;
    }
}

main()
{
    let values = iota(counter{ 0 }, counter{ 3 });
})"
    );
    test_parser::assert_true(
        has_diagnostic(postfix_only, diagnostic_kind::missing_concept_item),
        "std.ranges.iota should reject types without prefix ++");

    auto missing_equality = analyze_with_std_io(
        "iota_missing_equality.cp",
        R"(import std;

struct counter {
    value: i32;
}

impl counter {
    operator prefix ++(self&) -> this&
    {
        value += 1;
        return ref self;
    }
}

main()
{
    let values = iota(counter{ 0 }, counter{ 3 });
})"
    );
    test_parser::assert_true(
        has_diagnostic(missing_equality, diagnostic_kind::missing_concept_item),
        "std.ranges.iota should reject types without equality comparison");
}

auto check_meta_and_ranges_semantics() -> void
{
    auto meta = analyze_with_std_io(
        "std_meta_semantics.cp",
        R"(import std;

apply<F>(value: i32, callback: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}

main() -> i32
{
    type raw = read_type<i32&>;
    type plain = remove_reference<i32&>;
    type pointed = pointee<i32*>;
    type first = tuple_element<(i32, bool), 0>;
    let a: raw = 1;
    let b: plain = 2;
    let c: pointed = 3;
    let d: first = 4;
    return apply(32, f(value: i32) -> i32 { return value + a + b + c + d; });
})"
    );
    test_parser::assert_true(meta.accepted(), "std.meta type queries and callable should pass semantic analysis");

    auto direct_meta = analyze_with_std_io(
        "std_meta_direct_import_semantics.cp",
        R"(import std.meta;

main() -> i32
{
    type raw = read_type<i32&>;
    let value: raw = 1;
    return value;
})"
    );
    test_parser::assert_true(direct_meta.accepted(), "std.meta should expose type queries through a direct module import");

    auto callable_meta = analyze_with_std_io(
        "std_meta_callable_variants.cp",
        R"(import std.meta;

id(value: i32) -> i32
{
    return value;
}

struct invoker {
    offset: i32;
}

impl invoker {
    operator ()(self const&, value: i32) -> i32
    {
        return value + offset;
    }
}

main() -> i32
{
    let callback = f(value: i32) -> i32 {
        return value + 1;
    };
    let op = invoker{ 2 };
    type closure_result = call_result<decltype(callback), i32>;
    type function_result = call_result<f(i32) -> i32, i32>;
    type operator_result = call_result<invoker, i32>;
    let from_closure: closure_result = callback(3);
    let from_function: function_result = id(4);
    let from_operator: operator_result = op(5);
    return from_closure + from_function + from_operator;
})"
    );
    test_parser::assert_true(callable_meta.accepted(), "std.meta call_result should handle closures, function types, and operator ()");

    auto callable_pack_meta = analyze_with_std_io(
        "std_meta_callable_type_pack.cp",
        R"(import std.meta;

make<F, Args...>() -> call_result<F, Args...>
requires F: callable<Args...>
{
    return 42;
}

main() -> i32
{
    type zero_arg_result = call_result<f() -> i32>;
    type two_arg_result = call_result<f(i32, bool) -> i32, i32, bool>;
    let zero: zero_arg_result = make<f() -> i32>();
    let two: two_arg_result = make<f(i32, bool) -> i32, i32, bool>();
    return zero + two - 42;
})"
    );
    test_parser::assert_true(
        callable_pack_meta.accepted(),
        "std.meta call_result and callable should expand generic type parameter packs");

    auto callable_tail_pack_meta = analyze_with_std_io(
        "std_meta_callable_tail_type_pack.cp",
        R"(import std.meta;

make<F, Args...>() -> call_result<F, Args..., bool>
requires F: callable<Args..., bool>
{
    return 42;
}

main() -> i32
{
    type result = call_result<f(i32, bool) -> i32, i32, bool>;
    let value: result = make<f(i32, bool) -> i32, i32>();
    return value;
})"
    );
    test_parser::assert_true(
        callable_tail_pack_meta.accepted(),
        "std.meta call_result and callable should allow fixed type arguments after an expanded pack");

    auto generic_meta_substitution = analyze_with_std_io(
        "generic_meta_and_associated_substitution.cp",
        R"(import std.meta;

concept readable {
    type item;
    get(self const&) -> item;
}

struct box<T> {
    value: T;
}

impl<T> readable for box<T> {
    type item = T;

    get(self const&) -> T
    {
        return value;
    }
}

id(value: i32) -> i32
{
    return value;
}

read<T: readable>(value: T) -> T::item
{
    return value.get();
}

apply<F, T>(callback: F, value: T) -> call_result<F, T>
{
    return callback(value);
}

main() -> i32
{
    let item = box<i32>{ 20 };
    return read(item) + apply(id, 22);
})"
    );
    test_parser::assert_true(
        generic_meta_substitution.accepted(),
        "generic substitution should handle meta queries and associated return types");

    auto unimported_meta = analyze_with_std_io(
        "std_meta_unimported_rejected.cp",
        R"(main()
{
    type raw = read_type<i32&>;
})"
    );
    test_parser::assert_true(
        has_diagnostic(unimported_meta, diagnostic_kind::unknown_type),
        "std.meta type queries should not be available without importing std.meta or a re-exporting module"
    );

    using enum diagnostic_kind;
    auto expect_std_diagnostic = [](std::string_view name, std::string text, diagnostic_kind kind) {
        auto result = analyze_with_std_io(name, std::move(text));
        test_parser::assert_true(
            has_diagnostic(result, kind),
            std::format("{} should report {}", name, spec(kind).code));
    };
    expect_std_diagnostic("meta_read_type_requires_type.cp", "import std.meta; main() { type bad = read_type<1>; }", invalid_type_argument);
    expect_std_diagnostic("meta_read_type_arity.cp", "import std.meta; main() { type bad = read_type<i32, bool>; }", invalid_type_argument);
    expect_std_diagnostic("meta_pointee_requires_pointer.cp", "import std.meta; main() { type bad = pointee<i32>; }", invalid_type_argument);
    expect_std_diagnostic("meta_tuple_element_requires_tuple.cp", "import std.meta; main() { type bad = tuple_element<i32, 0>; }", invalid_type_argument);
    expect_std_diagnostic("meta_tuple_element_bounds.cp", "import std.meta; main() { type bad = tuple_element<(i32, bool), 2>; }", invalid_type_argument);
    expect_std_diagnostic("meta_call_result_requires_callable.cp", "import std.meta; main() { type bad = call_result<i32>; }", not_callable);
    expect_std_diagnostic("meta_call_result_arity.cp", "import std.meta; main() { type bad = call_result<f(i32) -> i32>; }", argument_count_mismatch);
    expect_std_diagnostic("meta_call_result_argument_type.cp", "import std.meta; main() { type bad = call_result<f(i32) -> i32, bool>; }", type_mismatch);
    expect_std_diagnostic(
        "meta_callable_type_pack_argument_type.cp",
        R"(import std.meta;

accept<F, Args...>()
requires F: callable<Args...>
{
}

main()
{
    accept<f(i32) -> i32, bool>();
})",
        missing_concept_item
    );

    auto ranges = analyze_with_std_io(
        "std_ranges_pipeline_semantics.cp",
        R"(import std;

make_values() -> vector<i32>
{
    let values = vector<i32>{};
    values.push_back(1);
    values.push_back(2);
    return move values;
}

main() -> i32
{
    let values: [i32; 3] = [1, 2, 3];
    let count = iota(0, 8)
        .filter(f(value: i32) -> bool { return value != 3; })
        .transform(f(value: i32) -> i32 { return value + 1; })
        .take(4 as usize)
        .count();
    let repeated = repeat(1).take(5 as usize).count();
    let indexed = repeat(0).enumerate().take(3 as usize).count();
    let borrowed = values.filter(f(value: i32&) -> bool { return value > 1; }).count();
    const fixed = values;
    let const_borrowed = fixed.enumerate().count();
    let owned = make_values().enumerate().count();
    let direct = iota(0, 3);
    let direct_count = direct.count();
    return count as i32 + repeated as i32 + indexed as i32 + borrowed as i32 + const_borrowed as i32 + owned as i32 + direct_count as i32;
})"
    );
    test_parser::assert_true(ranges.accepted(), "std.ranges adapters and terminals should compose through UFCS");

    auto full_surface = analyze_with_std_io(
        "std_ranges_full_surface_semantics.cp",
        R"(import std;

first_two<R>(source: R forward&)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return take_view<V>{ .source = move view, .count = 2 as usize };
}

main() -> i32
{
    let empty_count = empty<i32>().count();
    let single_count = single(1).count();
    let dropped = iota(0, 5).drop(2 as usize).count();
    let zipped = iota(0, 3).zip(iota(3, 6)).count();
    let concatenated = iota(0, 2).concat(iota(2, 4)).count();
    let any_ok = iota(0, 5).any(f(value: i32) -> bool { return value == 3; });
    let all_ok = iota(0, 3).all_of(f(value: i32) -> bool { return value < 3; });
    let found = iota(0, 5).find(f(value: i32) -> bool { return value == 4; });
    let transformed = iota(0, 4)
        .transform(f(value: i32) -> bool { return value == 2; })
        .any(f(value: bool) -> bool { return value; });
    let custom = iota(0, 4).first_two().count();
    if(any_ok and all_ok and found.has_value() and transformed) {
        return empty_count as i32 + single_count as i32 + dropped as i32 + zipped as i32 + concatenated as i32 + custom as i32;
    }
    return 1;
})"
    );
    test_parser::assert_true(full_surface.accepted(), "std.ranges first-batch sources, adapters, and terminals should pass semantic analysis");

    auto repeat_count = analyze_with_std_io("repeat_count_rejected.cp", "import std; main() { let values = repeat(1, 5); }");
    test_parser::assert_true(
        has_diagnostic(repeat_count, diagnostic_kind::argument_count_mismatch),
        "std.ranges should express finite repeat with repeat(value).take(count)"
    );

    auto iota_end = analyze_with_std_io("iota_end_rejected.cp", "import std; main() { let values = iota(5); }");
    test_parser::assert_true(
        has_diagnostic(iota_end, diagnostic_kind::argument_count_mismatch),
        "std.ranges should keep iota as the two-argument half-open source"
    );

    auto to_vector = analyze_with_std_io("to_vector_rejected.cp", "import std; main() { let values = iota(0, 3).to_vector(); }");
    test_parser::assert_true(not to_vector.accepted(), "std.ranges should not expose to_vector terminal");

    auto to_container = analyze_with_std_io("to_container_rejected.cp", "import std; main() { let values = iota(0, 3).to<vector>(); }");
    test_parser::assert_true(not to_container.accepted(), "std.ranges should not promise to<Container>() before CTAD/type-constructor inference");
}

auto check_ordered_collections_semantics() -> void
{
    auto checked = analyze_with_std_io(
        "ordered_collections_semantics.cp",
        R"(import std;

main() -> i32
{
    let values = map<i32, i32>{};
    let first = values.insert(3, 30);
    let second = values.insert(1, 10);
    let duplicate = values.insert(3, 99);
    values[2] = 20;
    let found = values.find(1);
    *found = *found + 1;

    let keys = set<i32>{};
    let a = keys.insert(4);
    let b = keys.insert(2);
    let c = keys.insert(4);

    values.nth(1 as usize).value = values.nth(1 as usize).value + 1;

    if(first.inserted and second.inserted and not duplicate.inserted and a.inserted and b.inserted and not c.inserted) {
        return values.nth(0 as usize).key + values.nth(1 as usize).value + values.at(3) + keys.nth(0 as usize) + keys.rank(4) as i32;
    }
    return 0;
})"
    );
    test_parser::assert_true(checked.accepted(), "std map/set should type-check insert/find/at/nth/rank/operator[]");
}

auto check_error_handling_semantics() -> void
{
    auto checked = analyze_with_std_io(
        "error_handling_semantics.cp",
        R"(import std;

fail() -> !
{
    panic("bad");
}

main() -> i32
{
    let item = optional<i32>::none;
    let value: i32 = match item {
        .some(actual) => actual,
        .none => panic("none"),
    };
    assert(true);
    return value;
})"
    );
    test_parser::assert_true(checked.accepted(), "panic, assert, and ! should pass semantic analysis");

    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "never_return.cp",
        R"(fail() -> !
{
    return panic("bad");
})");
    auto never_checked = analyze_single(sources, parsed);
    test_parser::assert_true(never_checked.accepted(), "returning a never expression should satisfy -> !");
    test_parser::assert_true(
        function_return_type(sources, parsed, never_checked, "fail") == semantic_type_ids::never,
        "explicit ! return type should lower to semantic never type");

    auto dereference_checked = analyze_with_std_io(
        "optional_expected_dereference.cp",
        R"(import std;

main() -> i32
{
    let item = optional<i32>::some(3);
    let result = expected<i32,str>::value(4);
    *item = 20;
    *result = 22;
    return *item + *result;
})"
    );
    test_parser::assert_true(dereference_checked.accepted(), "optional and expected dereference should type-check through panic arms");

    expect_diagnostic("bad_never_value.cp", "main() { let value: ! = 1; }", diagnostic_kind::type_mismatch);
    expect_diagnostic("bad_never_return_value.cp", "bad() -> ! { return 1; }", diagnostic_kind::return_type_mismatch);
    expect_diagnostic("bad_never_return_unit.cp", "bad() -> ! { return; }", diagnostic_kind::return_type_mismatch);
    expect_diagnostic("bad_never_fallthrough.cp", "bad() -> ! { }", diagnostic_kind::return_type_mismatch);
    expect_diagnostic("bad_assert_condition.cp", "main() { assert(1); }", diagnostic_kind::type_mismatch);
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

auto check_static_local_semantics() -> void
{
    auto accepted = analyze_one(
        "static_local_ok.cp",
        R"(main() -> i32
{
    let static counter = 0;
    counter += 1;
    return counter;
})"
    );
    test_parser::assert_true(accepted.accepted(), "static local should pass semantic analysis");
    test_parser::assert_true(has_static_local(accepted, "counter"), "static local should be recorded on its symbol");

    expect_diagnostic(
        "const_static_assign.cp",
        "main() { const static answer = 1; answer = 2; }",
        diagnostic_kind::assign_to_const
    );
    expect_diagnostic(
        "static_ref_rejected.cp",
        "main() { let value = 1; let static ref alias = value; }",
        diagnostic_kind::invalid_assignment_target
    );
    expect_diagnostic(
        "static_destructure_rejected.cp",
        "main() { let static (left, right) = (1, 2); }",
        diagnostic_kind::invalid_assignment_target
    );
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

auto check_first_argument_ufcs_semantics() -> void
{
    auto checked = analyze_one(
        "first_argument_ufcs.cp",
        R"(struct point {
    value: i32;
}

impl point {
    add(self const&, rhs: i32) -> i32
    {
        return value + rhs;
    }
}

main() -> i32
{
    let item = point{ .value = 40 };
    return add(item, 2);
})"
    );
    test_parser::assert_true(checked.accepted(), "ordinary calls should fall back to first-argument UFCS methods");
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
    test_parser::assert_true(types.intern(never_type{}) == semantic_type_ids::never, "never intern should return its fixed id");
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
            "never_inferred_return.cp",
            R"(fail()
{
    return panic("bad");
}

main() -> i32
{
    if(false) {
        return fail();
    }
    return 0;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "all-never return should pass inference");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "fail") == semantic_type_ids::never,
            "all-never return should infer never");
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

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "control_flow_inferred_returns.cp",
            R"(while_return(flag: bool)
{
    while(flag) {
        return 1;
    }
    return 0;
}

do_return(flag: bool)
{
    do {
        return 2;
    } while(flag);
    return 0;
}

for_value_return()
{
    for(let value : [3, 4]) {
        return value;
    }
    return 0;
}

for_ref_return()
{
    let values = [5, 6];
    for(let ref value : values) {
        return value + 0;
    }
    return 0;
}

if_else_return(flag: bool)
{
    if(flag) {
        return 7;
    } else {
        return 8;
    }
}

main() -> i32
{
    return while_return(false) + do_return(false) + for_value_return() + for_ref_return() + if_else_return(true);
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "control-flow inferred returns should pass");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "while_return") == semantic_type_ids::i32,
            "while return should infer i32");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "do_return") == semantic_type_ids::i32,
            "do-while return should infer i32");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "for_value_return") == semantic_type_ids::i32,
            "for value return should infer i32");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "for_ref_return") == semantic_type_ids::i32,
            "for ref return should infer i32");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "if_else_return") == semantic_type_ids::i32,
            "if/else return should infer i32");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "new_and_never_match_inferred_returns.cp",
            R"(variant optional<T> {
    none;
    some(T);
}

struct point {
    value: i32;
}

make_point()
{
    return new point{ .value = 42 };
}

pick(value: optional<i32>)
{
    return match value {
        .some(item) => item,
        .none => panic("missing value"),
    };
}

main() -> i32
{
    let pointer = make_point();
    let value = pick(optional<i32>::some(42));
    delete pointer;
    return value;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "new expression and never-arm match inferred returns should pass");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "pick") == semantic_type_ids::i32,
            "match with a never arm should infer the non-never arm type");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "all_never_match_inferred_return.cp",
            R"(variant abort {
    stop;
    again;
}

fail(value: abort)
{
    return match value {
        .stop => panic("stop"),
        .again => panic("again"),
    };
}

main() -> i32
{
    if(false) {
        return fail(abort::stop);
    }
    return 0;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "match expression with only never arms should pass inference");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "fail") == semantic_type_ids::never,
            "match expression with only never arms should infer never");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "generic_variant_match_payload_inferred_returns.cp",
            R"(variant choice<T> {
    none;
    pair(T, i32);
}

make_choice<T>(value: T)
{
    return choice<T>::pair(value, 1);
}

extract<T>(value: T)
{
    return match make_choice(value) {
        .pair(item, tag) => item,
        .none => value,
    };
}

main() -> i32
{
    return extract(42);
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(
            checked.accepted(),
            "generic variant payload bindings should feed inferred return types");
    }

    {
        auto sources = source_manager{};
        auto parsed = parse_source(
            sources,
            "template_if_and_associated_call_inferred_returns.cp",
            R"(struct box {
    value: i32;
}

impl box {
    make(value: i32) -> box
    {
        return box{ value };
    }
}

choose()
{
    template if(not false and (true or false)) {
        return 20;
    } else {
        return 0;
    }
}

factory()
{
    return box::make(22);
}

main() -> i32
{
    return choose() + factory().value;
})");
        auto checked = analyze_single(sources, parsed);
        test_parser::assert_true(checked.accepted(), "template-if expressions and associated calls should infer returns");
        test_parser::assert_true(
            function_return_type(sources, parsed, checked, "choose") == semantic_type_ids::i32,
            "constexpr template-if expression should infer i32");
    }
}

auto check_nrvo_and_direct_initializer_metadata() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "nrvo_metadata.cp",
        R"(struct item {
    value: i32;
}

make() -> item
{
    let result = item{ .value = 42 };
    return result;
}

make_grouped() -> item
{
    let result = item{ .value = 42 };
    return (result);
}

make_moved() -> item
{
    let result = item{ .value = 42 };
    return move result;
}

make_field() -> i32
{
    let result = item{ .value = 42 };
    return result.value;
}

choose(flag: bool) -> item
{
    let left = item{ .value = 1 };
    let right = item{ .value = 2 };
    if(flag) {
        return left;
    }
    return right;
}

id(value: item) -> item
{
    return value;
}

main() -> i32
{
    let from_call = make();
    let values = [1, 2, 3];
    let tuple = (1, 2);
    let aggregate = item{ .value = 42 };
    return from_call.value + values[0] + tuple.0 + aggregate.value - 44;
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "NRVO metadata source should pass semantic analysis");

    auto make = function_named(sources, parsed, "make");
    auto grouped = function_named(sources, parsed, "make_grouped");
    auto moved = function_named(sources, parsed, "make_moved");
    auto field = function_named(sources, parsed, "make_field");
    auto choose = function_named(sources, parsed, "choose");
    auto id = function_named(sources, parsed, "id");
    test_parser::assert_true(checked.nrvo_candidate_of(make).valid(), "return local should record an NRVO candidate");
    test_parser::assert_true(checked.nrvo_candidate_of(grouped).valid(), "return (local) should record an NRVO candidate");
    test_parser::assert_true(not checked.nrvo_candidate_of(moved).valid(), "return move local should not record NRVO");
    test_parser::assert_true(not checked.nrvo_candidate_of(field).valid(), "return local.field should not record NRVO");
    test_parser::assert_true(not checked.nrvo_candidate_of(choose).valid(), "different local returns should not record NRVO");
    test_parser::assert_true(not checked.nrvo_candidate_of(id).valid(), "parameter returns should not record NRVO");

    auto const& main_body = as<block_statement_syntax>(parsed.ast.node(parsed.ast.node(function_named(sources, parsed, "main")).body));
    for(auto index : std::views::iota(0uz, 4uz)) {
        test_parser::assert_true(
            checked.direct_initializer_of(main_body.statements[index]),
            "call and aggregate declarations should record direct initialization");
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

    {
        auto sources = source_manager{};
        auto units = std::array {
            parse_source(sources, "math.cp", "export module math;\nadd(x: i32, y: i32) -> i32 { return x + y; }"),
            parse_source(sources, "main.cp", "import math;\nmain() -> i32 { return add(1, 2); }"),
        };

        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::unexported_name),
            "imports should explain when a referenced declaration exists but is not exported");
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

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "base.cp",
                R"(export module base;

export concept marker {
}

export struct value {
    item: i32;
}

impl marker for value {
}

impl i32 {
    bump(self&) -> void
    {
        self += 1;
    }
}

export operator +(left: value const&, right: value const&) -> value
{
    return value{ left.item + right.item };
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

accept<T: marker>(item: T) -> i32
{
    return 1;
}

main() -> i32
{
    let left = value{ 20 };
    let right = value{ 21 };
    let sum = left + right;
    let counter = 0;
    counter.bump();
    return sum.item + counter + accept(left);
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(checked.accepted(), "export import should re-export types, concepts, operators, and extension methods");
        test_parser::assert_true(not checked.expression_operators.empty(), "re-exported operator should be selected");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "base.cp",
                R"(export module base;

export visible() -> i32
{
    return hidden();
}

hidden() -> i32
{
    return 42;
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
    return hidden();
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::unknown_name),
            "export import should not re-export private declarations from the imported module");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "types.cp",
                R"(export module types;

export struct item {
    value: i32;
})"),
            parse_source(
                sources,
                "main.cp",
                R"(import types;

item() -> i32
{
    return 1;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::import_conflict),
            "imported type should conflict with an existing visible function name");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "concepts.cp",
                R"(export module concepts;

export concept marker {
})"),
            parse_source(
                sources,
                "main.cp",
                R"(import concepts;

struct marker {
    value: i32;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::import_conflict),
            "imported concept should conflict with an existing visible type name");
    }

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

pick(value: optional<i32>)
{
    return match value {
        .some(item) => item,
        .none => 0,
    };
}

main() -> i32
{
    return pick(optional<i32>::some(42));
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            checked.accepted(),
            "export import should re-export generic variants for match and inferred returns");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "base.cp",
                R"(export module base;

export first_nonzero<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        if(value != 0) {
            return value;
        }
    }
    return 0;
}

export type_count<T...>()
{
    let total = 0;
    template fo)" "r" R"( (type U : T...) {
        type current = U;
        total = total + 1;
    }
    return total;
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
    return first_nonzero(0, 40) + type_count<i32, bool>();
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            checked.accepted(),
            "export import should re-export generic parameter-pack functions with inferred returns");
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

    {
        auto result = analyze_one(
            "composite_type_suffixes.cp",
            R"(read_array_pointer(value: [i32; 2]*) -> i32
{
    return (*value)[0];
}

read_array_ref(value: [i32; 2] const&) -> i32
{
    return value[1];
}

take_array_move(value: [i32; 2] move&) -> i32
{
    return value[0];
}

read_tuple_ref(value: (i32, bool) const&) -> i32
{
    return value.0;
}

take_tuple_move(value: (i32, bool) move&) -> i32
{
    return value.0;
}

read_grouped_pointer(value: (i32)*) -> i32
{
    return *value;
}

use_storage(value: storage [i32; 2]*) -> i32
{
    return 1;
}

main() -> i32
{
    let values = [20, 1];
    let pair = (2, true);
    let item = 3;
    let slots = storage [i32; 2]{};
    return read_array_pointer(&values) + read_array_ref(values) + take_array_move(move values) + read_tuple_ref(pair) + take_tuple_move(move pair) + read_grouped_pointer(&item) + use_storage(&slots) + 14;
})"
        );
        test_parser::assert_true(result.accepted(), "array, storage, tuple, and grouped type suffixes should pass semantic analysis");
    }

    {
        auto result = analyze_one(
            "mutable_aggregate_assignment.cp",
            R"(main() -> i32
{
    let values = [1, 2];
    values = [3, 4];
    let pair = (5, 6);
    pair = (7, 8);
    let slots = storage [i32; 2]{};
    slots = storage [i32; 2]{};
    return values[0] + pair.0 + 32;
})"
        );
        test_parser::assert_true(result.accepted(), "array, tuple, and storage values should be mutable assignment targets");
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
    expect_diagnostic (
        "constructor_paren_call_is_not_struct_initialization.cp",
        "struct point { x: i32; y: i32; } impl point { point(x: i32, y: i32) { return point{ .x = x, .y = y }; } } main() { let p = point(1, 2); }",
        not_callable
    );
}

auto check_struct_field_default_semantics() -> void
{
    auto result = analyze_one(
        "struct_field_defaults.cp",
        R"(struct point {
    x: i32 = 7;
    y: i32 = 11;
}

main() -> i32
{
    let empty = point{};
    let named = point{ .y = 5 };
    let positional = point{ 3 };
    return empty.x + named.y + positional.x;
})"
    );
    test_parser::assert_true(result.accepted(), "struct field default source should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic(
        "bad_struct_field_default.cp",
        "struct point { x: i32 = true; } main() { let value = point{}; }",
        type_mismatch
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

    auto default_result = analyze_one(
        "generic_struct_default_argument.cp",
        R"(struct pair<T, U = T> {
    first: T;
    second: U;
}

main() -> i32
{
    let same: pair<i32> = pair<i32>{ .first = 20, .second = 2 };
    let mixed: pair<i32, bool> = pair<i32, bool>{ .first = 1, .second = true };
    if(mixed.second) {
        return same.first + same.second + mixed.first;
    }
    return 0;
})"
    );
    test_parser::assert_true(default_result.accepted(), "generic struct default argument source should pass semantic analysis");

    auto all_default_result = analyze_one(
        "generic_struct_all_default_arguments.cp",
        R"(struct pair<T = i32, U = T> {
    first: T;
    second: U;
}

variant maybe<T = i32> {
    none;
    some(T);
}

main() -> i32
{
    let same: pair = pair{ .first = 20, .second = 1 };
    let item: maybe = maybe::some(21);
    return same.first + same.second + match item {
        .some(value) => value,
        .none => 0,
    };
})"
    );
    test_parser::assert_true(all_default_result.accepted(), "generic struct and variant default arguments should work without explicit arguments");

    auto implicit_impl_pattern_result = analyze_one(
        "implicit_impl_type_pattern_generics.cp",
        R"(struct box<T> {
    value: T;
}

impl [T; N] {
    first(self const&) -> T
    {
        return self[0];
    }
}

impl (box<T>) {
    get(self const&) -> T
    {
        return value;
    }
}

main() -> i32
{
    let values = [40, 2];
    let item = box<i32>{ 3 };
    return values.first() + item.get() - 1;
})"
    );
    test_parser::assert_true(
        implicit_impl_pattern_result.accepted(),
        "implicit impl patterns should collect array and grouped type parameters");

    auto tuple_impl_pattern_result = analyze_one(
        "tuple_impl_target_collects_implicit_parameters.cp",
        R"(impl (T, U) {
})"
    );
    test_parser::assert_true(
        has_diagnostic(tuple_impl_pattern_result, diagnostic_kind::unknown_type),
        "tuple impl target should be rejected after collecting implicit type parameters");
    test_parser::assert_true(
        not has_diagnostic(tuple_impl_pattern_result, diagnostic_kind::invalid_type_argument),
        "tuple impl target should not fail before tuple pattern parameters are bound");
    test_parser::assert_true(
        tuple_impl_pattern_result.diagnostics.size() == 1uz,
        "tuple impl target should only report the unsupported target");

    auto storage_impl_pattern_result = analyze_one(
        "storage_impl_target_collects_implicit_parameters.cp",
        R"(impl storage [T; N] {
})"
    );
    test_parser::assert_true(
        has_diagnostic(storage_impl_pattern_result, diagnostic_kind::unknown_type),
        "storage impl target should be rejected after collecting implicit type and length parameters");
    test_parser::assert_true(
        not has_diagnostic(storage_impl_pattern_result, diagnostic_kind::invalid_type_argument),
        "storage impl target should not fail before storage pattern parameters are bound");
    test_parser::assert_true(
        storage_impl_pattern_result.diagnostics.size() == 1uz,
        "storage impl target should only report the unsupported target");

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

inc(input: i32) -> i32
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

    auto default_result = analyze_one(
        "generic_function_default_argument.cp",
        R"(choose<T, U = T>(left: T, right: U) -> T
{
    return left;
}

make<N: usize = 4>() -> [i32; N]
{
    return [1, 2, 3, 4];
}

main() -> i32
{
    let values = make();
    return choose<i32>(1, 2) + choose<i32, bool>(3, true) + values[0];
})"
    );
    test_parser::assert_true(default_result.accepted(), "generic function default argument source should pass semantic analysis");

    auto inferred_parameter_result = analyze_one(
        "inferred_parameter_generic_function.cp",
        R"(read(value const&) -> i32
{
    return value;
}

borrow(value&) -> i32
{
    return value;
}

take(value move&) -> i32
{
    return value;
}

add(left, right) -> i32
{
    return left + right;
}

main() -> i32
{
    let value = 4;
    return read(const ref value) + borrow(ref value) + take(move value) + add(20, 5);
})"
    );
    test_parser::assert_true(inferred_parameter_result.accepted(), "inferred parameter generic functions should pass semantic analysis");
    test_parser::assert_true(
        inferred_parameter_result.function_instances.size() == 4,
        "inferred parameter generic calls should create concrete function instances");

    auto compound_inference_result = analyze_one(
        "compound_generic_type_inference.cp",
        R"(struct box<T> {
    value: T;
}

variant optional<T> {
    none;
    some(T);
}

id<T>(input: T) -> T
{
    return input;
}

inc(input: i32) -> i32
{
    return input;
}

array_head<T, N: usize>(values: [T; N]) -> T
{
    return values[0];
}

tuple_second<T, U>(value: (T, U)) -> U
{
    return value.1;
}

call_once<T>(callback: f(T) -> T, value: T) -> T
{
    return callback(value);
}

box_value<T>(value: box<T>) -> T
{
    return value.value;
}

optional_value<T>(value: optional<T>) -> T
{
    return match value {
        .some(item) => item,
        .none => 0,
    };
}

pointer_value<T>(value: T*) -> T
{
    return *value;
}

ref_value<T>(value: T const&) -> T
{
    return value;
}

main() -> i32
{
    let value = 7;
    let pair = (1, 2);
    return array_head([1, 2]) + tuple_second(pair) + call_once(inc, 3) + box_value(box<i32>{ 4 }) + optional_value(optional<i32>::some(5)) + pointer_value(&value) + ref_value(const ref value) + 13;
})"
    );
    test_parser::assert_true(
        compound_inference_result.accepted(),
        "generic calls should infer type arguments through arrays, tuples, functions, structs, variants, pointers, and references");

    auto generic_inferred_return = analyze_one(
        "generic_function_inferred_return.cp",
        "id<T>(input: T) { return input; } main() -> i32 { return id(1); }"
    );
    test_parser::assert_true(generic_inferred_return.accepted(), "generic functions should infer return types per concrete instance");

    auto generic_method_inferred_return = analyze_one(
        "generic_method_variant_match_inferred_return.cp",
        R"(variant optional<T> {
    none;
    some(T);
}

struct box<T> {
    value: T;
}

impl<T> box<T> {
    value_or(self const&, fallback: T)
    {
        let item = optional<T>::some(value);
        return match item {
            .some(current) => current,
            .none => fallback,
        };
    }
}

main() -> i32
{
    let item = box<i32>{ 42 };
    return item.value_or(0);
})"
    );
    test_parser::assert_true(
        generic_method_inferred_return.accepted(),
        "generic methods should infer concrete return types through variant match expressions");

    auto generic_template_if_inferred_return = analyze_one(
        "generic_template_if_inferred_return.cp",
        R"(select<T>(value: T)
{
    template if(T == i32) {
        return value;
    } else {
        return true;
    }
}

main() -> i32
{
    let ok: bool = select(true);
    if(ok) {
        return select(42);
    }
    return 0;
})"
    );
    test_parser::assert_true(
        generic_template_if_inferred_return.accepted(),
        "generic functions should infer return types from the selected template-if branch per instance");

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
        "inferred_parameter_default_value.cp",
        "touch(value = 1) -> i32 { return value; } main() -> i32 { return touch(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "inferred_parameter_explicit_type_argument.cp",
        "read(value const&) -> i32 { return value; } main() -> i32 { return read<i32>(1); }",
        invalid_type_argument
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
    template fo)" "r" R"( (const ref value : values...) {
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

    auto inferred_return_result = analyze_one(
        "parameter_pack_template_for_inferred_return.cp",
        R"(first_nonzero<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        if(value != 0) {
            return value;
        }
    }
    return 0;
}

main() -> i32
{
    return first_nonzero(0, 42);
})"
    );
    test_parser::assert_true(
        inferred_return_result.accepted(),
        "return inside template for should infer the function instance return type"
    );

    auto forward_inferred_return_result = analyze_one(
        "forward_parameter_pack_template_for_inferred_return.cp",
        R"(main() -> i32
{
    return type_count<i32, bool, i32>() * 10 + first_nonzero(0, 12);
}

type_count<T...>()
{
    let total = 0;
    template fo)" "r" R"( (type U : T...) {
        type current = U;
        total = total + 1;
    }
    return total;
}

first_nonzero<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        if(value != 0) {
            return value;
        }
    }
    return 0;
})"
    );
    test_parser::assert_true(
        forward_inferred_return_result.accepted(),
        "forward calls should infer return types for type and value parameter pack instances"
    );

    auto type_pack_return_result = analyze_one(
        "type_pack_template_for_inferred_return.cp",
        R"(first_i32<T...>()
{
    template fo)" "r" R"( (type U : T...) {
        template if(U == i32) {
            return 40;
        }
    }
    return 2;
}

main() -> i32
{
    return first_i32<bool, i32>();
})"
    );
    test_parser::assert_true(
        type_pack_return_result.accepted(),
        "return inside type-pack template for should infer the function instance return type"
    );

    auto variant_pack_return_result = analyze_one(
        "variant_parameter_pack_match_inferred_return.cp",
        R"(variant optional<T> {
    none;
    some(T);
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
    return first_some(optional<i32>::none, optional<i32>::some(42));
})"
    );
    test_parser::assert_true(
        variant_pack_return_result.accepted(),
        "variant match inside parameter-pack expansion should infer the function instance return type"
    );

    auto compound_pack_result = analyze_one(
        "compound_parameter_pack_inference.cp",
        R"(struct box<T> {
    value: T;
}

sum_arrays<T...>(values: [T; 2]...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value[0];
    }
    return total;
}

sum_tuples<T...>(values: (T, i32)...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value.1;
    }
    return total;
}

sum_boxes<T...>(values: box<T>...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value.value;
    }
    return total;
}

main() -> i32
{
    let first = (0, 3);
    let second = (0, 4);
    return sum_arrays([1, 0], [2, 0]) + sum_tuples(first, second) + sum_boxes(box<i32>{ 5 }, box<i32>{ 6 }) + 21;
})"
    );
    test_parser::assert_true(
        compound_pack_result.accepted(),
        "parameter packs should infer type arguments through arrays, tuples, and generic structs"
    );

    auto fixed_prefix_pack_result = analyze_one(
        "fixed_prefix_parameter_pack_inference.cp",
        R"(head_plus_count<Head, Tail...>(head: Head, tail: Tail...)
{
    let total = head;
    template fo)" "r" R"( (let value : tail...) {
        total = total + 1;
    }
    return total;
}

main() -> i32
{
    return head_plus_count(40, true, 2) + head_plus_count<i32>(1);
})"
    );
    test_parser::assert_true(
        fixed_prefix_pack_result.accepted(),
        "parameter packs should allow fixed generic and value parameters before the tail pack");

    auto empty_pack_fallback_return_result = analyze_one(
        "empty_parameter_pack_fallback_inferred_return.cp",
        R"(first_or<T...>(values: T...)
{
    template fo)" "r" R"( (let value : values...) {
        return value;
    }
    return 9;
}

main() -> i32
{
    return first_or();
})"
    );
    test_parser::assert_true(
        empty_pack_fallback_return_result.accepted(),
        "empty parameter-pack expansions should still infer from the fallback return");

    auto concept_pack_result = analyze_one(
        "concept_type_parameter_pack.cp",
        "concept any_of<T...> { } main() { }"
    );
    test_parser::assert_true(concept_pack_result.accepted(), "concept declarations should allow type parameter packs");

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
        "parameter_pack_struct_type_argument_expansion.cp",
        "struct box<T> { value: T; } bad<Args...>() { type item = box<Args...>; } main() { bad<i32>(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "parameter_pack_variant_type_argument_expansion.cp",
        "variant choice<T> { none; some(T); } bad<Args...>() { type item = choice<Args...>; } main() { bad<i32>(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "meta_non_variadic_query_pack_expansion.cp",
        "import std.meta; bad<Args...>() { type item = read_type<Args...>; } main() { bad<i32>(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "parameter_pack_mixed_inferred_return.cp",
        "first<T...>(values: T...) { template fo" "r (let value : values...) { return value; } } main() { let value = first(1, true); }",
        return_type_mismatch
    );
    expect_diagnostic(
        "parameter_pack_empty_inferred_return.cp",
        "first<T...>(values: T...) { template fo" "r (let value : values...) { return value; } } main() { let value = first(); }",
        type_mismatch
    );
    expect_diagnostic(
        "parameter_pack_non_final_value_pack.cp",
        "bad<T...>(values: T..., last: i32) -> i32 { return last; } main() -> i32 { return bad(1, 2); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "value_pack_without_type_pack.cp",
        "bad(values: i32...) -> i32 { return 0; } main() -> i32 { return bad(1); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "generic_pack_non_final_type_pack.cp",
        "bad<T..., U>() -> i32 { return 0; } main() -> i32 { return bad<i32, bool>(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "struct_type_parameter_pack.cp",
        "struct tuple<T...> { } main() { }",
        invalid_type_argument
    );
    expect_diagnostic(
        "variant_type_parameter_pack.cp",
        "variant choice<T...> { none; } main() { }",
        invalid_type_argument
    );
    expect_diagnostic(
        "impl_type_parameter_pack.cp",
        "struct box<T> { value: T; } impl<T...> box<i32> { } main() { }",
        invalid_type_argument
    );
    expect_diagnostic(
        "concept_impl_type_parameter_pack.cp",
        "concept mark { } struct item { } impl<T...> mark for item { } main() { }",
        invalid_type_argument
    );
    expect_diagnostic(
        "lambda_default_parameter.cp",
        "main() { let item = f(value: i32 = 1) -> i32 { return value; }; }",
        invalid_type_argument
    );
    expect_diagnostic(
        "parameter_pack_default_value.cp",
        "bad<T...>(values: T... = 1) -> i32 { return 0; } main() -> i32 { return bad<i32>(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "inferred_parameter_default_value.cp",
        "bad(value = 1) -> i32 { return value; } main() -> i32 { return bad(); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "parameter_after_default_value.cp",
        "bad(first: i32 = 1, second: i32) -> i32 { return first + second; } main() -> i32 { return bad(1, 2); }",
        invalid_type_argument
    );
    expect_diagnostic(
        "template_for_break_outer_loop.cp",
        "bad<T...>(values: T...) -> i32 { while(true) { template fo" "r (let value : values...) { break; } } return 0; } main() -> i32 { return bad(1); }",
        invalid_break
    );
}

auto check_template_if_semantics() -> void
{
    auto result = analyze_one(
        "template_if_type_selection.cp",
        R"(select<T>(value: T) -> i32
{
    template if(T == i32) {
        return 1;
    } else template if(T == bool) {
        return 2;
    } else {
        return missing_name;
    }
}

main() -> i32
{
    return select(1) + select(true);
})"
    );
    test_parser::assert_true(result.accepted(), "template if should select per-instantiation type branches");
    test_parser::assert_true(result.template_if_selections.size() == 2, "template if should record selected bodies");
    test_parser::assert_true(
        std::ranges::all_of(result.template_if_selections, [](auto const& entry) {
            return entry.second.has_body;
        }),
        "template if selections should point at a concrete body");

    auto inferred_result = analyze_one(
        "template_if_inferred_return.cp",
        "select() { template if(true) { return 1; } else { return true; } } main() -> i32 { return select(); }"
    );
    test_parser::assert_true(inferred_result.accepted(), "template if should exclude unselected branches from return inference");

    auto inferred_conditions = analyze_one(
        "template_if_condition_inferred_return.cp",
        R"(concept marker {
}

impl marker for i32 {
}

by_type()
{
    template if(i32 == i32) {
        return 1;
    } else {
        return true;
    }
}

by_concept()
{
    template if(i32: marker) {
        return 2;
    } else {
        return true;
    }
}

by_not()
{
    template if(not false) {
        return 3;
    } else {
        return true;
    }
}

by_and()
{
    template if(1 < 2 and 'a' == 'a') {
        return 4;
    } else {
        return true;
    }
}

by_or()
{
    template if(false or 4 >= 4) {
        return 5;
    } else {
        return true;
    }
}

by_else()
{
    template if(1 > 2) {
        return true;
    } else {
        return 6;
    }
}

main() -> i32
{
    return by_type() + by_concept() + by_not() + by_and() + by_or() + by_else();
})"
    );
    test_parser::assert_true(
        inferred_conditions.accepted(),
        "template if type, concept, and boolean conditions should feed return inference");

    auto inferred_expression_conditions = analyze_one(
        "template_if_expression_inferred_return.cp",
        R"(by_group()
{
    template if((not false)) {
        return 1;
    }
    return 0;
}

by_negative()
{
    template if(-1 < 0) {
        return 2;
    }
    return 0;
}

by_not_equal()
{
    template if(1 != 2) {
        return 3;
    }
    return 0;
}

by_less_equal()
{
    template if(2 <= 2) {
        return 4;
    }
    return 0;
}

by_and_short_circuit()
{
    template if(false and missing_name) {
        return 0;
    } else {
        return 5;
    }
}

by_or_short_circuit()
{
    template if(true or missing_name) {
        return 6;
    }
    return 0;
}

main() -> i32
{
    return by_group() + by_negative() + by_not_equal() + by_less_equal() + by_and_short_circuit() + by_or_short_circuit();
})"
    );
    test_parser::assert_true(
        inferred_expression_conditions.accepted(),
        "template if expression folding should feed return inference");

    using enum diagnostic_kind;
    expect_diagnostic(
        "template_if_non_bool.cp",
        "main() -> i32 { template if(1) { return 1; } return 0; }",
        condition_not_bool
    );
    expect_diagnostic(
        "template_if_non_constexpr.cp",
        "main() -> i32 { let flag = true; template if(flag) { return 1; } return 0; }",
        invalid_operator
    );
}

auto check_concept_semantics() -> void
{
    auto result = analyze_one(
        "concept_impl.cp",
        R"(concept cursor {
    type item;
    next(self&) -> item;
}

concept sized_cursor {
    requires cursor;
    remaining(self const&) -> i32;
}

struct range_iter {
    value: i32;
    remaining_value: i32;
}

impl cursor for range_iter {
    type item = i32;

    next(self&) -> i32
    {
        return value;
    }
}

impl sized_cursor for range_iter {
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

    auto generic_concept_result = analyze_one(
        "generic_concept_default_argument.cp",
        R"(concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}

struct box<T> {
    value: T;
}

struct word {
    value: i32;
}

struct text {
    value: i32;
}

impl partial_eq for i32 {
    equals(self const&, rhs: i32 const&) -> bool
    {
        return self == rhs;
    }
}

impl<T> partial_eq for box<T>
requires T: partial_eq
{
    equals(self const&, rhs: box<T> const&) -> bool
    {
        return value.equals(rhs.value);
    }
}

impl partial_eq<text> for word {
    equals(self const&, rhs: text const&) -> bool
    {
        return value == rhs.value;
    }
}

main() -> bool
{
    let left = box<i32>{ .value = 1 };
    let right = box<i32>{ .value = 1 };
    let name = word{ .value = 2 };
    let other = text{ .value = 2 };
    return left.equals(right) and name.equals(other);
})"
    );
    test_parser::assert_true(generic_concept_result.accepted(), "generic concept default argument source should pass semantic analysis");

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

    auto conditional_requires_result = analyze_one(
        "conditional_concept_impl_requires.cp",
        R"(concept mark {
}

concept parent {
}

concept child {
}

concept int_box {
}

struct box<T> {
    value: T;
}

impl mark for i32 {
}

impl parent for box<i32> {
}

impl<T> child for box<T>
requires parent
{
}

impl<T> int_box for box<T>
requires T: mark and T == i32
{
}

accept_child<T: child>(value: T) -> i32
{
    return 20;
}

accept_int_box<T: int_box>(value: T) -> i32
{
    return 22;
}

main() -> i32
{
    return accept_child(box<i32>{ 1 }) + accept_int_box(box<i32>{ 2 });
})"
    );
    test_parser::assert_true(
        conditional_requires_result.accepted(),
        "conditional concept impl requires should handle parent, bound, and equality constraints");

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
	        "match_payload_binding_count_too_few.cp",
	        "variant option { none; some(i32); } main() { let value = option::some(1); let out = match value { .some => 1, .none => 0, }; }",
	        argument_count_mismatch
	    );
	    expect_diagnostic (
	        "match_payload_binding_count_too_many.cp",
	        "variant option { none; some(i32); } main() { let value = option::some(1); let out = match value { .some(item, extra) => item, .none => 0, }; }",
	        argument_count_mismatch
	    );
	    expect_diagnostic (
	        "non_exhaustive_match.cp",
	        "variant option { none; some(i32); } main() { let value = option::none; let out = match value { .none => 0, }; }",
	        non_exhaustive_match
	    );
        expect_diagnostic (
            "duplicate_match_case.cp",
            "variant option { none; some(i32); } main() { let value = option::none; let out = match value { .none => 0, .none => 1, .some(item) => item, }; }",
            duplicate_variant_case
        );
        expect_diagnostic (
            "match_on_non_variant.cp",
            "main() { let out = match 1 { _ => 0, }; }",
            type_mismatch
        );
        expect_diagnostic (
            "match_arm_type_mismatch.cp",
            "variant option { none; some(i32); } main() { let value = option::none; let out = match value { .none => 0, .some(item) => true, }; }",
            type_mismatch
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
    expect_diagnostic(
        "conditional_parent_requires_unsatisfied.cp",
        R"(concept parent {
}

concept child {
}

struct box<T> {
    value: T;
}

impl parent for box<i32> {
}

impl<T> child for box<T>
requires parent
{
}

accept_child<T: child>(value: T) -> i32
{
    return 1;
}

main() -> i32
{
    let value = accept_child(box<bool>{ true });
    return value;
})",
        missing_concept_item
    );
    expect_diagnostic(
        "conditional_type_requires_unsatisfied.cp",
        R"(concept mark {
}

concept int_box {
}

struct box<T> {
    value: T;
}

impl mark for i32 {
}

impl<T> int_box for box<T>
requires T: mark and T == i32
{
}

accept_int_box<T: int_box>(value: T) -> i32
{
    return 1;
}

main() -> i32
{
    let value = accept_int_box(box<bool>{ true });
    return value;
})",
        missing_concept_item
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

    auto storage_parsed = parse_source(
        sources,
        "storage.cp",
        R"(struct no_default {
    value: i32&;
}

struct holder<T, N: usize> {
    items: storage [T; N];
    single: storage T;
}

main() -> i32
{
    let one = storage no_default{};
    let many = storage [i32; 4]{};
    let p = one.slot();
    let q = many.data();
    let r = many.slot(2);
    const fixed = storage [i32; 2]{};
    let readonly = fixed.data();
    return 0;
})");
    auto storage_checked = analyze_single(sources, storage_parsed);
    test_parser::assert_true(storage_checked.accepted(), "storage source should pass semantic analysis");
    auto const& storage_body = as<block_statement_syntax>(storage_parsed.ast.node(storage_parsed.ast.node(storage_parsed.root->functions.front()).body));
    auto const& one_decl = as<declaration_statement_syntax>(storage_parsed.ast.node(storage_body.statements[0]));
    auto one_type = storage_checked.type_of(0uz, one_decl.initializer);
    test_parser::assert_true(
        std::holds_alternative<storage_type>(storage_checked.types.get(one_type)),
        "storage T{} should lower to storage_type");
    auto const& slot_decl = as<declaration_statement_syntax>(storage_parsed.ast.node(storage_body.statements[2]));
    auto slot_builtin = storage_checked.builtin_call_of(0uz, slot_decl.initializer);
    test_parser::assert_true(
        slot_builtin.kind == semantic_builtin_call_kind::storage_slot,
        "storage slot() should record builtin metadata");
    auto const& data_decl = as<declaration_statement_syntax>(storage_parsed.ast.node(storage_body.statements[3]));
    auto data_builtin = storage_checked.builtin_call_of(0uz, data_decl.initializer);
    test_parser::assert_true(
        data_builtin.kind == semantic_builtin_call_kind::storage_slot and data_builtin.type == semantic_type_ids::i32,
        "storage data() should expose the storage element pointer");
    auto const& readonly_decl = as<declaration_statement_syntax>(storage_parsed.ast.node(storage_body.statements[6]));
    auto readonly_type = storage_checked.type_of(0uz, readonly_decl.initializer);
    auto const* readonly_pointer = std::get_if<pointer_type>(&storage_checked.types.get(readonly_type));
    test_parser::assert_true(
        readonly_pointer != nullptr and readonly_pointer->is_const,
        "const storage data() should expose a const element pointer");

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
        "extern_c_inferred_return.cp",
        "extern \"C\" text() { return \"abc\"; }",
        invalid_type_argument
    );
    expect_diagnostic(
        "extern_c_generic.cp",
        "extern \"C\" id<T>(value: T) -> T;",
        invalid_type_argument
    );
}

auto check_enum_opaque_and_fs_semantics() -> void
{
    auto enum_values = analyze_one(
        "enum_constant_values.cp",
        R"(enum flags : i32 {
    add = 1 + 2;
    sub = (8 - 3);
    mul = 2 * 3;
    div = 8 / 2;
    rem = 9 % 4;
    neg = -1;
    inv = ~0;
    bit = 1 | 2 & 3 ^ 4;
    shl = 1 << 3;
    shr = 8 >> 1;
}

type raw_flags = i32;

main() -> i32
{
    let value: raw_flags = flags::add as i32;
    return value;
})"
    );
    test_parser::assert_true(enum_values.accepted(), "enum constant integer expressions should pass semantic analysis");

    auto accepted = analyze_with_std_io(
        "enum_opaque_fs_ok.cp",
        R"(import std;

main() -> i32
{
    let flag = open_flag::read;
    let bits: u8 = flag as u8;
    let options = open_options{}.write().create().truncate();
    if(bits == 1 and options.bits() == 14) {
        return 0;
    }
    return 1;
})"
    );
    test_parser::assert_true(accepted.accepted(), "enum, opaque open_options, and std.fs should pass semantics");

    expect_diagnostic(
        "enum_implicit_int.cp",
        "enum open_flag : u8 { read = 1; } main() { let bits: u8 = open_flag::read; }",
        diagnostic_kind::type_mismatch
    );
    expect_diagnostic(
        "duplicate_enum_case.cp",
        "enum marker : u8 { first = 1; first = 2; }",
        diagnostic_kind::duplicate_variant_case
    );
    expect_diagnostic(
        "enum_non_integer_case_value.cp",
        "enum marker : u8 { first = true; }",
        diagnostic_kind::invalid_type_argument
    );
    expect_diagnostic(
        "enum_bad_underlying_type.cp",
        "enum marker : bool { first = 1; }",
        diagnostic_kind::invalid_type_argument
    );
    expect_diagnostic(
        "enum_bitwise.cp",
        "enum open_flag : u8 { read = 1; write = 2; } main() { let bits = open_flag::read | open_flag::write; }",
        diagnostic_kind::invalid_operator
    );
    expect_diagnostic(
        "opaque_implicit_raw.cp",
        "type handle = opaque u8*; main() { let h: handle = (nullptr as u8*) as handle; let raw: u8* = h; }",
        diagnostic_kind::type_mismatch
    );
    expect_diagnostic(
        "opaque_pointer_index.cp",
        "type handle = opaque u8*; main() { let h: handle = (nullptr as u8*) as handle; let item = h[0]; }",
        diagnostic_kind::invalid_operator
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

auto check_forward_reference_semantics() -> void
{
    auto checked = analyze_one(
        "forward_reference.cp",
        R"(sink_ref(value: i32&) -> i32
{
    return value;
}

sink_move(value: i32 move&) -> i32
{
    return value;
}

relay_ref<T>(value: T forward&) -> i32
{
    return sink_ref(forward value);
}

relay_move<T>(value: T forward&) -> i32
{
    return sink_move(forward value);
}

read(value: i32 const&) -> i32
{
    return value;
}

relay_read<T>(value: T forward&) -> i32
{
    return read(forward value);
}

copy_value(value: i32) -> i32
{
    return value;
}

main() -> i32
{
    let first = 1;
    relay_ref(first);
    relay_move(2);
    let second = 3;
    relay_move(move second);
    const third = 4;
    relay_read(third);
    read(const first);
    copy_value(const first);
    return 0;
})"
    );
    test_parser::assert_true(checked.accepted(), "forward references and const expressions should pass semantic analysis");

    using enum diagnostic_kind;
    expect_diagnostic("forward_non_parameter.cp", "main() { let value = 1; let other = forward value; }", invalid_operator);
    expect_diagnostic(
        "forward_const_lvalue_to_mutable_rejected.cp",
        "take(value: i32&) -> void { } relay<T>(value: T forward&) -> void { take(forward value); } main() { const value = 1; relay(value); }",
        type_mismatch
    );
    expect_diagnostic("const_move_operand.cp", "main() { let value = 1; let other = const (move value); }", invalid_assignment_target);
    expect_diagnostic("move_const_operand.cp", "main() { let value = 1; let other = move (const value); }", invalid_assignment_target);
    expect_diagnostic("concrete_forward_ref.cp", "take(value: i32 forward&) { } main() { let value = 1; take(value); }", invalid_type_argument);
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
        "deleted_constructor_blocks_aggregate_fallback.cp",
        "struct box { value: i32; } impl box { box(value: i32) = delete; } main() { let item = box{ 1 }; }",
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

auto check_module_diagnostic_semantics() -> void
{
    using enum diagnostic_kind;
    expect_diagnostic("export_import_without_module.cp", "export import math;", export_requires_module);
    expect_diagnostic("export_struct_without_module.cp", "export struct item { value: i32; }", export_requires_module);
    expect_diagnostic("export_enum_without_module.cp", "export enum marker : u8 { first = 1; }", export_requires_module);
    expect_diagnostic("export_type_without_module.cp", "export type value = i32;", export_requires_module);
    expect_diagnostic("export_operator_without_module.cp", "export operator +(left: i32, right: i32) -> i32 { return left + right; }", export_requires_module);
    expect_diagnostic("duplicate_top_level_function.cp", "value() -> i32 { return 1; } value() -> i32 { return 2; }", duplicate_symbol);
    expect_diagnostic("duplicate_top_level_struct.cp", "struct item { value: i32; } struct item { other: i32; }", duplicate_symbol);
    expect_diagnostic("duplicate_top_level_enum.cp", "enum item : u8 { first = 1; } enum item : u8 { second = 2; }", duplicate_symbol);
    expect_diagnostic("duplicate_top_level_type_alias.cp", "type item = i32; type item = bool;", duplicate_symbol);
    expect_diagnostic("type_alias_requires_value.cp", "type item;", expected_type);
    expect_diagnostic("unknown_parent_concept.cp", "concept cursor { requires missing; }", unknown_concept);
    expect_diagnostic("unknown_bound_concept.cp", "concept cursor { requires T: missing; }", unknown_concept);
    expect_diagnostic("duplicate_concept_type_item.cp", "concept cursor { type item; type item; }", duplicate_symbol);
    expect_diagnostic("duplicate_concept_function_item.cp", "concept cursor { next(self&) -> i32; next(self&) -> i32; }", duplicate_symbol);
    expect_diagnostic("concept_self_parameter_must_be_receiver.cp", "concept cursor { next(self: i32) -> i32; }", invalid_self_parameter);
    expect_diagnostic("concept_parameter_requires_type.cp", "concept cursor { next(self&, value) -> i32; }", invalid_type_argument);
    expect_diagnostic("invalid_impl_target.cp", "impl i32* { value(self const&) -> i32 { return 0; } }", unknown_type);
    expect_diagnostic("duplicate_impl_associated_type.cp", "struct item { value: i32; } impl item { type value = i32; type value = bool; }", duplicate_symbol);
    expect_diagnostic("impl_type_alias_requires_value.cp", "struct item { value: i32; } impl item { type value; }", expected_type);
    expect_diagnostic("default_constructor_takes_no_parameters.cp", "struct item { value: i32; } impl item { item(value: i32) = default; }", invalid_constructor);
    expect_diagnostic("constructor_cannot_return_type.cp", "struct item { value: i32; } impl item { item() -> item { return item{ 1 }; } }", invalid_constructor);
    expect_diagnostic("destructor_name_must_match.cp", "struct item { value: i32; } impl item { ~other() { } }", invalid_destructor);
    expect_diagnostic("duplicate_destructor.cp", "struct item { value: i32; } impl item { ~item() { } ~item() { } }", duplicate_symbol);
    expect_diagnostic("member_self_must_be_receiver.cp", "struct item { value: i32; } impl item { get(self: item) -> i32 { return value; } }", invalid_self_parameter);
    expect_diagnostic("member_self_target_mismatch.cp", "struct item { value: i32; } struct other { value: i32; } impl item { get(self: other const&) -> i32 { return 0; } }", invalid_self_parameter);
    expect_diagnostic("duplicate_associated_function.cp", "struct item { value: i32; } impl item { make() -> item { return item{ 1 }; } make() -> item { return item{ 2 }; } }", duplicate_symbol);
    expect_diagnostic("array_impl_requires_self_receiver.cp", "impl [i32; 2] { size() -> i32 { return 2; } }", invalid_self_parameter);
    expect_diagnostic("concept_impl_unknown_target.cp", "concept marker { } impl marker for i32* { }", unknown_type);
    expect_diagnostic("concept_impl_duplicate_associated_type.cp", "concept cursor { type item; } struct range { value: i32; } impl cursor for range { type item = i32; type item = bool; }", duplicate_symbol);
    expect_diagnostic("concept_impl_associated_type_requires_value.cp", "concept cursor { type item; } struct range { value: i32; } impl cursor for range { type item; }", expected_type);
    expect_diagnostic("concept_impl_self_must_be_receiver.cp", "concept cursor { next(self&) -> i32; } struct range { value: i32; } impl cursor for range { next(self: range) -> i32 { return 0; } }", invalid_self_parameter);
    expect_diagnostic("concept_impl_self_target_mismatch.cp", "concept cursor { next(self&) -> i32; } struct range { value: i32; } struct other { value: i32; } impl cursor for range { next(self: other const&) -> i32 { return 0; } }", invalid_self_parameter);
}

auto check_default_compare_semantics() -> void
{
    auto checked = analyze_with_std_io(
        "default_compare.cp",
        R"(import std;

enum mark : u8 {
    first = 1;
    second = 2;
}

struct item {
    production: usize;
    dot: usize;
    lookahead: mark;
}

impl item {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}

accepts_three_way<T: three_way_comparable<T, weak_ordering>>(value: T) -> T
{
    return value;
}

main()
{
    let one = accepts_three_way<mark>(mark::first);
    let two = accepts_three_way<item>(item{ .production = 1 as usize, .dot = 0 as usize, .lookahead = mark::second });
    let relation = one <=> mark::second;
    let item_relation = two <=> two;
})"
    );
    test_parser::assert_true(checked.accepted(), "defaulted comparison should pass semantic analysis");
    auto compare_symbol = symbol_id{};
    for(auto index = 0uz; index < checked.symbols.size(); ++index) {
        auto const& symbol = checked.symbols[index];
        if(symbol.body_kind == semantic_function_body_kind::defaulted_compare) {
            compare_symbol = symbol_id{ static_cast<std::uint32_t>(index) };
            break;
        }
    }
    test_parser::assert_true(compare_symbol.valid(), "defaulted comparison should be classified separately");
    auto found = checked.default_compare_fields.find(compare_symbol);
    test_parser::assert_true(found != checked.default_compare_fields.end(), "defaulted comparison should record fields");
    test_parser::assert_true(found->second.size() == 3uz, "defaulted comparison should record each struct field");
    test_parser::assert_true(found->second[2].enum_builtin, "enum fields should use builtin enum comparison");

    auto bad_field = analyze_with_std_io(
        "bad_default_compare_field.cp",
        R"(import std;

struct payload {
    value: i32;
}

struct item {
    payload: payload;
}

impl item {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
})"
    );
    test_parser::assert_true(has_diagnostic(bad_field, diagnostic_kind::invalid_operator), "field without <=> should reject defaulted comparison");

    auto bad_rhs = analyze_with_std_io(
        "bad_default_compare_rhs.cp",
        R"(import std;

struct item {
    value: i32;
}

impl item {
    operator <=>(self const&, rhs: i32 const&) -> weak_ordering = default;
})"
    );
    test_parser::assert_true(has_diagnostic(bad_rhs, diagnostic_kind::invalid_operator), "rhs must be this const&");

    auto bad_return = analyze_with_std_io(
        "bad_default_compare_return.cp",
        R"(import std;

struct item {
    value: i32;
}

impl item {
    operator <=>(self const&, rhs: this const&) -> i32 = default;
})"
    );
    test_parser::assert_true(has_diagnostic(bad_return, diagnostic_kind::invalid_operator), "return type must be weak_ordering");

    auto bad_target = analyze_with_std_io(
        "bad_default_compare_target.cp",
        R"(import std;

variant optional {
    none;
}

impl optional {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
})"
    );
    test_parser::assert_true(has_diagnostic(bad_target, diagnostic_kind::invalid_operator), "non-struct target should reject defaulted comparison");

    auto bad_generic = analyze_with_std_io(
        "bad_default_compare_generic.cp",
        R"(import std;

struct box<T> {
    value: T;
}

impl<T> box<T> {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
})"
    );
    test_parser::assert_true(has_diagnostic(bad_generic, diagnostic_kind::invalid_operator), "generic struct target should reject defaulted comparison");

    auto implicit_struct_compare = analyze_with_std_io(
        "implicit_struct_compare_rejected.cp",
        R"(import std;

struct item {
    value: i32;
}

main()
{
    let left = item{ 1 };
    let right = item{ 2 };
    let relation = left <=> right;
})"
    );
    test_parser::assert_true(has_diagnostic(implicit_struct_compare, diagnostic_kind::invalid_operator), "structs should not get implicit comparison");

    auto different_enum_compare = analyze_with_std_io(
        "different_enum_compare_rejected.cp",
        R"(import std;

enum left_mark : u8 {
    value = 1;
}

enum right_mark : u8 {
    value = 1;
}

main()
{
    let relation = left_mark::value <=> right_mark::value;
})"
    );
    test_parser::assert_true(has_diagnostic(different_enum_compare, diagnostic_kind::invalid_operator), "different enum types should not compare");
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

auto check_lambda_capture_modes() -> void
{
    auto sources = source_manager{};
    auto parsed = parse_source(
        sources,
        "lambda_capture_modes.cp",
        R"(make_read()
{
    let snapshot = 10;
    return f() {
        snapshot
    };
}

make_write()
{
    let owned = 0;
    return f() {
        owned = owned + 1;
        owned
    };
}

make_pair()
{
    let pair_count = 0;
    let inc = f() {
        pair_count = pair_count + 1;
        pair_count
    };
    let get = f() {
        pair_count
    };
    return (inc, get);
}

main() -> i32
{
    let readonly = 1;
    let read = f() {
        readonly
    };
    let local_mut = 0;
    let write = f() {
        local_mut = local_mut + 1;
        local_mut
    };
    return read() + write();
})");
    auto checked = analyze_single(sources, parsed);
    test_parser::assert_true(checked.accepted(), "lambda capture mode source should pass semantic analysis");
    test_parser::assert_true(
        has_capture_mode(checked, "readonly", semantic_lambda_capture_mode::const_ref),
        "non-escaping read-only capture should be const_ref");
    test_parser::assert_true(
        has_capture_mode(checked, "local_mut", semantic_lambda_capture_mode::ref),
        "non-escaping mutable capture should be ref");
    test_parser::assert_true(
        has_capture_mode(checked, "snapshot", semantic_lambda_capture_mode::copy),
        "escaping read-only capture should be copy");
    test_parser::assert_true(
        has_capture_mode(checked, "owned", semantic_lambda_capture_mode::owned_mut_copy),
        "escaping mutable capture should be owned_mut_copy");
    test_parser::assert_true(
        has_capture_mode(checked, "pair_count", semantic_lambda_capture_mode::copy),
        "second escaping closure should copy the shared source independently");
    test_parser::assert_true(
        has_capture_mode(checked, "pair_count", semantic_lambda_capture_mode::owned_mut_copy),
        "mutating escaping closure should own an independent mutable copy");
    test_parser::assert_true(
        has_diagnostic(checked, diagnostic_kind::independent_closure_capture),
        "multiple escaping captures with mutation should produce a warning");
}

auto check_complex_lambda_capture_semantics() -> void
{
    auto checked = analyze_one(
        "lambda_complex_capture_walk.cp",
        R"(variant optional {
    none;
    some(i32);
}

struct point {
    value: i32;
}

make_complex()
{
    let bias = 1;
    let item = point{ .value = 2 };
    return f(flag: bool) -> i32 {
        let total = bias;
        let values = [1, 2];
        let pair = (values[0], item.value);
        let pointer = new point{ .value = 3 };
        delete pointer;
        if(flag) {
            total = total + pair.0;
        } else {
            total = total + pair.1;
        }
        while(false) {
            total = total + 1;
            break;
        }
        do {
            total = total + 1;
        } while(false);
        for(let value : values) {
            total = total + value;
            continue;
        }
        let current = match optional::some(total) {
            .some(actual) => actual,
            .none => 0,
        };
        let nested = {
            let inner = current;
            inner
        };
        total + nested
    };
}

main() -> i32
{
    let callback = make_complex();
    return callback(true);
})"
    );
    test_parser::assert_true(checked.accepted(), "complex escaping lambda capture walk should pass semantic analysis");
    test_parser::assert_true(
        has_capture_mode(checked, "bias", semantic_lambda_capture_mode::copy),
        "escaping lambda should copy read-only captured values");
    test_parser::assert_true(
        has_capture_mode(checked, "item", semantic_lambda_capture_mode::copy),
        "escaping lambda should copy captured aggregate values");

    auto generic_checked = analyze_one(
        "generic_lambda_capture_walk.cp",
        R"(variant optional {
    none;
    some(i32);
}

struct point {
    value: i32;
}

use(value: i32) -> i32
{
    return value;
}

main()
{
    let seed = 1;
    let holder = point{ .value = 2 };
    let values = [1, 2, 3];
    let index = 0;
    let flag = true;
    let callback = f<T...>(items: T...) -> i32 {
        let local = seed;
        let pair = (local, holder.value);
        let array = [pair.0, values[index]];
        let pointer = new point{ .value = array[0] };
        delete pointer;
        local = local + (seed as i32);
        use(local);
        if(flag) {
            local = local + pair.1;
        } else {
            local = local + values[1];
        }
        while(false) {
            local = local + 1;
            break;
        }
        do {
            local = local + 1;
        } while(false);
        for(let value : values) {
            local = local + value;
            continue;
        }
        let current = match optional::some(local) {
            .some(actual) => actual,
            .none => 0,
        };
        let nested = {
            let inner = current;
            inner + seed
        };
        let nested_lambda = f() -> i32 => seed;
        local + nested
    };
})"
    );
    test_parser::assert_true(generic_checked.accepted(), "generic lambda capture walk should pass semantic analysis");
    test_parser::assert_true(
        has_capture_mode(generic_checked, "seed", semantic_lambda_capture_mode::const_ref),
        "non-escaping generic lambda should capture read-only outer values by const ref");
    test_parser::assert_true(
        has_capture_mode(generic_checked, "holder", semantic_lambda_capture_mode::const_ref),
        "generic lambda should collect member-expression captures");
    test_parser::assert_true(
        has_capture_mode(generic_checked, "values", semantic_lambda_capture_mode::const_ref),
        "generic lambda should collect index and range captures");
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

    auto update_result = analyze_one(
        "operator_update_overload.cp",
        R"(concept equality_comparable<Rhs = this> {
    operator ==(self const&, rhs: Rhs const&) -> bool;
}

concept incrementable {
    operator prefix ++(self&) -> this&;
}

accepts<T: equality_comparable<T> and T: incrementable>(value: T) -> T
{
    return value;
}

take_ref(value: i32&) -> void
{
    value = 5;
}

struct counter {
    value: i32;
}

impl counter {
    operator ==(self const&, rhs: this const&) -> bool
    {
        return value == rhs.value;
    }

    operator prefix ++(self&) -> this&
    {
        value += 1;
        return ref self;
    }

    operator postfix ++(self&) -> this
    {
        let old: counter = self;
        value += 1;
        return old;
    }

    operator prefix --(self&) -> this&
    {
        value -= 1;
        return ref self;
    }

    operator postfix --(self&) -> this
    {
        let old: counter = self;
        value -= 1;
        return old;
    }
}

main() -> i32
{
    let number = 1;
    take_ref(++number);
    take_ref(--number);
    let builtin = accepts<i32>(number);
    let cursor = counter{ 0 };
    let old = cursor++;
    ++cursor;
    let old_down = cursor--;
    --cursor;
    ++cursor;
    ++cursor;
    let checked = accepts<counter>(cursor);
    if(old == counter{ 0 } and old_down == counter{ 2 } and checked == counter{ 2 }) {
        return builtin;
    }
    return 0;
})"
    );
    test_parser::assert_true(update_result.accepted(), "update operators and builtin concepts should pass semantic analysis");

    auto compare_result = analyze_with_std_io(
        "str_compare_concepts.cp",
        R"(import std;

accepts_eq<T: equality_comparable<T>>(value: T) -> T
{
    return value;
}

accepts_three_way<T: three_way_comparable<T, weak_ordering>>(value: T) -> T
{
    return value;
}

main()
{
    let text: str = "abc";
    let same = accepts_eq<str>(text);
    let ordered = accepts_three_way<str>(text);
    let result = same <=> ordered;
})"
    );
    test_parser::assert_true(compare_result.accepted(), "str should satisfy equality and three-way comparable concepts");

    auto builtin_extension = analyze_one(
        "builtin_extension_operators.cp",
        R"(plus10(value: i32&) -> void
{
    value += 10;
}

operator +(left: i32, right: i32) -> i32
{
    return builtin(left + right);
}

impl i32 {
    bump(self&) -> void
    {
        self += 1;
    }
}

main() -> i32
{
    let value = 2;
    value.plus10();
    value.bump();
    let table: [[i32; 2]; 1] = [[1, 2]];
    let custom = value + 3;
    let raw = builtin(value + 3 + table[0][1]);
    return custom + raw;
})"
    );
    test_parser::assert_true(builtin_extension.accepted(), "builtin types should allow impl, UFCS, operator override, and builtin escape");
    test_parser::assert_true(
        builtin_extension.expression_operators.size() == 2uz,
        "builtin(...) should suppress user operator selection across the whole expression tree"
    );
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
                "ints.cp",
                R"(export module ints;

impl i32 {
    bump(self&) -> void
    {
        self += 1;
    }

    operator +(self, rhs: i32) -> i32
    {
        return builtin(self + rhs);
    }
})"),
            parse_source(
                sources,
                "main.cp",
                R"(import ints;

main()
{
    let value = 1;
    value.bump();
    let next = value + 2;
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(checked.accepted(), "imported builtin extension methods and operators should be visible");
        test_parser::assert_true(not checked.expression_operators.empty(), "imported builtin extension operator should be selected");
    }

    {
        auto sources = source_manager{};
        auto units = std::vector<parse_result> {
            parse_source(
                sources,
                "ints.cp",
                R"(export module ints;

impl i32 {
    hidden(self&) -> void
    {
        self += 1;
    }
})"),
            parse_source(
                sources,
                "main.cp",
                R"(main()
{
    let value = 1;
    value.hidden();
})")
        };
        auto checked = analyze_semantics(sources, std::span<parse_result const>{ units });
        test_parser::assert_true(
            has_diagnostic(checked, diagnostic_kind::unknown_member),
            "unimported builtin extension method should not be visible"
        );
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
    expect_diagnostic("missing_return.cp", "main() -> i32 { let value = 1; }", missing_return);
    expect_diagnostic("bad_void_value.cp", "main() { let value: void = {}; }", unknown_type);
	    expect_diagnostic (
	        "mixed_inferred_return.cp",
	        "f() { if(true) { return 1; } return true; }",
	        cannot_infer_return_type
	    );
        expect_diagnostic (
            "unit_and_value_inferred_return.cp",
            "f() { if(true) { return; } return 1; }",
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
    expect_diagnostic("never_qualified.cp", "main() { type bad = ! const; }", invalid_type_argument);
    expect_diagnostic(
        "this_type_arguments.cp",
        "struct item { value: i32; } impl item { get(self const&) -> this<i32> { return self; } }",
        invalid_type_argument
    );
    expect_diagnostic("type_alias_arguments.cp", "type value = i32; main() { let item: value<i32> = 1; }", invalid_type_argument);
    expect_diagnostic("integer_parameter_as_type.cp", "make<N: usize>() -> N { return 1; } main() { return make<1>(); }", expected_type);
    expect_diagnostic("generic_parameter_arguments.cp", "make<T>() -> T<i32> { return 1; } main() { return make<i32>(); }", invalid_type_argument);
    expect_diagnostic("bad_alloc_count.cp", "main() { let pointer = alloc<i32>(true); }", type_mismatch);
    expect_diagnostic("bad_construct_value.cp", "main() { let pointer = alloc<i32>(1); construct_at(pointer, true); }", type_mismatch);
    expect_diagnostic("storage_direct_index.cp", "main() { let item = storage i32{}; let value = item[0]; }", invalid_operator);
    expect_diagnostic("storage_bad_initializer.cp", "main() { let item = storage i32{1}; }", type_mismatch);
    expect_diagnostic("storage_temp_slot.cp", "main() { let pointer = (storage i32{}).slot(); }", invalid_assignment_target);
    expect_diagnostic("bad_nullptr_inference.cp", "main() { let pointer = nullptr; }", type_mismatch);
    expect_diagnostic("bad_ref_initializer.cp", "main() { let ref value = 1 + 2; }", invalid_assignment_target);
    expect_diagnostic("bad_destructure_initializer.cp", "main() { let (a, b) = 1; }", type_mismatch);
    expect_diagnostic("lambda_parameter_infer.cp", "main() { let f = f(x) => x; }", type_mismatch);
    expect_diagnostic("builtin_bad_arg_count.cp", "main() { let value = builtin(); }", argument_count_mismatch);
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
    expect_diagnostic (
        "incrementable_requires_prefix.cp",
        "concept equality_comparable<Rhs = this> { operator ==(self const&, rhs: Rhs const&) -> bool; } concept incrementable { operator prefix ++(self&) -> this&; } struct value { item: i32; } impl value { operator ==(self const&, rhs: this const&) -> bool { return item == rhs.item; } } use<T: equality_comparable<T> and T: incrementable>(value: T) -> T { return value; } main() { let item = use<value>(value{ 1 }); }",
        missing_concept_item
    );
    expect_diagnostic (
        "incrementable_rejects_postfix_only.cp",
        "concept equality_comparable<Rhs = this> { operator ==(self const&, rhs: Rhs const&) -> bool; } concept incrementable { operator prefix ++(self&) -> this&; } struct value { item: i32; } impl value { operator ==(self const&, rhs: this const&) -> bool { return item == rhs.item; } operator postfix ++(self&) -> this { return self; } } use<T: equality_comparable<T> and T: incrementable>(value: T) -> T { return value; } main() { let item = use<value>(value{ 1 }); }",
        missing_concept_item
    );
    expect_diagnostic (
        "equality_comparable_requires_equal.cp",
        "concept equality_comparable<Rhs = this> { operator ==(self const&, rhs: Rhs const&) -> bool; } concept incrementable { operator prefix ++(self&) -> this&; } struct value { item: i32; } impl value { operator prefix ++(self&) -> this& { return ref self; } } use<T: equality_comparable<T> and T: incrementable>(value: T) -> T { return value; } main() { let item = use<value>(value{ 1 }); }",
        missing_concept_item
    );
}

} // namespace

auto main() -> int
{
    check_fixture_examples();
    check_io_semantics();
    check_std_layered_imports();
    check_range_for_binding_semantics();
    check_sort_and_callable_semantics();
    check_iota_semantics();
    check_meta_and_ranges_semantics();
    check_ordered_collections_semantics();
    check_error_handling_semantics();
    check_side_tables();
    check_static_local_semantics();
    check_array_index_semantics();
    check_tuple_member_semantics();
    check_first_argument_ufcs_semantics();
    check_fixed_type_ids();
    check_inferred_return_types();
    check_nrvo_and_direct_initializer_metadata();
    check_contiguous_unit_batch();
    check_anonymous_modules();
    check_reference_pointer_types();
    check_struct_impl_semantics();
    check_struct_field_default_semantics();
    check_generic_struct_semantics();
    check_generic_function_semantics();
    check_parameter_pack_semantics();
    check_template_if_semantics();
    check_concept_semantics();
    check_function_type_and_memory_semantics();
    check_extern_c_semantics();
    check_enum_opaque_and_fs_semantics();
    check_function_body_kind_semantics();
    check_module_diagnostic_semantics();
    check_default_compare_semantics();
    check_string_index_semantics();
    check_decltype_ref_and_destructuring_semantics();
    check_ownership_frontend_semantics();
    check_forward_reference_semantics();
    check_like_receiver_and_delete_semantics();
    check_lambda_semantics();
    check_lambda_capture_modes();
    check_complex_lambda_capture_semantics();
    check_operator_overload_semantics();
    check_operator_import_semantics();
    check_operator_negative_semantics();
    check_negative_cases();
    return 0;
}
