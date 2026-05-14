import std;
import parser;

namespace {

auto read_all(std::istream& input) -> std::string
{
    return std::string(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    );
}

auto print_lexer_diagnostic(source_manager const& sources, lexer_diagnostic const& value) -> void
{
    auto position = sources.position(value.primary_span.start);
    std::cout
        << "lexer:" << position.line << ':' << position.column << ": "
        << value.message << '\n';
}

auto print_parser_diagnostic(source_manager const& sources, parser_diagnostic const& value) -> void
{
    auto position = sources.position(value.primary_span.start);
    std::cout
        << "parser:" << position.line << ':' << position.column << ": "
        << value.message << '\n';
}
} // namespace

auto main(int argc, char** argv) -> int
{
    auto path = std::optional<std::string>{};
    auto arguments = std::span(argv, static_cast<std::size_t>(argc));

    for(auto raw_argument : arguments | std::views::drop(1)) {
        auto argument = std::string_view(raw_argument);
        if(argument.starts_with("--")) {
            std::cerr << "unexpected option: " << argument << '\n';
            return 2;
        }

        if(path) {
            std::cerr << "unexpected extra argument: " << argument << '\n';
            return 2;
        }

        path = std::string(argument);
    }

    auto sources = source_manager{};
    auto name = std::string{"<stdin>"};
    auto input = std::string{};

    if(path and *path != "-") {
        name = *path;
        auto stream = std::ifstream{*path};
        if(not stream.is_open()) {
            std::cerr << "failed to open " << *path << '\n';
            return 2;
        }
        input = read_all(stream);
    } else {
        input = read_all(std::cin);
    }

    auto file = sources.add_source(name, input);
    auto result = parse_translation_unit(sources, file);

    std::cout << (result.accepted ? "accepted" : "rejected") << '\n';
    for(auto const& diagnostic : result.lexer_diagnostics) {
        print_lexer_diagnostic(sources, diagnostic);
    }
    for(auto const& diagnostic : result.parser_diagnostics) {
        print_parser_diagnostic(sources, diagnostic);
    }

    return result.accepted ? 0 : 1;
}
