import std;

#include "assert.hpp"

namespace {

struct test_tools
{
    std::filesystem::path cp{};
    std::filesystem::path clang{};
    std::filesystem::path llvm_as{};
    std::filesystem::path examples{};
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

auto check_types_demo_emit_ll(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("types-demo");
    auto output = dir / "types.ll";
    auto bitcode = dir / "types.bc";
    auto source = tools.examples / "types_demo.cp";

    auto status = compile(tools, { source.string(), "--emit", "ll", "-o", output.string() });
    test_parser::assert_true(status == 0, "cp should emit LLVM IR for types_demo.cp");
    test_parser::assert_true(
        run_status({ tools.llvm_as.string(), output.string(), "-o", bitcode.string() }) == 0,
        "llvm-as should accept types_demo LLVM IR"
    );
}

auto check_design_examples(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("design-examples");
    auto output = dir / "examples.ll";
    auto bitcode = dir / "examples.bc";
    auto app = dir / "examples";
    auto math = tools.examples / "math.cp";
    auto flow = tools.examples / "flow_demo.cp";
    auto types = tools.examples / "types_demo.cp";
    auto main = tools.examples / "main.cp";

    auto ll_status = compile(
        tools,
        { math.string(), flow.string(), types.string(), main.string(), "--emit", "ll", "-o", output.string() }
    );
    test_parser::assert_true(ll_status == 0, "cp should emit LLVM IR for all design examples");
    test_parser::assert_true(
        run_status({ tools.llvm_as.string(), output.string(), "-o", bitcode.string() }) == 0,
        "llvm-as should accept design examples LLVM IR"
    );

    auto bin_status = compile(
        tools,
        { math.string(), flow.string(), types.string(), main.string(), "-o", app.string() }
    );
    test_parser::assert_true(bin_status == 0, "cp should emit binary for all design examples");
    test_parser::assert_true(std::filesystem::exists(app), "design examples binary should exist");
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
} // namespace

auto main(int argc, char** argv) -> int
{
    test_parser::assert_true(argc == 5, "compiler behavior test expects cp, clang, llvm-as, and examples paths");
    auto tools = test_tools {
        .cp = argv[1],
        .clang = argv[2],
        .llvm_as = argv[3],
        .examples = argv[4],
    };

    check_binary_exit(tools);
    check_emit_ll(tools);
    check_emit_obj(tools);
    check_multi_input(tools);
    check_error_rejects_output(tools);
    check_keep_ll(tools);
    check_keep_obj(tools);
    check_sum_non_negative(tools);
    check_accumulate_and_countdown(tools);
    check_labeled_for_binary(tools);
    check_types_demo_emit_ll(tools);
    check_design_examples(tools);
    check_const_double_pointer_reference_alias(tools);
    check_mutable_double_pointer_reference_write(tools);
    check_i64_triple_pointer_read_write(tools);
    check_i64_pointer_reference_slot_alias(tools);
    check_reference_parameter_alias(tools);
    return 0;
}
