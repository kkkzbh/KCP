import std;
import compiler.import_resolver;

#include "assert.hpp"

namespace {
auto unique_temp_dir(std::string_view name) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / std::format("cp-import-resolver-test-{}-{}", name, stamp);
    std::filesystem::create_directories(path);
    return path;
}

auto write_source(std::filesystem::path const& path, std::string_view text) -> void
{
    std::filesystem::create_directories(path.parent_path());
    auto stream = std::ofstream{ path };
    test_parser::assert_true(stream.is_open(), std::format("should open {}", path.string()));
    stream << text;
    test_parser::assert_true(static_cast<bool>(stream), std::format("should write {}", path.string()));
}

auto find_path(cp_imports::resolve_result const& result, std::filesystem::path const& path) -> std::size_t
{
    auto normalized = cp_imports::normalize_path(path);
    for(auto index = 0uz; index < result.files.size(); ++index) {
        if(result.files[index].path == normalized) {
            return index;
        }
    }
    test_parser::fail(std::format("resolved files should include {}", normalized));
}
} // namespace

auto main() -> int
{
    auto root = unique_temp_dir("workspace");
    auto app_dir = root / "app";
    auto main_path = app_dir / "main.cp";
    auto local_path = app_dir / "local.cp";
    auto math_path = root / "math" / "core.cp";
    auto parser_path = root / "parser" / "lr" / "parser.cp";
    auto ignored_path = root / "build" / "ignored.cp";

    write_source(
        main_path,
        R"(import local;
import math.core;
import parser.lr;

main() -> i32
{
    return local_value() + add(20, 20) + parse();
})");
    write_source(local_path, "export module local; export local_value() -> i32 { return 1; }");
    write_source(math_path, "export module math.core; export add(x: i32, y: i32) -> i32 { return x + y; }");
    write_source(parser_path, "export module parser.lr; export parse() -> i32 { return 1; }");
    write_source(ignored_path, "export module ignored; ignored() -> i32 { return 0; }");

    auto resolved = cp_imports::resolve_source_files(cp_imports::resolve_request {
        .active_file = main_path.string(),
        .target_file = main_path.string(),
        .import_roots = { root.string() },
        .search_roots = { root.string(), root.string() },
        .follow_stdlib_imports = true,
    });

    auto local_index = find_path(resolved, local_path);
    auto math_index = find_path(resolved, math_path);
    auto parser_index = find_path(resolved, parser_path);
    auto main_index = find_path(resolved, main_path);
    test_parser::assert_true(local_index < main_index, "direct sibling import should be ordered before the active file");
    test_parser::assert_true(math_index < main_index, "hierarchical import-root source should be ordered before the active file");
    test_parser::assert_true(parser_index < main_index, "folder aggregate import-root source should be ordered before the active file");
    test_parser::assert_true(
        std::ranges::none_of(resolved.files, [&](cp_imports::source_file const& file) {
            return file.path == cp_imports::normalize_path(ignored_path);
        }),
        "search discovery should skip build directories");

    auto std_main = root / "std_main.cp";
    write_source(std_main, "import std.io; main() -> i32 { return 0; }");
    auto std_skipped = cp_imports::resolve_source_files(cp_imports::resolve_request {
        .active_file = std_main.string(),
        .target_file = std_main.string(),
        .import_roots = { root.string() },
        .follow_stdlib_imports = false,
    });
    test_parser::assert_true(std_skipped.files.size() == 1uz, "stdlib imports should be skipped when requested");

    return 0;
}
