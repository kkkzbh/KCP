import std;
import parser;

namespace {
[[nodiscard]] auto read_all(std::istream& input) -> std::string
{
    return std::string(
        std::istreambuf_iterator<char>{ input },
        std::istreambuf_iterator<char>{});
}

auto print_lexer_diagnostic(source_manager const& sources, diagnostic const& value) -> void
{
    auto const position = sources.position(value.primary_span.file, value.primary_span.start);
    std::cout
        << "lexer:" << position.line << ':' << position.column << ": "
        << value.message << '\n';
}

auto print_parser_diagnostic(source_manager const& sources, parser_diagnostic const& value) -> void
{
    auto const position = sources.position(value.primary_span.file, value.primary_span.start);
    std::cout
        << "parser:" << position.line << ':' << position.column << ": "
        << value.message << '\n';
}
}

auto main(int argc, char** argv) -> int
{
    auto trace_enabled = false;
    auto path = std::optional<std::string>{};
    auto const arguments = std::span(argv, static_cast<std::size_t>(argc));

    for(auto const raw_argument : arguments | std::views::drop(1)) {
        auto const argument = std::string_view(raw_argument);
        if(argument == "--trace") {
            trace_enabled = true;
            continue;
        }

        if(path.has_value()) {
            std::cerr << "unexpected extra argument: " << argument << '\n';
            return 2;
        }

        path = std::string(argument);
    }

    auto sources = source_manager{};
    auto name = std::string{ "<stdin>" };
    auto input = std::string{};

    if(path.has_value() and *path != "-") {
        name = *path;
        auto stream = std::ifstream{ *path };
        if(not stream.is_open()) {
            std::cerr << "failed to open " << *path << '\n';
            return 2;
        }
        input = read_all(stream);
    } else {
        input = read_all(std::cin);
    }

    auto const file = sources.add_source(name, input);
    auto const result = parse_translation_unit(sources, file, parse_options{
        .trace_enabled = trace_enabled,
    });

    std::cout << (result.accepted ? "accepted" : "rejected") << '\n';
    for(auto const& diagnostic : result.lexer_diagnostics) {
        print_lexer_diagnostic(sources, diagnostic);
    }
    for(auto const& diagnostic : result.diagnostics) {
        print_parser_diagnostic(sources, diagnostic);
    }

    if(trace_enabled) {
        for(auto const& event : result.trace) {
            std::cout << format_trace_event(sources, event) << '\n';
        }
    }

    return result.accepted ? 0 : 1;
}
