export module parser.recursive_descent;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import parser.trace;
import parser.expression_precedence;

export struct parse_options
{
    bool trace_enabled{};
};

export struct parse_result
{
    bool accepted{};
    std::unique_ptr<translation_unit_syntax> syntax_tree{};
    std::vector<parser_diagnostic> diagnostics{};
    std::vector<diagnostic> lexer_diagnostics{};
    std::vector<trace_event> trace{};
};

export [[nodiscard]] auto parse_translation_unit(
    source_manager const& sources,
    file_id file,
    parse_options options = {}) -> parse_result;

namespace {
struct recursive_descent_parser final
{
    recursive_descent_parser(
        source_manager const& sources,
        std::vector<token> tokens,
        parse_options options)
        : sources_(sources), tokens_(std::move(tokens)), options_(options)
    {
    }

    [[nodiscard]] auto parse() -> parse_result
    {
        auto tree = parse_translation_unit_node();
        auto const accepted = diagnostics_.empty() and tree != nullptr and peek().kind == token_kind::eof;

        return parse_result{
            .accepted = accepted,
            .syntax_tree = std::move(tree),
            .diagnostics = std::move(diagnostics_),
            .trace = std::move(trace_),
        };
    }

private:
    struct trace_scope
    {
        trace_scope(recursive_descent_parser& parser, std::string_view label)
            : parser(parser), label(label)
        {
            parser.record_trace(trace_event_kind::enter, this->label, parser.peek());
        }

        ~trace_scope()
        {
            if(not completed) {
                parser.record_trace(trace_event_kind::fail, label, parser.peek());
            }
        }

        auto succeed() -> void
        {
            if(completed) {
                return;
            }
            completed = true;
            parser.record_trace(trace_event_kind::exit, label, parser.peek());
        }

        recursive_descent_parser& parser;
        std::string label;
        bool completed{};
    };

    [[nodiscard]] auto peek(std::size_t lookahead = 0uz) const -> token
    {
        if(lookahead < injected_.size()) {
            return injected_[lookahead];
        }

        auto const raw_index = index_ + lookahead - injected_.size();
        if(raw_index >= tokens_.size()) {
            return tokens_.back();
        }

        return tokens_[raw_index];
    }

    [[nodiscard]] auto previous_token() const -> token
    {
        if(not consumed_.has_value()) {
            return tokens_.front();
        }

        return *consumed_;
    }

    [[nodiscard]] auto at(token_kind kind) const -> bool
    {
        return peek().kind == kind;
    }

    [[nodiscard]] auto at_any(std::initializer_list<token_kind> kinds) const -> bool
    {
        return std::ranges::find(kinds, peek().kind) != kinds.end();
    }

    [[nodiscard]] auto is_unsupported_construct_token(token_kind kind) const -> bool
    {
        return kind == token_kind::kw_struct or kind == token_kind::kw_impl or kind == token_kind::kw_trait;
    }

    auto report_unsupported_construct() -> void
    {
        report_current(
            parser_diagnostic_code::unsupported_construct,
            "struct/impl/trait are reserved for later experiments and are not part of the 2.1 grammar");
    }

    auto record_trace(trace_event_kind kind, std::string_view label, token current) -> void
    {
        if(not options_.trace_enabled) {
            return;
        }

        trace_.push_back(trace_event{
            .kind = kind,
            .label = std::string(label),
            .current = current,
        });
    }

    [[nodiscard]] auto consume() -> token
    {
        auto const current = peek();
        if(not injected_.empty()) {
            injected_.pop_front();
        } else if(index_ < tokens_.size()) {
            ++index_;
        }

        consumed_ = current;
        record_trace(trace_event_kind::match, to_string(current.kind), current);
        return current;
    }

    auto report(parser_diagnostic_code code, std::string message, span location) -> void
    {
        diagnostics_.push_back(parser_diagnostic{
            .code = code,
            .message = std::move(message),
            .primary_span = location,
        });
    }

    auto report_current(parser_diagnostic_code code, std::string message) -> void
    {
        report(code, std::move(message), peek().source_span);
    }

    [[nodiscard]] auto expect(token_kind kind, std::string_view what) -> std::optional<token>
    {
        if(at(kind)) {
            return consume();
        }

        report_current(
            parser_diagnostic_code::expected_token,
            std::format("expected {}, got {}", what, to_string(peek().kind)));
        return std::nullopt;
    }

    [[nodiscard]] auto expect_identifier(std::string_view what) -> std::optional<token>
    {
        if(at(token_kind::identifier)) {
            return consume();
        }

        report_current(
            parser_diagnostic_code::expected_identifier,
            std::format("expected {}, got {}", what, to_string(peek().kind)));
        return std::nullopt;
    }

    auto split_current_double_greater() -> bool
    {
        if(not injected_.empty() or index_ >= tokens_.size()) {
            return false;
        }

        auto const current = tokens_[index_];
        if(current.kind != token_kind::greater_greater
           and current.kind != token_kind::greater_greater_equal) {
            return false;
        }

        auto const first = token{
            .kind = token_kind::greater,
            .source_span = span{
                .file = current.source_span.file,
                .start = current.source_span.start,
                .end = current.source_span.start + 1,
            },
            .flags = current.flags,
        };

        auto const second_kind =
            current.kind == token_kind::greater_greater ? token_kind::greater : token_kind::greater_equal;

        auto const second = token{
            .kind = second_kind,
            .source_span = span{
                .file = current.source_span.file,
                .start = current.source_span.start + 1,
                .end = current.source_span.end,
            },
            .flags = current.flags,
        };

        ++index_;
        injected_.push_back(first);
        injected_.push_back(second);
        return true;
    }

    [[nodiscard]] auto expect_closing_angle() -> std::optional<token>
    {
        if(at(token_kind::greater)) {
            return consume();
        }

        if(split_current_double_greater() and at(token_kind::greater)) {
            return consume();
        }

        report_current(
            parser_diagnostic_code::expected_token,
            std::format("expected '>', got {}", to_string(peek().kind)));
        return std::nullopt;
    }

    auto synchronize_statement() -> void
    {
        auto paren_depth = 0uz;
        auto brace_depth = 0uz;
        auto bracket_depth = 0uz;

        while(not at(token_kind::eof)) {
            if(paren_depth == 0uz and brace_depth == 0uz and bracket_depth == 0uz
               and at_any({ token_kind::semicolon, token_kind::r_brace })) {
                break;
            }

            switch(peek().kind) {
            case token_kind::l_paren: ++paren_depth; break;
            case token_kind::r_paren:
                if(paren_depth > 0uz) {
                    --paren_depth;
                }
                break;
            case token_kind::l_brace: ++brace_depth; break;
            case token_kind::r_brace:
                if(brace_depth > 0uz) {
                    --brace_depth;
                }
                break;
            case token_kind::l_bracket: ++bracket_depth; break;
            case token_kind::r_bracket:
                if(bracket_depth > 0uz) {
                    --bracket_depth;
                }
                break;
            default: break;
            }

            (void)consume();
        }

        if(at(token_kind::semicolon)) {
            (void)consume();
        }
    }

    auto synchronize_top_level() -> void
    {
        while(not at_any({
            token_kind::semicolon,
            token_kind::kw_import,
            token_kind::kw_export,
            token_kind::identifier,
            token_kind::eof,
        })) {
            (void)consume();
        }

        if(at(token_kind::semicolon)) {
            (void)consume();
        }
    }

    auto skip_unsupported_top_level_construct() -> void
    {
        auto brace_depth = 0uz;

        while(not at(token_kind::eof)) {
            if(at(token_kind::l_brace)) {
                ++brace_depth;
                (void)consume();
                continue;
            }

            if(at(token_kind::r_brace)) {
                (void)consume();
                if(brace_depth == 0uz) {
                    return;
                }

                --brace_depth;
                if(brace_depth == 0uz) {
                    return;
                }
                continue;
            }

            if(brace_depth == 0uz and at(token_kind::semicolon)) {
                (void)consume();
                return;
            }

            (void)consume();
        }
    }

    [[nodiscard]] auto starts_expression(token_kind kind) const -> bool
    {
        using enum token_kind;
        return kind == identifier
            or kind == integer_literal
            or kind == float_literal
            or kind == char_literal
            or kind == string_literal
            or kind == kw_true
            or kind == kw_false
            or kind == l_paren
            or kind == l_bracket
            or kind == l_brace
            or kind == plus
            or kind == minus
            or kind == kw_not
            or kind == tilde
            or kind == amp
            or kind == star
            or kind == plus_plus
            or kind == minus_minus;
    }

    [[nodiscard]] auto parse_translation_unit_node() -> std::unique_ptr<translation_unit_syntax>
    {
        auto frame = trace_scope{ *this, "translation_unit" };
        auto unit = std::make_unique<translation_unit_syntax>();
        auto const start = peek().source_span;

        if(at(token_kind::kw_export) and peek(1).kind == token_kind::kw_module) {
            unit->module_header = parse_module_header();
        }

        while(at(token_kind::kw_import)) {
            auto item = parse_import_declaration();
            if(item.has_value()) {
                unit->imports.push_back(std::move(*item));
            } else {
                synchronize_top_level();
            }
        }

        while(not at(token_kind::eof)) {
            auto const before_index = index_;
            if(at(token_kind::kw_export) and is_unsupported_construct_token(peek(1).kind)) {
                (void)consume();
                report_unsupported_construct();
                skip_unsupported_top_level_construct();
                continue;
            }

            if(is_unsupported_construct_token(peek().kind)) {
                report_unsupported_construct();
                skip_unsupported_top_level_construct();
                continue;
            }

            auto function = parse_function();
            if(function.has_value()) {
                unit->functions.push_back(std::move(*function));
            } else {
                report_current(
                    parser_diagnostic_code::unexpected_token,
                    std::format("unexpected top-level token {}", to_string(peek().kind)));

                if(index_ == before_index and injected_.empty()) {
                    (void)consume();
                }
                synchronize_top_level();
            }
        }

        unit->full_span = combine_spans(start, peek().source_span);
        frame.succeed();
        return unit;
    }

    [[nodiscard]] auto parse_module_header() -> std::unique_ptr<module_header_syntax>
    {
        auto frame = trace_scope{ *this, "module_header" };
        auto const export_kw = expect(token_kind::kw_export, "'export'");
        auto const module_kw = expect(token_kind::kw_module, "'module'");
        auto name = parse_qualified_name();
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not export_kw.has_value() or not module_kw.has_value() or not name.has_value() or not semicolon.has_value()) {
            return nullptr;
        }

        auto result = std::make_unique<module_header_syntax>();
        result->exported = true;
        result->name = std::move(*name);
        result->full_span = combine_spans(export_kw->source_span, semicolon->source_span);
        frame.succeed();
        return result;
    }

    [[nodiscard]] auto parse_import_declaration() -> std::optional<import_syntax>
    {
        auto frame = trace_scope{ *this, "import" };
        auto const import_kw = expect(token_kind::kw_import, "'import'");
        auto name = parse_qualified_name();
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not import_kw.has_value() or not name.has_value() or not semicolon.has_value()) {
            return std::nullopt;
        }

        frame.succeed();
        return import_syntax{
            .full_span = combine_spans(import_kw->source_span, semicolon->source_span),
            .name = std::move(*name),
        };
    }

    [[nodiscard]] auto parse_function() -> std::optional<function_syntax>
    {
        auto frame = trace_scope{ *this, "function" };
        auto exported = false;
        token start{};

        if(at(token_kind::kw_export)) {
            if(peek(1).kind == token_kind::kw_module) {
                return std::nullopt;
            }
            exported = true;
            start = consume();
        } else if(at(token_kind::identifier)) {
            start = peek();
        } else {
            return std::nullopt;
        }

        auto const name = expect_identifier("function name");
        auto const l_paren = expect(token_kind::l_paren, "'('");
        if(not name.has_value() or not l_paren.has_value()) {
            return std::nullopt;
        }

        auto parameters = std::vector<parameter_syntax>{};
        if(not at(token_kind::r_paren)) {
            auto parameter = parse_parameter();
            if(not parameter.has_value()) {
                return std::nullopt;
            }
            parameters.push_back(std::move(*parameter));

            while(at(token_kind::comma)) {
                (void)consume();
                auto next = parse_parameter();
                if(not next.has_value()) {
                    return std::nullopt;
                }
                parameters.push_back(std::move(*next));
            }
        }

        auto const r_paren = expect(token_kind::r_paren, "')'");
        if(not r_paren.has_value()) {
            return std::nullopt;
        }

        std::unique_ptr<type_syntax> return_type{};
        if(at(token_kind::arrow)) {
            (void)consume();
            return_type = parse_type();
            if(return_type == nullptr) {
                return std::nullopt;
            }
        }

        auto body = parse_block_statement();
        if(body == nullptr) {
            return std::nullopt;
        }

        auto function = function_syntax{
            .exported = exported,
            .name = name->source_span,
            .parameters = std::move(parameters),
            .return_type = std::move(return_type),
            .body = std::move(body),
        };
        function.full_span = combine_spans(exported ? start.source_span : name->source_span, function.body->full_span);
        frame.succeed();
        return function;
    }

    [[nodiscard]] auto parse_parameter() -> std::optional<parameter_syntax>
    {
        auto frame = trace_scope{ *this, "parameter" };
        auto is_const = false;
        token start = peek();
        if(at(token_kind::kw_const)) {
            is_const = true;
            start = consume();
        }

        auto const name = expect_identifier("parameter name");
        auto const colon = expect(token_kind::colon, "':'");
        auto type = parse_type();
        if(not name.has_value() or not colon.has_value() or type == nullptr) {
            return std::nullopt;
        }

        frame.succeed();
        return parameter_syntax{
            .full_span = combine_spans(is_const ? start.source_span : name->source_span, type->full_span),
            .is_const = is_const,
            .name = name->source_span,
            .type = std::move(type),
        };
    }

    [[nodiscard]] auto parse_statement() -> std::unique_ptr<statement_syntax>
    {
        switch(peek().kind) {
        case token_kind::l_brace: return parse_block_statement();
        case token_kind::kw_let:
        case token_kind::kw_const: return parse_declaration_statement();
        case token_kind::kw_if: return parse_if_statement();
        case token_kind::kw_while: return parse_while_statement();
        case token_kind::kw_do: return parse_do_while_statement();
        case token_kind::kw_for: return parse_for_statement();
        case token_kind::kw_break: return parse_break_continue_statement(statement_syntax_kind::break_stmt);
        case token_kind::kw_continue: return parse_break_continue_statement(statement_syntax_kind::continue_stmt);
        case token_kind::kw_return: return parse_return_statement();
        case token_kind::kw_struct:
        case token_kind::kw_impl:
        case token_kind::kw_trait:
            report_unsupported_construct();
            return nullptr;
        default:
            if(peek().kind == token_kind::l_brace) {
                return nullptr;
            }
            if(not starts_expression(peek().kind) or peek().kind == token_kind::l_brace) {
                report_current(
                    parser_diagnostic_code::expected_statement,
                    std::format("expected statement, got {}", to_string(peek().kind)));
                return nullptr;
            }
            return parse_expression_statement();
        }
    }

    [[nodiscard]] auto parse_block_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "block" };
        auto const open = expect(token_kind::l_brace, "'{'");
        if(not open.has_value()) {
            return nullptr;
        }

        auto block = std::make_unique<statement_syntax>();
        block->kind = statement_syntax_kind::block;
        while(not at_any({ token_kind::r_brace, token_kind::eof })) {
            auto const before = index_;
            auto statement = parse_statement();
            if(statement != nullptr) {
                block->statements.push_back(std::move(statement));
                continue;
            }

            if(index_ == before and injected_.empty() and not at_any({ token_kind::r_brace, token_kind::eof })) {
                (void)consume();
            }
            synchronize_statement();
        }

        auto const close = expect(token_kind::r_brace, "'}'");
        if(not close.has_value()) {
            return nullptr;
        }

        block->full_span = combine_spans(open->source_span, close->source_span);
        frame.succeed();
        return block;
    }

    [[nodiscard]] auto parse_declaration_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "declaration_stmt" };
        auto const start = consume();
        auto const name = expect_identifier("declaration name");
        std::unique_ptr<type_syntax> type{};
        if(at(token_kind::colon)) {
            (void)consume();
            type = parse_type();
            if(type == nullptr) {
                return nullptr;
            }
        }
        auto const equal = expect(token_kind::equal, "'='");
        auto initializer = parse_expression();
        if(initializer == nullptr) {
            return nullptr;
        }
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not name.has_value() or not equal.has_value() or not semicolon.has_value()) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::declaration;
        statement->is_const = start.kind == token_kind::kw_const;
        statement->name = name->source_span;
        statement->declared_type = std::move(type);
        statement->expressions.push_back(std::move(initializer));
        statement->full_span = combine_spans(start.source_span, semicolon->source_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_if_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "if_stmt" };
        auto const start = expect(token_kind::kw_if, "'if'");
        auto const l_paren = expect(token_kind::l_paren, "'('");
        auto condition = parse_expression();
        auto const r_paren = expect(token_kind::r_paren, "')'");
        auto then_block = parse_block_statement();
        if(not start.has_value() or not l_paren.has_value() or condition == nullptr or not r_paren.has_value()
           or then_block == nullptr) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::if_stmt;
        statement->expressions.push_back(std::move(condition));
        statement->statements.push_back(std::move(then_block));

        span end = statement->statements.back()->full_span;
        if(at(token_kind::kw_else)) {
            (void)consume();
            if(at(token_kind::kw_if)) {
                auto else_if = parse_if_statement();
                if(else_if == nullptr) {
                    return nullptr;
                }
                end = else_if->full_span;
                statement->statements.push_back(std::move(else_if));
            } else {
                auto else_block = parse_block_statement();
                if(else_block == nullptr) {
                    return nullptr;
                }
                end = else_block->full_span;
                statement->statements.push_back(std::move(else_block));
            }
        }

        statement->full_span = combine_spans(start->source_span, end);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_while_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "while_stmt" };
        auto const start = expect(token_kind::kw_while, "'while'");
        auto const l_paren = expect(token_kind::l_paren, "'('");
        auto condition = parse_expression();
        auto const r_paren = expect(token_kind::r_paren, "')'");
        auto body = parse_block_statement();
        if(not start.has_value() or not l_paren.has_value() or condition == nullptr or not r_paren.has_value()
           or body == nullptr) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::while_stmt;
        statement->expressions.push_back(std::move(condition));
        statement->statements.push_back(std::move(body));
        statement->full_span = combine_spans(start->source_span, statement->statements.front()->full_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_do_while_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "do_while_stmt" };
        auto const start = expect(token_kind::kw_do, "'do'");
        auto body = parse_block_statement();
        auto const while_kw = expect(token_kind::kw_while, "'while'");
        auto const l_paren = expect(token_kind::l_paren, "'('");
        auto condition = parse_expression();
        auto const r_paren = expect(token_kind::r_paren, "')'");
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not start.has_value() or body == nullptr or not while_kw.has_value() or not l_paren.has_value()
           or condition == nullptr or not r_paren.has_value() or not semicolon.has_value()) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::do_while_stmt;
        statement->expressions.push_back(std::move(condition));
        statement->statements.push_back(std::move(body));
        statement->full_span = combine_spans(start->source_span, semicolon->source_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_for_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "for_stmt" };
        auto const start = expect(token_kind::kw_for, "'for'");
        if(not start.has_value()) {
            return nullptr;
        }

        std::optional<span> label{};
        if(at(token_kind::colon)) {
            (void)consume();
            auto const label_token = expect_identifier("loop label");
            if(not label_token.has_value()) {
                return nullptr;
            }
            label = label_token->source_span;
        }

        auto const l_paren = expect(token_kind::l_paren, "'('");
        if(not l_paren.has_value()) {
            return nullptr;
        }

        if(not at_any({ token_kind::kw_let, token_kind::kw_const })) {
            report_current(
                parser_diagnostic_code::expected_token,
                "expected 'let' or 'const' in for binding");
            return nullptr;
        }
        auto const binding_kw = consume();
        auto const binding_name = expect_identifier("for binding name");
        auto const colon = expect(token_kind::colon, "':'");
        auto range = parse_expression();
        auto const r_paren = expect(token_kind::r_paren, "')'");
        auto body = parse_block_statement();
        if(not binding_name.has_value() or not colon.has_value() or range == nullptr or not r_paren.has_value()
           or body == nullptr) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::for_stmt;
        statement->is_const = binding_kw.kind == token_kind::kw_const;
        statement->name = binding_name->source_span;
        statement->label = label;
        statement->expressions.push_back(std::move(range));
        statement->statements.push_back(std::move(body));
        statement->full_span = combine_spans(start->source_span, statement->statements.front()->full_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_break_continue_statement(statement_syntax_kind kind)
        -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, kind == statement_syntax_kind::break_stmt ? "break_stmt" : "continue_stmt" };
        auto const start = consume();
        std::optional<span> label{};
        if(at(token_kind::identifier)) {
            label = consume().source_span;
        }
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not semicolon.has_value()) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = kind;
        statement->label = label;
        statement->full_span = combine_spans(start.source_span, semicolon->source_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_return_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "return_stmt" };
        auto const start = expect(token_kind::kw_return, "'return'");
        if(not start.has_value()) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::return_stmt;
        if(not at(token_kind::semicolon)) {
            auto value = parse_expression();
            if(value == nullptr) {
                return nullptr;
            }
            statement->expressions.push_back(std::move(value));
        }

        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not semicolon.has_value()) {
            return nullptr;
        }

        statement->full_span = combine_spans(start->source_span, semicolon->source_span);
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_expression_statement() -> std::unique_ptr<statement_syntax>
    {
        auto frame = trace_scope{ *this, "expr_stmt" };
        auto expression = parse_expression();
        if(expression == nullptr) {
            return nullptr;
        }
        auto const semicolon = expect(token_kind::semicolon, "';'");
        if(not semicolon.has_value()) {
            return nullptr;
        }

        auto statement = std::make_unique<statement_syntax>();
        statement->kind = statement_syntax_kind::expr_stmt;
        statement->full_span = combine_spans(expression->full_span, semicolon->source_span);
        statement->expressions.push_back(std::move(expression));
        frame.succeed();
        return statement;
    }

    [[nodiscard]] auto parse_type() -> std::unique_ptr<type_syntax>
    {
        auto frame = trace_scope{ *this, "type" };
        if(not at(token_kind::identifier)) {
            report_current(
                parser_diagnostic_code::expected_type,
                std::format("expected type, got {}", to_string(peek().kind)));
            return nullptr;
        }

        auto name = parse_qualified_name();
        if(not name.has_value()) {
            return nullptr;
        }

        auto type = std::make_unique<type_syntax>();
        type->name = std::move(*name);
        type->full_span = type->name.full_span;

        if(at(token_kind::less)) {
            (void)consume();
            if(at(token_kind::greater)) {
                report_current(parser_diagnostic_code::expected_type, "empty type argument list is not allowed");
                return nullptr;
            }

            auto argument = parse_type_argument();
            if(not argument.has_value()) {
                return nullptr;
            }

            append_type_argument(*type, std::move(*argument));

            while(at(token_kind::comma)) {
                (void)consume();
                auto next = parse_type_argument();
                if(not next.has_value()) {
                    return nullptr;
                }
                append_type_argument(*type, std::move(*next));
            }

            auto const close = expect_closing_angle();
            if(not close.has_value()) {
                return nullptr;
            }
            type->full_span = combine_spans(type->full_span, close->source_span);
        }

        if(at(token_kind::kw_const)) {
            type->const_qualified = true;
            auto const qualifier = consume();
            type->full_span = combine_spans(type->full_span, qualifier.source_span);
        }

        while(at_any({ token_kind::amp, token_kind::star })) {
            auto const suffix = consume();
            type->suffix_operators.push_back(suffix.kind);
            type->full_span = combine_spans(type->full_span, suffix.source_span);
        }

        frame.succeed();
        return type;
    }

    struct parsed_type_argument
    {
        std::unique_ptr<type_syntax> type{};
        std::optional<span> literal{};
    };

    auto append_type_argument(type_syntax& target, parsed_type_argument value) -> void
    {
        if(value.type != nullptr) {
            target.type_arguments.push_back(std::move(value.type));
        } else if(value.literal.has_value()) {
            target.literal_arguments.push_back(*value.literal);
        }
    }

    [[nodiscard]] auto parse_type_argument() -> std::optional<parsed_type_argument>
    {
        if(at(token_kind::integer_literal)) {
            return parsed_type_argument{
                .literal = consume().source_span,
            };
        }

        if(at_any({ token_kind::greater, token_kind::greater_greater, token_kind::greater_greater_equal })) {
            report_current(
                parser_diagnostic_code::expected_type,
                "expected type argument before closing angle bracket");
            return std::nullopt;
        }

        auto nested = parse_type();
        if(nested == nullptr) {
            return std::nullopt;
        }

        return parsed_type_argument{
            .type = std::move(nested),
        };
    }

    [[nodiscard]] auto parse_qualified_name() -> std::optional<qualified_name_syntax>
    {
        auto frame = trace_scope{ *this, "qualified_name" };
        auto result = qualified_name_syntax{};
        auto const first = expect_identifier("identifier");
        if(not first.has_value()) {
            return std::nullopt;
        }

        result.components.push_back(first->source_span);
        result.full_span = first->source_span;

        while(at(token_kind::colon_colon)) {
            (void)consume();
            auto const next = expect_identifier("qualified name component");
            if(not next.has_value()) {
                return std::nullopt;
            }
            result.components.push_back(next->source_span);
            result.full_span = combine_spans(result.full_span, next->source_span);
        }

        frame.succeed();
        return result;
    }

    [[nodiscard]] auto parse_expression() -> std::unique_ptr<expr_syntax>
    {
        auto frame = trace_scope{ *this, "expression" };
        auto value = parse_assignment();
        if(value != nullptr) {
            frame.succeed();
        }
        return value;
    }

    [[nodiscard]] auto parse_assignment() -> std::unique_ptr<expr_syntax>
    {
        auto frame = trace_scope{ *this, "assignment" };
        auto left = parse_expression_pratt(0);
        if(left == nullptr) {
            return nullptr;
        }

        if(not is_assignment_operator(peek().kind)) {
            frame.succeed();
            return left;
        }

        auto const operation = consume();
        auto right = parse_assignment();
        if(right == nullptr) {
            return nullptr;
        }

        auto result = std::make_unique<expr_syntax>();
        result->kind = expr_syntax_kind::assignment;
        result->operator_kind = operation.kind;
        result->full_span = combine_spans(left->full_span, right->full_span);
        result->operands.push_back(std::move(left));
        result->operands.push_back(std::move(right));
        frame.succeed();
        return result;
    }

    [[nodiscard]] auto is_assignment_operator(token_kind kind) const -> bool
    {
        using enum token_kind;
        return kind == equal
            or kind == plus_equal
            or kind == minus_equal
            or kind == star_equal
            or kind == slash_equal
            or kind == percent_equal
            or kind == amp_equal
            or kind == pipe_equal
            or kind == caret_equal
            or kind == less_less_equal
            or kind == greater_greater_equal;
    }

    // Pratt-style binary expression parser. Prefix / call / postfix / primary
    // continue to flow through recursive descent (parse_unary → parse_postfix
    // → parse_primary). The shared cp_binary_operator_table from
    // parser.expression_precedence is the single source of truth for binary
    // operator precedence and is also consumed by the operator-precedence
    // experiment module.
    [[nodiscard]] auto parse_expression_pratt(int min_bp) -> std::unique_ptr<expr_syntax>
    {
        auto left = parse_unary();
        if(left == nullptr) {
            return nullptr;
        }

        while(true) {
            auto const op_kind = peek().kind;
            auto const entry = find_binary_operator(op_kind);
            if(not entry.has_value() or entry->left_bp < min_bp) {
                break;
            }

            if(op_kind == token_kind::kw_as) {
                auto const cast_kw = consume();
                record_trace(trace_event_kind::enter, "cast", cast_kw);
                auto type = parse_type();
                if(type == nullptr) {
                    return nullptr;
                }

                auto cast = std::make_unique<expr_syntax>();
                cast->kind = expr_syntax_kind::cast;
                cast->operator_kind = cast_kw.kind;
                cast->full_span = combine_spans(left->full_span, type->full_span);
                cast->operands.push_back(std::move(left));
                cast->type_operand = std::move(type);
                left = std::move(cast);
                record_trace(trace_event_kind::exit, "cast", peek());
                continue;
            }

            auto const operation = consume();
            record_trace(trace_event_kind::enter, std::format("binary({})", to_string(op_kind)), operation);
            auto right = parse_expression_pratt(entry->right_bp);
            if(right == nullptr) {
                return nullptr;
            }

            auto combined = std::make_unique<expr_syntax>();
            combined->kind = expr_syntax_kind::binary;
            combined->operator_kind = operation.kind;
            combined->full_span = combine_spans(left->full_span, right->full_span);
            combined->operands.push_back(std::move(left));
            combined->operands.push_back(std::move(right));
            left = std::move(combined);
            record_trace(trace_event_kind::exit, std::format("binary({})", to_string(op_kind)), peek());
        }

        return left;
    }

    [[nodiscard]] auto parse_unary() -> std::unique_ptr<expr_syntax>
    {
        auto frame = trace_scope{ *this, "unary" };
        if(at_any({
            token_kind::plus,
            token_kind::minus,
            token_kind::kw_not,
            token_kind::tilde,
            token_kind::amp,
            token_kind::star,
            token_kind::plus_plus,
            token_kind::minus_minus,
        })) {
            auto const operation = consume();
            auto operand = parse_unary();
            if(operand == nullptr) {
                return nullptr;
            }

            auto expression = std::make_unique<expr_syntax>();
            expression->kind = expr_syntax_kind::unary;
            expression->operator_kind = operation.kind;
            expression->full_span = combine_spans(operation.source_span, operand->full_span);
            expression->operands.push_back(std::move(operand));
            frame.succeed();
            return expression;
        }

        auto expression = parse_postfix();
        if(expression != nullptr) {
            frame.succeed();
        }
        return expression;
    }

    [[nodiscard]] auto parse_postfix() -> std::unique_ptr<expr_syntax>
    {
        auto frame = trace_scope{ *this, "postfix" };
        auto operand = parse_primary();
        if(operand == nullptr) {
            return nullptr;
        }

        while(true) {
            if(at(token_kind::l_paren)) {
                auto const open = consume();
                auto call = std::make_unique<expr_syntax>();
                call->kind = expr_syntax_kind::call;
                call->operands.push_back(std::move(operand));

                if(not at(token_kind::r_paren)) {
                    auto argument = parse_expression();
                    if(argument == nullptr) {
                        return nullptr;
                    }
                    call->operands.push_back(std::move(argument));

                    while(at(token_kind::comma)) {
                        (void)consume();
                        auto next = parse_expression();
                        if(next == nullptr) {
                            return nullptr;
                        }
                        call->operands.push_back(std::move(next));
                    }
                }

                auto const close = expect(token_kind::r_paren, "')'");
                if(not close.has_value()) {
                    return nullptr;
                }

                call->full_span = combine_spans(call->operands.front()->full_span, close->source_span);
                operand = std::move(call);
                continue;
            }

            if(at_any({ token_kind::plus_plus, token_kind::minus_minus })) {
                auto const operation = consume();
                auto unary = std::make_unique<expr_syntax>();
                unary->kind = expr_syntax_kind::unary;
                unary->operator_kind = operation.kind;
                unary->full_span = combine_spans(operand->full_span, operation.source_span);
                unary->operands.push_back(std::move(operand));
                operand = std::move(unary);
                continue;
            }

            break;
        }

        frame.succeed();
        return operand;
    }

    [[nodiscard]] auto parse_primary() -> std::unique_ptr<expr_syntax>
    {
        auto frame = trace_scope{ *this, "primary" };
        std::unique_ptr<expr_syntax> expression{};

        switch(peek().kind) {
        case token_kind::identifier: {
            auto name = parse_qualified_name();
            if(not name.has_value()) {
                return nullptr;
            }
            expression = std::make_unique<expr_syntax>();
            expression->kind = expr_syntax_kind::name;
            expression->name = std::move(*name);
            expression->full_span = expression->name.full_span;
            break;
        }
        case token_kind::integer_literal:
        case token_kind::float_literal:
        case token_kind::char_literal:
        case token_kind::string_literal:
        case token_kind::kw_true:
        case token_kind::kw_false: {
            auto const literal = consume();
            expression = std::make_unique<expr_syntax>();
            expression->kind = expr_syntax_kind::literal;
            expression->full_span = literal.source_span;
            break;
        }
        case token_kind::l_bracket:
            expression = parse_bracket_literal(expr_syntax_kind::array_literal, token_kind::l_bracket, token_kind::r_bracket);
            break;
        case token_kind::l_brace:
            expression = parse_bracket_literal(expr_syntax_kind::sequence_literal, token_kind::l_brace, token_kind::r_brace);
            break;
        case token_kind::l_paren:
            expression = parse_paren_expression();
            break;
        default:
            report_current(
                parser_diagnostic_code::expected_expression,
                std::format("expected expression, got {}", to_string(peek().kind)));
            return nullptr;
        }

        frame.succeed();
        return expression;
    }

    [[nodiscard]] auto parse_bracket_literal(
        expr_syntax_kind kind,
        token_kind open_kind,
        token_kind close_kind) -> std::unique_ptr<expr_syntax>
    {
        auto const open = expect(open_kind, open_kind == token_kind::l_bracket ? "'['" : "'{'");
        if(not open.has_value()) {
            return nullptr;
        }

        auto expression = std::make_unique<expr_syntax>();
        expression->kind = kind;
        if(not at(close_kind)) {
            auto element = parse_expression();
            if(element == nullptr) {
                return nullptr;
            }
            expression->operands.push_back(std::move(element));

            while(at(token_kind::comma)) {
                (void)consume();
                auto next = parse_expression();
                if(next == nullptr) {
                    return nullptr;
                }
                expression->operands.push_back(std::move(next));
            }
        }

        auto const close = expect(close_kind, close_kind == token_kind::r_bracket ? "']'" : "'}'");
        if(not close.has_value()) {
            return nullptr;
        }

        expression->full_span = combine_spans(open->source_span, close->source_span);
        return expression;
    }

    [[nodiscard]] auto parse_paren_expression() -> std::unique_ptr<expr_syntax>
    {
        auto const open = expect(token_kind::l_paren, "'('");
        if(not open.has_value()) {
            return nullptr;
        }

        auto first = parse_expression();
        if(first == nullptr) {
            return nullptr;
        }

        auto expression = std::make_unique<expr_syntax>();
        if(at(token_kind::comma)) {
            expression->kind = expr_syntax_kind::tuple_literal;
            expression->operands.push_back(std::move(first));

            while(at(token_kind::comma)) {
                (void)consume();
                auto next = parse_expression();
                if(next == nullptr) {
                    return nullptr;
                }
                expression->operands.push_back(std::move(next));
            }
        } else {
            expression->kind = expr_syntax_kind::grouped;
            expression->operands.push_back(std::move(first));
        }

        auto const close = expect(token_kind::r_paren, "')'");
        if(not close.has_value()) {
            return nullptr;
        }

        expression->full_span = combine_spans(open->source_span, close->source_span);
        return expression;
    }

    source_manager const& sources_;
    std::vector<token> tokens_;
    parse_options options_{};
    std::size_t index_{};
    std::deque<token> injected_{};
    std::optional<token> consumed_{};
    std::vector<parser_diagnostic> diagnostics_{};
    std::vector<trace_event> trace_{};
};
} // namespace

auto parse_translation_unit(source_manager const& sources, file_id file, parse_options options) -> parse_result
{
    auto lexer_sink = vector_diagnostic_sink{};
    auto lex = lexer{ sources, file, lexer_sink };
    auto tokens = lex.tokenize_all();

    auto result = parse_result{
        .accepted = false,
        .lexer_diagnostics = lexer_sink.diagnostics(),
    };

    auto const has_invalid_token = std::ranges::any_of(tokens, [](token const& value) {
        return value.kind == token_kind::invalid;
    });

    if(not result.lexer_diagnostics.empty() or has_invalid_token) {
        auto const lexical_failure_span = [&]() -> span {
            if(not result.lexer_diagnostics.empty()) {
                return result.lexer_diagnostics.front().primary_span;
            }

            auto const invalid = std::ranges::find_if(tokens, [](token const& value) {
                return value.kind == token_kind::invalid;
            });
            if(invalid != tokens.end()) {
                return invalid->source_span;
            }

            return tokens.empty() ? span{} : tokens.front().source_span;
        }();

        result.diagnostics.push_back(parser_diagnostic{
            .code = parser_diagnostic_code::lexical_failure,
            .message = "lexical analysis reported errors; parser did not continue",
            .primary_span = lexical_failure_span,
        });
        return result;
    }

    auto parser = recursive_descent_parser{ sources, std::move(tokens), options };
    auto parsed = parser.parse();
    parsed.lexer_diagnostics = std::move(result.lexer_diagnostics);
    return parsed;
}
