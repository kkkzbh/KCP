export module parser;

import std;
import source;
export import lexer.token;
export import parser.ast;
export import diagnostic;

struct parser
{
    explicit parser(std::vector<token> tokens) :
        tokens(std::move(tokens)) {}

    auto parse_translation_unit_node() -> std::optional<translation_unit_syntax>;
    auto parse_module_header() -> std::optional<module_header_syntax>;
    auto parse_import_declaration(bool exported = false, std::optional<source_span> export_span = std::nullopt)
        -> std::optional<import_syntax>;
    auto parse_module_name() -> std::optional<module_name_syntax>;
    auto starts_function_definition() const -> bool;
    auto starts_top_level_item() const -> bool;
    auto check_contextual(std::string_view text, std::size_t lookahead = 0uz) const -> bool;
    auto starts_parameter() const -> bool;
    auto expect_parameter_start() -> bool;
    auto parse_function(function_syntax_kind kind = function_syntax_kind::free_function) -> std::optional<function_id>;
    auto parse_struct_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<struct_id>;
    auto parse_struct_field() -> std::optional<struct_field_syntax>;
    auto parse_variant_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<variant_id>;
    auto parse_variant_case() -> std::optional<variant_case_syntax>;
    auto parse_impl_block() -> std::optional<impl_id>;
    auto parse_concept_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<concept_id>;
    auto parse_concept_item() -> std::optional<concept_item_syntax>;
    auto parse_concept_requires() -> std::optional<concept_requires_syntax>;
    auto parse_requires_clause_until(token_kind end) -> std::optional<concept_requires_syntax>;
    auto parse_concept_requires_constraints_until(token_kind end) -> std::optional<std::vector<concept_requires_constraint_syntax>>;
    auto parse_concept_requires_primary() -> std::optional<concept_requires_constraint_syntax>;
    auto parse_type_alias() -> std::optional<type_alias_syntax>;
    auto parse_concept_function_requirement() -> std::optional<concept_function_requirement_syntax>;
    auto parse_concept_impl_block() -> std::optional<concept_impl_id>;
    auto parse_impl_item(type_syntax const& impl_type) -> std::optional<function_id>;
    auto parse_default_constructor(source_span start, source_span name, std::vector<parameter_syntax> parameters)
        -> std::optional<function_id>;
    auto parse_parameter_list() -> std::optional<std::vector<parameter_syntax>>;
    auto parse_parameter() -> std::optional<parameter_syntax>;
    auto parse_lambda_parameter_list() -> std::optional<std::vector<parameter_syntax>>;
    auto parse_lambda_parameter() -> std::optional<parameter_syntax>;
    auto parse_generic_parameter_list() -> std::optional<std::vector<generic_parameter_syntax>>;
    auto parse_generic_parameter() -> std::optional<generic_parameter_syntax>;
    auto consume_ellipsis() -> std::optional<source_span>;

    auto starts_type(token_kind kind) const -> bool;
    auto expect_type_start() -> bool;
    auto parse_type(bool allow_associated_names = true) -> std::optional<type_id>;
    auto looks_like_function_type() const -> bool;
    auto parse_function_type() -> std::optional<type_id>;
    auto parse_function_type_parameter() -> std::optional<function_type_parameter_syntax>;
    auto looks_like_decltype() const -> bool;
    auto parse_decltype() -> std::optional<type_id>;
    auto parse_type_argument_list() -> std::optional<std::vector<type_argument_syntax>>;
    auto starts_type_argument(token_kind kind) const -> bool;
    auto expect_type_argument_start() -> bool;
    auto parse_type_argument() -> std::optional<type_argument_syntax>;

    auto starts_expression(token_kind kind) const -> bool;
    auto expect_expression_start() -> bool;
    auto parse_expression() -> std::optional<expr_id>;
    auto parse_assignment() -> std::optional<expr_id>;
    auto is_assignment_operator(token_kind kind) const -> bool;
    auto parse_expression_pratt(int min_bp) -> std::optional<expr_id>;
    auto parse_unary() -> std::optional<expr_id>;
    auto parse_postfix() -> std::optional<expr_id>;
    auto parse_primary() -> std::optional<expr_id>;
    auto looks_like_generic_call_suffix() const -> bool;
    auto looks_like_associated_name_expression() const -> bool;
    auto parse_associated_name_expression() -> std::optional<expr_id>;
    auto parse_match_expression() -> std::optional<expr_id>;
    auto parse_match_arm() -> std::optional<match_arm_syntax>;
    auto parse_match_pattern() -> std::optional<match_pattern_syntax>;
    auto parse_lambda_expression() -> std::optional<expr_id>;
    auto parse_lambda_body(source_span start) -> std::optional<stmt_id>;
    auto looks_like_type_initializer() const -> bool;
    auto parse_type_initializer() -> std::optional<expr_id>;
    auto parse_struct_initializer_list(type_id type, source_span start) -> std::optional<expr_id>;
    auto parse_block_expression() -> std::optional<expr_id>;
    auto type_from_name(source_span name) -> type_id;
    auto parse_array_literal() -> std::optional<expr_id>;
    auto parse_paren_expression() -> std::optional<expr_id>;

    auto parse_statement() -> std::optional<stmt_id>;
    auto parse_expected_expression() -> std::optional<expr_id>;
    auto parse_expected_type() -> std::optional<type_id>;
    auto parse_block_statement() -> std::optional<stmt_id>;
    auto parse_declaration_statement() -> std::optional<stmt_id>;
    auto parse_type_alias_statement() -> std::optional<stmt_id>;
    auto parse_if_statement() -> std::optional<stmt_id>;
    auto parse_while_statement() -> std::optional<stmt_id>;
    auto parse_do_while_statement() -> std::optional<stmt_id>;
    auto parse_for_statement() -> std::optional<stmt_id>;
    auto parse_break_continue_statement(bool is_break) -> std::optional<stmt_id>;
    auto parse_return_statement() -> std::optional<stmt_id>;
    auto parse_expression_statement() -> std::optional<stmt_id>;

    auto peek(std::size_t lookahead = 0uz) const -> token
    {
        if(lookahead < injected.size()) {
            return injected[lookahead];
        }

        auto raw_index = index + lookahead - injected.size();
        if(raw_index >= tokens.size()) {
            return tokens.back();
        }

        return tokens[raw_index];
    }

    auto check(token_kind kind) const -> bool
    {
        return peek().kind == kind;
    }

    auto check_any(std::initializer_list<token_kind> kinds) const -> bool
    {
        return std::ranges::find(kinds, peek().kind) != kinds.end();
    }

    auto consume() -> token
    {
        auto current = peek();
        if(not injected.empty()) {
            injected.pop_front();
        } else if(index < tokens.size()) {
            ++index;
        }

        return current;
    }

    auto report_current(diagnostic_kind kind, std::string message) -> void
    {
        diagnostics.report(kind, std::move(message), peek().span);
    }

    auto token_name(token_kind kind) const -> std::string_view
    {
        using enum token_kind;
        switch(kind) {
            case eof:
                return "end of file";
            case invalid:
                return "valid token";
            case identifier:
                return "identifier";
            case integer_literal:
                return "integer literal";
            case float_literal:
                return "float literal";
            case char_literal:
                return "char literal";
            case string_literal:
                return "string literal";
            case kw_let:
                return "'let'";
            case kw_const:
                return "'const'";
            case kw_if:
                return "'if'";
            case kw_else:
                return "'else'";
            case kw_while:
                return "'while'";
            case kw_do:
                return "'do'";
            case kw_for:
                return "'for'";
            case kw_break:
                return "'break'";
            case kw_continue:
                return "'continue'";
            case kw_return:
                return "'return'";
            case kw_import:
                return "'import'";
            case kw_export:
                return "'export'";
            case kw_module:
                return "'module'";
            case kw_struct:
                return "'struct'";
            case kw_impl:
                return "'impl'";
            case kw_concept:
                return "'concept'";
            case kw_as:
                return "'as'";
            case kw_true:
                return "'true'";
            case kw_false:
                return "'false'";
            case kw_and:
                return "'and'";
            case kw_or:
                return "'or'";
            case kw_not:
                return "'not'";
            case l_paren:
                return "'('";
            case r_paren:
                return "')'";
            case l_brace:
                return "'{'";
            case r_brace:
                return "'}'";
            case l_bracket:
                return "'['";
            case r_bracket:
                return "']'";
            case comma:
                return "','";
            case semicolon:
                return "';'";
            case colon:
                return "':'";
            case colon_colon:
                return "'::'";
            case dot:
                return "'.'";
            case arrow:
                return "'->'";
            case plus:
                return "'+'";
            case plus_equal:
                return "'+='";
            case minus:
                return "'-'";
            case minus_equal:
                return "'-='";
            case star:
                return "'*'";
            case star_equal:
                return "'*='";
            case slash:
                return "'/'";
            case slash_equal:
                return "'/='";
            case percent:
                return "'%'";
            case percent_equal:
                return "'%='";
            case equal:
                return "'='";
            case equal_equal:
                return "'=='";
            case bang_equal:
                return "'!='";
            case less:
                return "'<'";
            case less_equal:
                return "'<='";
            case greater:
                return "'>'";
            case greater_equal:
                return "'>='";
            case amp:
                return "'&'";
            case amp_equal:
                return "'&='";
            case pipe:
                return "'|'";
            case pipe_equal:
                return "'|='";
            case caret:
                return "'^'";
            case caret_equal:
                return "'^='";
            case tilde:
                return "'~'";
            case less_less:
                return "'<<'";
            case less_less_equal:
                return "'<<='";
            case greater_greater:
                return "'>>'";
            case greater_greater_equal:
                return "'>>='";
            case plus_plus:
                return "'++'";
            case minus_minus:
                return "'--'";
            case question:
                return "'?'";
        }

        std::unreachable();
    }

    auto expect(token_kind kind) -> std::optional<token>
    {
        if(not check(kind)) {
            report_current(
                diagnostic_kind::expected_token,
                std::format("expected {}, got {}", token_name(kind), token_name(peek().kind))
            );
            return std::nullopt;
        }

        return consume();
    }

    auto expect_identifier(std::string_view what) -> std::optional<token>
    {
        if(not check(token_kind::identifier)) {
            report_current(
                diagnostic_kind::expected_identifier,
                std::format("expected {}, got {}", what, token_name(peek().kind))
            );
            return std::nullopt;
        }

        return consume();
    }

    auto split_current_double_greater() -> bool
    {
        if(not injected.empty() or index >= tokens.size()) {
            return false;
        }

        auto current = tokens[index];
        if (
            current.kind != token_kind::greater_greater
            and current.kind != token_kind::greater_greater_equal
        ) {
            return false;
        }

        auto first = token {
            .kind = token_kind::greater,
            .span = source_span {
                .start = current.span.start,
                .end = current.span.start + 1,
            },
            .flags = current.flags,
        };

        auto second_kind = (
            current.kind == token_kind::greater_greater
                ? token_kind::greater
                : token_kind::greater_equal
        );

        auto second = token {
            .kind = second_kind,
            .span = source_span {
                .start = current.span.start + 1,
                .end = current.span.end,
            },
            .flags = current.flags,
        };

        ++index;
        injected.emplace_back(first);
        injected.emplace_back(second);
        return true;
    }

    auto expect_closing_angle() -> std::optional<token>
    {
        if(not check(token_kind::greater)) {
            if(not split_current_double_greater() or not check(token_kind::greater)) {
                report_current(
                    diagnostic_kind::expected_token,
                    std::format(
                        "expected {}, got {}",
                        token_name(token_kind::greater),
                        token_name(peek().kind)
                    )
                );
                return std::nullopt;
            }
        }

        return consume();
    }

    auto synchronize_statement() -> void
    {
        auto brace_depth = 0uz;

        while(not check(token_kind::eof)) {
            if(brace_depth == 0uz) {
                if(check(token_kind::semicolon)) {
                    consume();
                    return;
                }

                if(check(token_kind::r_brace)) {
                    return;
                }
            }

            switch(peek().kind) {
                case token_kind::l_brace:
                    ++brace_depth;
                    break;
                case token_kind::r_brace:
                    if(brace_depth > 0uz) {
                        --brace_depth;
                    }
                    break;
                default:
                    break;
            }

            consume();
        }
    }

    auto synchronize_import_list() -> void
    {
        while(not check_any({
            token_kind::semicolon,
            token_kind::kw_import,
            token_kind::kw_export,
            token_kind::kw_struct,
            token_kind::kw_impl,
            token_kind::kw_concept,
            token_kind::identifier,
            token_kind::eof,
        })) {
            consume();
        }

        if(check(token_kind::semicolon)) {
            consume();
        }
    }

    auto synchronize_function_list() -> void
    {
        while(not check_any({
            token_kind::semicolon,
            token_kind::kw_export,
            token_kind::kw_struct,
            token_kind::kw_impl,
            token_kind::kw_concept,
            token_kind::identifier,
            token_kind::eof,
        })) {
            consume();
        }

        if(check(token_kind::semicolon)) {
            consume();
        }
    }

    std::vector<token> tokens;
    std::size_t index{};
    std::deque<token> injected{};
    ast_arena arena{};
    diagnostic_collector diagnostics{};
    std::vector<function_id> synthetic_functions{};
};

export struct [[nodiscard]] parse_result
{
    bool accepted{};
    ast_arena ast{};
    std::optional<translation_unit_syntax> root{};
    std::vector<diagnostic> diagnostics{};
};

export auto parse_translation_unit(std::vector<token> tokens) -> parse_result
{
    auto state = parser{ std::move(tokens) };
    auto root = state.parse_translation_unit_node();
    auto accepted = state.diagnostics.empty() and root and state.peek().kind == token_kind::eof;

    return parse_result {
        .accepted = accepted,
        .ast = std::move(state.arena),
        .root = std::move(root),
        .diagnostics = state.diagnostics.take(),
    };
}
