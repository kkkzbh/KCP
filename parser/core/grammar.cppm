export module parser.grammar;

import std;

export struct grammar_production
{
    std::string lhs;
    std::vector<std::string> rhs;
};

export struct grammar_definition
{
    std::string start_symbol;
    std::vector<grammar_production> productions;
};

export struct grammar_analysis
{
    std::map<std::string, std::set<std::string>> first_sets;
    std::map<std::string, std::set<std::string>> follow_sets;
    std::vector<std::set<std::string>> select_sets;
    std::vector<std::string> conflicts;
};

export [[nodiscard]] auto epsilon_symbol() -> std::string_view;
export [[nodiscard]] auto end_marker_symbol() -> std::string_view;
export [[nodiscard]] auto cp_ll1_grammar() -> grammar_definition;
export [[nodiscard]] auto analyze_grammar(grammar_definition const& grammar) -> grammar_analysis;

namespace {
[[nodiscard]] auto is_nonterminal(
    std::set<std::string> const& nonterminals,
    std::string_view symbol) -> bool
{
    return std::ranges::find(nonterminals, symbol) != nonterminals.end();
}

[[nodiscard]] auto first_of_sequence(
    grammar_definition const& grammar,
    std::map<std::string, std::set<std::string>> const& first_sets,
    std::vector<std::string> const& sequence) -> std::set<std::string>
{
    auto const epsilon = std::string(epsilon_symbol());
    auto result = std::set<std::string>{};

    auto nonterminals = std::set<std::string>{};
    for(auto const& production : grammar.productions) {
        nonterminals.insert(production.lhs);
    }

    if(sequence.empty()) {
        result.insert(epsilon);
        return result;
    }

    auto nullable_prefix = true;
    for(auto const& symbol : sequence) {
        if(not is_nonterminal(nonterminals, symbol)) {
            result.insert(symbol);
            nullable_prefix = false;
            break;
        }

        auto const& first = first_sets.at(symbol);
        for(auto const& item : first) {
            if(item != epsilon) {
                result.insert(item);
            }
        }

        if(not first.contains(epsilon)) {
            nullable_prefix = false;
            break;
        }
    }

    if(nullable_prefix) {
        result.insert(epsilon);
    }

    return result;
}
} // namespace

auto epsilon_symbol() -> std::string_view
{
    return "<epsilon>";
}

auto end_marker_symbol() -> std::string_view
{
    return "$";
}

auto cp_ll1_grammar() -> grammar_definition
{
    using production = grammar_production;
    return grammar_definition{
        .start_symbol = "TranslationUnit",
        .productions = {
            production{ "TranslationUnit", { "TranslationUnitPrefix" } },
            production{ "TranslationUnitPrefix", { "kw_export", "TranslationUnitAfterExport" } },
            production{ "TranslationUnitPrefix", { "kw_import", "QualifiedName", "semicolon", "ImportListTail", "FunctionList" } },
            production{ "TranslationUnitPrefix", { "identifier", "FunctionAfterName", "FunctionList" } },
            production{ "TranslationUnitPrefix", {} },
            production{ "TranslationUnitAfterExport", { "kw_module", "QualifiedName", "semicolon", "ImportList", "FunctionList" } },
            production{ "TranslationUnitAfterExport", { "identifier", "FunctionAfterName", "FunctionList" } },
            production{ "ImportList", { "kw_import", "QualifiedName", "semicolon", "ImportList" } },
            production{ "ImportList", {} },
            production{ "ImportListTail", { "kw_import", "QualifiedName", "semicolon", "ImportListTail" } },
            production{ "ImportListTail", {} },
            production{ "FunctionList", { "FunctionLead", "FunctionList" } },
            production{ "FunctionList", {} },
            production{ "FunctionLead", { "kw_export", "identifier", "FunctionAfterName" } },
            production{ "FunctionLead", { "identifier", "FunctionAfterName" } },
            production{ "FunctionAfterName", { "l_paren", "ParameterListOpt", "r_paren", "ReturnTypeOpt", "Block" } },
            production{ "ParameterListOpt", { "ParameterList" } },
            production{ "ParameterListOpt", {} },
            production{ "ParameterList", { "Parameter", "ParameterListTail" } },
            production{ "ParameterListTail", { "comma", "Parameter", "ParameterListTail" } },
            production{ "ParameterListTail", {} },
            production{ "Parameter", { "ParameterConstOpt", "identifier", "colon", "Type" } },
            production{ "ParameterConstOpt", { "kw_const" } },
            production{ "ParameterConstOpt", {} },
            production{ "ReturnTypeOpt", { "arrow", "Type" } },
            production{ "ReturnTypeOpt", {} },
            production{ "Block", { "l_brace", "StatementList", "r_brace" } },
            production{ "StatementList", { "Statement", "StatementList" } },
            production{ "StatementList", {} },
            production{ "Statement", { "Block" } },
            production{ "Statement", { "DeclarationStmt" } },
            production{ "Statement", { "IfStmt" } },
            production{ "Statement", { "WhileStmt" } },
            production{ "Statement", { "DoWhileStmt" } },
            production{ "Statement", { "ForStmt" } },
            production{ "Statement", { "BreakStmt" } },
            production{ "Statement", { "ContinueStmt" } },
            production{ "Statement", { "ReturnStmt" } },
            production{ "Statement", { "ExprStmt" } },
            production{ "DeclarationStmt", { "LetOrConst", "identifier", "DeclTypeOpt", "equal", "Expression", "semicolon" } },
            production{ "LetOrConst", { "kw_let" } },
            production{ "LetOrConst", { "kw_const" } },
            production{ "DeclTypeOpt", { "colon", "Type" } },
            production{ "DeclTypeOpt", {} },
            production{ "IfStmt", { "kw_if", "l_paren", "Expression", "r_paren", "Block", "ElseOpt" } },
            production{ "ElseOpt", { "kw_else", "ElseBranch" } },
            production{ "ElseOpt", {} },
            production{ "ElseBranch", { "IfStmt" } },
            production{ "ElseBranch", { "Block" } },
            production{ "WhileStmt", { "kw_while", "l_paren", "Expression", "r_paren", "Block" } },
            production{ "DoWhileStmt", { "kw_do", "Block", "kw_while", "l_paren", "Expression", "r_paren", "semicolon" } },
            production{ "ForStmt", { "kw_for", "ForLabelOpt", "l_paren", "ForBinding", "colon", "Expression", "r_paren", "Block" } },
            production{ "ForLabelOpt", { "colon", "identifier" } },
            production{ "ForLabelOpt", {} },
            production{ "ForBinding", { "LetOrConst", "identifier" } },
            production{ "BreakStmt", { "kw_break", "LabelOpt", "semicolon" } },
            production{ "ContinueStmt", { "kw_continue", "LabelOpt", "semicolon" } },
            production{ "LabelOpt", { "identifier" } },
            production{ "LabelOpt", {} },
            production{ "ReturnStmt", { "kw_return", "ReturnExprOpt", "semicolon" } },
            production{ "ReturnExprOpt", { "Expression" } },
            production{ "ReturnExprOpt", {} },
            production{ "ExprStmt", { "ExpressionNoBrace", "semicolon" } },
            production{ "Type", { "QualifiedName", "TypeArgumentOpt", "TypeConstOpt", "TypeSuffixList" } },
            production{ "TypeArgumentOpt", { "type_less", "TypeArgumentList", "type_greater" } },
            production{ "TypeArgumentOpt", {} },
            production{ "TypeArgumentList", { "TypeArgument", "TypeArgumentTail" } },
            production{ "TypeArgumentTail", { "comma", "TypeArgument", "TypeArgumentTail" } },
            production{ "TypeArgumentTail", {} },
            production{ "TypeArgument", { "Type" } },
            production{ "TypeArgument", { "integer_literal" } },
            production{ "TypeConstOpt", { "kw_const" } },
            production{ "TypeConstOpt", {} },
            production{ "TypeSuffixList", { "TypeSuffix", "TypeSuffixList" } },
            production{ "TypeSuffixList", {} },
            production{ "TypeSuffix", { "type_amp" } },
            production{ "TypeSuffix", { "type_star" } },
            production{ "QualifiedName", { "identifier", "QualifiedNameTail" } },
            production{ "QualifiedNameTail", { "colon_colon", "identifier", "QualifiedNameTail" } },
            production{ "QualifiedNameTail", {} },
            production{ "Expression", { "Assignment" } },
            production{ "ExpressionNoBrace", { "AssignmentNoBrace" } },
            production{ "Assignment", { "Unary", "AssignmentTail" } },
            production{ "AssignmentNoBrace", { "UnaryNoBrace", "AssignmentTail" } },
            production{ "AssignmentTail", { "AssignmentOperator", "Assignment" } },
            production{ "AssignmentTail", {} },
            production{ "AssignmentOperator", { "equal" } },
            production{ "AssignmentOperator", { "plus_equal" } },
            production{ "AssignmentOperator", { "minus_equal" } },
            production{ "AssignmentOperator", { "star_equal" } },
            production{ "AssignmentOperator", { "slash_equal" } },
            production{ "AssignmentOperator", { "percent_equal" } },
            production{ "AssignmentOperator", { "amp_equal" } },
            production{ "AssignmentOperator", { "pipe_equal" } },
            production{ "AssignmentOperator", { "caret_equal" } },
            production{ "AssignmentOperator", { "less_less_equal" } },
            production{ "AssignmentOperator", { "greater_greater_equal" } },
            production{ "Unary", { "PrefixOperator", "Unary" } },
            production{ "Unary", { "Postfix" } },
            production{ "UnaryNoBrace", { "PrefixOperator", "UnaryNoBrace" } },
            production{ "UnaryNoBrace", { "PostfixNoBrace" } },
            production{ "PrefixOperator", { "plus" } },
            production{ "PrefixOperator", { "minus" } },
            production{ "PrefixOperator", { "kw_not" } },
            production{ "PrefixOperator", { "tilde" } },
            production{ "PrefixOperator", { "amp" } },
            production{ "PrefixOperator", { "star" } },
            production{ "PrefixOperator", { "plus_plus" } },
            production{ "PrefixOperator", { "minus_minus" } },
            production{ "Postfix", { "Primary", "PostfixTail" } },
            production{ "PostfixNoBrace", { "PrimaryNoBrace", "PostfixTail" } },
            production{ "PostfixTail", { "CallSuffix", "PostfixTail" } },
            production{ "PostfixTail", { "PostfixOperator", "PostfixTail" } },
            production{ "PostfixTail", {} },
            production{ "CallSuffix", { "l_paren", "ArgumentListOpt", "r_paren" } },
            production{ "PostfixOperator", { "plus_plus" } },
            production{ "PostfixOperator", { "minus_minus" } },
            production{ "ArgumentListOpt", { "ArgumentList" } },
            production{ "ArgumentListOpt", {} },
            production{ "ArgumentList", { "Expression", "ArgumentListTail" } },
            production{ "ArgumentListTail", { "comma", "Expression", "ArgumentListTail" } },
            production{ "ArgumentListTail", {} },
            production{ "Primary", { "Literal" } },
            production{ "Primary", { "QualifiedName" } },
            production{ "Primary", { "ArrayLiteral" } },
            production{ "Primary", { "SequenceLiteral" } },
            production{ "Primary", { "ParenPrimary" } },
            production{ "PrimaryNoBrace", { "Literal" } },
            production{ "PrimaryNoBrace", { "QualifiedName" } },
            production{ "PrimaryNoBrace", { "ArrayLiteral" } },
            production{ "PrimaryNoBrace", { "ParenPrimary" } },
            production{ "ParenPrimary", { "l_paren", "Expression", "ParenPrimaryTail" } },
            production{ "ParenPrimaryTail", { "r_paren" } },
            production{ "ParenPrimaryTail", { "comma", "Expression", "TupleTail", "r_paren" } },
            production{ "TupleTail", { "comma", "Expression", "TupleTail" } },
            production{ "TupleTail", {} },
            production{ "ArrayLiteral", { "l_bracket", "ElementListOpt", "r_bracket" } },
            production{ "SequenceLiteral", { "l_brace", "ElementListOpt", "r_brace" } },
            production{ "ElementListOpt", { "ElementList" } },
            production{ "ElementListOpt", {} },
            production{ "ElementList", { "Expression", "ElementListTail" } },
            production{ "ElementListTail", { "comma", "Expression", "ElementListTail" } },
            production{ "ElementListTail", {} },
            production{ "Literal", { "integer_literal" } },
            production{ "Literal", { "float_literal" } },
            production{ "Literal", { "char_literal" } },
            production{ "Literal", { "string_literal" } },
            production{ "Literal", { "kw_true" } },
            production{ "Literal", { "kw_false" } },
        },
    };
}

auto analyze_grammar(grammar_definition const& grammar) -> grammar_analysis
{
    auto const epsilon = std::string(epsilon_symbol());
    auto const eof_marker = std::string(end_marker_symbol());
    auto nonterminals = std::set<std::string>{};
    for(auto const& production : grammar.productions) {
        nonterminals.insert(production.lhs);
    }

    auto analysis = grammar_analysis{};
    for(auto const& nonterminal : nonterminals) {
        analysis.first_sets[nonterminal] = {};
        analysis.follow_sets[nonterminal] = {};
    }
    analysis.follow_sets[grammar.start_symbol].insert(eof_marker);

    auto changed = true;
    while(changed) {
        changed = false;
        for(auto const& production : grammar.productions) {
            auto& target = analysis.first_sets[production.lhs];
            auto const before = target.size();
            auto const first = first_of_sequence(grammar, analysis.first_sets, production.rhs);
            target.insert(first.begin(), first.end());
            changed = changed or before != target.size();
        }
    }

    changed = true;
    while(changed) {
        changed = false;
        for(auto const& production : grammar.productions) {
            for(auto const index : std::views::iota(0uz, production.rhs.size())) {
                auto const& symbol = production.rhs[index];
                if(not is_nonterminal(nonterminals, symbol)) {
                    continue;
                }

                auto const beta = std::vector<std::string>(
                    production.rhs.begin() + static_cast<std::ptrdiff_t>(index + 1),
                    production.rhs.end());

                auto const beta_first = first_of_sequence(grammar, analysis.first_sets, beta);
                auto& follow = analysis.follow_sets[symbol];
                auto const before = follow.size();
                for(auto const& item : beta_first) {
                    if(item != epsilon) {
                        follow.insert(item);
                    }
                }

                if(beta.empty() or beta_first.contains(epsilon)) {
                    auto const& lhs_follow = analysis.follow_sets[production.lhs];
                    follow.insert(lhs_follow.begin(), lhs_follow.end());
                }

                changed = changed or before != follow.size();
            }
        }
    }

    analysis.select_sets.reserve(grammar.productions.size());
    auto grouped = std::map<std::string, std::vector<std::pair<std::size_t, std::set<std::string>>>>{};

    for(auto const index : std::views::iota(0uz, grammar.productions.size())) {
        auto const& production = grammar.productions[index];
        auto const first = first_of_sequence(grammar, analysis.first_sets, production.rhs);
        auto select = std::set<std::string>{};
        for(auto const& item : first) {
            if(item != epsilon) {
                select.insert(item);
            }
        }
        if(first.contains(epsilon)) {
            auto const& follow = analysis.follow_sets[production.lhs];
            select.insert(follow.begin(), follow.end());
        }

        analysis.select_sets.push_back(select);
        grouped[production.lhs].push_back({ index, select });
    }

    for(auto const& [lhs, items] : grouped) {
        for(auto const left : std::views::iota(0uz, items.size())) {
            for(auto const right : std::views::iota(left + 1, items.size())) {
                auto overlap = std::vector<std::string>{};
                std::ranges::set_intersection(
                    items[left].second,
                    items[right].second,
                    std::back_inserter(overlap));
                if(not overlap.empty()) {
                    analysis.conflicts.push_back(std::format(
                        "{} conflict between production {} and {} on {}",
                        lhs,
                        items[left].first,
                        items[right].first,
                        std::accumulate(
                            std::next(overlap.begin()),
                            overlap.end(),
                            overlap.front(),
                            [](std::string acc, std::string_view item) {
                                return std::move(acc).append(",").append(item);
                            })));
                }
            }
        }
    }

    return analysis;
}
