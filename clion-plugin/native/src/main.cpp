import std;
import cp_lexer_helper;

auto main(int argc, char** argv) -> int
{
    auto arguments = std::vector<std::string_view>{};
    arguments.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for(auto index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    return cp_lexer_helper::run_cli(std::span(arguments), std::cin, std::cout, std::cerr);
}
