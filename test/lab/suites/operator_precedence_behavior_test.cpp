#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "assert.hpp"

namespace {

struct test_tools
{
    std::filesystem::path cp{};
    std::filesystem::path lab_root{};
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
    auto output = std::string{};
    for(auto const& argument : arguments) {
        if(not output.empty()) {
            output += ' ';
        }
        output += shell_quote(argument);
    }
    return output;
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

auto lab_test_links_gcov() -> bool
{
    auto const* value = std::getenv("CP_COMPILER_TEST_LINK_GCOV");
    return value != nullptr and std::string_view{ value } == "1";
}

auto unique_temp_dir(std::string_view name) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / std::format("cp-lab-op-test-{}-{}", name, stamp);
    std::filesystem::create_directories(path);
    return path;
}

auto module_sources(test_tools const& tools) -> std::vector<std::string>
{
    return {
        (tools.lab_root / "source/source.cp").string(),
        (tools.lab_root / "diagnostic/stage.cp").string(),
        (tools.lab_root / "diagnostic/kind.cp").string(),
        (tools.lab_root / "diagnostic/severity.cp").string(),
        (tools.lab_root / "diagnostic/record.cp").string(),
        (tools.lab_root / "diagnostic/catalog.cp").string(),
        (tools.lab_root / "diagnostic/collector.cp").string(),
        (tools.lab_root / "diagnostic/diagnostic.cp").string(),
        (tools.lab_root / "preprocessor/preprocessor.cp").string(),
        (tools.lab_root / "lexer/token.cp").string(),
        (tools.lab_root / "lexer/charset.cp").string(),
        (tools.lab_root / "lexer/keyword.cp").string(),
        (tools.lab_root / "lexer/state.cp").string(),
        (tools.lab_root / "lexer/lexer.cp").string()
    };
}

auto compile_main(test_tools const& tools, std::filesystem::path const& output) -> int
{
    auto arguments = std::vector<std::string>{ tools.cp.string() };
    auto modules = module_sources(tools);
    arguments.insert(arguments.end(), modules.begin(), modules.end());
    arguments.push_back((tools.lab_root / "parser/op/main.cp").string());
    arguments.push_back("-o");
    arguments.push_back(output.string());
    if(lab_test_links_gcov()) {
        arguments.emplace_back("--link-arg");
        arguments.emplace_back("-lgcov");
    }
    return run_status(arguments);
}

auto check_main(test_tools const& tools) -> void
{
    auto dir = unique_temp_dir("main");
    auto app = dir / "operator-precedence-main";
    auto status = compile_main(tools, app);
    test_parser::assert_true(exit_code(status) == 0, "operator-precedence main should compile");
    status = run_status({ app.string() });
    test_parser::assert_true(exit_code(status) == 0, "operator-precedence main should pass");
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc != 3) {
        test_parser::fail("usage: lab_operator_precedence_behavior_test <cp> <lab-root>");
    }

    auto tools = test_tools{
        .cp = argv[1],
        .lab_root = argv[2]
    };
    check_main(tools);
    return 0;
}
