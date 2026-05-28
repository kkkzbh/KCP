export module compiler.import_resolver;

import std;
import source;
import preprocessor;
import lexer;
import parser;

export namespace cp_imports {

struct source_file
{
    std::string path;
    std::string text;
};

struct resolve_request
{
    std::string active_file;
    std::string target_file;
    std::vector<std::string> entry_files;
    std::vector<std::string> import_roots;
    std::vector<std::string> search_roots;
    std::vector<source_file> files;
    bool follow_stdlib_imports{ true };
};

struct resolve_result
{
    std::string active_file;
    std::vector<source_file> files;
};

auto normalize_path(std::filesystem::path const& path) -> std::string;
auto resolve_source_files(resolve_request request) -> resolve_result;

} // namespace cp_imports

namespace cp_imports {
namespace {

struct module_source
{
    source_file file;
    std::optional<std::string> exported_module;
    std::vector<std::string> imported_modules;
};

enum class module_scope : std::uint8_t
{
    directory,
    import_root,
};

enum class module_layout : std::uint8_t
{
    hierarchical,
    folder_aggregate,
    aggregate,
    stdlib_hierarchical,
    sibling_aggregate,
    sibling_leaf,
};

struct module_candidate
{
    std::string path;
    module_scope scope{};
    std::size_t root_index{};
    module_layout layout{};
};

auto scope_priority(module_scope value) -> std::size_t
{
    using enum module_scope;
    switch(value) {
    case directory: return 0uz;
    case import_root: return 1uz;
    }
    std::unreachable();
}

auto layout_priority(module_layout value) -> std::size_t
{
    using enum module_layout;
    switch(value) {
    case hierarchical: return 0uz;
    case folder_aggregate: return 1uz;
    case aggregate: return 2uz;
    case stdlib_hierarchical: return 3uz;
    case sibling_aggregate: return 4uz;
    case sibling_leaf: return 5uz;
    }
    std::unreachable();
}

auto module_name_text(source_manager const& sources, module_name_syntax const& name) -> std::string
{
    return ast_source_view{ sources }.module_name(name);
}

auto module_relative_path(std::string_view name) -> std::filesystem::path
{
    auto result = std::filesystem::path{};
    auto start = 0uz;
    while(start < name.size()) {
        auto end = name.find('.', start);
        if(end == std::string_view::npos) {
            auto leaf = std::string{ name.substr(start) };
            leaf += ".cp";
            result /= leaf;
            break;
        }
        result /= std::string{ name.substr(start, end - start) };
        start = end + 1uz;
    }
    return result;
}

auto module_leaf_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    auto const position = name.rfind('.');
    if(position == std::string_view::npos) {
        return std::nullopt;
    }
    auto leaf = std::string{ name.substr(position + 1uz) };
    leaf += ".cp";
    return std::filesystem::path{ leaf };
}

auto aggregate_relative_path(std::string_view name) -> std::filesystem::path
{
    auto components = std::vector<std::string>{};
    auto start = 0uz;
    while(start < name.size()) {
        auto const end = name.find('.', start);
        auto const count = end == std::string_view::npos ? name.size() - start : end - start;
        components.emplace_back(name.substr(start, count));
        if(end == std::string_view::npos) {
            break;
        }
        start = end + 1uz;
    }

    auto result = std::filesystem::path{};
    for(auto const& component : components) {
        result /= component;
    }
    return result / (components.back() + ".cp");
}

auto folder_aggregate_relative_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    auto components = std::vector<std::string>{};
    auto start = 0uz;
    while(start < name.size()) {
        auto const end = name.find('.', start);
        auto const count = end == std::string_view::npos ? name.size() - start : end - start;
        components.emplace_back(name.substr(start, count));
        if(end == std::string_view::npos) {
            break;
        }
        start = end + 1uz;
    }
    if(components.size() < 2uz) {
        return std::nullopt;
    }

    auto result = std::filesystem::path{};
    for(auto const& component : components) {
        result /= component;
    }
    return result / (components.front() + ".cp");
}

auto folder_aggregate_leaf_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    auto const first = name.find('.');
    if(first == std::string_view::npos or name.find('.', first + 1uz) != std::string_view::npos) {
        return std::nullopt;
    }
    return std::filesystem::path{ std::string{ name.substr(0uz, first) } + ".cp" };
}

auto stdlib_relative_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    auto prefix = std::string_view{ "std." };
    if(not name.starts_with(prefix)) {
        return std::nullopt;
    }
    return module_relative_path(name.substr(prefix.size()));
}

auto read_all(std::filesystem::path const& path) -> std::optional<std::string>
{
    auto stream = std::ifstream{ path };
    if(not stream.is_open()) {
        return std::nullopt;
    }
    return std::string{
        std::istreambuf_iterator<char>{ stream },
        std::istreambuf_iterator<char>{}
    };
}

auto parse_source(std::string path, std::string text) -> module_source
{
    auto sources = source_manager{};
    auto const file = sources.add_source(path, text);
    auto preprocessed = preprocess(sources, file);
    if(contains_error_diagnostic(std::span{ preprocessed.diagnostics })) {
        return module_source{ .file = source_file{ .path = std::move(path), .text = std::move(text) } };
    }

    auto lexical = lex(preprocessed);
    if(contains_error_diagnostic(std::span{ lexical.diagnostics })) {
        return module_source{ .file = source_file{ .path = std::move(path), .text = std::move(text) } };
    }

    auto parsed = parse_translation_unit(std::move(lexical.tokens));
    auto result = module_source{ .file = source_file{ .path = std::move(path), .text = std::move(text) } };
    if(not parsed.root) {
        return result;
    }
    if(parsed.root->module_header) {
        result.exported_module = module_name_text(sources, parsed.root->module_header->name);
    }
    for(auto const& import : parsed.root->imports) {
        result.imported_modules.emplace_back(module_name_text(sources, import.name));
    }
    return result;
}

auto is_stdlib_import(std::string_view module_name) -> bool
{
    return module_name == "std" or module_name.starts_with("std.");
}

auto is_skipped_directory(std::filesystem::path const& path) -> bool
{
    auto const name = path.filename().string();
    auto constexpr skipped = std::array{
        ".git",
        ".gradle",
        ".idea",
        ".intellijPlatform",
        "build",
        "build-clion-plugin-native",
        "cmake-build-debug",
        "cmake-build-release",
        "node_modules",
        "out",
    };
    return std::ranges::find(skipped, name) != skipped.end();
}

auto is_cp_source(std::filesystem::path const& path) -> bool
{
    return path.extension() == ".cp";
}

auto candidate(std::filesystem::path const& path, module_scope scope, std::size_t root_index, module_layout layout)
    -> module_candidate
{
    return module_candidate{
        .path = normalize_path(path),
        .scope = scope,
        .root_index = root_index,
        .layout = layout,
    };
}

auto direct_directory_candidates(std::filesystem::path const& base, std::string_view module_name) -> std::vector<module_candidate>
{
    auto result = std::vector<module_candidate>{};
    result.emplace_back(candidate(base / module_relative_path(module_name), module_scope::directory, 0uz, module_layout::hierarchical));
    if(auto path = folder_aggregate_leaf_path(module_name)) {
        result.emplace_back(candidate(base / *path, module_scope::directory, 0uz, module_layout::sibling_aggregate));
    }
    if(auto path = module_leaf_path(module_name)) {
        result.emplace_back(candidate(base / *path, module_scope::directory, 0uz, module_layout::sibling_leaf));
    }
    if(auto path = folder_aggregate_relative_path(module_name)) {
        result.emplace_back(candidate(base / *path, module_scope::directory, 0uz, module_layout::folder_aggregate));
    }
    result.emplace_back(candidate(base / aggregate_relative_path(module_name), module_scope::directory, 0uz, module_layout::aggregate));
    return result;
}

auto import_root_candidates(std::filesystem::path const& root, std::size_t root_index, std::string_view module_name)
    -> std::vector<module_candidate>
{
    auto result = std::vector<module_candidate>{};
    result.emplace_back(candidate(root / module_relative_path(module_name), module_scope::import_root, root_index, module_layout::hierarchical));
    if(auto path = folder_aggregate_relative_path(module_name)) {
        result.emplace_back(candidate(root / *path, module_scope::import_root, root_index, module_layout::folder_aggregate));
    }
    result.emplace_back(candidate(root / aggregate_relative_path(module_name), module_scope::import_root, root_index, module_layout::aggregate));
    if(auto path = stdlib_relative_path(module_name)) {
        result.emplace_back(candidate(root / *path, module_scope::import_root, root_index, module_layout::stdlib_hierarchical));
    }
    return result;
}

auto module_candidate_less(module_candidate const& left, module_candidate const& right) -> bool
{
    return std::tuple{
        scope_priority(left.scope),
        left.root_index,
        layout_priority(left.layout),
        left.path,
    } < std::tuple{
        scope_priority(right.scope),
        right.root_index,
        layout_priority(right.layout),
        right.path,
    };
}

struct resolver
{
    explicit resolver(resolve_request request) :
        request_(std::move(request))
    {
        normalize_request_paths();
        for(auto& file : request_.files) {
            add_source(std::move(file.path), std::move(file.text));
        }
        discover_search_sources();
    }

    auto resolve() -> resolve_result
    {
        auto output = std::vector<source_file>{};
        auto loaded = std::set<std::string>{};
        auto visiting = std::set<std::string>{};

        auto visit = [&](this auto& self, std::string const& path) -> void {
            if(loaded.contains(path) or visiting.contains(path)) {
                return;
            }
            auto source = source_for(path);
            if(not source) {
                return;
            }

            visiting.emplace(path);
            for(auto const& module_name : source->imported_modules) {
                if(not request_.follow_stdlib_imports and is_stdlib_import(module_name)) {
                    continue;
                }
                if(auto resolved = resolve_import(path, module_name)) {
                    self(*resolved);
                }
            }
            visiting.erase(path);

            loaded.emplace(path);
            output.emplace_back(source->file);
        };

        for(auto const& path : entry_files()) {
            visit(path);
        }

        return resolve_result{
            .active_file = request_.active_file,
            .files = std::move(output),
        };
    }

private:
    auto normalize_request_paths() -> void
    {
        request_.active_file = normalize_path(request_.active_file);
        if(request_.target_file.empty()) {
            request_.target_file = request_.active_file;
        } else {
            request_.target_file = normalize_path(request_.target_file);
        }
        for(auto& path : request_.entry_files) {
            path = normalize_path(path);
        }
        for(auto& root : request_.import_roots) {
            root = normalize_path(root);
        }
        for(auto& root : request_.search_roots) {
            root = normalize_path(root);
        }
    }

    auto entry_files() const -> std::vector<std::string>
    {
        auto result = request_.entry_files;
        if(result.empty()) {
            result.emplace_back(request_.target_file);
            if(request_.active_file != request_.target_file) {
                result.emplace_back(request_.active_file);
            }
        }
        auto seen = std::set<std::string>{};
        auto unique = std::vector<std::string>{};
        for(auto& path : result) {
            if(seen.emplace(path).second) {
                unique.emplace_back(std::move(path));
            }
        }
        return unique;
    }

    auto add_source(std::string path, std::string text) -> void
    {
        path = normalize_path(path);
        sources_by_path_.insert_or_assign(path, parse_source(path, std::move(text)));
    }

    auto source_for(std::string const& path) -> std::optional<module_source>
    {
        if(auto const found = sources_by_path_.find(path); found != sources_by_path_.end()) {
            return found->second;
        }
        auto text = read_all(path);
        if(not text) {
            return std::nullopt;
        }
        add_source(path, std::move(*text));
        return sources_by_path_[path];
    }

    auto indexed_paths(std::string_view module_name) const -> std::vector<std::string>
    {
        auto result = std::vector<std::string>{};
        for(auto const& [path, source] : sources_by_path_) {
            if(source.exported_module and *source.exported_module == module_name) {
                result.emplace_back(path);
            }
        }
        return result;
    }

    auto ranked_candidates(std::string const& importer_path, std::string_view module_name) const
        -> std::vector<module_candidate>
    {
        auto result = std::vector<module_candidate>{};
        auto const base = std::filesystem::path{ importer_path }.parent_path();
        if(not base.empty()) {
            auto direct = direct_directory_candidates(base, module_name);
            result.insert(result.end(), direct.begin(), direct.end());
        }
        for(auto index = 0uz; index < request_.import_roots.size(); ++index) {
            auto candidates = import_root_candidates(request_.import_roots[index], index, module_name);
            result.insert(result.end(), candidates.begin(), candidates.end());
        }

        auto seen = std::set<std::string>{};
        auto unique = std::vector<module_candidate>{};
        for(auto& item : result) {
            if(seen.emplace(item.path).second) {
                unique.emplace_back(std::move(item));
            }
        }
        return unique;
    }

    auto choose_indexed_source(std::string const& importer_path, std::string_view module_name) const -> std::optional<std::string>
    {
        auto paths = indexed_paths(module_name);
        if(paths.empty()) {
            return std::nullopt;
        }

        auto candidates = ranked_candidates(importer_path, module_name);
        auto rank_by_path = std::map<std::string, module_candidate>{};
        for(auto const& item : candidates) {
            rank_by_path.emplace(item.path, item);
        }

        auto ranked = std::vector<module_candidate>{};
        auto unranked = std::vector<std::string>{};
        for(auto& path : paths) {
            if(auto const found = rank_by_path.find(path); found != rank_by_path.end()) {
                ranked.emplace_back(found->second);
            } else {
                unranked.emplace_back(std::move(path));
            }
        }
        if(not ranked.empty()) {
            std::ranges::sort(ranked, module_candidate_less);
            return ranked.front().path;
        }
        std::ranges::sort(unranked);
        return unranked.front();
    }

    auto resolve_import(std::string const& importer_path, std::string_view module_name) -> std::optional<std::string>
    {
        if(auto indexed = choose_indexed_source(importer_path, module_name)) {
            return indexed;
        }

        auto candidates = ranked_candidates(importer_path, module_name);
        for(auto const& item : candidates) {
            if(sources_by_path_.contains(item.path)) {
                continue;
            }
            if(auto text = read_all(item.path)) {
                add_source(item.path, std::move(*text));
            }
        }

        if(auto indexed = choose_indexed_source(importer_path, module_name)) {
            return indexed;
        }

        std::ranges::sort(candidates, module_candidate_less);
        for(auto const& item : candidates) {
            if(sources_by_path_.contains(item.path) or std::filesystem::is_regular_file(item.path)) {
                return item.path;
            }
        }
        return std::nullopt;
    }

    auto discover_search_sources() -> void
    {
        auto visited = std::set<std::string>{};
        for(auto const& root_name : request_.search_roots) {
            auto const root = std::filesystem::path{ root_name };
            auto error = std::error_code{};
            if(not std::filesystem::is_directory(root, error) or not visited.emplace(root_name).second) {
                continue;
            }

            auto count = 0uz;
            auto iterator = std::filesystem::recursive_directory_iterator{
                root,
                std::filesystem::directory_options::skip_permission_denied,
                error,
            };
            auto const end = std::filesystem::recursive_directory_iterator{};
            while(iterator != end and count < max_discovered_sources) {
                if(error) {
                    error.clear();
                    iterator.increment(error);
                    continue;
                }

                auto const path = iterator->path();
                if(iterator->is_directory(error)) {
                    if(is_skipped_directory(path)) {
                        iterator.disable_recursion_pending();
                    }
                    iterator.increment(error);
                    continue;
                }
                if(iterator->is_regular_file(error) and is_cp_source(path)) {
                    auto normalized = normalize_path(path);
                    if(not sources_by_path_.contains(normalized)) {
                        if(auto text = read_all(path)) {
                            add_source(std::move(normalized), std::move(*text));
                        }
                    }
                    ++count;
                }
                iterator.increment(error);
            }
        }
    }

    auto constexpr static max_discovered_sources = 4096uz;

    resolve_request request_;
    std::map<std::string, module_source> sources_by_path_;
};

} // namespace

auto normalize_path(std::filesystem::path const& path) -> std::string
{
    auto error = std::error_code{};
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if(error) {
        normalized = std::filesystem::absolute(path, error);
    }
    return error ? path.lexically_normal().string() : normalized.lexically_normal().string();
}

auto resolve_source_files(resolve_request request) -> resolve_result
{
    return resolver{ std::move(request) }.resolve();
}

} // namespace cp_imports
