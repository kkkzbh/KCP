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
}

export struct item_less {
}

export struct item_set {
    // 一个状态就是一组 LR(1) 项目。状态不是运行时栈上的某个语法树节点，而是“当前可能处在这些产生式位置”的集合。
    items: set<item, item_less>;
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
}

export enum parser_action_kind : u8 {
    // shift：当前状态看到某个终结符时，把这个 token 移入栈，并跳到目标状态。
    shift = 0;
    // reduce：当前状态看到某个 lookahead 时，按某条产生式把栈顶若干符号归约成左部非终结符。
    reduce = 1;
    // accept：增广产生式 program 已经完成，且向前看符号是 eof。
    accept = 2;
    // error：ACTION 表中没有合法动作。
    error = 3;
}

export struct parser_action {
    kind: parser_action_kind;
    target_state: usize;
    production: usize;
}

impl parser_action {
    parser_action()
    {
        return parser_action{
            .kind = parser_action_kind::error,
            .target_state = 0 as usize,
            .production = 0 as usize
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

export struct transition_key_less {
}

export struct action_key_less {
}

export struct goto_key_less {
}

export struct parser_tables {
    grammar: grammar;
    // 规范 LR(1) 项目集族 C，每个 item_set 是 DFA 的一个状态。
    states: vector<item_set>;
    // DFA 边：GOTO(I, X) = J。X 可以是终结符，也可以是非终结符。
    transitions: map<transition_key, usize, transition_key_less>;
    // ACTION[state, terminal]：运行时看到输入 token 时执行 shift/reduce/accept/error。
    action_table: map<action_key, parser_action, action_key_less>;
    // GOTO[state, nonterminal]：规约完成后，根据栈顶状态和产生式左部跳转。
    goto_table: map<goto_key, usize, goto_key_less>;
    // 如果同一格 ACTION 被填入不同动作，就记录冲突；LR(1) 文法应当为 0。
    conflicts: vector<parser_conflict>;
}

item_less_than(left: item const&, right: item const&) -> bool
{
    if(left.production != right.production) {
        return left.production < right.production;
    }
    if(left.dot != right.dot) {
        return left.dot < right.dot;
    }
    return token_rank(left.lookahead) < token_rank(right.lookahead);
}

impl item_less {
    operator ()(self const&, left: item const&, right: item const&) -> bool
    {
        return item_less_than(left, right);
    }
}

transition_key_less_than(left: transition_key const&, right: transition_key const&) -> bool
{
    if(left.state != right.state) {
        return left.state < right.state;
    }
    return symbol_less_than(left.symbol, right.symbol);
}

impl transition_key_less {
    operator ()(self const&, left: transition_key const&, right: transition_key const&) -> bool
    {
        return transition_key_less_than(left, right);
    }
}

action_key_less_than(left: action_key const&, right: action_key const&) -> bool
{
    if(left.state != right.state) {
        return left.state < right.state;
    }
    return token_rank(left.terminal) < token_rank(right.terminal);
}

impl action_key_less {
    operator ()(self const&, left: action_key const&, right: action_key const&) -> bool
    {
        return action_key_less_than(left, right);
    }
}

goto_key_less_than(left: goto_key const&, right: goto_key const&) -> bool
{
    if(left.state != right.state) {
        return left.state < right.state;
    }
    return nonterminal_rank(left.nonterminal) < nonterminal_rank(right.nonterminal);
}

impl goto_key_less {
    operator ()(self const&, left: goto_key const&, right: goto_key const&) -> bool
    {
        return goto_key_less_than(left, right);
    }
}

action_equal(left: parser_action const&, right: parser_action const&) -> bool
{
    return left.kind == right.kind and left.target_state == right.target_state and left.production == right.production;
}

collect_symbols(grammar: grammar const&) -> set<grammar_symbol, grammar_symbol_less>
{
    let result = set<grammar_symbol, grammar_symbol_less>{};
    result.insert(symbol_terminal(token_kind::eof));
    result.insert(symbol_epsilon());

    let production_index: usize = 0;
    while(production_index < grammar.productions.size()) {
        let production = &grammar.productions[production_index];
        result.insert(symbol_nonterminal((*production).lhs));
        let symbol_index: usize = 0;
        while(symbol_index < (*production).rhs.size()) {
            result.insert((*production).rhs[symbol_index]);
            symbol_index += 1;
        }
        production_index += 1;
    }
    return result;
}

merge_symbol_set(target: set<grammar_symbol, grammar_symbol_less>&, source: set<grammar_symbol, grammar_symbol_less> const&) -> bool
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

first_of_production_rhs(sequence: vector<grammar_symbol> const&, first_sets: map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less> const&) -> set<grammar_symbol, grammar_symbol_less>
{
    let result = set<grammar_symbol, grammar_symbol_less>{};
    let epsilon = symbol_epsilon();
    let nullable = true;
    let index: usize = 0;
    while(index < sequence.size()) {
        let current = sequence[index];
        let current_first_ref = first_sets.find(current);
        assert(current_first_ref.has_value(), "first set missing symbol");
        let current_first = &*current_first_ref;
        let first_index: usize = 0;
        while(first_index < (*current_first).size()) {
            let item = (*current_first).nth(first_index);
            if(not symbol_equal(item, epsilon)) {
                result.insert(item);
            }
            first_index += 1;
        }
        if(not (*current_first).contains(epsilon)) {
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

build_first_sets(grammar: grammar const&) -> map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less>
{
    // FIRST(X) 表示从符号 X 开始最终可能先看到哪些终结符。
    // 终结符的 FIRST 就是自己；非终结符的 FIRST 需要反复扫描产生式，直到没有新符号可加入。
    // LR(1) closure 里要算 FIRST(beta a)，用来决定新项目的 lookahead，所以 FIRST 集是构造 LR(1) 表的前置数据。
    let first_sets = map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less>{};
    let symbols = collect_symbols(grammar);
    let index: usize = 0;
    while(index < symbols.size()) {
        let symbol = symbols.nth(index);
        let values = set<grammar_symbol, grammar_symbol_less>{};
        if(symbol.kind == grammar_symbol_kind::terminal or symbol.kind == grammar_symbol_kind::epsilon) {
            values.insert(symbol);
        }
        first_sets.insert(symbol, values);
        index += 1;
    }

    let changed = true;
    while(changed) {
        changed = false;
        let production_index: usize = 0;
        while(production_index < grammar.productions.size()) {
            let production = &grammar.productions[production_index];
            let sequence_first = first_of_production_rhs((*production).rhs, first_sets);
            let lhs = symbol_nonterminal((*production).lhs);
            let target = first_sets.find(lhs);
            assert(target.has_value(), "first set missing nonterminal");
            if(merge_symbol_set(ref *target, sequence_first)) {
                changed = true;
            }
            production_index += 1;
        }
    }
    return first_sets;
}

item_next_symbol(grammar: grammar const&, item: item) -> optional<grammar_symbol>
{
    let production = &grammar.productions[item.production];
    if(item.dot >= (*production).rhs.size()) {
        return optional<grammar_symbol>::none;
    }
    return optional<grammar_symbol>::some((*production).rhs[item.dot]);
}

insert_closure_items_for_terminal(grammar: grammar const&, result: set<item, item_less>&, lhs: nonterminal_kind, lookahead: token_kind) -> bool
{
    let changed = false;
    let production_index: usize = 0;
    while(production_index < grammar.productions.size()) {
        let candidate = &grammar.productions[production_index];
        if((*candidate).lhs == lhs) {
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

insert_closure_items_for_first_suffix(grammar: grammar const&, result: set<item, item_less>&, lhs: nonterminal_kind, rhs: vector<grammar_symbol> const&, start: usize, lookahead: token_kind, first_sets: map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less> const&) -> bool
{
    let changed = false;
    let epsilon = symbol_epsilon();
    let nullable = true;
    let index = start;
    while(index < rhs.size()) {
        let current = rhs[index];
        let current_first_ref = first_sets.find(current);
        assert(current_first_ref.has_value(), "first set missing symbol");
        let current_first = &*current_first_ref;
        let first_index: usize = 0;
        while(first_index < (*current_first).size()) {
            let symbol = (*current_first).nth(first_index);
            if(not symbol_equal(symbol, epsilon)) {
                assert(symbol.kind == grammar_symbol_kind::terminal, "first set contains nonterminal symbol");
                if(insert_closure_items_for_terminal(grammar, result, lhs, symbol.terminal)) {
                    changed = true;
                }
            }
            first_index += 1;
        }
        if(not (*current_first).contains(epsilon)) {
            nullable = false;
            break;
        }
        index += 1;
    }
    if(nullable and insert_closure_items_for_terminal(grammar, result, lhs, lookahead)) {
        changed = true;
    }
    return changed;
}

closure_items(grammar: grammar const&, seed: set<item, item_less>, first_sets: map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less> const&) -> set<item, item_less>
{
    // closure(I)：如果状态里有 [A -> alpha . B beta, a]，说明下一步可能要识别 B。
    // 因此必须把 B 的所有产生式 [B -> . gamma, b] 加进来。
    // b 不是随便填的，它来自 FIRST(beta a)：如果 beta 能推出空串，就把原项目的 lookahead a 继续传给 B。
    let result = move seed;
    let changed = true;
    while(changed) {
        changed = false;
        let index: usize = 0;
        while(index < result.size()) {
            let item = result.nth(index);
            let next = item_next_symbol(grammar, item);
            if(next.has_value() and (*next).kind == grammar_symbol_kind::nonterminal) {
                let production = &grammar.productions[item.production];
                if(insert_closure_items_for_first_suffix(grammar, result, (*next).nonterminal, (*production).rhs, item.dot + 1, item.lookahead, first_sets)) {
                    changed = true;
                }
            }
            index += 1;
        }
    }
    return result;
}

goto_items(grammar: grammar const&, items: set<item, item_less> const&, symbol: grammar_symbol, first_sets: map<grammar_symbol, set<grammar_symbol, grammar_symbol_less>, grammar_symbol_less> const&) -> set<item, item_less>
{
    // goto(I, X)：把 I 中所有点前是 X 的项目向右移动一格，再对结果做 closure。
    // 它就是 LR 自动机的状态转移函数；后面填表时，终结符边变成 shift，非终结符边变成 GOTO 表项。
    let moved = set<item, item_less>{};
    let index: usize = 0;
    while(index < items.size()) {
        let item = items.nth(index);
        let next = item_next_symbol(grammar, item);
        if(next.has_value() and symbol_equal(*next, symbol)) {
            moved.insert(item{
                .production = item.production,
                .dot = item.dot + 1,
                .lookahead = item.lookahead
            });
        }
        index += 1;
    }
    return closure_items(grammar, move moved, first_sets);
}

next_symbols(grammar: grammar const&, items: set<item, item_less> const&) -> set<grammar_symbol, grammar_symbol_less>
{
    let result = set<grammar_symbol, grammar_symbol_less>{};
    let index: usize = 0;
    while(index < items.size()) {
        let item = items.nth(index);
        let next = item_next_symbol(grammar, item);
        if(next.has_value()) {
            result.insert(*next);
        }
        index += 1;
    }
    return result;
}

item_sets_equal(left: item_set const&, right: item_set const&) -> bool
{
    if(left.items.size() != right.items.size()) {
        return false;
    }
    let index: usize = 0;
    while(index < left.items.size()) {
        if(not right.items.contains(left.items.nth(index))) {
            return false;
        }
        index += 1;
    }
    return true;
}

find_state(states: vector<item_set> const&, items: item_set const&) -> optional<usize>
{
    let index: usize = 0;
    while(index < states.size()) {
        if(item_sets_equal(states[index], items)) {
            return optional<usize>::some(index);
        }
        index += 1;
    }
    return optional<usize>::none;
}

set_action(table: parser_tables&, key: action_key, action: parser_action) -> void
{
    // 同一格 ACTION[state, terminal] 只能有一个动作。
    // 如果出现 shift/reduce 或 reduce/reduce，说明这份文法不是当前 LR(1) 表能无歧义处理的形式。
    let existing = table.action_table.find(key);
    if(existing.has_value()) {
        if(not action_equal(*existing, action)) {
            table.conflicts.push_back(parser_conflict{
                .state = key.state,
                .terminal = key.terminal,
                .existing = *existing,
                .incoming = action
            });
        }
        return;
    }
    table.action_table.insert(key, action);
}

build_actions(table: parser_tables&) -> void
{
    // 第一轮处理 DFA 的边：
    //   1. state --terminal--> target  填 ACTION[state, terminal] = shift target
    //   2. state --nonterminal--> target 填 GOTO[state, nonterminal] = target
    let transition_index: usize = 0;
    while(transition_index < table.transitions.size()) {
        let entry = table.transitions.nth(transition_index);
        if(entry.key.symbol.kind == grammar_symbol_kind::terminal) {
            set_action(
                table,
                action_key{ .state = entry.key.state, .terminal = entry.key.symbol.terminal },
                parser_action{
                    .kind = parser_action_kind::shift,
                    .target_state = entry.value,
                    .production = 0 as usize
                }
            );
        } else if(entry.key.symbol.kind == grammar_symbol_kind::nonterminal) {
            table.goto_table.insert(
                goto_key{ .state = entry.key.state, .nonterminal = entry.key.symbol.nonterminal },
                entry.value
            );
        }
        transition_index += 1;
    }

    // 第二轮处理点已经到产生式末尾的项目：
    //   [A -> alpha ., a] 在 ACTION[state, a] 填 reduce A -> alpha
    //   [S' -> program ., eof] 填 accept
    // LR(1) 的关键就在这里：规约只发生在项目自己的 lookahead 上，不像 LR(0) 那样所有终结符都规约。
    let state_index: usize = 0;
    while(state_index < table.states.size()) {
        let items = &table.states[state_index].items;
        let item_index: usize = 0;
        while(item_index < (*items).size()) {
            let item = (*items).nth(item_index);
            let production = &table.grammar.productions[item.production];
            if(item.dot == (*production).rhs.size()) {
                if((*production).lhs == nonterminal_kind::augmented) {
                    set_action(
                        table,
                        action_key{ .state = state_index, .terminal = item.lookahead },
                        parser_action{
                            .kind = parser_action_kind::accept,
                            .target_state = state_index,
                            .production = item.production
                        }
                    );
                } else {
                    set_action(
                        table,
                        action_key{ .state = state_index, .terminal = item.lookahead },
                        parser_action{
                            .kind = parser_action_kind::reduce,
                            .target_state = state_index,
                            .production = item.production
                        }
                    );
                }
            }
            item_index += 1;
        }
        state_index += 1;
    }
}

export build_parser_tables() -> parser_tables
{
    // 构造规范 LR(1) 自动机：
    //   1. 从 [S' -> . program, eof] 做 closure 得到 I0。
    //   2. 对每个已有状态 I，枚举点后的符号 X，计算 goto(I, X)。
    //   3. 新 item_set 没出现过就加入 states，出现过就复用旧编号。
    //   4. 自动机稳定后再由 transitions 和完成项目填 ACTION/GOTO 表。
    let grammar = make_minic_grammar();
    let first_sets = build_first_sets(grammar);
    let states = vector<item_set>{};
    let transitions = map<transition_key, usize, transition_key_less>{};

    let start = set<item, item_less>{};
    start.insert(item{ .production = 0 as usize, .dot = 0 as usize, .lookahead = token_kind::eof });
    states.push_back(item_set{ .items = closure_items(grammar, move start, first_sets) });

    let state_index: usize = 0;
    while(state_index < states.size()) {
        let symbols = next_symbols(grammar, states[state_index].items);
        let symbol_index: usize = 0;
        while(symbol_index < symbols.size()) {
            let symbol = symbols.nth(symbol_index);
            let target_items = item_set{ .items = goto_items(grammar, states[state_index].items, symbol, first_sets) };
            if(target_items.items.size() != 0) {
                let existing = find_state(states, target_items);
                let target_index = states.size();
                if(existing.has_value()) {
                    target_index = *existing;
                } else {
                    states.push_back(move target_items);
                }
                transitions.insert(transition_key{ .state = state_index, .symbol = symbol }, target_index);
            }
            symbol_index += 1;
        }
        state_index += 1;
    }

    let table = parser_tables{
        .grammar = move grammar,
        .states = move states,
        .transitions = move transitions,
        .action_table = map<action_key, parser_action, action_key_less>{},
        .goto_table = map<goto_key, usize, goto_key_less>{},
        .conflicts = vector<parser_conflict>{}
    };
    build_actions(table);
    return table;
}
