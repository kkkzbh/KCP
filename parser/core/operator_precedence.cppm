export module parser.operator_precedence;

import std;
import lexer;
import parser.grammar;
import parser.diagnostic;
import parser.trace;
import parser.expression_precedence;

export struct op_precedence_relation
{
    enum class kind : std::uint8_t
    {
        none,
        lt, // <·
        eq, // =·
        gt, // ·>
    };
};

export struct op_precedence_table
{
    std::vector<std::string> terminals;
    std::map<std::string, std::set<std::string>> firstvt;
    std::map<std::string, std::set<std::string>> lastvt;
    std::map<std::pair<std::string, std::string>, op_precedence_relation::kind> matrix;
    std::vector<std::string> conflicts;
    bool is_operator_grammar{};
    bool is_operator_precedence_grammar{};
};

export struct op_parse_options
{
    bool trace_enabled{};
};

export struct op_parse_result
{
    bool accepted{};
    std::vector<parser_diagnostic> diagnostics;
    std::vector<trace_event> trace;
};

export [[nodiscard]] auto build_op_precedence_table(grammar_definition const& grammar)
    -> op_precedence_table;

export [[nodiscard]] auto parse_with_op_precedence(
    grammar_definition const& grammar,
    op_precedence_table const& table,
    std::span<std::string const> input,
    op_parse_options options = {}) -> op_parse_result;

export [[nodiscard]] auto tokens_to_terminal_names(std::span<token const> tokens)
    -> std::vector<std::string>;

export [[nodiscard]] auto cp_expression_op_grammar() -> grammar_definition;

export [[nodiscard]] auto to_string(op_precedence_relation::kind value) -> std::string_view;

namespace {

[[nodiscard]] auto collect_nonterminals(grammar_definition const& grammar) -> std::set<std::string>
{
    auto nonterminals = std::set<std::string>{};
    for(auto const& production : grammar.productions) {
        nonterminals.insert(production.lhs);
    }
    return nonterminals;
}

[[nodiscard]] auto is_nonterminal(
    std::set<std::string> const& nonterminals,
    std::string_view symbol) -> bool
{
    return std::ranges::find(nonterminals, symbol) != nonterminals.end();
}

auto check_operator_grammar(
    grammar_definition const& grammar,
    std::set<std::string> const& nonterminals,
    op_precedence_table& table) -> void
{
    table.is_operator_grammar = true;
    for(auto const index : std::views::iota(0uz, grammar.productions.size())) {
        auto const& production = grammar.productions[index];
        if(production.rhs.empty()) {
            table.is_operator_grammar = false;
            table.conflicts.push_back(std::format(
                "production {} ({} -> ε) violates operator grammar: epsilon production not allowed",
                index,
                production.lhs));
            continue;
        }

        for(auto const i : std::views::iota(0uz, production.rhs.size() - 1uz)) {
            auto const& a = production.rhs[i];
            auto const& b = production.rhs[i + 1uz];
            if(is_nonterminal(nonterminals, a) and is_nonterminal(nonterminals, b)) {
                table.is_operator_grammar = false;
                table.conflicts.push_back(std::format(
                    "production {} ({} -> ...) violates operator grammar: adjacent nonterminals {} {}",
                    index,
                    production.lhs,
                    a,
                    b));
            }
        }
    }
}

auto compute_firstvt_lastvt(
    grammar_definition const& grammar,
    std::set<std::string> const& nonterminals,
    op_precedence_table& table) -> void
{
    for(auto const& nonterminal : nonterminals) {
        table.firstvt[nonterminal] = {};
        table.lastvt[nonterminal] = {};
    }

    auto changed = true;
    while(changed) {
        changed = false;
        for(auto const& production : grammar.productions) {
            if(production.rhs.empty()) {
                continue;
            }

            auto const& first_symbol = production.rhs.front();
            auto& fv = table.firstvt[production.lhs];
            auto const before_size = fv.size();

            if(not is_nonterminal(nonterminals, first_symbol)) {
                fv.insert(first_symbol);
            } else {
                if(production.rhs.size() >= 2uz) {
                    auto const& second_symbol = production.rhs[1uz];
                    if(not is_nonterminal(nonterminals, second_symbol)) {
                        fv.insert(second_symbol);
                    }
                }
                auto const& nested = table.firstvt[first_symbol];
                fv.insert(nested.begin(), nested.end());
            }

            if(fv.size() != before_size) {
                changed = true;
            }
        }
    }

    changed = true;
    while(changed) {
        changed = false;
        for(auto const& production : grammar.productions) {
            if(production.rhs.empty()) {
                continue;
            }

            auto const& last_symbol = production.rhs.back();
            auto& lv = table.lastvt[production.lhs];
            auto const before_size = lv.size();

            if(not is_nonterminal(nonterminals, last_symbol)) {
                lv.insert(last_symbol);
            } else {
                if(production.rhs.size() >= 2uz) {
                    auto const& penult = production.rhs[production.rhs.size() - 2uz];
                    if(not is_nonterminal(nonterminals, penult)) {
                        lv.insert(penult);
                    }
                }
                auto const& nested = table.lastvt[last_symbol];
                lv.insert(nested.begin(), nested.end());
            }

            if(lv.size() != before_size) {
                changed = true;
            }
        }
    }
}

auto record_relation(
    op_precedence_table& table,
    std::string const& a,
    std::string const& b,
    op_precedence_relation::kind relation) -> void
{
    auto const key = std::pair{ a, b };
    auto const it = table.matrix.find(key);
    if(it == table.matrix.end()) {
        table.matrix.emplace(key, relation);
        return;
    }

    if(it->second == relation) {
        return;
    }

    table.is_operator_precedence_grammar = false;
    table.conflicts.push_back(std::format(
        "matrix conflict at ({}, {}): {} vs {}",
        a,
        b,
        to_string(it->second),
        to_string(relation)));
}

auto build_matrix(
    grammar_definition const& grammar,
    std::set<std::string> const& nonterminals,
    op_precedence_table& table) -> void
{
    for(auto const& production : grammar.productions) {
        auto const& rhs = production.rhs;
        if(rhs.size() < 2uz) {
            continue;
        }

        for(auto const i : std::views::iota(0uz, rhs.size() - 1uz)) {
            auto const& xi = rhs[i];
            auto const& xi1 = rhs[i + 1uz];

            auto const xi_is_nt = is_nonterminal(nonterminals, xi);
            auto const xi1_is_nt = is_nonterminal(nonterminals, xi1);

            if(not xi_is_nt and not xi1_is_nt) {
                record_relation(table, xi, xi1, op_precedence_relation::kind::eq);
            }

            if(not xi_is_nt and xi1_is_nt) {
                for(auto const& b : table.firstvt.at(xi1)) {
                    record_relation(table, xi, b, op_precedence_relation::kind::lt);
                }
            }

            if(xi_is_nt and not xi1_is_nt) {
                for(auto const& a : table.lastvt.at(xi)) {
                    record_relation(table, a, xi1, op_precedence_relation::kind::gt);
                }
            }

            if(i + 2uz < rhs.size()) {
                auto const& xi2 = rhs[i + 2uz];
                if(not xi_is_nt and xi1_is_nt and not is_nonterminal(nonterminals, xi2)) {
                    record_relation(table, xi, xi2, op_precedence_relation::kind::eq);
                }
            }
        }
    }

    auto const end_marker = std::string(end_marker_symbol());
    auto const& start_firstvt = table.firstvt.at(grammar.start_symbol);
    for(auto const& a : start_firstvt) {
        record_relation(table, end_marker, a, op_precedence_relation::kind::lt);
    }
    auto const& start_lastvt = table.lastvt.at(grammar.start_symbol);
    for(auto const& a : start_lastvt) {
        record_relation(table, a, end_marker, op_precedence_relation::kind::gt);
    }
    record_relation(table, end_marker, end_marker, op_precedence_relation::kind::eq);
}

auto collect_terminals(
    grammar_definition const& grammar,
    std::set<std::string> const& nonterminals,
    op_precedence_table& table) -> void
{
    auto terminals = std::set<std::string>{};
    for(auto const& production : grammar.productions) {
        for(auto const& symbol : production.rhs) {
            if(not is_nonterminal(nonterminals, symbol)) {
                terminals.insert(symbol);
            }
        }
    }
    terminals.insert(std::string(end_marker_symbol()));

    table.terminals.assign(terminals.begin(), terminals.end());
}

[[nodiscard]] auto last_terminal_index(
    std::vector<std::string> const& stack,
    std::set<std::string> const& nonterminals) -> std::optional<std::size_t>
{
    for(auto const i : std::views::iota(0uz, stack.size()) | std::views::reverse) {
        if(not is_nonterminal(nonterminals, stack[i])) {
            return i;
        }
    }
    return std::nullopt;
}

auto record_step_trace(
    op_parse_result& result,
    bool trace_enabled,
    std::vector<std::string> const& stack,
    std::span<std::string const> remaining,
    std::string_view action) -> void
{
    if(not trace_enabled) {
        return;
    }

    auto stack_text = std::string{};
    for(auto const i : std::views::iota(0uz, stack.size())) {
        if(i != 0uz) {
            stack_text += ' ';
        }
        stack_text += stack[i];
    }

    auto input_text = std::string{};
    for(auto const i : std::views::iota(0uz, remaining.size())) {
        if(i != 0uz) {
            input_text += ' ';
        }
        input_text += remaining[i];
    }

    result.trace.push_back(trace_event{
        .kind = trace_event_kind::match,
        .label = std::format("[{}] | [{}] | {}", stack_text, input_text, action),
        .current = token{},
    });
}

} // namespace

auto to_string(op_precedence_relation::kind value) -> std::string_view
{
    using enum op_precedence_relation::kind;
    switch(value) {
    case none: return ".";
    case lt: return "<.";
    case eq: return "=.";
    case gt: return ".>";
    }
    return "?";
}

auto build_op_precedence_table(grammar_definition const& grammar) -> op_precedence_table
{
    auto table = op_precedence_table{};
    table.is_operator_precedence_grammar = true;

    auto const nonterminals = collect_nonterminals(grammar);
    collect_terminals(grammar, nonterminals, table);
    check_operator_grammar(grammar, nonterminals, table);

    if(not table.is_operator_grammar) {
        table.is_operator_precedence_grammar = false;
        return table;
    }

    compute_firstvt_lastvt(grammar, nonterminals, table);
    build_matrix(grammar, nonterminals, table);

    return table;
}

auto parse_with_op_precedence(
    grammar_definition const& grammar,
    op_precedence_table const& table,
    std::span<std::string const> input,
    op_parse_options options) -> op_parse_result
{
    auto result = op_parse_result{};

    if(not table.is_operator_grammar or not table.is_operator_precedence_grammar) {
        result.diagnostics.push_back(parser_diagnostic{
            .code = parser_diagnostic_code::unexpected_token,
            .message = "grammar is not an operator-precedence grammar; cannot parse",
        });
        return result;
    }

    auto nonterminals = collect_nonterminals(grammar);
    auto const placeholder = std::string{ "N" };
    nonterminals.insert(placeholder);
    auto const end_marker = std::string(end_marker_symbol());

    auto stack = std::vector<std::string>{ end_marker };
    auto buffered_input = std::vector<std::string>(input.begin(), input.end());
    if(buffered_input.empty() or buffered_input.back() != end_marker) {
        buffered_input.push_back(end_marker);
    }

    auto cursor = 0uz;

    auto const trace = options.trace_enabled;
    record_step_trace(result, trace, stack, std::span<std::string const>(buffered_input).subspan(cursor), "init");

    while(true) {
        auto const top_terminal = last_terminal_index(stack, nonterminals);
        if(not top_terminal.has_value()) {
            result.diagnostics.push_back(parser_diagnostic{
                .code = parser_diagnostic_code::unexpected_token,
                .message = "operator-precedence stack contains no terminal",
            });
            return result;
        }

        auto const& top = stack[*top_terminal];
        auto const& current = buffered_input[cursor];

        if(top == end_marker and current == end_marker) {
            if(stack.size() == 2uz and stack[0] == end_marker and is_nonterminal(nonterminals, stack[1])) {
                record_step_trace(
                    result,
                    trace,
                    stack,
                    std::span<std::string const>(buffered_input).subspan(cursor),
                    "accept");
                result.accepted = true;
                return result;
            }
            result.diagnostics.push_back(parser_diagnostic{
                .code = parser_diagnostic_code::unexpected_token,
                .message = "input ended but stack is not in accepting configuration",
            });
            return result;
        }

        auto const it = table.matrix.find({ top, current });
        if(it == table.matrix.end()) {
            result.diagnostics.push_back(parser_diagnostic{
                .code = parser_diagnostic_code::unexpected_token,
                .message = std::format("no precedence relation between {} and {}", top, current),
            });
            return result;
        }

        switch(it->second) {
        case op_precedence_relation::kind::lt:
        case op_precedence_relation::kind::eq: {
            stack.push_back(current);
            ++cursor;
            record_step_trace(
                result,
                trace,
                stack,
                std::span<std::string const>(buffered_input).subspan(cursor),
                std::format("shift {}", current));
            break;
        }
        case op_precedence_relation::kind::gt: {
            // Find the leftmost <· — i.e. the rightmost terminal pair (a, b) in the stack
            // with a <· b. The handle is everything strictly to the right of a.
            auto handle_start = stack.size();
            for(auto const i : std::views::iota(0uz, stack.size()) | std::views::reverse) {
                if(is_nonterminal(nonterminals, stack[i])) {
                    handle_start = i;
                    continue;
                }
                if(handle_start == stack.size()) {
                    handle_start = i;
                    continue;
                }
                auto const right_terminal_index = [&]() -> std::size_t {
                    for(auto const j : std::views::iota(i + 1uz, stack.size())) {
                        if(not is_nonterminal(nonterminals, stack[j])) {
                            return j;
                        }
                    }
                    return stack.size();
                }();
                if(right_terminal_index == stack.size()) {
                    handle_start = i;
                    continue;
                }
                auto const rel_it = table.matrix.find({ stack[i], stack[right_terminal_index] });
                if(rel_it != table.matrix.end() and rel_it->second == op_precedence_relation::kind::lt) {
                    handle_start = i + 1uz;
                    break;
                }
                handle_start = i;
            }

            if(handle_start >= stack.size()) {
                result.diagnostics.push_back(parser_diagnostic{
                    .code = parser_diagnostic_code::unexpected_token,
                    .message = "no leftmost <· marker found while reducing",
                });
                return result;
            }

            auto handle_text = std::string{};
            for(auto const j : std::views::iota(handle_start, stack.size())) {
                if(j != handle_start) {
                    handle_text += ' ';
                }
                handle_text += stack[j];
            }
            stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(handle_start), stack.end());
            stack.push_back(placeholder);
            record_step_trace(
                result,
                trace,
                stack,
                std::span<std::string const>(buffered_input).subspan(cursor),
                std::format("reduce {}", handle_text));
            break;
        }
        case op_precedence_relation::kind::none:
            result.diagnostics.push_back(parser_diagnostic{
                .code = parser_diagnostic_code::unexpected_token,
                .message = std::format("unexpected none relation between {} and {}", top, current),
            });
            return result;
        }
    }
}

auto tokens_to_terminal_names(std::span<token const> tokens) -> std::vector<std::string>
{
    auto names = std::vector<std::string>{};
    names.reserve(tokens.size());
    for(auto const& current : tokens) {
        if(current.kind == token_kind::eof) {
            names.emplace_back(end_marker_symbol());
            continue;
        }
        names.emplace_back(to_string(current.kind));
    }
    return names;
}

auto cp_expression_op_grammar() -> grammar_definition
{
    // Group binary operators by left binding power. The std::map iterates in
    // ascending key order, so the smallest binding power becomes the topmost
    // (lowest-precedence) layer — matching the textbook E/T/F arithmetic grammar
    // shape. kw_as is excluded: its real-language right-hand side is a Type, not
    // an expression, so it does not belong in a binary-operator grammar.
    auto by_level = std::map<int, std::vector<std::string_view>>{};
    for(auto const& entry : cp_binary_operator_table) {
        if(entry.kind == token_kind::kw_as) {
            continue;
        }
        by_level[entry.left_bp].push_back(entry.terminal_name);
    }

    auto const level_count = by_level.size();
    auto const level_name = [](std::size_t i) { return std::format("E{}", i); };

    auto productions = std::vector<grammar_production>{};
    auto level_index = 0uz;
    for(auto const& [bp, ops] : by_level) {
        auto const this_name = level_name(level_index);
        auto const next_name = level_name(level_index + 1uz);

        productions.push_back(grammar_production{
            .lhs = this_name,
            .rhs = { next_name },
        });

        for(auto const& op : ops) {
            productions.push_back(grammar_production{
                .lhs = this_name,
                .rhs = { this_name, std::string(op), next_name },
            });
        }
        ++level_index;
    }

    auto const primary_name = level_name(level_count);
    productions.push_back(grammar_production{
        .lhs = primary_name,
        .rhs = { "l_paren", level_name(0uz), "r_paren" },
    });

    auto const atoms = std::array<std::string_view, 7uz>{
        "identifier",
        "integer_literal",
        "float_literal",
        "char_literal",
        "string_literal",
        "kw_true",
        "kw_false",
    };
    for(auto const& atom : atoms) {
        productions.push_back(grammar_production{
            .lhs = primary_name,
            .rhs = { std::string(atom) },
        });
    }

    return grammar_definition{
        .start_symbol = level_name(0uz),
        .productions = std::move(productions),
    };
}
