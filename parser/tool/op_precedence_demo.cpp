import std;
import parser;
import lexer.scanner;

namespace {

auto read_all(std::istream& input) -> std::string
{
    return std::string(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    );
}

auto print_set(
    std::string_view label,
    std::map<std::string, std::set<std::string>> const& sets
) -> void
{
    std::cout << label << ":\n";
    for(auto const& [name, items] : sets) {
        std::cout << "  " << name << " = {";
        auto first = true;
        for(auto const& item : items) {
            if(not first) {
                std::cout << ", ";
            }
            first = false;
            std::cout << item;
        }
        std::cout << "}\n";
    }
}

auto print_matrix(op_precedence_table const& table) -> void
{
    auto column_width = 4uz;
    for(auto const& terminal : table.terminals) {
        column_width = std::max(column_width, terminal.size() + 1uz);
    }

    std::cout << "Operator-precedence matrix:\n";
    std::cout << std::string(column_width, ' ');
    for(auto const& column : table.terminals) {
        std::cout << std::left << std::setw(static_cast<int>(column_width)) << column;
    }
    std::cout << '\n';

    for(auto const& row : table.terminals) {
        std::cout << std::left << std::setw(static_cast<int>(column_width)) << row;
        for(auto const& column : table.terminals) {
            auto it = table.matrix.find({row, column});
            auto cell = (
                it == table.matrix.end()
                    ? std::string_view{"."}
                    : to_string(it->second)
            );
            std::cout << std::left << std::setw(static_cast<int>(column_width)) << cell;
        }
        std::cout << '\n';
    }
}

auto print_terminal_stream(std::span<std::string const> names) -> void
{
    std::cout << "Terminal stream:";
    for(auto const& name : names) {
        std::cout << ' ' << name;
    }
    std::cout << '\n';
}

} // namespace

auto main(int argc, char** argv) -> int
{
    auto trace_enabled = false;
    auto path = std::optional<std::string>{};
    auto arguments = std::span(argv, static_cast<std::size_t>(argc));

    for(auto raw_argument : arguments | std::views::drop(1)) {
        auto argument = std::string_view(raw_argument);
        if(argument == "--trace") {
            trace_enabled = true;
            continue;
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
        auto stream = std::ifstream{*path};
        if(stream.is_open()) {
            name = *path;
            input = read_all(stream);
        } else {
            name = "<argument>";
            input = *path;
        }
    } else {
        input = read_all(std::cin);
    }

    auto file = sources.add_source(name, input);
    auto sink = std::vector<lexer_diagnostic>{};
    auto scanner = lexer{sources, file, sink};
    auto tokens = scanner.tokenize_all();

    auto grammar = cp_expression_op_grammar();
    auto table = build_op_precedence_table(grammar);

    std::cout << "Grammar productions:\n";
    for(auto i : std::views::iota(0uz, grammar.productions.size())) {
        auto const& production = grammar.productions[i];
        std::cout << "  " << i << ": " << production.lhs << " ->";
        if(production.rhs.empty()) {
            std::cout << " ε";
        } else {
            for(auto const& symbol : production.rhs) {
                std::cout << ' ' << symbol;
            }
        }
        std::cout << '\n';
    }

    std::cout << "is_operator_grammar: " << std::boolalpha << table.is_operator_grammar << '\n';
    std::cout << "is_operator_precedence_grammar: " << table.is_operator_precedence_grammar << '\n';
    if(not table.conflicts.empty()) {
        std::cout << "Conflicts:\n";
        for(auto const& conflict : table.conflicts) {
            std::cout << "  " << conflict << '\n';
        }
    }

    print_set("FIRSTVT", table.firstvt);
    print_set("LASTVT", table.lastvt);
    print_matrix(table);

    auto terminal_names = tokens_to_terminal_names(tokens);
    print_terminal_stream(terminal_names);

    auto result = parse_with_op_precedence (
        grammar,
        table,
        terminal_names,
        op_parse_options {
            .trace_enabled = trace_enabled,
        }
    );

    std::cout << (result.accepted ? "accepted" : "rejected") << '\n';
    for(auto const& diagnostic : result.diagnostics) {
        std::cout << "diag: " << diagnostic.message << '\n';
    }

    if(trace_enabled) {
        std::cout << "Trace:\n";
        for(auto const& step : result.trace) {
            std::cout << "  " << format_trace_step(step) << '\n';
        }
    }

    return result.accepted ? 0 : 1;
}
