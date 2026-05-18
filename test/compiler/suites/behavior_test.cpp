import std;

#include "assert.hpp"

namespace {

struct test_tools
{
    std::filesystem::path cp{};
    std::filesystem::path clang{};
    std::filesystem::path llvm_as{};
    std::filesystem::path fixture_examples{};
};

struct example_case
{
    std::string name{};
    std::vector<std::filesystem::path> sources{};
    int expected_exit{};
};

auto shell_quote(std::string_view value) -> std::string
{
    auto output = std::string{ "'" };
    for(auto character : value) {
        if(character == '\'') {
            output += "'\\''";
        } else {
            output += character;
        }
    }
    output += '\'';
    return output;
}

auto shell_join(std::vector<std::string> const& arguments) -> std::string
{
    return (
        arguments
        | std::views::transform([](std::string const& value) { return shell_quote(value); })
        | std::views::join_with(' ')
        | std::ranges::to<std::string>()
    );
}

auto run_status(std::vector<std::string> const& arguments) -> int
{
    auto command = shell_join(arguments) + " >/dev/null 2>&1";
    return std::system(command.c_str());
}

auto exit_code(int status) -> int
{
    if(status < 0) {
        return status;
    }
    return status / 256;
}

auto unique_temp_dir(std::string_view name) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / std::format("cp-compiler-test-{}-{}", name, stamp);
    std::filesystem::create_directories(path);
    return path;
}

auto write_source(std::filesystem::path const& path, std::string_view text) -> void
{
    auto stream = std::ofstream{ path };
    test_parser::assert_true(stream.is_open(), std::format("should open {}", path.string()));
    stream << text;
    test_parser::assert_true(static_cast<bool>(stream), std::format("should write {}", path.string()));
}

auto read_text(std::filesystem::path const& path) -> std::string
{
    auto stream = std::ifstream{ path };
    test_parser::assert_true(stream.is_open(), std::format("should open {}", path.string()));
    return std::string(
        std::istreambuf_iterator<char>{ stream },
        std::istreambuf_iterator<char>{}
    );
}

auto compile(test_tools const& tools, std::vector<std::string> arguments) -> int
{
    arguments.insert(arguments.begin(), tools.cp.string());
    return run_status(arguments);
}

auto check_binary_exit(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("bin");
    auto source = dir / "answer.cp";
    auto app = dir / "answer";
    write_source(source, "main() -> i32 { return 42; }");

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should emit a binary");
    test_parser::assert_true(std::filesystem::exists(app), "binary output should exist");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "binary should return 42");
}

auto check_emit_ll(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("ll");
    auto source = dir / "answer.cp";
    auto output = dir / "answer.ll";
    auto bitcode = dir / "answer.bc";
    write_source(source, "main() -> i32 { return 42; }");

    auto status = compile(tools, { source.string(), "--emit", "ll", "-o", output.string() });
    test_parser::assert_true(status == 0, "cp should emit LLVM IR");
    auto text = read_text(output);
    test_parser::assert_true(text.contains("define") and text.contains("@main"), "LLVM IR should define main");
    test_parser::assert_true(text.contains("ret i32 42"), "LLVM IR should return 42");
    test_parser::assert_true(
        run_status({ tools.llvm_as.string(), output.string(), "-o", bitcode.string() }) == 0,
        "llvm-as should accept emitted LLVM IR"
    );
}

auto check_emit_obj(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("obj");
    auto source = dir / "answer.cp";
    auto object = dir / "answer.o";
    auto app = dir / "answer";
    write_source(source, "main() -> i32 { return 42; }");

    auto status = compile(tools, { source.string(), "--emit", "obj", "-o", object.string() });
    test_parser::assert_true(status == 0, "cp should emit object code");
    test_parser::assert_true(std::filesystem::exists(object), "object output should exist");
    test_parser::assert_true(
        run_status({ tools.clang.string(), object.string(), "-o", app.string() }) == 0,
        "clang should link emitted object"
    );
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "object-linked binary should return 42");
}

auto check_multi_input(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("multi");
    auto math = dir / "math.cp";
    auto main = dir / "main.cp";
    auto app = dir / "multi";
    write_source(math, "export module math; export add(x: i32, y: i32) -> i32 { return x + y; }");
    write_source(main, "import math; main() -> i32 { return add(20, 22); }");

    auto status = compile(tools, { math.string(), main.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile multiple inputs together");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "multi-input binary should return 42");
}

auto check_error_rejects_output(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("error");
    auto source = dir / "bad.cp";
    auto app = dir / "bad";
    write_source(source, "main() -> i32 { return true; }");

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status != 0, "semantic error should make cp fail");
    test_parser::assert_true(not std::filesystem::exists(app), "failed compile should not produce output");
}

auto check_keep_ll(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("keep-ll");
    auto source = dir / "answer.cp";
    auto app = dir / "answer";
    auto ll = dir / "answer.ll";
    auto bitcode = dir / "answer.bc";
    write_source(source, "main() -> i32 { return 42; }");

    auto status = compile(tools, { source.string(), "-o", app.string(), "--keep-ll", ll.string() });
    test_parser::assert_true(status == 0, "cp should emit binary while keeping LLVM IR");
    test_parser::assert_true(std::filesystem::exists(app), "binary should exist when keeping LLVM IR");
    test_parser::assert_true(std::filesystem::exists(ll), "kept LLVM IR should exist");
    test_parser::assert_true(
        run_status({ tools.llvm_as.string(), ll.string(), "-o", bitcode.string() }) == 0,
        "llvm-as should accept kept LLVM IR"
    );
}

auto check_keep_obj(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("keep-obj");
    auto source = dir / "answer.cp";
    auto app = dir / "answer";
    auto object = dir / "answer.o";
    write_source(source, "main() -> i32 { return 42; }");

    auto status = compile(tools, { source.string(), "-o", app.string(), "--keep-obj", object.string() });
    test_parser::assert_true(status == 0, "cp should emit binary while keeping object code");
    test_parser::assert_true(std::filesystem::exists(app), "binary should exist when keeping object code");
    test_parser::assert_true(std::filesystem::exists(object), "kept object should exist");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "kept-object binary should return 42");
}

auto check_sum_non_negative(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("sum-for");
    auto source = dir / "sum.cp";
    auto app = dir / "sum";
    write_source(
        source,
        R"(sum_non_negative(values: array<i32,4>) -> i32
{
    let total: i32 = 0;
    for(let value : values) {
        if(value < 0) {
            continue;
        }
        total = total + value;
    }
    return total;
}

main() -> i32
{
    return sum_non_negative([3, -1, 4, 0]);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile range-for sum");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 7, "range-for sum should return 7");
}

auto check_array_index_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("array-index");
    auto source = dir / "array_index.cp";
    auto app = dir / "array_index";
    write_source(
        source,
        R"(pick(values: array<i32,3> const&, index: i32) -> i32
{
    return values[index];
}

main() -> i32
{
    let data = [4, 5, 6];
    let index = 1;
    data[index] = 8;
    let rows = [[1, 2, 3], [4, 5, 6]];
    return data[index] + rows[index][2] + [7, 8, 9][0] + pick(data, index);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile array index reads and writes");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 29, "array index binary should return indexed values");
}

auto check_tuple_index_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("tuple-index");
    auto source = dir / "tuple_index.cp";
    auto app = dir / "tuple_index";
    write_source(
        source,
        R"(main() -> i32
{
    let pair = (10, 20);
    pair[0] = 22;
    return pair[0] + pair[1];
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile tuple index reads and writes");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "tuple index binary should return indexed values");
}

auto check_accumulate_and_countdown(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("while-do");
    auto source = dir / "flow.cp";
    auto app = dir / "flow";
    write_source(
        source,
        R"(accumulate_until(limit: i32) -> i32
{
    let total: i32 = 0;
    let current: i32 = 0;
    while(current < limit) {
        total = total + current;
        current = current + 1;
    }
    return total;
}

countdown(start: i32) -> i32
{
    let current = start;
    do {
        current = current - 1;
    } while(current > 0);
    return current;
}

main() -> i32
{
    return accumulate_until(4) + countdown(3);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile while and do-while flow");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 6, "flow binary should return 6");
}

auto check_labeled_for_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("labeled-for");
    auto source = dir / "labeled.cp";
    auto app = dir / "labeled";
    write_source(
        source,
        R"(contains_target(rows: array<array<i32,3>,2>, target: i32) -> bool
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
}

main() -> i32
{
    let rows: array<array<i32,3>,2> = [[1, 2, 3], [-1, 4, 5]];
    if(contains_target(rows, 3)) {
        return 42;
    }
    return 1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile labeled range-for");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "labeled range-for should return 42");
}

auto check_types_example_emit_ll(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("types-example");
    auto output = dir / "types.ll";
    auto bitcode = dir / "types.bc";
    auto source = tools.fixture_examples / "types" / "main.cp";

    auto status = compile(tools, { source.string(), "--emit", "ll", "-o", output.string() });
    test_parser::assert_true(status == 0, "cp should emit LLVM IR for types example");
    test_parser::assert_true(
        run_status({ tools.llvm_as.string(), output.string(), "-o", bitcode.string() }) == 0,
        "llvm-as should accept types example LLVM IR"
    );
}

auto check_fixture_examples(test_tools const& tools) -> void
{
    auto groups = std::vector<example_case> {
        { "basics", { tools.fixture_examples / "basics" / "main.cp" }, 21 },
        { "modules", { tools.fixture_examples / "modules" / "main.cp" }, 42 },
        { "types", { tools.fixture_examples / "types" / "main.cp" }, 96 },
        { "flow", { tools.fixture_examples / "flow" / "main.cp" }, 0 },
        { "structs", { tools.fixture_examples / "structs" / "main.cp" }, 42 },
        { "concepts", { tools.fixture_examples / "concepts" / "main.cp" }, 10 },
        { "generics", { tools.fixture_examples / "generics" / "main.cp" }, 42 },
        { "variants", { tools.fixture_examples / "variants" / "main.cp" }, 42 },
        { "lambdas", { tools.fixture_examples / "lambdas" / "main.cp" }, 42 },
        { "memory", { tools.fixture_examples / "memory" / "main.cp" }, 42 },
        { "std", { tools.fixture_examples / "std" / "main.cp" }, 42 },
    };

    for(auto const& group : groups) {
        auto dir = unique_temp_dir("fixture-" + group.name);
        auto output = dir / "example.ll";
        auto bitcode = dir / "example.bc";
        auto app = dir / "example";
        auto ll_args = (
            group.sources
            | std::views::transform([](std::filesystem::path const& path) { return path.string(); })
            | std::ranges::to<std::vector<std::string>>()
        );
        ll_args.insert(ll_args.end(), { "--emit", "ll", "-o", output.string() });

        auto ll_status = compile(tools, ll_args);
        test_parser::assert_true(ll_status == 0, std::format("cp should emit LLVM IR for {} example", group.name));
        test_parser::assert_true(
            run_status({ tools.llvm_as.string(), output.string(), "-o", bitcode.string() }) == 0,
            std::format("llvm-as should accept {} example LLVM IR", group.name)
        );

        auto bin_args = (
            group.sources
            | std::views::transform([](std::filesystem::path const& path) { return path.string(); })
            | std::ranges::to<std::vector<std::string>>()
        );
        bin_args.insert(bin_args.end(), { "-o", app.string() });

        auto bin_status = compile(tools, bin_args);
        test_parser::assert_true(bin_status == 0, std::format("cp should emit binary for {} example", group.name));
        test_parser::assert_true(std::filesystem::exists(app), std::format("{} example binary should exist", group.name));
        test_parser::assert_true(
            exit_code(run_status({ app.string() })) == group.expected_exit,
            std::format("{} example binary should return {}", group.name, group.expected_exit)
        );
    }
}

auto check_const_double_pointer_reference_alias(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("const-ref-alias");
    auto source = dir / "ref_alias.cp";
    auto app = dir / "ref_alias";
    write_source(
        source,
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

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile i32 const**& alias assignment");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 11, "i32 const**& should alias pp");
}

auto check_mutable_double_pointer_reference_write(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("mutable-ref-write");
    auto source = dir / "ref_write.cp";
    auto app = dir / "ref_write";
    write_source(
        source,
        R"(main() -> i32
{
    let first: i32 = 7;
    let second: i32 = 11;
    let p: i32* = &first;
    let q: i32* = &second;
    let pp: i32** = &p;
    let rr: i32**& = pp;
    rr = &q;
    **pp = 13;
    return second;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile mutable i32**& writes");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 13, "i32**& should write through pp");
}

auto check_i64_triple_pointer_read_write(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("i64-triple-pointer");
    auto source = dir / "triple_pointer.cp";
    auto app = dir / "triple_pointer";
    write_source(
        source,
        R"(main() -> i32
{
    let value: i64 = 9;
    let p: i64* = &value;
    let pp: i64** = &p;
    let ppp: i64*** = &pp;
    ***ppp = 33;
    ***ppp += 4;
    if(value == 37 and ***ppp == 37) {
        return 42;
    }
    return 1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile i64*** read/write");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "i64*** should read and write final pointee");
}

auto check_i64_pointer_reference_slot_alias(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("i64-pointer-reference");
    auto source = dir / "pointer_reference.cp";
    auto app = dir / "pointer_reference";
    write_source(
        source,
        R"(main() -> i32
{
    let first: i64 = 5;
    let second: i64 = 11;
    let pointer: i64* = &first;
    let alias: i64*& = pointer;
    alias = &second;
    *alias = 20;
    *pointer += 2;
    if(first == 5 and second == 22) {
        return 42;
    }
    return 1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile i64*& slot aliasing");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "i64*& should assign the pointer slot and write through");
}

auto check_reference_parameter_alias(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("reference-param");
    auto source = dir / "ref_param.cp";
    auto app = dir / "ref_param";
    write_source(
        source,
        R"(set_target(slot: i32**&, target: i32*)
{
    slot = &target;
}

main() -> i32
{
    let first: i32 = 7;
    let second: i32 = 11;
    let p: i32* = &first;
    let q: i32* = &second;
    let pp: i32** = &p;
    set_target(pp, q);
    **pp = 13;
    return second;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile reference parameters");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 13, "reference parameter should alias caller slot");
}

auto check_destructor_cleanup_paths(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("destructor-paths");
    auto source = dir / "destructor_paths.cp";
    auto app = dir / "destructor_paths";
    write_source(
        source,
        R"(struct ordered_guard {
    counter: i32*;
    id: i32;
}

impl ordered_guard {
    ~ordered_guard()
    {
        *counter = *counter * 10 + id;
    }
}

struct count_guard {
    counter: i32*;
}

impl count_guard {
    ~count_guard()
    {
        *counter += 1;
    }
}

reverse_order() -> i32
{
    let count = 0;
    {
        let first = ordered_guard{ .counter = &count, .id = 1 };
        let second = ordered_guard{ .counter = &count, .id = 2 };
    }
    return count;
}

break_count() -> i32
{
    let count = 0;
    for(let value : [0, 1, 2]) {
        let item = count_guard{ .counter = &count };
        if(value == 1) {
            break;
        }
    }
    return count;
}

continue_count() -> i32
{
    let count = 0;
    for(let value : [0, 1, 2]) {
        let item = count_guard{ .counter = &count };
        if(value < 2) {
            continue;
        }
    }
    return count;
}

main() -> i32
{
    return reverse_order() + break_count() * 10 + continue_count();
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile destructor cleanup paths");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 44, "destructors should run on block, break, and continue paths");
}

auto check_return_destructor_ir(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("return-destructor");
    auto source = dir / "return_destructor.cp";
    auto output = dir / "return_destructor.ll";
    write_source(
        source,
        R"(struct guard {
    counter: i32*;
}

impl guard {
    ~guard()
    {
        *counter += 1;
    }
}

main() -> i32
{
    let count = 0;
    {
        let item = guard{ .counter = &count };
        return count;
    }
})"
    );

    auto status = compile(tools, { source.string(), "--emit", "ll", "-o", output.string() });
    test_parser::assert_true(status == 0, "cp should emit LLVM IR for return cleanup");
    auto text = read_text(output);
    auto call = text.find("call void @cp.local.guard.guard.");
    auto ret = call == std::string::npos ? std::string::npos : text.find("ret i32", call);
    test_parser::assert_true(call != std::string::npos, "return path should call the destructor");
    test_parser::assert_true(ret != std::string::npos and call < ret, "return path destructor call should precede ret");
}

auto check_function_pointer_memory_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("function-pointer-memory");
    auto source = dir / "function_pointer_memory.cp";
    auto app = dir / "function_pointer_memory";
    write_source(
        source,
        R"(add(x: i32, y: i32) -> i32
{
    return x + y;
}

main() -> i32
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
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile function pointer memory binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "function pointer memory binary should return 42");
}

auto check_pointer_difference_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("pointer-difference");
    auto source = dir / "pointer_difference.cp";
    auto app = dir / "pointer_difference";
    write_source(
        source,
        R"(main() -> i32
{
    let values = [10, 20, 30];
    let first = &values[0];
    let third = &values[2];
    let distance: isize = third - first;
    return i32(distance) + 40;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile pointer difference binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "pointer difference should count elements");
}

auto check_decltype_ref_destructure_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("decltype-ref-destructure");
    auto source = dir / "decltype_ref_destructure.cp";
    auto app = dir / "decltype_ref_destructure";
    write_source(
        source,
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
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile decltype/ref/destructure binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "decltype/ref/destructure binary should return 42");
}

auto check_lambda_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("lambda");
    auto source = dir / "lambda.cp";
    auto app = dir / "lambda";
    write_source(
        source,
        R"(main() -> i32
{
    let inc: f(i32) -> i32 =
        fn(x) => x + 1;
    let square = fn(x: i32) -> i32 {
        x * x
    };
    let bias = 1;
    let add_bias = fn(x: i32) -> i32 {
        x + bias
    };
    return inc(20) + square(4) + add_bias(1) + 3;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile lambda binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "lambda binary should return 42");
}

auto check_generic_struct_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("generic-struct");
    auto source = dir / "generic_struct.cp";
    auto app = dir / "generic_struct";
    write_source(
        source,
        R"(struct vector<T> {
    item: T;
    size: usize;
}

main() -> i32
{
    let values: vector<i32> = vector<i32>{ .item = 40, .size = 2 };
    return values.item + i32(values.size);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile generic struct binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "generic struct binary should return 42");
}

auto check_generic_function_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("generic-function");
    auto source = dir / "generic_function.cp";
    auto app = dir / "generic_function";
    write_source(
        source,
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
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile generic function binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 22, "generic function binary should return monomorphized value");
}

auto check_imported_generic_function_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("imported-generic-function");
    auto math = dir / "math.cp";
    auto main = dir / "main.cp";
    auto app = dir / "imported_generic_function";
    write_source(
        math,
        R"(export module math;

export id<T>(input: T) -> T
{
    return input;
})"
    );
    write_source(
        main,
        R"(import math;

main() -> i32
{
    return id<i32>(40) + id(2);
})"
    );

    auto status = compile(tools, { math.string(), main.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile imported generic function binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "imported generic function binary should return 42");
}

auto check_parameter_pack_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("parameter-pack");
    auto source = dir / "pack.cp";
    auto app = dir / "pack";
    write_source(
        source,
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

empty<T...>(values: T...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value;
    }
    return total;
}

main() -> i32
{
    return sum(10, 20, 9) + type_count<i32, bool, i32>() + empty();
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile parameter pack binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "parameter pack binary should return 42");
}

auto check_direct_iterator_consumes_original_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("direct-iterator");
    auto source = dir / "direct_iterator.cp";
    auto app = dir / "direct_iterator";
    write_source(
        source,
        R"(variant optional<T> {
    none;
    some(T);
}

concept iterator {
    type iter_item;
    next(self: Self&) -> optional<iter_item>;
}

struct count_iter {
    current: i32;
    end: i32;
}

impl iterator for count_iter {
    type iter_item = i32;

    next(self: count_iter&) -> optional<i32>
    {
        if(current < end) {
            let value = current;
            current = current + 1;
            return optional<i32>::some(value);
        }
        return optional<i32>::none;
    }
}

main() -> i32
{
    let iter = count_iter{ .current = 1, .end = 4 };
    let total = 0;
    for(let value : iter) {
        total = total + value;
    }
    return total + iter.current * 9;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile direct iterator range-for binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "direct iterator range-for should consume original cursor");
}

auto check_string_index_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("string-index");
    auto source = dir / "string_index.cp";
    auto app = dir / "string_index";
    write_source(
        source,
R"(main() -> i32
{
    let title: str = "cp";
    if(title[0] == 'c') {
        return 42;
    }
    return 1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile string index binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "string index binary should read first char");
}
} // namespace

auto main(int argc, char** argv) -> int
{
    test_parser::assert_true(argc == 5, "compiler behavior test expects cp, clang, llvm-as, and fixture examples paths");
    auto tools = test_tools {
        .cp = argv[1],
        .clang = argv[2],
        .llvm_as = argv[3],
        .fixture_examples = argv[4],
    };

    check_binary_exit(tools);
    check_emit_ll(tools);
    check_emit_obj(tools);
    check_multi_input(tools);
    check_error_rejects_output(tools);
    check_keep_ll(tools);
    check_keep_obj(tools);
    check_sum_non_negative(tools);
    check_array_index_binary(tools);
    check_tuple_index_binary(tools);
    check_accumulate_and_countdown(tools);
    check_labeled_for_binary(tools);
    check_types_example_emit_ll(tools);
    check_fixture_examples(tools);
    check_const_double_pointer_reference_alias(tools);
    check_mutable_double_pointer_reference_write(tools);
    check_i64_triple_pointer_read_write(tools);
    check_i64_pointer_reference_slot_alias(tools);
    check_reference_parameter_alias(tools);
    check_destructor_cleanup_paths(tools);
    check_return_destructor_ir(tools);
    check_function_pointer_memory_binary(tools);
    check_pointer_difference_binary(tools);
    check_decltype_ref_destructure_binary(tools);
    check_lambda_binary(tools);
    check_generic_struct_binary(tools);
    check_generic_function_binary(tools);
    check_imported_generic_function_binary(tools);
    check_parameter_pack_binary(tools);
    check_direct_iterator_consumes_original_binary(tools);
    check_string_index_binary(tools);
    return 0;
}
