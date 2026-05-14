module parser.syntax.recursive_descent:program;

import std;
import lexer;
import parser.ast;
import parser.diagnostic;
import :context;
import :type;
import :statement;

struct program_parser
{
    explicit program_parser(parser_context& context) :
        context(context) {}

    auto parse_translation_unit_node() -> std::optional<translation_unit_syntax>
    {
        auto unit = translation_unit_syntax{};
        auto start = context.peek().span;

        if(context.check(token_kind::kw_export) and context.peek(1).kind == token_kind::kw_module) {
            unit.module_header = parse_module_header();
        }

        while(context.check(token_kind::kw_import)) {
            auto item = parse_import_declaration();
            if(not item) {
                context.synchronize_import_list();
                continue;
            }

            unit.imports.emplace_back(std::move(*item));
        }

        while(not context.check(token_kind::eof)) {
            if (
                context.check(token_kind::kw_export)
                and context.peek(1).kind != token_kind::identifier
            ) {
                context.consume();
                context.report_current(
                    parser_diagnostic_code::expected_token,
                    std::format(
                        "expected function name after export, got {}",
                        context.token_name(context.peek().kind)
                    )
                );
                context.synchronize_function_list();
                continue;
            }

            if(not starts_function_definition()) {
                context.report_current(
                    parser_diagnostic_code::unexpected_token,
                    std::format(
                        "unexpected top-level token {}",
                        context.token_name(context.peek().kind)
                    )
                );
                context.synchronize_function_list();
                continue;
            }

            auto function = parse_function();
            if(not function) {
                context.synchronize_function_list();
                continue;
            }

            unit.functions.emplace_back(*function);
        }

        unit.full_span = combine_spans(start, context.peek().span);
        return std::move(unit);
    }

    auto parse_module_header() -> std::optional<module_header_syntax>
    {
        auto export_kw = context.expect(token_kind::kw_export);
        auto module_kw = context.expect(token_kind::kw_module);
        auto name = parse_module_name();
        auto semicolon = context.expect(token_kind::semicolon);
        if(not export_kw or not module_kw or not name or not semicolon) {
            return std::nullopt;
        }

        auto result = module_header_syntax {
            .full_span = combine_spans(export_kw->span, semicolon->span),
            .exported = true,
            .name = std::move(*name),
        };
        return result;
    }

    auto parse_import_declaration() -> std::optional<import_syntax>
    {
        auto import_kw = context.expect(token_kind::kw_import);
        auto name = parse_module_name();
        auto semicolon = context.expect(token_kind::semicolon);
        if(not import_kw or not name or not semicolon) {
            return std::nullopt;
        }
        return import_syntax {
            .full_span = combine_spans(import_kw->span, semicolon->span),
            .name = std::move(*name),
        };
    }

    auto parse_module_name() -> std::optional<module_name_syntax>
    {
        auto result = module_name_syntax{};
        auto first = context.expect_identifier("module name");
        if(not first) {
            return std::nullopt;
        }

        result.components.emplace_back(first->span);
        result.full_span = first->span;

        while(context.check(token_kind::dot)) {
            context.consume();
            auto next = context.expect_identifier("module name component");
            if(not next) {
                return std::nullopt;
            }
            result.components.emplace_back(next->span);
            result.full_span = combine_spans(result.full_span, next->span);
        }
        return result;
    }

    auto starts_function_definition() const -> bool
    {
        return (
            context.check(token_kind::identifier)
            or (
                context.check(token_kind::kw_export)
                and context.peek(1).kind == token_kind::identifier
            )
        );
    }

    auto starts_parameter() const -> bool
    {
        return context.check(token_kind::identifier) or context.check(token_kind::kw_const);
    }

    auto expect_parameter_start() -> bool
    {
        if(starts_parameter()) {
            return true;
        }

        context.report_current(
            parser_diagnostic_code::expected_identifier,
            std::format("expected parameter, got {}", context.token_name(context.peek().kind))
        );
        return false;
    }

    auto parse_function() -> std::optional<function_id>
    {
        auto exported = false;
        auto export_span = std::optional<source_span>{};

        if(context.check(token_kind::kw_export)) {
            exported = true;
            export_span = context.consume().span;
        }

        auto name = context.expect_identifier("function name");
        auto l_paren = context.expect(token_kind::l_paren);
        if(not name or not l_paren) {
            return std::nullopt;
        }

        auto parameters = parse_parameter_list();
        if(not parameters) {
            return std::nullopt;
        }

        auto r_paren = context.expect(token_kind::r_paren);
        if(not r_paren) {
            return std::nullopt;
        }

        auto return_type = std::optional<type_id>{};
        if(context.check(token_kind::arrow)) {
            context.consume();
            auto types = type_parser{ context };
            if(not types.expect_start()) {
                return std::nullopt;
            }
            return_type = types.parse_type();
            if(not return_type) {
                return std::nullopt;
            }
        }

        auto body = statement_parser{ context }.parse_block_statement();
        if(not body) {
            return std::nullopt;
        }

        auto full_start = export_span.value_or(name->span);
        auto function = function_syntax {
            .full_span = combine_spans(full_start, context.arena.span(*body)),
            .exported = exported,
            .name = name->span,
            .parameters = std::move(*parameters),
            .return_type = return_type,
            .body = *body,
        };
        return context.arena.add(std::move(function));
    }

    auto parse_parameter_list() -> std::optional<std::vector<parameter_syntax>>
    {
        auto parameters = std::vector<parameter_syntax>{};
        if(context.check(token_kind::r_paren)) {
            return parameters;
        }

        if(not expect_parameter_start()) {
            return std::nullopt;
        }
        auto parameter = parse_parameter();
        if(not parameter) {
            return std::nullopt;
        }
        parameters.emplace_back(std::move(*parameter));

        while(context.check(token_kind::comma)) {
            context.consume();
            if(not expect_parameter_start()) {
                return std::nullopt;
            }
            auto next = parse_parameter();
            if(not next) {
                return std::nullopt;
            }
            parameters.emplace_back(std::move(*next));
        }

        return parameters;
    }

    auto parse_parameter() -> std::optional<parameter_syntax>
    {
        auto is_const = false;
        auto const_span = std::optional<source_span>{};
        if(context.check(token_kind::kw_const)) {
            is_const = true;
            const_span = context.consume().span;
        }

        auto name = context.expect_identifier("parameter name");
        auto colon = context.expect(token_kind::colon);
        if(not name or not colon) {
            return std::nullopt;
        }

        auto types = type_parser{ context };
        if(not types.expect_start()) {
            return std::nullopt;
        }
        auto type = types.parse_type();
        if(not type) {
            return std::nullopt;
        }
        auto full_start = const_span.value_or(name->span);
        return parameter_syntax {
            .full_span = combine_spans(full_start, context.arena.span(*type)),
            .is_const = is_const,
            .name = name->span,
            .type = *type,
        };
    }

    parser_context& context;
};
