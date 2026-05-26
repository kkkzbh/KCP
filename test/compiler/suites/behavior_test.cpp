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

auto run_stdout(std::vector<std::string> const& arguments, std::filesystem::path const& output) -> int
{
    auto command = shell_join(arguments) + " >" + shell_quote(output.string()) + " 2>/dev/null";
    return std::system(command.c_str());
}

auto run_stderr(std::vector<std::string> const& arguments, std::filesystem::path const& output) -> int
{
    auto command = shell_join(arguments) + " >/dev/null 2>" + shell_quote(output.string());
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

auto check_short_circuit_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("short-circuit");
    auto source = dir / "short_circuit.cp";
    auto app = dir / "short_circuit";
    write_source(
        source,
        R"(main() -> i32
{
    let pointer: i32* = nullptr;
    if(pointer != nullptr and *pointer == 1) {
        return 1;
    }
    if(pointer == nullptr or *pointer == 1) {
        return 0;
    }
    return 2;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile short-circuit binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "short-circuit binary should not evaluate the skipped operand");
}

auto check_extern_c_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("extern-c");
    auto source = dir / "extern_c.cp";
    auto app = dir / "extern_c";
    write_source(
        source,
R"(extern "C" abs(value: i32) -> i32;

main() -> i32
{
    return abs(-42);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile extern C binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "extern C binary should call libc abs");
}

auto check_member_default_argument_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("member-default-argument");
    auto source = dir / "member_default_argument.cp";
    auto app = dir / "member_default_argument";
    write_source(
        source,
R"(struct counter {
    value: i32;
}

impl counter {
    advance(self&, count: usize = 1 as usize) -> void
    {
        let index: usize = 0;
        while(index < count) {
            value += 1;
            index += 1;
        }
    }
}

main() -> i32
{
    let value = counter{ .value = 0 };
    value.advance();
    value.advance(4 as usize);
    return value.value;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile member calls with default arguments");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 5, "member default argument should use the declared expression");
}

auto check_struct_field_default_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("struct-field-default");
    auto source = dir / "struct_field_default.cp";
    auto app = dir / "struct_field_default";
    write_source(
        source,
R"(struct point {
    x: i32 = 7;
    y: i32 = 11;
}

main() -> i32
{
    let empty = point{};
    let named = point{ .y = 5 };
    let positional = point{ 3 };
    return empty.x + empty.y + named.x + named.y + positional.x + positional.y;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile struct field defaults");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 44, "struct field defaults should fill omitted fields");
}

auto check_ufcs_receiver_conversion_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("ufcs-receiver-conversion");
    auto source = dir / "ufcs_receiver_conversion.cp";
    auto app = dir / "ufcs_receiver_conversion";
    write_source(
        source,
R"(wide(value: i64) -> i64
{
    return value + 1;
}

main() -> i32
{
    let value: i32 = 41;
    return value.wide() as i32;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile UFCS receiver implicit conversion");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "UFCS receiver should be converted before the call");
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

    auto extern_source = dir / "extern_bad.cp";
    auto extern_app = dir / "extern_bad";
    write_source(extern_source, "extern \"C\" text() { return \"abc\"; }");

    auto extern_status = compile(tools, { extern_source.string(), "-o", extern_app.string() });
    test_parser::assert_true(extern_status != 0, "extern C inferred return should make cp fail");
    test_parser::assert_true(not std::filesystem::exists(extern_app), "failed extern C compile should not produce output");
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
        R"(sum_non_negative(values: [i32; 4]) -> i32
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
        R"(pick(values: [i32; 3] const&, index: i32) -> i32
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

auto check_tuple_member_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("tuple-member");
    auto source = dir / "tuple_member.cp";
    auto app = dir / "tuple_member";
    write_source(
        source,
        R"(main() -> i32
{
    let pair = (10, 20);
    pair.0 = 22;
    return pair.0 + pair.1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile tuple member reads and writes");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "tuple member binary should return selected values");
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
        R"(contains_target(rows: [[i32; 3]; 2], target: i32) -> bool
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
    let rows: [[i32; 3]; 2] = [[1, 2, 3], [-1, 4, 5]];
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
        { "operators", { tools.fixture_examples / "operators" / "main.cp" }, 42 },
        { "ownership", { tools.fixture_examples / "ownership" / "main.cp" }, 92 },
        { "interop", { tools.fixture_examples / "interop" / "main.cp" }, 42 },
        { "errors", { tools.fixture_examples / "errors" / "main.cp" }, 42 },
        { "fs", { tools.fixture_examples / "fs" / "main.cp" }, 0 },
        { "std", { tools.fixture_examples / "std" / "main.cp" }, 118 },
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

auto check_reference_return_value_binding(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("reference-return-value");
    auto source = dir / "reference_return_value.cp";
    auto app = dir / "reference_return_value";
    write_source(
        source,
        R"(import std;

struct id {
    value: usize;
}

make(value: usize) -> optional<id>
{
    return optional<id>::some(id{ .value = value });
}

main() -> i32
{
    let first = make(3 as usize);
    if(not first.has_value()) {
        return 1;
    }
    let value = *first;
    let second = make(4 as usize);
    if(not second.has_value()) {
        return 2;
    }
    if(value.value != 3 as usize) {
        return 3;
    }
    return 42;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile copied reference-return value bindings");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "plain let should copy a reference-returned value");
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

auto check_nrvo_return_skips_returned_local_destructor(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("nrvo-return-destructor");
    auto source = dir / "nrvo_return_destructor.cp";
    auto app = dir / "nrvo_return_destructor";
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

make(counter: i32*) -> guard
{
    let result = guard{ .counter = counter };
    return result;
}

main() -> i32
{
    let count = 0;
    {
        let item = make(&count);
    }
    return count + 41;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile NRVO destructor binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "NRVO should not destroy the returned local in the callee");
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

auto check_storage_memory_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("storage-memory");
    auto source = dir / "storage_memory.cp";
    auto app = dir / "storage_memory";
    write_source(
        source,
        R"(struct guard {
    total: i32*;
    value: i32;
}

impl guard {
    ~guard()
    {
        *total = *total + value;
    }
}

main() -> i32
{
    let total = 0;
    let single = storage guard{};
    construct_at(single.slot(), guard{ .total = &total, .value = 10 });
    destroy_at(single.slot());

    let many = storage [guard; 2]{};
    construct_at(many.slot(0), guard{ .total = &total, .value = 20 });
    construct_at(many.data() + 1, guard{ .total = &total, .value = 12 });
    destroy_at(many.slot(1));
    destroy_at(many.data());
    return total;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile storage memory binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "storage slots should work with construct_at and destroy_at");
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
    return (distance as i32) + 40;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile pointer difference binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "pointer difference should count elements");
}

auto check_update_expression_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("update-expression");
    auto source = dir / "update_expression.cp";
    auto app = dir / "update_expression";
    write_source(
        source,
        R"(set_to(value: i32&, next: i32) -> void
{
    value = next;
}

main() -> i32
{
    let value = 10;
    let prefix_inc: i32 = ++value;
    let postfix_inc = value++;
    let prefix_dec: i32 = --value;
    let postfix_dec = value--;
    let standalone = 40;
    standalone++;
    standalone++;

    if(value != 10) {
        return 1;
    }
    set_to(++value, 10);
    if(value != 10) {
        return 2;
    }
    set_to(--value, 10);
    if(value != 10) {
        return 3;
    }

    return prefix_inc + postfix_inc + prefix_dec + postfix_dec + standalone - 44;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile update expression binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "update expressions should write back and preserve prefix/postfix values");
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
        f(x) => x + 1;
    let square = f(x: i32) -> i32 {
        x * x
    };
    let bias = 1;
    let add_bias = f(x: i32) -> i32 {
        x + bias
    };
    return inc(20) + square(4) + add_bias(1) + 3;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile lambda binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "lambda binary should return 42");
}

auto check_nested_inferred_lambda_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("nested-lambda");
    auto source = dir / "nested_lambda.cp";
    auto app = dir / "nested_lambda";
    write_source(
        source,
        R"(main() -> i32
{
    let y = 2;
    let func = f(x: i32) => x + y;
    let func2 = f() {
        func(1)
    };
    return func2();
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile nested inferred lambda binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 3, "nested inferred lambda should return 3");
}

auto check_generic_lambda_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("generic-lambda");
    auto source = dir / "generic_lambda.cp";
    auto app = dir / "generic_lambda";
    write_source(
        source,
        R"(main() -> i32
{
    let bias = 10;
    let add_bias = f<T>(x: T) -> T {
        x + bias
    };
    let identity = f<T>(x: T) -> T {
        x
    };
    return add_bias<i32>(2) + identity<i32>(30);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile generic lambda binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "generic lambda binary should return 42");
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
    return values.item + (values.size as i32);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile generic struct binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "generic struct binary should return 42");
}

auto check_const_generic_struct_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("const-generic-struct");
    auto source = dir / "const_generic_struct.cp";
    auto app = dir / "const_generic_struct";
    write_source(
        source,
        R"(struct buffer<T, N: usize> {
    data: [T; N];
}

pick<T, N: usize>(values: buffer<T, N>) -> T
{
    return values.data[0];
}

main() -> i32
{
    let values: buffer<i32, 4> = buffer<i32, 4>{ .data = [42, 1, 2, 3] };
    return pick(values);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile const generic struct binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "const generic struct binary should return selected value");
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

auto check_inferred_parameter_generic_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("inferred-parameter-generic");
    auto source = dir / "inferred_parameter_generic.cp";
    auto app = dir / "inferred_parameter_generic";
    write_source(
        source,
        R"(read(value const&) -> i32
{
    return value;
}

add(left, right) -> i32
{
    return left + right;
}

main() -> i32
{
    let value = 20;
    return add(read(const ref value), 22);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile inferred parameter generic binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "inferred parameter generic binary should return 42");
}

auto check_const_generic_array_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("const-generic-array");
    auto source = dir / "const_generic_array.cp";
    auto app = dir / "const_generic_array";
    write_source(
        source,
        R"(first<T, N: usize>(values: [T; N]) -> T
{
    return values[0];
}

main() -> i32
{
    return first([42, 1, 2]);
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile const generic array binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "const generic array binary should return selected value");
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

sum_ref<T...>(values: T const&...) -> i32
{
    let total = 0;
    template fo)" "r" R"( (let value : values...) {
        total = total + value;
    }
    return total;
}

main() -> i32
{
    return sum(10, 20, 9) + type_count<i32, bool, i32>() + empty() + sum_ref(0);
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
    next(self&) -> optional<iter_item>;
}

struct count_iter {
    current: i32;
    end: i32;
}

impl iterator for count_iter {
    type iter_item = i32;

    next(self&) -> optional<i32>
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

auto check_string_iteration_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("string-iteration");
    auto source = dir / "string_iteration.cp";
    auto app = dir / "string_iteration";
    write_source(
        source,
        R"(import std;

main() -> i32
{
    let text: str = "a\0b";
    if(text.size() != 3 or text.len != 3) {
        return 1;
    }
    let view = str{ .ptr = text.data(), .len = 2 };
    if(view.size() != 2 or view[1] != '\0') {
        return 2;
    }
    let index = 0;
    let matched = 0;
    for(let ch : text) {
        if(index == 0 and ch == 'a') {
            matched += 1;
        }
        if(index == 1 and ch == '\0') {
            matched += 1;
        }
        if(index == 2 and ch == 'b') {
            matched += 1;
        }
        index += 1;
    }
    for(let ch : "a\0b") {
        if(index == 3 and ch == 'a') {
            matched += 1;
        }
        if(index == 4 and ch == '\0') {
            matched += 1;
        }
        if(index == 5 and ch == 'b') {
            matched += 1;
        }
        index += 1;
    }
    if(index == 6 and matched == 6) {
        return 42;
    }
    return 3;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile string range-for binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "string range-for should iterate chars");
}

auto check_string_compare_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("string-compare");
    auto source = dir / "string_compare.cp";
    auto app = dir / "string_compare";
    write_source(
        source,
        R"(import std;

main() -> i32
{
    if(not ("abc" == "abc")) { return 1; }
    if(not ("abc" != "abd")) { return 2; }
    if(not ("abc" < "abd")) { return 3; }
    if(not ("abc" < "abcd")) { return 4; }
    if(not ("abcd" > "abc")) { return 5; }
    if(not ("a\0b" > "a\0a")) { return 6; }
    let ordering = "abc" <=> "abc";
    return match ordering {
        .less => 7,
        .equivalent => 42,
        .greater => 8,
    };
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile string comparison binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "string comparison binary should use lexicographic str operators");
}

auto check_string_copy_move_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("string-copy-move");
    auto source = dir / "string_copy_move.cp";
    auto app = dir / "string_copy_move";
    write_source(
        source,
        R"(import std;

copy_value(value: string) -> string
{
    return value;
}

main() -> i32
{
    let left = string{"b"};
    let right = string{"a"};
    let copied = right;
    right = left;
    if(copied.as_str() != "a") {
        return 1;
    }
    if(right.as_str() != "b") {
        return 2;
    }

    let moved = move right;
    if(moved.as_str() != "b") {
        return 3;
    }
    if(right.size() != 0) {
        return 4;
    }

    let explicit_copy = string{copied};
    if(explicit_copy.as_str() != "a") {
        return 5;
    }

    let values = vector<string>{};
    values.push_back(string{"b"});
    values.push_back(string{"a"});
    let old_second = values[1 as usize];
    values[1 as usize] = values[0 as usize];
    if(old_second.as_str() != "a") {
        return 6;
    }

    values.clear();
    values.push_back(string{"c"});
    values.push_back(string{"a"});
    values.push_back(string{"b"});
    sort(values, f(left: string const&, right: string const&) -> weak_ordering {
        return left.as_str() <=> right.as_str();
    });
    if(values[0 as usize].as_str() != "a" or values[1 as usize].as_str() != "b" or values[2 as usize].as_str() != "c") {
        return 7;
    }

    let copied_storage = alloc<string>(1);
    construct_at(copied_storage, values[0 as usize]);
    values[0 as usize] = string{"z"};
    if((*copied_storage).as_str() != "a") {
        return 8;
    }
    destroy_at(copied_storage);
    free(copied_storage);

    let moved_storage = alloc<string>(1);
    construct_at(moved_storage, move values[1 as usize]);
    if((*moved_storage).as_str() != "b") {
        return 9;
    }
    if(values[1 as usize].size() != 0) {
        return 10;
    }
    destroy_at(moved_storage);
    free(moved_storage);

    let roundtrip = copy_value(copied);
    if(roundtrip.as_str() != "a" or copied.as_str() != "a") {
        return 11;
    }

    return 42;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile string copy/move binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "string copy/move binary should preserve ownership");
}

auto check_io_nul_string_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("io-nul-string");
    auto source = dir / "io_nul_string.cp";
    auto app = dir / "io_nul_string";
    auto output = dir / "stdout.bin";
    write_source(
        source,
        R"(import std;

main() -> i32
{
    print("a\0b");
    return 0;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std.io nul string binary");
    test_parser::assert_true(exit_code(run_stdout({ app.string() }, output)) == 0, "std.io nul string binary should run");
    test_parser::assert_true(read_text(output) == std::string("a\0b", 3uz), "std.io should not truncate at nul");
}

auto check_io_print_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("io-print");
    auto source = dir / "io_print.cp";
    auto app = dir / "io_print";
    auto output = dir / "stdout.txt";
    write_source(
        source,
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
    let text = string{"owned"};
    let formatted = format("buffer {}", text);
    match formatted {
        .ok(value) => {
            println("formatted = {}", value);
        },
        .error(error) => {
            return 1;
        },
    };
    println("language = {}, score = {}", "cp", -42);
    println("literal braces = {{}}");
    println("bool = {}", true);
    println(
        "builtins = {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
        'x',
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
        1.5,
        2.5 as f32,
        text
    );
    println("custom = {}", point{ .x = 7, .y = 9 });
    return 0;
})cp"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std.io print binary");
    test_parser::assert_true(exit_code(run_stdout({ app.string() }, output)) == 0, "std.io print binary should run");
    test_parser::assert_true(
        read_text(output) == (
            "formatted = buffer owned\n"
            "language = cp, score = -42\n"
            "literal braces = {}\n"
            "bool = true\n"
            "builtins = x -1 -2 -3 -4 -5 1 2 3 4 5 1.500000 2.500000 owned\n"
            "custom = point(7, 9)\n"
        ),
        "std.io print binary should write expected stdout"
    );
}

auto check_io_format_errors_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("io-errors");
    auto source = dir / "io_errors.cp";
    auto app = dir / "io_errors";
    write_source(
        source,
        R"(import std;

is_placeholder_too_few(result: display_result) -> bool
{
    return match result {
        .ok => false,
        .error(error) => match error {
            .placeholder_too_few => true,
            .argument_too_many => false,
            .invalid_escape => false,
            .unsupported_specifier => false,
            .output_failed => false,
        },
    };
}

is_argument_too_many(result: display_result) -> bool
{
    return match result {
        .ok => false,
        .error(error) => match error {
            .placeholder_too_few => false,
            .argument_too_many => true,
            .invalid_escape => false,
            .unsupported_specifier => false,
            .output_failed => false,
        },
    };
}

is_invalid_escape(result: display_result) -> bool
{
    return match result {
        .ok => false,
        .error(error) => match error {
            .placeholder_too_few => false,
            .argument_too_many => false,
            .invalid_escape => true,
            .unsupported_specifier => false,
            .output_failed => false,
        },
    };
}

is_unsupported_specifier(result: display_result) -> bool
{
    return match result {
        .ok => false,
        .error(error) => match error {
            .placeholder_too_few => false,
            .argument_too_many => false,
            .invalid_escape => false,
            .unsupported_specifier => true,
            .output_failed => false,
        },
    };
}

main() -> i32
{
    if(
        is_placeholder_too_few(println("{} {}", 1))
        and is_argument_too_many(println("{}", 1, 2))
        and is_invalid_escape(println("{"))
        and is_invalid_escape(println("}"))
        and is_unsupported_specifier(println("{:}", 1))
    ) {
        return 42;
    }
    return 1;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std.io format error binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "std.io format errors should be observable");
}

auto check_file_io_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("file-io");
    auto source = dir / "file_io.cp";
    auto data = dir / "data.txt";
    auto app = dir / "file_io";
    write_source(
        source,
        std::format(
R"(import std;

main() -> i32
{{
    let path: str = "{}";
    match file::open(path, open_options{{}}.write().create().truncate()) {{
        .value(out) => {{
            let written = out.write_str("cp-io");
            if(written.value_or(0 as usize) != (5 as usize)) {{
                return 2;
            }}
            out.close();
        }},
        .unexpected(error) => {{ return 1; }},
    }};
    match file::open(path, open_options{{}}.read()) {{
        .value(input) => {{
            let storage = raw_buffer<u8>{{5}};
            let read = input.read(span<u8>{{storage.data(), 5}});
            if(read.value_or(0 as usize) != (5 as usize)) {{
                return 3;
            }}
            return (*(storage.data()) as i32) - 99;
        }},
        .unexpected(error) => {{ return 4; }},
    }};
    return 9;
}})",
            data.string()
        )
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std.fs file IO binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 0, "std.fs binary should write and read file data");
    test_parser::assert_true(read_text(data) == "cp-io", "std.fs binary should leave expected file contents");
}

auto check_operator_overload_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("operator-overload");
    auto source = dir / "operator_overload.cp";
    auto app = dir / "operator_overload";
    write_source (
        source,
        R"(struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    operator +(self const&, rhs: this const&) -> this
    {
        return vec2{ .x = x + rhs.x, .y = y + rhs.y };
    }

    operator +=(self&, rhs: this const&)
    {
        x += rhs.x;
        y += rhs.y;
    }
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

struct tagged {
    value: i32;
}

impl tagged {
    operator =(self&, rhs: this const&)
    {
        value = rhs.value + 5;
    }
}

main() -> i32
{
    let a = vec2{ 1, 2 };
    let b = vec2{ 3, 4 };
    let c = a + b;
    a += b;
    let holder = slot{ 0 };
    holder[0] = c.x + a.y;
    let lhs = tagged{ 1 };
    let rhs = tagged{ 9 };
    lhs = rhs;
    return holder[0] + lhs.value + 18;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile operator overload binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "operator overload binary should return computed value");
}

auto check_builtin_operator_escape_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("builtin-operator-escape");
    auto source = dir / "builtin_operator_escape.cp";
    auto app = dir / "builtin_operator_escape";
    write_source (
        source,
        R"(plus10(value: i32&) -> void
{
    value += 10;
}

operator +(left: i32, right: i32) -> i32
{
    return builtin(left + right + 1);
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
    return custom + raw + 5;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile builtin operator escape binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "builtin operator escape binary should return computed value");
}

auto check_update_operator_overload_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("update-operator-overload");
    auto source = dir / "update_operator_overload.cp";
    auto app = dir / "update_operator_overload";
    write_source(
        source,
        R"(struct counter {
    value: i32;
}

impl counter {
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

set_to(value: counter&, next: i32) -> void
{
    value.value = next;
}

main() -> i32
{
    let cursor = counter{ 10 };
    let prefix_inc: counter = ++cursor;
    let postfix_inc = cursor++;
    let prefix_dec: counter = --cursor;
    let postfix_dec = cursor--;
    set_to(++cursor, 30);
    set_to(--cursor, 21);
    return prefix_inc.value + postfix_inc.value + prefix_dec.value + postfix_dec.value + cursor.value - 23;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile custom update operator overload binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "custom update operator overload binary should return computed value");
}

auto check_std_sort_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("std-sort");
    auto source = dir / "std_sort.cp";
    auto app = dir / "std_sort";
    write_source(
        source,
        R"(import std;

struct record {
    key: i32;
    order: i32;
}

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(1);
    values.push_back(2);
    sort(values);
    if(values[0 as usize] != 1 or values[1 as usize] != 2 or values[2 as usize] != 3) {
        return 1;
    }
    values.sort(desc<i32>{});
    if(values[0 as usize] != 3 or values[1 as usize] != 2 or values[2 as usize] != 1) {
        return 2;
    }
    sort(span<i32>{values.data(), values.size()}, f(left: i32 const&, right: i32 const&) -> weak_ordering {
        return left <=> right;
    });
    if(values[0 as usize] != 1 or values[1 as usize] != 2 or values[2 as usize] != 3) {
        return 3;
    }

    let fixed: [i32; 4] = [4, 1, 3, 2];
    sort(fixed);
    if(fixed[0 as usize] != 1 or fixed[1 as usize] != 2 or fixed[2 as usize] != 3 or fixed[3 as usize] != 4) {
        return 4;
    }
    fixed.sort(f(left: i32 const&, right: i32 const&) -> weak_ordering {
        return desc<i32>{}(left, right);
    });
    if(fixed[0 as usize] != 4 or fixed[1 as usize] != 3 or fixed[2 as usize] != 2 or fixed[3 as usize] != 1) {
        return 5;
    }

    let text = string{"cba"};
    sort(text);
    if(text.size() != 3 or text[0 as usize] != 'a' or text[1 as usize] != 'b' or text[2 as usize] != 'c') {
        return 6;
    }
    if(*(text.data() + text.size()) != '\0') {
        return 7;
    }
    text.sort(desc<char>{});
    if(text.size() != 3 or text[0 as usize] != 'c' or text[1 as usize] != 'b' or text[2 as usize] != 'a') {
        return 8;
    }
    if(*(text.data() + text.size()) != '\0') {
        return 9;
    }

    let records = vector<record>{};
    let stable_records = vector<record>{};
    let index: i32 = 0;
    while(index < 70) {
        let key = index % 7;
        if(index % 2 == 0) {
            key = 6 - key;
        }
        records.push_back(record{ .key = key, .order = index });
        stable_records.push_back(record{ .key = key, .order = index });
        index += 1;
    }

    sort(records, f(left: record const&, right: record const&) -> weak_ordering {
        return left.key <=> right.key;
    });

    let last_seen: [i32; 7] = [-1, -1, -1, -1, -1, -1, -1];
    let record_index: usize = 0;
    while(record_index < records.size()) {
        if(record_index > 0 and records[record_index].key < records[record_index - 1].key) {
            return 10;
        }
        record_index += 1;
    }

    stable_sort(stable_records, f(left: record const&, right: record const&) -> weak_ordering {
        return left.key <=> right.key;
    });

    last_seen = [-1, -1, -1, -1, -1, -1, -1];
    record_index = 0;
    while(record_index < stable_records.size()) {
        if(record_index > 0 and stable_records[record_index].key < stable_records[record_index - 1].key) {
            return 11;
        }
        let key = stable_records[record_index].key;
        if(stable_records[record_index].order <= last_seen[key as usize]) {
            return 12;
        }
        last_seen[key as usize] = stable_records[record_index].order;
        record_index += 1;
    }

    return values[0 as usize] + values[1 as usize] * 10 + values[2 as usize] * 7;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std sort binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "std sort binary should return sorted checksum");
}

auto check_forward_reference_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("forward-reference");
    auto source = dir / "forward_reference.cp";
    auto app = dir / "forward_reference";
    write_source(
        source,
        R"(import std;

sink_ref(value: i32&) -> i32
{
    value += 1;
    return value;
}

sink_move(value: i32 move&) -> i32
{
    return value + 10;
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

copy_value(value: i32) -> i32
{
    return value + 1;
}

main() -> i32
{
    let first = 1;
    let a = relay_ref(first);
    let b = relay_move(2);
    let second = 3;
    let c = relay_move(move second);
    let d = read(const first);
    let e = copy_value(const first);
    return a + b + c + d + e + 10;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile forward reference binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "forward reference binary should preserve forwarding and const expression semantics");
}

auto check_iota_public_import_binary(test_tools const& tools) -> void
{
    auto modules = std::array{ std::string_view{ "std.ranges" }, std::string_view{ "std.ranges.iota" } };
    for(auto module_name : modules) {
        auto dir = unique_temp_dir("iota-public-import");
        auto source = dir / "iota_public_import.cp";
        auto app = dir / "iota_public_import";
        auto text = std::string{ "import " };
        text.append(module_name);
        text += R"(;

main() -> i32
{
    let values = iota(0, 3);
    let total = 0;
    for(let item : values) {
        total += item;
    }
    return total + 39;
})";
        write_source(source, text);

        auto status = compile(tools, { source.string(), "-o", app.string() });
        test_parser::assert_true(status == 0, "cp should compile iota through its public range modules");
        test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "public iota imports should expose iterable support");
    }
}

auto check_iota_custom_incrementable_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("iota-custom");
    auto source = dir / "iota_custom.cp";
    auto app = dir / "iota_custom";
    write_source(
        source,
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
    return total + 39;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile custom incrementable iota binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "custom incrementable iota should iterate by prefix ++");
}

auto check_std_map_set_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("std-map-set");
    auto source = dir / "std_map_set.cp";
    auto app = dir / "std_map_set";
    write_source(
        source,
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
    if(not first.inserted or not second.inserted or duplicate.inserted) {
        return 1;
    }
    if(values.size() != 3 or values.at(3) != 30 or values.nth(1 as usize).value != 20) {
        return 2;
    }
    values.nth(1 as usize).value = values.nth(1 as usize).value + 1;
    if(values.at(2) != 21) {
        return 6;
    }
    if(values.rank(3) != 2 or values.remove(8) or not values.remove(1)) {
        return 3;
    }

    let keys = set<i32>{};
    let a = keys.insert(4);
    let b = keys.insert(2);
    let c = keys.insert(4);
    if(not a.inserted or not b.inserted or c.inserted) {
        return 4;
    }
    if(keys.size() != 2 or keys.nth(0 as usize) != 2 or keys.rank(4) != 1) {
        return 5;
    }

    return values.at(2) + values.at(3) - keys.nth(0 as usize) - keys.rank(4) as i32 - values.size() as i32 - keys.size() as i32 - 3;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile std map/set binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 41, "std map/set binary should return ordered-container checksum");
}

auto check_std_map_set_btree_stress_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("std-map-set-btree-stress");
    auto source = dir / "std_map_set_btree_stress.cp";
    auto app = dir / "std_map_set_btree_stress";
    write_source(
        source,
        R"(import std;

check_set_order(values: set<i32>&, expected_count: i32, removed_mod: i32) -> i32
{
    if(values.size() != expected_count as usize) {
        return 10;
    }
    let index: usize = 0;
    let previous = -1000000;
    while(index < values.size()) {
        let value = values.nth(index);
        if(index > 0 as usize and not (previous < value)) {
            return 11;
        }
        if(removed_mod != 0 and value % removed_mod == 0) {
            return 12;
        }
        if(values.rank(value) != index) {
            return 13;
        }
        previous = value;
        index += 1;
    }
    return 0;
}

main() -> i32
{
    let values = set<i32>{};
    let index = 0;
    while(index < 400) {
        let key = (index * 37) % 997;
        values.insert(key);
        values.insert(key);
        index += 1;
    }
    let first_check = check_set_order(ref values, 400, 0);
    if(first_check != 0) {
        return first_check;
    }

    let probe = 0;
    while(probe < 997) {
        let rank = values.rank(probe);
        if(values.contains(probe) and (rank >= values.size() or values.nth(rank) != probe)) {
            return 14;
        }
        probe += 1;
    }

    let remove_index = 0;
    while(remove_index < 997) {
        if(remove_index % 3 == 0) {
            values.remove(remove_index);
        }
        remove_index += 1;
    }
    let second_check = check_set_order(ref values, 267, 3);
    if(second_check != 0) {
        return second_check;
    }

    let table = map<i32, i32>{};
    let insert_index = 0;
    while(insert_index < 300) {
        let key = (insert_index * 53) % 701;
        table.insert(key, key * 2 + 1);
        insert_index += 1;
    }
    let item_index: usize = 0;
    while(item_index < table.size()) {
        let item = table.nth(item_index);
        if(table.rank(item.key) != item_index or table.at(item.key) != item.key * 2 + 1) {
            return 20;
        }
        item_index += 1;
    }

    let erase = 0;
    while(erase < 701) {
        if(erase % 4 == 1) {
            table.remove(erase);
        }
        erase += 1;
    }
    let after_index: usize = 0;
    while(after_index < table.size()) {
        let item = table.nth(after_index);
        if(item.key % 4 == 1 or table.rank(item.key) != after_index) {
            return 21;
        }
        after_index += 1;
    }
    return 42;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile B-tree map/set stress binary");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "B-tree map/set stress binary should preserve ordering, rank, nth, and remove");
}

auto check_compiler_lab_workload_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("compiler-lab-workload");
    auto source = dir / "compiler_lab.cp";
    auto app = dir / "compiler_lab";
    write_source(
        source,
        R"(import std;

enum token_kind : u8 {
    file_end = 0;
    invalid = 1;
    kw_int = 2;
    kw_void = 3;
    kw_if = 4;
    kw_else = 5;
    kw_while = 6;
    kw_return = 7;
    identifier = 8;
    integer = 9;
    lparen = 10;
    rparen = 11;
    lbrace = 12;
    rbrace = 13;
    comma = 14;
    semicolon = 15;
    plus = 16;
    minus = 17;
    star = 18;
    slash = 19;
    percent = 20;
    assign = 21;
    equal_equal = 22;
    bang_equal = 23;
    less = 24;
    less_equal = 25;
    greater = 26;
    greater_equal = 27;
    and_and = 28;
    or_or = 29;
}

struct token {
    kind: token_kind;
    position: usize;
}

struct lexer_state {
    input: str;
    index: usize;
}

struct parser_state {
    tokens: vector<token>;
    index: usize;
}

struct quad {
    op: char;
    left: i32;
    right: i32;
    result: i32;
}

struct lr1_item {
    rule: i32;
    dot: i32;
    lookahead: i32;
}

impl token {
    token()
    {
        return token{ .kind = token_kind::invalid, .position = 0 as usize };
    }
}

impl lr1_item {
    operator <=>(self const&, rhs: lr1_item const&) -> weak_ordering
    {
        if(rule != rhs.rule) {
            return rule <=> rhs.rule;
        }
        if(dot != rhs.dot) {
            return dot <=> rhs.dot;
        }
        return lookahead <=> rhs.lookahead;
    }
}

is_alpha(ch: char) -> bool
{
    return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or ch == '_';
}

is_digit(ch: char) -> bool
{
    return ch >= '0' and ch <= '9';
}

is_alnum(ch: char) -> bool
{
    return is_alpha(ch) or is_digit(ch);
}

keyword_kind(input: str, start: usize, len: usize) -> token_kind
{
    if(len == 2 and input[start] == 'i' and input[start + 1] == 'f') {
        return token_kind::kw_if;
    }
    if(len == 3 and input[start] == 'i' and input[start + 1] == 'n' and input[start + 2] == 't') {
        return token_kind::kw_int;
    }
    if(len == 4 and input[start] == 'v' and input[start + 1] == 'o' and input[start + 2] == 'i' and input[start + 3] == 'd') {
        return token_kind::kw_void;
    }
    if(len == 4 and input[start] == 'e' and input[start + 1] == 'l' and input[start + 2] == 's' and input[start + 3] == 'e') {
        return token_kind::kw_else;
    }
    if(len == 5 and input[start] == 'w' and input[start + 1] == 'h' and input[start + 2] == 'i' and input[start + 3] == 'l' and input[start + 4] == 'e') {
        return token_kind::kw_while;
    }
    if(len == 6 and input[start] == 'r' and input[start + 1] == 'e' and input[start + 2] == 't' and input[start + 3] == 'u' and input[start + 4] == 'r' and input[start + 5] == 'n') {
        return token_kind::kw_return;
    }
    return token_kind::identifier;
}

skip_space_and_comments(state: lexer_state&) -> void
{
    let scanning = true;
    while(scanning) {
        scanning = false;
        while(state.index < state.input.size() and (state.input[state.index] == ' ' or state.input[state.index] == '\n' or state.input[state.index] == '\t')) {
            state.index += 1;
        }
        if(state.index + 1 < state.input.size() and state.input[state.index] == '/' and state.input[state.index + 1] == '/') {
            state.index += 2;
            while(state.index < state.input.size() and state.input[state.index] != '\n') {
                state.index += 1;
            }
            scanning = true;
        }
        if(state.index + 1 < state.input.size() and state.input[state.index] == '/' and state.input[state.index + 1] == '*') {
            state.index += 2;
            while(state.index + 1 < state.input.size() and not (state.input[state.index] == '*' and state.input[state.index + 1] == '/')) {
                state.index += 1;
            }
            if(state.index + 1 < state.input.size()) {
                state.index += 2;
            }
            scanning = true;
        }
    }
}

simple_token(kind: token_kind, position: usize) -> token
{
    return token{ .kind = kind, .position = position };
}

next_token(state: lexer_state&) -> token
{
    skip_space_and_comments(ref state);
    if(state.index >= state.input.size()) {
        return simple_token(token_kind::file_end, state.index);
    }

    let position = state.index;
    let ch = state.input[position];
    if(is_alpha(ch)) {
        state.index += 1;
        while(state.index < state.input.size() and is_alnum(state.input[state.index])) {
            state.index += 1;
        }
        return simple_token(keyword_kind(state.input, position, state.index - position), position);
    }
    if(is_digit(ch)) {
        state.index += 1;
        while(state.index < state.input.size() and is_digit(state.input[state.index])) {
            state.index += 1;
        }
        return simple_token(token_kind::integer, position);
    }

    state.index += 1;
    if(ch == '(') { return simple_token(token_kind::lparen, position); }
    if(ch == ')') { return simple_token(token_kind::rparen, position); }
    if(ch == '{') { return simple_token(token_kind::lbrace, position); }
    if(ch == '}') { return simple_token(token_kind::rbrace, position); }
    if(ch == ',') { return simple_token(token_kind::comma, position); }
    if(ch == ';') { return simple_token(token_kind::semicolon, position); }
    if(ch == '+') { return simple_token(token_kind::plus, position); }
    if(ch == '-') { return simple_token(token_kind::minus, position); }
    if(ch == '*') { return simple_token(token_kind::star, position); }
    if(ch == '/') { return simple_token(token_kind::slash, position); }
    if(ch == '%') { return simple_token(token_kind::percent, position); }
    if(ch == '=' and state.index < state.input.size() and state.input[state.index] == '=') {
        state.index += 1;
        return simple_token(token_kind::equal_equal, position);
    }
    if(ch == '!' and state.index < state.input.size() and state.input[state.index] == '=') {
        state.index += 1;
        return simple_token(token_kind::bang_equal, position);
    }
    if(ch == '<' and state.index < state.input.size() and state.input[state.index] == '=') {
        state.index += 1;
        return simple_token(token_kind::less_equal, position);
    }
    if(ch == '>' and state.index < state.input.size() and state.input[state.index] == '=') {
        state.index += 1;
        return simple_token(token_kind::greater_equal, position);
    }
    if(ch == '&' and state.index < state.input.size() and state.input[state.index] == '&') {
        state.index += 1;
        return simple_token(token_kind::and_and, position);
    }
    if(ch == '|' and state.index < state.input.size() and state.input[state.index] == '|') {
        state.index += 1;
        return simple_token(token_kind::or_or, position);
    }
    if(ch == '=') { return simple_token(token_kind::assign, position); }
    if(ch == '<') { return simple_token(token_kind::less, position); }
    if(ch == '>') { return simple_token(token_kind::greater, position); }
    return simple_token(token_kind::invalid, position);
}

lex_all(input: str) -> vector<token>
{
    let state = lexer_state{ .input = input, .index = 0 as usize };
    let output = vector<token>{};
    let done = false;
    while(not done) {
        let item = next_token(ref state);
        output.push_back(item);
        done = item.kind == token_kind::file_end;
    }
    return move output;
}

peek_kind(state: parser_state&) -> token_kind
{
    return state.tokens[state.index].kind;
}

accept(state: parser_state&, kind: token_kind) -> bool
{
    if(peek_kind(ref state) == kind) {
        state.index += 1;
        return true;
    }
    return false;
}

parse_expression_level(state: parser_state&, level: i32) -> bool
{
    if(level == 6) {
        if(accept(ref state, token_kind::plus) or accept(ref state, token_kind::minus)) {
            return parse_expression_level(ref state, 6);
        }
        if(accept(ref state, token_kind::integer)) {
            return true;
        }
        if(accept(ref state, token_kind::identifier)) {
            if(accept(ref state, token_kind::lparen)) {
                if(peek_kind(ref state) != token_kind::rparen) {
                    if(not parse_expression_level(ref state, 0)) {
                        return false;
                    }
                    while(accept(ref state, token_kind::comma)) {
                        if(not parse_expression_level(ref state, 0)) {
                            return false;
                        }
                    }
                }
                return accept(ref state, token_kind::rparen);
            }
            return true;
        }
        if(accept(ref state, token_kind::lparen)) {
            if(not parse_expression_level(ref state, 0)) {
                return false;
            }
            return accept(ref state, token_kind::rparen);
        }
        return false;
    }

    if(not parse_expression_level(ref state, level + 1)) {
        return false;
    }
    let scanning = true;
    while(scanning) {
        scanning = false;
        if(level == 0 and accept(ref state, token_kind::or_or)) { scanning = true; }
        if(level == 1 and accept(ref state, token_kind::and_and)) { scanning = true; }
        if(level == 2 and (accept(ref state, token_kind::equal_equal) or accept(ref state, token_kind::bang_equal))) { scanning = true; }
        if(level == 3 and (accept(ref state, token_kind::less) or accept(ref state, token_kind::less_equal) or accept(ref state, token_kind::greater) or accept(ref state, token_kind::greater_equal))) { scanning = true; }
        if(level == 4 and (accept(ref state, token_kind::plus) or accept(ref state, token_kind::minus))) { scanning = true; }
        if(level == 5 and (accept(ref state, token_kind::star) or accept(ref state, token_kind::slash) or accept(ref state, token_kind::percent))) { scanning = true; }
        if(scanning and not parse_expression_level(ref state, level + 1)) {
            return false;
        }
    }
    return true;
}

parse_params(state: parser_state&) -> bool
{
    if(peek_kind(ref state) == token_kind::rparen) {
        return true;
    }
    if(not accept(ref state, token_kind::kw_int)) {
        return false;
    }
    if(not accept(ref state, token_kind::identifier)) {
        return false;
    }
    while(accept(ref state, token_kind::comma)) {
        if(not accept(ref state, token_kind::kw_int)) {
            return false;
        }
        if(not accept(ref state, token_kind::identifier)) {
            return false;
        }
    }
    return true;
}

parse_construct(state: parser_state&, symbol: i32) -> bool
{
    if(symbol == 0) {
        while(peek_kind(ref state) != token_kind::file_end) {
            if(not parse_construct(ref state, 1)) {
                return false;
            }
        }
        return accept(ref state, token_kind::file_end);
    }
    if(symbol == 1) {
        if(not (accept(ref state, token_kind::kw_int) or accept(ref state, token_kind::kw_void))) {
            return false;
        }
        if(not accept(ref state, token_kind::identifier)) {
            return false;
        }
        if(not accept(ref state, token_kind::lparen)) {
            return false;
        }
        if(not parse_params(ref state)) {
            return false;
        }
        if(not accept(ref state, token_kind::rparen)) {
            return false;
        }
        return parse_construct(ref state, 2);
    }
    if(symbol == 2) {
        if(not accept(ref state, token_kind::lbrace)) {
            return false;
        }
        while(peek_kind(ref state) != token_kind::rbrace and peek_kind(ref state) != token_kind::file_end) {
            if(not parse_construct(ref state, 3)) {
                return false;
            }
        }
        return accept(ref state, token_kind::rbrace);
    }

    if(accept(ref state, token_kind::kw_int)) {
        if(not accept(ref state, token_kind::identifier)) {
            return false;
        }
        if(accept(ref state, token_kind::assign) and not parse_expression_level(ref state, 0)) {
            return false;
        }
        return accept(ref state, token_kind::semicolon);
    }
    if(accept(ref state, token_kind::identifier)) {
        if(accept(ref state, token_kind::assign)) {
            if(not parse_expression_level(ref state, 0)) {
                return false;
            }
            return accept(ref state, token_kind::semicolon);
        }
        if(accept(ref state, token_kind::lparen)) {
            if(peek_kind(ref state) != token_kind::rparen) {
                if(not parse_expression_level(ref state, 0)) {
                    return false;
                }
                while(accept(ref state, token_kind::comma)) {
                    if(not parse_expression_level(ref state, 0)) {
                        return false;
                    }
                }
            }
            return accept(ref state, token_kind::rparen) and accept(ref state, token_kind::semicolon);
        }
        return false;
    }
    if(accept(ref state, token_kind::kw_if)) {
        if(not accept(ref state, token_kind::lparen)) { return false; }
        if(not parse_expression_level(ref state, 0)) { return false; }
        if(not accept(ref state, token_kind::rparen)) { return false; }
        if(not parse_construct(ref state, 2)) { return false; }
        if(accept(ref state, token_kind::kw_else)) {
            return parse_construct(ref state, 2);
        }
        return true;
    }
    if(accept(ref state, token_kind::kw_while)) {
        if(not accept(ref state, token_kind::lparen)) { return false; }
        if(not parse_expression_level(ref state, 0)) { return false; }
        if(not accept(ref state, token_kind::rparen)) { return false; }
        return parse_construct(ref state, 2);
    }
    if(accept(ref state, token_kind::kw_return)) {
        if(peek_kind(ref state) != token_kind::semicolon and not parse_expression_level(ref state, 0)) {
            return false;
        }
        return accept(ref state, token_kind::semicolon);
    }
    if(peek_kind(ref state) == token_kind::lbrace) {
        return parse_construct(ref state, 2);
    }
    return false;
}

parse_program(input: str) -> bool
{
    let tokens = lex_all(input);
    let state = parser_state{ .tokens = move tokens, .index = 0 as usize };
    return parse_construct(ref state, 0);
}

check_grammar_metadata() -> i32
{
    let stmt_first = set<i32>{};
    stmt_first.insert(2);
    stmt_first.insert(4);
    stmt_first.insert(6);
    stmt_first.insert(7);
    stmt_first.insert(8);
    stmt_first.insert(12);
    if(stmt_first.size() != 6 or stmt_first.rank(8) != 4 or not stmt_first.contains(12)) {
        return 0;
    }
    let expr_first = set<i32>{};
    expr_first.insert(8);
    expr_first.insert(9);
    expr_first.insert(10);
    expr_first.insert(16);
    expr_first.insert(17);
    return stmt_first.size() as i32 + expr_first.size() as i32;
}

check_lr1_item_sets() -> i32
{
    let closure = set<lr1_item>{};
    closure.insert(lr1_item{ .rule = 3, .dot = 0, .lookahead = 15 });
    closure.insert(lr1_item{ .rule = 1, .dot = 1, .lookahead = 0 });
    closure.insert(lr1_item{ .rule = 1, .dot = 1, .lookahead = 13 });
    closure.insert(lr1_item{ .rule = 1, .dot = 1, .lookahead = 13 });
    let first = closure.nth(0 as usize);
    if(closure.size() != 3 or first.rule != 1 or first.lookahead != 0) {
        return 0;
    }
    let goto_by_symbol = map<i32, set<lr1_item>>{};
    goto_by_symbol[8].insert(lr1_item{ .rule = 1, .dot = 2, .lookahead = 0 });
    goto_by_symbol[8].insert(lr1_item{ .rule = 1, .dot = 2, .lookahead = 13 });
    if(goto_by_symbol[8].size() != 2) {
        return 0;
    }
    return closure.rank(lr1_item{ .rule = 3, .dot = 0, .lookahead = 15 }) as i32 + goto_by_symbol[8].size() as i32;
}

emit(quads: vector<quad>&, op: char, left: i32, right: i32) -> i32
{
    let id = quads.size() as i32;
    quads.push_back(quad{ .op = op, .left = left, .right = right, .result = id });
    return id;
}

eval_quads(quads: vector<quad>&) -> i32
{
    let registers = vector<i32>{quads.size(), 0};
    let index: usize = 0;
    while(index < quads.size()) {
        let item = quads[index];
        if(item.op == '+') {
            registers[item.result as usize] = item.left + item.right;
        } else {
            registers[item.result as usize] = item.left * item.right;
        }
        index += 1;
    }
    return registers[0 as usize] + registers[1 as usize] - registers.size() as i32 - 1;
}

check_intermediate_code() -> i32
{
    let quads = vector<quad>{};
    let left = emit(ref quads, '+', 2, 3);
    let right = emit(ref quads, '*', 4, 5);
    if(left != 0 or right != 1 or quads.size() != 2) {
        return 0;
    }
    return eval_quads(ref quads);
}

main() -> i32
{
    let source = "/* miniC */ int add(int a, int b) { return a + b; } int main() { int x = 1; int y = 2; int z = add(x, y); if (z > 2) { z = z * 10; } else { z = 0; } while (z > 0) { z = z - 1; } return z; }";
    let tokens = lex_all(source);
    if(tokens.size() < 60 or tokens[0 as usize].kind != token_kind::kw_int or tokens[tokens.size() - 1].kind != token_kind::file_end) {
        return 1;
    }
    if(not parse_program(source)) {
        return 2;
    }
    if(parse_program("int main( { return 0; }")) {
        return 3;
    }
    if(check_grammar_metadata() != 11) {
        return 4;
    }
    if(check_lr1_item_sets() != 4) {
        return 5;
    }
    if(check_intermediate_code() != 22) {
        return 6;
    }
    return 42;
}
)"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile miniC compiler-lab workload binary");
    test_parser::assert_true(
        exit_code(run_status({ app.string() })) == 42,
        "miniC compiler-lab workload should cover lexer, recursive descent LL(1), LR(1) items, and quads"
    );
}

auto check_compiler_lab_ll1_sets_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("compiler-lab-ll1-sets");
    auto source = dir / "ll1_sets.cp";
    auto app = dir / "ll1_sets";
    write_source(
        source,
        R"(import std;

struct production {
    lhs: i32;
    a: i32;
    b: i32;
    c: i32;
    d: i32;
    len: i32;
}

impl production {
    production()
    {
        return production{ .lhs = 0, .a = 0, .b = 0, .c = 0, .d = 0, .len = 0 };
    }
}

symbol_at(rule: production const&, index: i32) -> i32
{
    if(index == 0) { return rule.a; }
    if(index == 1) { return rule.b; }
    if(index == 2) { return rule.c; }
    return rule.d;
}

insert_changed(values: set<i32>&, value: i32) -> bool
{
    let before = values.size();
    values.insert(value);
    return values.size() != before;
}

add_first_symbol(first: map<i32, set<i32>>&, symbol: i32, target: set<i32>&) -> bool
{
    let changed = false;
    if(symbol < 100) {
        changed = insert_changed(ref target, symbol);
    } else {
        let values = first[symbol];
        let index: usize = 0;
        while(index < values.size()) {
            let item = values.nth(index);
            if(item != -1 and insert_changed(ref target, item)) {
                changed = true;
            }
            index += 1;
        }
    }
    return changed;
}

symbol_nullable(first: map<i32, set<i32>>&, symbol: i32) -> bool
{
    if(symbol < 100) {
        return symbol == -1;
    }
    return first[symbol].contains(-1);
}

add_first_sequence(first: map<i32, set<i32>>&, rule: production const&, start: i32, target: set<i32>&) -> bool
{
    let changed = false;
    if(start >= rule.len) {
        return insert_changed(ref target, -1);
    }
    let index = start;
    while(index < rule.len) {
        let symbol = symbol_at(rule, index);
        if(add_first_symbol(ref first, symbol, ref target)) {
            changed = true;
        }
        if(not symbol_nullable(ref first, symbol)) {
            return changed;
        }
        index += 1;
    }
    if(insert_changed(ref target, -1)) {
        changed = true;
    }
    return changed;
}

compute_first(rules: vector<production>&) -> map<i32, set<i32>>
{
    let first = map<i32, set<i32>>{};
    let changed = true;
    while(changed) {
        changed = false;
        let index: usize = 0;
        while(index < rules.size()) {
            let rule = rules[index];
            if(add_first_sequence(ref first, rule, 0, ref first[rule.lhs])) {
                changed = true;
            }
            index += 1;
        }
    }
    return move first;
}

compute_follow(rules: vector<production>&, first: map<i32, set<i32>>&) -> map<i32, set<i32>>
{
    let follow = map<i32, set<i32>>{};
    follow[100].insert(0);
    let changed = true;
    while(changed) {
        changed = false;
        let rule_index: usize = 0;
        while(rule_index < rules.size()) {
            let rule = rules[rule_index];
            let rhs_index = 0;
            while(rhs_index < rule.len) {
                let symbol = symbol_at(rule, rhs_index);
                if(symbol >= 100) {
                    if(add_first_sequence(ref first, rule, rhs_index + 1, ref follow[symbol])) {
                        if(follow[symbol].contains(-1)) {
                            follow[symbol].remove(-1);
                        } else {
                            changed = true;
                        }
                    }
                    if(rhs_index + 1 >= rule.len or symbol_nullable(ref first, symbol_at(rule, rhs_index + 1))) {
                        let lhs_follow = follow[rule.lhs];
                        let index: usize = 0;
                        while(index < lhs_follow.size()) {
                            if(insert_changed(ref follow[symbol], lhs_follow.nth(index))) {
                                changed = true;
                            }
                            index += 1;
                        }
                    }
                }
                rhs_index += 1;
            }
            rule_index += 1;
        }
    }
    return move follow;
}

select_size(rule: production const&, first: map<i32, set<i32>>&, follow: map<i32, set<i32>>&) -> i32
{
    let selected = set<i32>{};
    add_first_sequence(ref first, rule, 0, ref selected);
    if(selected.contains(-1)) {
        selected.remove(-1);
        let values = follow[rule.lhs];
        let index: usize = 0;
        while(index < values.size()) {
            selected.insert(values.nth(index));
            index += 1;
        }
    }
    return selected.size() as i32;
}

main() -> i32
{
    let rules = vector<production>{};
    rules.push_back(production{ .lhs = 100, .a = 1, .b = 101, .c = 2, .d = 0, .len = 3 });
    rules.push_back(production{ .lhs = 101, .a = 4, .b = 5, .c = 102, .d = 0, .len = 3 });
    rules.push_back(production{ .lhs = 101, .a = 0, .b = 0, .c = 0, .d = 0, .len = 0 });
    rules.push_back(production{ .lhs = 102, .a = 3, .b = 4, .c = 5, .d = 102, .len = 4 });
    rules.push_back(production{ .lhs = 102, .a = 0, .b = 0, .c = 0, .d = 0, .len = 0 });

    let first = compute_first(ref rules);
    let follow = compute_follow(ref rules, ref first);
    if(not first[101].contains(4) or not first[101].contains(-1)) { return 1; }
    if(not first[102].contains(3) or not first[102].contains(-1)) { return 2; }
    if(not follow[101].contains(2) or not follow[102].contains(2)) { return 3; }
    return select_size(rules[1], ref first, ref follow)
        + select_size(rules[2], ref first, ref follow)
        + select_size(rules[3], ref first, ref follow)
        + select_size(rules[4], ref first, ref follow)
        + 37;
}
)"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile LL(1) FIRST/FOLLOW/SELECT workload");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 41, "LL(1) workload should compute expected SELECT sizes");
}

auto check_compiler_lab_semantic_scope_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("compiler-lab-semantic-scope");
    auto source = dir / "semantic_scope.cp";
    auto app = dir / "semantic_scope";
    write_source(
        source,
        R"(import std;

struct symbol {
    ty: i32;
    depth: i32;
}

struct scope {
    symbols: map<i32, symbol>;
}

struct analyzer {
    scopes: vector<scope>;
    diagnostics: i32;
}

impl symbol {
    symbol()
    {
        return symbol{ .ty = -1, .depth = -1 };
    }
}

impl scope {
    scope() = default;
}

impl analyzer {
    analyzer()
    {
        return analyzer{ .scopes = vector<scope>{}, .diagnostics = 0 };
    }

    enter_scope(self&) -> void
    {
        scopes.push_back(scope{});
    }

    leave_scope(self&) -> void
    {
        scopes.pop_back();
    }

    declare(self&, name: i32, ty: i32) -> bool
    {
        let index = scopes.size() - 1;
        if(scopes[index].symbols.contains(name)) {
            diagnostics += 1;
            return false;
        }
        scopes[index].symbols[name] = symbol{ .ty = ty, .depth = index as i32 };
        return true;
    }

    lookup_type(self&, name: i32) -> i32
    {
        let index = scopes.size();
        while(index > 0) {
            index -= 1;
            if(scopes[index].symbols.contains(name)) {
                return scopes[index].symbols.at(name).ty;
            }
        }
        diagnostics += 1;
        return -1;
    }

    check_return(self&, expected: i32, actual: i32) -> void
    {
        if(expected != actual) {
            diagnostics += 1;
        }
    }
}

main() -> i32
{
    let sema = analyzer{};
    sema.enter_scope();
    if(not sema.declare(1, 10)) { return 1; }
    if(sema.declare(1, 10)) { return 2; }
    sema.enter_scope();
    if(not sema.declare(1, 20)) { return 3; }
    if(sema.lookup_type(1) != 20) { return 4; }
    if(sema.lookup_type(2) != -1) { return 5; }
    sema.check_return(10, sema.lookup_type(1));
    sema.leave_scope();
    if(sema.lookup_type(1) != 10) { return 6; }
    sema.check_return(10, sema.lookup_type(1));
    if(sema.diagnostics != 3) { return 7; }
    return 42;
}
)"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile semantic analyzer scope workload");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 42, "semantic scope workload should handle shadowing and diagnostics");
}

auto check_compiler_lab_lr1_table_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("compiler-lab-lr1-table");
    auto source = dir / "lr1_table.cp";
    auto app = dir / "lr1_table";
    write_source(
        source,
        R"(import std;

struct table_key {
    state: i32;
    symbol: i32;
}

struct lr1_item {
    rule: i32;
    dot: i32;
    lookahead: i32;
}

variant action {
    shift(i32);
    reduce(i32);
    accept;
    error;
}

impl table_key {
    operator <=>(self const&, rhs: table_key const&) -> weak_ordering
    {
        if(state != rhs.state) {
            return state <=> rhs.state;
        }
        return symbol <=> rhs.symbol;
    }
}

impl lr1_item {
    operator <=>(self const&, rhs: lr1_item const&) -> weak_ordering
    {
        if(rule != rhs.rule) { return rule <=> rhs.rule; }
        if(dot != rhs.dot) { return dot <=> rhs.dot; }
        return lookahead <=> rhs.lookahead;
    }
}

score(act: action const&) -> i32
{
    return match act {
        .shift(state) => state * 10,
        .reduce(rule) => rule,
        .accept => 100,
        .error => -100,
    };
}

main() -> i32
{
    let closure = set<lr1_item>{};
    closure.insert(lr1_item{ .rule = 0, .dot = 0, .lookahead = 0 });
    closure.insert(lr1_item{ .rule = 1, .dot = 0, .lookahead = 0 });
    closure.insert(lr1_item{ .rule = 1, .dot = 0, .lookahead = 0 });
    if(closure.size() != 2) { return 1; }

    let actions = map<table_key, action>{};
    actions.insert(table_key{ .state = 0, .symbol = 1 }, action::shift(2));
    actions.insert(table_key{ .state = 2, .symbol = 0 }, action::reduce(1));
    actions.insert(table_key{ .state = 1, .symbol = 0 }, action::accept);

    let gotos = map<table_key, i32>{};
    gotos.insert(table_key{ .state = 0, .symbol = 100 }, 1);

    if(actions.size() != 3 or gotos.at(table_key{ .state = 0, .symbol = 100 }) != 1) {
        return 2;
    }
    return score(actions.at(table_key{ .state = 0, .symbol = 1 }))
        + score(actions.at(table_key{ .state = 2, .symbol = 0 }))
        + score(actions.at(table_key{ .state = 1, .symbol = 0 }))
        + closure.rank(lr1_item{ .rule = 1, .dot = 0, .lookahead = 0 }) as i32
        + 20;
}
)"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile LR(1) action/goto table workload");
    test_parser::assert_true(exit_code(run_status({ app.string() })) == 142, "LR(1) table workload should preserve action and item ordering");
}

auto check_panic_assert_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("panic-assert");

    auto assert_true_source = dir / "assert_true.cp";
    auto assert_true_app = dir / "assert_true";
    write_source(
        assert_true_source,
        R"(main() -> i32
{
    assert(true);
    return 42;
})"
    );
    auto assert_true_status = compile(tools, { assert_true_source.string(), "-o", assert_true_app.string() });
    test_parser::assert_true(assert_true_status == 0, "cp should compile assert(true)");
    test_parser::assert_true(exit_code(run_status({ assert_true_app.string() })) == 42, "assert(true) should continue");

    auto panic_source = dir / "panic.cp";
    auto panic_app = dir / "panic";
    auto panic_stderr = dir / "panic.stderr";
    write_source(
        panic_source,
        R"(main() -> i32
{
    panic("boom");
})"
    );
    auto panic_status = compile(tools, { panic_source.string(), "-o", panic_app.string() });
    test_parser::assert_true(panic_status == 0, "cp should compile panic binary");
    test_parser::assert_true(exit_code(run_stderr({ panic_app.string() }, panic_stderr)) != 0, "panic binary should fail");
    test_parser::assert_true(read_text(panic_stderr).contains("panic: boom"), "panic should write message to stderr");

    auto assert_false_source = dir / "assert_false.cp";
    auto assert_false_app = dir / "assert_false";
    auto assert_false_stderr = dir / "assert_false.stderr";
    write_source(
        assert_false_source,
        R"(main() -> i32
{
    assert(false, "bad");
    return 0;
})"
    );
    auto assert_false_status = compile(tools, { assert_false_source.string(), "-o", assert_false_app.string() });
    test_parser::assert_true(assert_false_status == 0, "cp should compile assert(false)");
    test_parser::assert_true(exit_code(run_stderr({ assert_false_app.string() }, assert_false_stderr)) != 0, "assert(false) should fail");
    test_parser::assert_true(read_text(assert_false_stderr).contains("panic: bad"), "assert(false) should write message to stderr");

    auto release_assert_app = dir / "release_assert_false";
    auto release_assert_status = compile(tools, { "--release", assert_false_source.string(), "-o", release_assert_app.string() });
    test_parser::assert_true(release_assert_status == 0, "cp --release should compile assert(false)");
    test_parser::assert_true(exit_code(run_status({ release_assert_app.string() })) == 0, "cp --release should elide assert(false)");

    auto side_effect_source = dir / "release_assert_side_effect.cp";
    auto side_effect_app = dir / "release_assert_side_effect";
    write_source(
        side_effect_source,
        R"(mark(value: i32&) -> bool
{
    value += 1;
    return false;
}

main() -> i32
{
    let count = 0;
    assert(mark(count), "side effect");
    return count + 42;
})"
    );
    auto side_effect_status = compile(tools, { "--release", side_effect_source.string(), "-o", side_effect_app.string() });
    test_parser::assert_true(side_effect_status == 0, "cp --release should compile assert side effect binary");
    test_parser::assert_true(exit_code(run_status({ side_effect_app.string() })) == 42, "cp --release should not evaluate assert arguments");

    auto release_ll = dir / "release_assert.ll";
    auto release_ll_status = compile(tools, { "--release", assert_false_source.string(), "--emit", "ll", "-o", release_ll.string() });
    test_parser::assert_true(release_ll_status == 0, "cp --release should emit LLVM IR for assert(false)");
    auto release_ll_text = read_text(release_ll);
    test_parser::assert_true(not release_ll_text.contains("assert.fail"), "cp --release LLVM IR should not contain assert fail blocks");
    test_parser::assert_true(not release_ll_text.contains("bad"), "cp --release LLVM IR should not contain elided assert messages");

    auto verbose_app = dir / "release_verbose";
    auto verbose_stderr = dir / "release_verbose.stderr";
    auto verbose_status = run_stderr(
        { tools.cp.string(), "--release", "--verbose", assert_true_source.string(), "-o", verbose_app.string() },
        verbose_stderr
    );
    test_parser::assert_true(verbose_status == 0, "cp --release --verbose should compile");
    auto verbose_text = read_text(verbose_stderr);
    test_parser::assert_true(verbose_text.contains("-O3"), "cp --release should pass -O3 to clang");
    test_parser::assert_true(verbose_text.contains("-DNDEBUG"), "cp --release should pass -DNDEBUG to clang");
}

auto check_checked_index_panic_binary(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("checked-index");
    auto source = dir / "checked_index.cp";
    auto app = dir / "checked_index";
    auto stderr_path = dir / "checked_index.stderr";
    write_source(
        source,
        R"(import std;

main() -> i32
{
    let text = string{"hi"};
    let index = 3 as usize;
    let ch = text[index];
    return 0;
})"
    );

    auto status = compile(tools, { source.string(), "-o", app.string() });
    test_parser::assert_true(status == 0, "cp should compile checked string indexing");
    test_parser::assert_true(exit_code(run_stderr({ app.string() }, stderr_path)) != 0, "out-of-bounds string index should panic");
    test_parser::assert_true(read_text(stderr_path).contains("string index out of bounds"), "index panic should use contract message");

    auto array_source = dir / "checked_array.cp";
    auto array_app = dir / "checked_array";
    auto array_stderr = dir / "checked_array.stderr";
    write_source(
        array_source,
        R"(main() -> i32
{
    let values = [1, 2];
    let index = 2;
    return values[index];
})"
    );
    auto array_status = compile(tools, { array_source.string(), "-o", array_app.string() });
    test_parser::assert_true(array_status == 0, "cp should compile checked array indexing");
    test_parser::assert_true(exit_code(run_stderr({ array_app.string() }, array_stderr)) != 0, "out-of-bounds array index should panic");
    test_parser::assert_true(read_text(array_stderr).contains("array index out of bounds"), "array index panic should use contract message");
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
    check_short_circuit_binary(tools);
    check_extern_c_binary(tools);
    check_member_default_argument_binary(tools);
    check_struct_field_default_binary(tools);
    check_ufcs_receiver_conversion_binary(tools);
    check_emit_ll(tools);
    check_emit_obj(tools);
    check_multi_input(tools);
    check_error_rejects_output(tools);
    check_keep_ll(tools);
    check_keep_obj(tools);
    check_sum_non_negative(tools);
    check_array_index_binary(tools);
    check_tuple_member_binary(tools);
    check_accumulate_and_countdown(tools);
    check_labeled_for_binary(tools);
    check_types_example_emit_ll(tools);
    check_fixture_examples(tools);
    check_const_double_pointer_reference_alias(tools);
    check_mutable_double_pointer_reference_write(tools);
    check_i64_triple_pointer_read_write(tools);
    check_i64_pointer_reference_slot_alias(tools);
    check_reference_parameter_alias(tools);
    check_reference_return_value_binding(tools);
    check_destructor_cleanup_paths(tools);
    check_return_destructor_ir(tools);
    check_nrvo_return_skips_returned_local_destructor(tools);
    check_function_pointer_memory_binary(tools);
    check_storage_memory_binary(tools);
    check_pointer_difference_binary(tools);
    check_update_expression_binary(tools);
    check_decltype_ref_destructure_binary(tools);
    check_lambda_binary(tools);
    check_nested_inferred_lambda_binary(tools);
    check_generic_lambda_binary(tools);
    check_generic_struct_binary(tools);
    check_const_generic_struct_binary(tools);
    check_generic_function_binary(tools);
    check_inferred_parameter_generic_binary(tools);
    check_const_generic_array_binary(tools);
    check_imported_generic_function_binary(tools);
    check_parameter_pack_binary(tools);
    check_direct_iterator_consumes_original_binary(tools);
    check_string_index_binary(tools);
    check_string_iteration_binary(tools);
    check_string_compare_binary(tools);
    check_string_copy_move_binary(tools);
    check_io_print_binary(tools);
    check_io_nul_string_binary(tools);
    check_io_format_errors_binary(tools);
    check_file_io_binary(tools);
    check_operator_overload_binary(tools);
    check_builtin_operator_escape_binary(tools);
    check_update_operator_overload_binary(tools);
    check_std_sort_binary(tools);
    check_forward_reference_binary(tools);
    check_iota_public_import_binary(tools);
    check_iota_custom_incrementable_binary(tools);
    check_std_map_set_binary(tools);
    check_std_map_set_btree_stress_binary(tools);
    check_compiler_lab_workload_binary(tools);
    check_compiler_lab_ll1_sets_binary(tools);
    check_compiler_lab_semantic_scope_binary(tools);
    check_compiler_lab_lr1_table_binary(tools);
    check_panic_assert_binary(tools);
    check_checked_index_panic_binary(tools);
    return 0;
}
