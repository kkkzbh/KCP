export module parser.table;

import std;
import lexer.token;
export import parser.grammar;

// LR(1) 项目写作 [A -> alpha . beta, a]。
// production 指向产生式 A -> alpha beta，dot 表示点在右部的位置，lookahead 就是那个多出来的 1 个向前看终结符 a。
// LR(0) 只有 production 和 dot；LR(1) 多了 lookahead，所以同一个核心项目可以因为不同向前看符号而表示不同规约条件。
export struct item {
    production: usize;
    dot: usize;
    lookahead: token_kind;
}

impl item {
    item()
    {
        return item{
            .production = 0 as usize,
            .dot = 0 as usize,
            .lookahead = token_kind::eof
        };
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}

export struct transition_key {
    state: usize;
    symbol: grammar_symbol;
}

impl transition_key {
    transition_key()
    {
        return transition_key{
            .state = 0 as usize,
            .symbol = grammar_symbol{}
        };
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}

export variant action_data {
    // shift：当前状态看到某个终结符时，把这个 token 移入栈，并跳到目标状态。
    shift(usize);
    // reduce：当前状态看到某个 lookahead 时，按某条产生式把栈顶若干符号归约成左部非终结符。
    reduce(usize);
    // accept：增广产生式 program 已经完成，且向前看符号是 eof。
    accept;
    // error：ACTION 表中没有合法动作。
    error;
}

export struct parser_action {
    data: action_data = action_data::error;
}

impl parser_action {
    parser_action(data: action_data)
    {
        return parser_action{ .data = data };
    }

    operator ==(self const&, rhs: this const&) -> bool
    {
        return match data {
            .shift(left) => match rhs.data {
                .shift(right) => left == right,
                _ => false,
            },
            .reduce(left) => match rhs.data {
                .reduce(right) => left == right,
                _ => false,
            },
            .accept => match rhs.data {
                .accept => true,
                _ => false,
            },
            .error => match rhs.data {
                .error => true,
                _ => false,
            },
        };
    }
}

export struct action_key {
    state: usize;
    terminal: token_kind;
}

impl action_key {
    action_key()
    {
        return action_key{ .state = 0 as usize, .terminal = token_kind::eof };
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}

export struct goto_key {
    state: usize;
    nonterminal: nonterminal_kind;
}

impl goto_key {
    goto_key()
    {
        return goto_key{ .state = 0 as usize, .nonterminal = nonterminal_kind::augmented };
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}

export struct parser_conflict {
    state: usize;
    terminal: token_kind;
    existing: parser_action;
    incoming: parser_action;
}

impl parser_conflict {
    parser_conflict()
    {
        return parser_conflict{
            .state = 0 as usize,
            .terminal = token_kind::eof,
            .existing = parser_action{},
            .incoming = parser_action{}
        };
    }
}

export struct parser_tables {
    grammar: grammar;
    // 规范 LR(1) 项目集族 C，每个 set<item> 是 DFA 的一个状态。
    states: vector<set<item>>;
    // DFA 边：GOTO(I, X) = J。X 可以是终结符，也可以是非终结符。
    transitions: map<transition_key, usize>;
    // ACTION[state, terminal]：运行时看到输入 token 时执行 shift/reduce/accept/error。
    action_table: map<action_key, parser_action>;
    // GOTO[state, nonterminal]：规约完成后，根据栈顶状态和产生式左部跳转。
    goto_table: map<goto_key, usize>;
    // 如果同一格 ACTION 被填入不同动作，就记录冲突；LR(1) 文法应当为 0。
    conflicts: vector<parser_conflict>;
}

struct builder {
    grammar: grammar;
    first_sets: map<grammar_symbol, set<grammar_symbol>>;
    states: vector<set<item>>;
    transitions: map<transition_key, usize>;
    action_table: map<action_key, parser_action>;
    goto_table: map<goto_key, usize>;
    conflicts: vector<parser_conflict>;
}

impl builder {
    builder(source: grammar)
    {
        return builder{
            .grammar = move source,
            .first_sets = map<grammar_symbol, set<grammar_symbol>>{},
            .states = vector<set<item>>{},
            .transitions = map<transition_key, usize>{},
            .action_table = map<action_key, parser_action>{},
            .goto_table = map<goto_key, usize>{},
            .conflicts = vector<parser_conflict>{}
        };
    }

    collect_symbols(self const&) -> set<grammar_symbol>
    {
        let result = set<grammar_symbol>{};
        result.insert(symbol_terminal(token_kind::eof));
        result.insert(symbol_epsilon());

        let production_index: usize = 0;
        while(production_index < grammar.productions.size()) {
            const ref production = grammar.productions[production_index];
            result.insert(symbol_nonterminal(production.lhs));
            let symbol_index: usize = 0;
            while(symbol_index < production.rhs.size()) {
                result.insert(production.rhs[symbol_index]);
                symbol_index += 1;
            }
            production_index += 1;
        }
        return result;
    }

    merge_symbols(self const&, target: set<grammar_symbol>&, source: set<grammar_symbol> const&) -> bool
    {
        let changed = false;
        let index: usize = 0;
        while(index < source.size()) {
            let inserted = target.insert(source.nth(index));
            if(inserted.inserted) {
                changed = true;
            }
            index += 1;
        }
        return changed;
    }

    first_of(self const&, sequence: vector<grammar_symbol> const&) -> set<grammar_symbol>
    {
        let result = set<grammar_symbol>{};
        let epsilon = symbol_epsilon();
        let nullable = true;
        let index: usize = 0;
        while(index < sequence.size()) {
            let current = sequence[index];
            let current_first_ref = first_sets.find(current);
            assert(current_first_ref.has_value(), "first set missing symbol");
            const ref current_first = *current_first_ref;
            let first_index: usize = 0;
            while(first_index < current_first.size()) {
                let item = current_first.nth(first_index);
                if(not symbol_equal(item, epsilon)) {
                    result.insert(item);
                }
                first_index += 1;
            }
            if(not current_first.contains(epsilon)) {
                nullable = false;
                break;
            }
            index += 1;
        }
        if(nullable) {
            result.insert(epsilon);
        }
        return result;
    }

    build_first_sets(self&) -> void
    {
        // FIRST(X) 表示从符号 X 开始最终可能先看到哪些终结符。
        // closure 需要 FIRST(beta a) 来决定新项目的 lookahead。
        first_sets = map<grammar_symbol, set<grammar_symbol>>{};
        let symbols = collect_symbols();
        let symbol_index: usize = 0;
        while(symbol_index < symbols.size()) {
            let symbol = symbols.nth(symbol_index);
            let values = set<grammar_symbol>{};
            if(symbol_is_terminal(symbol) or symbol_is_epsilon(symbol)) {
                values.insert(symbol);
            }
            first_sets.insert(symbol, values);
            symbol_index += 1;
        }

        let changed = true;
        while(changed) {
            changed = false;
            let production_index: usize = 0;
            while(production_index < grammar.productions.size()) {
                const ref production = grammar.productions[production_index];
                let sequence_first = first_of(production.rhs);
                let lhs = symbol_nonterminal(production.lhs);
                let target = first_sets.find(lhs);
                assert(target.has_value(), "first set missing nonterminal");
                let ref target_first = *target;
                if(merge_symbols(ref target_first, sequence_first)) {
                    changed = true;
                }
                production_index += 1;
            }
        }
    }

    next(self const&, current: item) -> optional<grammar_symbol>
    {
        const ref production = grammar.productions[current.production];
        if(current.dot >= production.rhs.size()) {
            return optional<grammar_symbol>::none;
        }
        return optional<grammar_symbol>::some(production.rhs[current.dot]);
    }

    add_items(self const&, result: set<item>&, lhs: nonterminal_kind, lookahead: token_kind) -> bool
    {
        let changed = false;
        let production_index: usize = 0;
        while(production_index < grammar.productions.size()) {
            const ref production = grammar.productions[production_index];
            if(production.lhs == lhs) {
                let inserted = result.insert(item{
                    .production = production_index,
                    .dot = 0 as usize,
                    .lookahead = lookahead
                });
                if(inserted.inserted) {
                    changed = true;
                }
            }
            production_index += 1;
        }
        return changed;
    }

    add_items_for_suffix(self const&, result: set<item>&, lhs: nonterminal_kind, rhs: vector<grammar_symbol> const&, start: usize, lookahead: token_kind) -> bool
    {
        let changed = false;
        let epsilon = symbol_epsilon();
        let nullable = true;
        let index = start;
        while(index < rhs.size()) {
            let current = rhs[index];
            let current_first_ref = first_sets.find(current);
            assert(current_first_ref.has_value(), "first set missing symbol");
            const ref current_first = *current_first_ref;
            let first_index: usize = 0;
            while(first_index < current_first.size()) {
                let symbol = current_first.nth(first_index);
                if(not symbol_equal(symbol, epsilon)) {
                    assert(symbol_is_terminal(symbol), "first set contains nonterminal symbol");
                    if(add_items(result, lhs, symbol_terminal_kind(symbol))) {
                        changed = true;
                    }
                }
                first_index += 1;
            }
            if(not current_first.contains(epsilon)) {
                nullable = false;
                break;
            }
            index += 1;
        }
        if(nullable and add_items(result, lhs, lookahead)) {
            changed = true;
        }
        return changed;
    }

    closure(self const&, seed: set<item>) -> set<item>
    {
        // closure(I)：补全一个状态。对项目 [A -> alpha . B beta, a]：
        // B 是点号后面马上要识别的非终结符；beta 是同一条产生式右部里 B 后面剩下的那串符号。
        // 如果 beta 能先产生终结符，就用那些终结符作为新项目 [B -> . gamma, b] 的 lookahead b；
        // 如果 beta 可以为空，原项目的 lookahead a 也要继续传给 B。
        let result = move seed;
        let changed = true;
        while(changed) {
            changed = false;
            let index: usize = 0;
            while(index < result.size()) {
                let current = result.nth(index);
                let symbol = next(current);
                if(symbol.has_value()) {
                    const ref next_symbol = *symbol;
                    if(not symbol_is_nonterminal(next_symbol)) {
                        index += 1;
                        continue;
                    }
                    const ref production = grammar.productions[current.production];
                    // current.dot 指向 B；current.dot + 1 是 beta 在 rhs 中的起点。
                    if(add_items_for_suffix(result, symbol_nonterminal_kind(next_symbol), production.rhs, current.dot + 1, current.lookahead)) {
                        changed = true;
                    }
                }
                index += 1;
            }
        }
        return result;
    }

    go(self const&, items: set<item> const&, symbol: grammar_symbol) -> set<item>
    {
        // go(I, X)：把 I 中所有点前是 X 的项目向右移动一格，再对结果做 closure。
        let moved = set<item>{};
        let index: usize = 0;
        while(index < items.size()) {
            let current = items.nth(index);
            let item_symbol = next(current);
            if(item_symbol.has_value()) {
                const ref next_symbol = *item_symbol;
                if(not symbol_equal(next_symbol, symbol)) {
                    index += 1;
                    continue;
                }
                moved.insert(item{
                    .production = current.production,
                    .dot = current.dot + 1,
                    .lookahead = current.lookahead
                });
            }
            index += 1;
        }
        return closure(move moved);
    }

    next_symbols(self const&, items: set<item> const&) -> set<grammar_symbol>
    {
        let result = set<grammar_symbol>{};
        let index: usize = 0;
        while(index < items.size()) {
            let current = items.nth(index);
            let symbol = next(current);
            if(symbol.has_value()) {
                const ref next_symbol = *symbol;
                result.insert(next_symbol);
            }
            index += 1;
        }
        return result;
    }

    same_items(self const&, left: set<item> const&, right: set<item> const&) -> bool
    {
        if(left.size() != right.size()) {
            return false;
        }
        let index: usize = 0;
        while(index < left.size()) {
            if(not right.contains(left.nth(index))) {
                return false;
            }
            index += 1;
        }
        return true;
    }

    find_state(self const&, items: set<item> const&) -> optional<usize>
    {
        let state_index: usize = 0;
        while(state_index < states.size()) {
            if(same_items(states[state_index], items)) {
                return optional<usize>::some(state_index);
            }
            state_index += 1;
        }
        return optional<usize>::none;
    }

    set_action(self&, key: action_key, action: parser_action) -> void
    {
        let existing = action_table.find(key);
        if(existing.has_value()) {
            const ref existing_action = *existing;
            if(not (existing_action == action)) {
                conflicts.push_back(parser_conflict{
                    .state = key.state,
                    .terminal = key.terminal,
                    .existing = existing_action,
                    .incoming = action
                });
            }
            return;
        }
        action_table.insert(key, action);
    }

    build_actions(self&) -> void
    {
        let transition_index: usize = 0;
        while(transition_index < transitions.size()) {
            let entry = transitions.nth(transition_index);
            if(symbol_is_terminal(entry.key.symbol)) {
                set_action(
                    action_key{ .state = entry.key.state, .terminal = symbol_terminal_kind(entry.key.symbol) },
                    parser_action{action_data::shift(entry.value)}
                );
            } else if(symbol_is_nonterminal(entry.key.symbol)) {
                goto_table.insert(
                    goto_key{ .state = entry.key.state, .nonterminal = symbol_nonterminal_kind(entry.key.symbol) },
                    entry.value
                );
            }
            transition_index += 1;
        }

        let state_index: usize = 0;
        while(state_index < states.size()) {
            const ref state_items = states[state_index];
            let item_index: usize = 0;
            while(item_index < state_items.size()) {
                let current = state_items.nth(item_index);
                const ref production = grammar.productions[current.production];
                if(current.dot == production.rhs.size()) {
                    if(production.lhs == nonterminal_kind::augmented) {
                        set_action(
                            action_key{ .state = state_index, .terminal = current.lookahead },
                            parser_action{action_data::accept}
                        );
                    } else {
                        set_action(
                            action_key{ .state = state_index, .terminal = current.lookahead },
                            parser_action{action_data::reduce(current.production)}
                        );
                    }
                }
                item_index += 1;
            }
            state_index += 1;
        }
    }

    build(self&) -> parser_tables
    {
        build_first_sets();

        let start = set<item>{};
        start.insert(item{ .production = 0 as usize, .dot = 0 as usize, .lookahead = token_kind::eof });
        states.push_back(closure(move start));

        let state_index: usize = 0;
        while(state_index < states.size()) {
            let symbols = next_symbols(states[state_index]);
            let symbol_index: usize = 0;
            while(symbol_index < symbols.size()) {
                let symbol = symbols.nth(symbol_index);
                let target_items = go(states[state_index], symbol);
                if(target_items.size() != 0) {
                    let existing = find_state(target_items);
                    let target_index = states.size();
                    if(existing.has_value()) {
                        const ref existing_state = *existing;
                        target_index = existing_state;
                    } else {
                        states.push_back(move target_items);
                    }
                    transitions.insert(transition_key{ .state = state_index, .symbol = symbol }, target_index);
                }
                symbol_index += 1;
            }
            state_index += 1;
        }

        build_actions();
        return parser_tables{
            .grammar = move grammar,
            .states = move states,
            .transitions = move transitions,
            .action_table = move action_table,
            .goto_table = move goto_table,
            .conflicts = move conflicts
        };
    }
}

export build_parser_tables() -> parser_tables
{
    let tables = builder{make_minic_grammar()};
    return tables.build();
}
