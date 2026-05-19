import std;
import preprocessor;
import lexer;
import parser;
import semantic;
import codegen.ir;
import codegen.llvm;

#ifndef CP_RUNTIME_LIBRARY_PATH
#define CP_RUNTIME_LIBRARY_PATH ""
#endif

#ifndef CP_STDLIB_ROOT_PATH
#define CP_STDLIB_ROOT_PATH ""
#endif

namespace {

enum class emit_kind : std::uint8_t
{
    bin,
    ll,
    obj,
};

struct cli_options
{
    std::vector<std::string> inputs{};
    std::string output{ "a.out" };
    emit_kind emit{ emit_kind::bin };
    std::optional<std::string> keep_ll{};
    std::optional<std::string> keep_obj{};
    std::string clang{ "clang" };
    std::vector<std::string> clang_args{};
    std::vector<std::string> link_args{};
    bool dump_mir{};
    bool verbose{};
    bool help{};
};

struct parse_options_result
{
    bool accepted{};
    cli_options options{};
    std::string error{};
};

auto read_all(std::filesystem::path const& path) -> std::optional<std::string>
{
    auto stream = std::ifstream{ path };
    if(not stream.is_open()) {
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>{ stream },
        std::istreambuf_iterator<char>{}
    );
}

auto normalized_path(std::filesystem::path const& path) -> std::string
{
    auto error = std::error_code{};
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if(error) {
        canonical = std::filesystem::absolute(path, error);
    }
    return error ? path.lexically_normal().string() : canonical.lexically_normal().string();
}

auto module_name(source_manager const& sources, module_name_syntax const& name) -> std::string
{
    auto result = std::string{};
    for(auto component : name.components) {
        if(not result.empty()) {
            result += '.';
        }
        result += sources.slice(component);
    }
    return result;
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

auto module_leaf_path(std::string_view name) -> std::filesystem::path
{
    auto position = name.rfind('.');
    auto leaf = position == std::string_view::npos ? name : name.substr(position + 1uz);
    auto result = std::string{ leaf };
    result += ".cp";
    return result;
}

auto import_roots(std::string_view executable) -> std::vector<std::filesystem::path>
{
    auto roots = std::vector<std::filesystem::path>{ std::filesystem::current_path() };
    if constexpr(std::string_view{ CP_STDLIB_ROOT_PATH }.size() != 0uz) {
        roots.emplace_back(CP_STDLIB_ROOT_PATH);
    }
    auto error = std::error_code{};
    auto path = std::filesystem::weakly_canonical(std::filesystem::path{ executable }, error);
    if(error) {
        path = std::filesystem::absolute(std::filesystem::path{ executable }, error);
    }
    if(not error) {
        path = path.parent_path();
        for(auto depth = 0uz; depth < 5uz and not path.empty(); ++depth) {
            roots.emplace_back(path);
            path = path.parent_path();
        }
    }
    return roots;
}

auto resolve_import_path(
    std::filesystem::path const& importer,
    std::string_view name,
    std::span<std::filesystem::path const> roots
)
    -> std::optional<std::filesystem::path>
{
    auto const base = importer.parent_path();
    auto candidates = std::vector<std::filesystem::path> {
        base / module_leaf_path(name),
        base / module_relative_path(name),
    };
    for(auto const& root : roots) {
        candidates.emplace_back(root / module_relative_path(name));
    }
    for(auto const& candidate : candidates) {
        auto error = std::error_code{};
        if(std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }
    return std::nullopt;
}

auto print_help(std::ostream& output) -> void
{
    output
        << "usage: cp [options] <input.cp>...\n"
        << "\n"
        << "output control:\n"
        << "  -o, --output <path>      output path, default a.out\n"
        << "  --emit <bin|ll|obj>      output kind, default bin\n"
        << "  --keep-ll <path>         keep LLVM IR when emitting obj/bin\n"
        << "  --keep-obj <path>        keep object file when emitting bin\n"
        << "\n"
        << "LLVM/clang control:\n"
        << "  --clang <path>           clang executable, default clang\n"
        << "  --clang-arg <arg>        extra clang argument, repeatable\n"
        << "  --link-arg <arg>         extra link argument, repeatable\n"
        << "  --no-link                equivalent to --emit obj\n"
        << "\n"
        << "diagnostics:\n"
        << "  --dump-mir               print MIR to stderr\n"
        << "  --verbose                print clang commands\n"
        << "  -h, --help               print this help\n";
}

auto parse_emit_kind(std::string_view value) -> std::optional<emit_kind>
{
    if(value == "bin") {
        return emit_kind::bin;
    }
    if(value == "ll") {
        return emit_kind::ll;
    }
    if(value == "obj") {
        return emit_kind::obj;
    }
    return std::nullopt;
}

auto parse_options(std::span<char* const> arguments) -> parse_options_result
{
    auto result = parse_options_result {
        .accepted = false,
        .options = cli_options{},
    };

    auto consume_value = [&](std::size_t& index, std::string_view option) -> std::optional<std::string> {
        if(index + 1uz >= arguments.size()) {
            result.error = std::format("{} requires an argument", option);
            return std::nullopt;
        }
        ++index;
        return std::string{ arguments[index] };
    };

    for(auto index = 1uz; index < arguments.size(); ++index) {
        auto argument = std::string_view{ arguments[index] };
        if(argument == "-h" or argument == "--help") {
            result.options.help = true;
            result.accepted = true;
            return result;
        }
        if(argument == "-o" or argument == "--output") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.output = *value;
            continue;
        }
        if(argument == "--emit") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            auto parsed = parse_emit_kind(*value);
            if(not parsed) {
                result.error = std::format("unknown emit kind '{}'", *value);
                return result;
            }
            result.options.emit = *parsed;
            continue;
        }
        if(argument == "--keep-ll") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.keep_ll = *value;
            continue;
        }
        if(argument == "--keep-obj") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.keep_obj = *value;
            continue;
        }
        if(argument == "--clang") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.clang = *value;
            continue;
        }
        if(argument == "--clang-arg") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.clang_args.push_back(*value);
            continue;
        }
        if(argument == "--link-arg") {
            auto value = consume_value(index, argument);
            if(not value) {
                return result;
            }
            result.options.link_args.push_back(*value);
            continue;
        }
        if(argument == "--no-link") {
            result.options.emit = emit_kind::obj;
            continue;
        }
        if(argument == "--dump-mir") {
            result.options.dump_mir = true;
            continue;
        }
        if(argument == "--verbose") {
            result.options.verbose = true;
            continue;
        }
        if(argument.starts_with('-')) {
            result.error = std::format("unexpected option '{}'", argument);
            return result;
        }
        result.options.inputs.emplace_back(argument);
    }

    if(result.options.inputs.empty()) {
        result.error = "expected at least one input file";
        return result;
    }

    result.accepted = true;
    return result;
}

auto print_position(source_manager const& sources, source_span span) -> void
{
    auto const [file, _] = sources.locate(span.start);
    auto position = sources.position(span.start);
    std::cerr << sources.name(file) << ':' << position.line << ':' << position.column << ": ";
}

auto print_diagnostic(source_manager const& sources, diagnostic const& value) -> void
{
    print_position(sources, value.primary_span);
    auto const info = spec(value.kind);
    std::cerr << stage_name(info.stage) << '(' << info.code << "): " << value.message << '\n';
}

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

auto run_command(std::vector<std::string> const& arguments, bool verbose) -> bool
{
    auto command = shell_join(arguments);
    if(verbose) {
        std::cerr << command << '\n';
    }
    return std::system(command.c_str()) == 0;
}

auto unique_temp_path(std::string_view suffix) -> std::filesystem::path
{
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto name = std::format("cp-{}{}", stamp, suffix);
    return std::filesystem::temp_directory_path() / name;
}

auto write_text(std::filesystem::path const& path, std::string_view text) -> bool
{
    auto stream = std::ofstream{ path };
    if(not stream.is_open()) {
        return false;
    }
    stream << text;
    return static_cast<bool>(stream);
}

auto remove_if_exists(std::filesystem::path const& path) -> void
{
    auto error = std::error_code{};
    std::filesystem::remove(path, error);
}

auto append_all(std::vector<std::string>& output, std::vector<std::string> const& values) -> void
{
    output.insert(output.end(), values.begin(), values.end());
}

auto resolve_tool_path(std::string const& tool) -> std::string
{
    if(tool.find('/') != std::string::npos) {
        return tool;
    }
    auto candidate = std::filesystem::path{ "/usr/bin" } / tool;
    auto error = std::error_code{};
    if(std::filesystem::is_regular_file(candidate, error)) {
        return candidate.string();
    }
    return tool;
}

auto compile_object(
    cli_options const& options,
    std::filesystem::path const& ll_path,
    std::filesystem::path const& obj_path
) -> bool
{
    auto command = std::vector<std::string>{ resolve_tool_path(options.clang) };
    command.emplace_back("-Wno-override-module");
    append_all(command, options.clang_args);
    command.emplace_back("-c");
    command.emplace_back(ll_path.string());
    command.emplace_back("-o");
    command.emplace_back(obj_path.string());
    return run_command(command, options.verbose);
}

auto link_binary(
    cli_options const& options,
    std::filesystem::path const& input_path,
    std::filesystem::path const& output_path
) -> bool
{
    auto command = std::vector<std::string>{ resolve_tool_path(options.clang) };
    command.emplace_back("-Wno-override-module");
    append_all(command, options.clang_args);
    command.emplace_back(input_path.string());
    if constexpr(std::string_view{ CP_RUNTIME_LIBRARY_PATH }.size() != 0uz) {
        command.emplace_back(CP_RUNTIME_LIBRARY_PATH);
    }
    command.emplace_back("-o");
    command.emplace_back(output_path.string());
    append_all(command, options.link_args);
    return run_command(command, options.verbose);
}

auto emit_output(cli_options const& options, std::string const& llvm_ir) -> int
{
    if(options.emit == emit_kind::ll) {
        if(not write_text(options.output, llvm_ir)) {
            std::cerr << "failed to write " << options.output << '\n';
            return 2;
        }
        return 0;
    }

    auto ll_path = std::filesystem::path{};
    if(options.keep_ll) {
        ll_path = *options.keep_ll;
    } else {
        ll_path = unique_temp_path(".ll");
    }
    auto remove_ll = not options.keep_ll;
    if(not write_text(ll_path, llvm_ir)) {
        std::cerr << "failed to write " << ll_path.string() << '\n';
        return 2;
    }

    if(options.emit == emit_kind::obj) {
        auto ok = compile_object(options, ll_path, options.output);
        if(remove_ll) {
            remove_if_exists(ll_path);
        }
        if(not ok) {
            std::cerr << "clang failed while emitting object\n";
            return 1;
        }
        return 0;
    }

    if(options.keep_obj) {
        auto object = std::filesystem::path{ *options.keep_obj };
        auto compiled = compile_object(options, ll_path, object);
        if(remove_ll) {
            remove_if_exists(ll_path);
        }
        if(not compiled) {
            std::cerr << "clang failed while emitting object\n";
            return 1;
        }
        if(not link_binary(options, object, options.output)) {
            std::cerr << "clang failed while linking binary\n";
            return 1;
        }
        return 0;
    }

    auto linked = link_binary(options, ll_path, options.output);
    if(remove_ll) {
        remove_if_exists(ll_path);
    }
    if(not linked) {
        std::cerr << "clang failed while linking binary\n";
        return 1;
    }
    return 0;
}
} // namespace

auto main(int argc, char** argv) -> int
{
    auto parsed_options = parse_options(std::span<char* const>{ argv, static_cast<std::size_t>(argc) });
    if(not parsed_options.accepted) {
        std::cerr << parsed_options.error << '\n';
        print_help(std::cerr);
        return 2;
    }
    auto const& options = parsed_options.options;
    if(options.help) {
        print_help(std::cout);
        return 0;
    }

    auto sources = source_manager{};
    auto parsed_units = std::vector<parse_result>{};
    parsed_units.reserve(options.inputs.size());
    auto roots = import_roots(argv[0]);
    auto pending_inputs = (
        options.inputs
        | std::views::transform([](std::string const& value) { return std::filesystem::path{ value }; })
        | std::ranges::to<std::vector<std::filesystem::path>>()
    );
    auto loaded_inputs = std::set<std::string>{};
    auto parse_ok = true;
    for(auto input_index = 0uz; input_index < pending_inputs.size(); ++input_index) {
        auto input = pending_inputs[input_index];
        auto normalized = normalized_path(input);
        if(loaded_inputs.contains(normalized)) {
            continue;
        }
        loaded_inputs.emplace(std::move(normalized));

        auto text = read_all(input);
        if(not text) {
            std::cerr << "failed to open " << input.string() << '\n';
            return 2;
        }
        auto file = sources.add_source(input.string(), std::move(*text));
        auto preprocessed = preprocess(sources, file);
        for(auto const& diagnostic : preprocessed.diagnostics) {
            print_diagnostic(sources, diagnostic);
        }
        if(not preprocessed.diagnostics.empty()) {
            parse_ok = false;
            continue;
        }

        auto lexical = lex(preprocessed);
        for(auto const& diagnostic : lexical.diagnostics) {
            print_diagnostic(sources, diagnostic);
        }
        if(not lexical.diagnostics.empty()) {
            parse_ok = false;
            continue;
        }

        auto parsed = parse_translation_unit(std::move(lexical.tokens));
        parse_ok = parse_ok and parsed.accepted;
        for(auto const& diagnostic : parsed.diagnostics) {
            print_diagnostic(sources, diagnostic);
        }
        if(parsed.accepted and parsed.root) {
            for(auto const& import : parsed.root->imports) {
                auto imported_module = module_name(sources, import.name);
                auto resolved = resolve_import_path(input, imported_module, roots);
                if(not resolved) {
                    continue;
                }
                if(not loaded_inputs.contains(normalized_path(*resolved))) {
                    pending_inputs.emplace_back(std::move(*resolved));
                }
            }
        }
        parsed_units.push_back(std::move(parsed));
    }
    if(not parse_ok) {
        return 1;
    }

    auto checked = analyze_semantics(
        sources,
        std::span<parse_result const>{ parsed_units.data(), parsed_units.size() }
    );
    for(auto const& diagnostic : checked.diagnostics) {
        print_diagnostic(sources, diagnostic);
    }
    if(not checked.accepted()) {
        return 1;
    }

    auto ir = emit_ir(
        sources,
        std::span<parse_result const>{ parsed_units.data(), parsed_units.size() },
        checked
    );
    if(not ir.accepted) {
        std::cerr << "codegen: " << ir.error << '\n';
        return 1;
    }
    if(options.dump_mir) {
        std::cerr << dump_ir(ir.module);
    }

    auto emitted = emit_llvm_ir(ir.module);
    if(not emitted.verified) {
        std::cerr << "llvm verifier failed:\n" << emitted.error << '\n';
        return 1;
    }

    return emit_output(options, emitted.ir);
}
