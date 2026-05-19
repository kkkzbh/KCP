module parser:program;

import std;
import source;
import lexer;
import parser.ast;
import diagnostic;
import :state;

auto parser::parse_translation_unit_node() -> std::optional<translation_unit_syntax>
{
    auto unit = translation_unit_syntax{};
    auto start = peek().span;

    if(check(token_kind::kw_export) and peek(1).kind == token_kind::kw_module) {
        unit.module_header = parse_module_header();
    }

    while(check(token_kind::kw_import) or (check(token_kind::kw_export) and peek(1uz).kind == token_kind::kw_import)) {
        auto exported = false;
        auto export_span = std::optional<source_span>{};
        if(check(token_kind::kw_export)) {
            exported = true;
            export_span = consume().span;
        }
        auto item = parse_import_declaration(exported, export_span);
        if(not item) {
            synchronize_import_list();
            continue;
        }

        unit.imports.emplace_back(std::move(*item));
    }

    while(not check(token_kind::eof)) {
        auto exported = false;
        auto export_span = std::optional<source_span>{};
        if(check(token_kind::kw_export)) {
            exported = true;
            export_span = consume().span;
        }

        if(check(token_kind::kw_struct)) {
            auto declaration = parse_struct_declaration(exported, export_span);
            if(not declaration) {
                synchronize_function_list();
                continue;
            }
            unit.structs.emplace_back(*declaration);
            continue;
        }

        if(check_contextual("variant")) {
            auto declaration = parse_variant_declaration(exported, export_span);
            if(not declaration) {
                synchronize_function_list();
                continue;
            }
            unit.variants.emplace_back(*declaration);
            continue;
        }

        if(check(token_kind::kw_concept)) {
            auto declaration = parse_concept_declaration(exported, export_span);
            if(not declaration) {
                synchronize_function_list();
                continue;
            }
            unit.concepts.emplace_back(*declaration);
            continue;
        }

        if(exported and check(token_kind::kw_impl)) {
            report_current(
                diagnostic_kind::unexpected_token,
                "impl blocks cannot be exported"
            );
            synchronize_function_list();
            continue;
        }

        if(not exported and check(token_kind::kw_impl)) {
            if(peek(2).kind == token_kind::kw_for) {
                auto impl = parse_concept_impl_block();
                if(not impl) {
                    synchronize_function_list();
                    continue;
                }
                unit.concept_impls.emplace_back(*impl);
                continue;
            }

            auto impl = parse_impl_block();
            if(not impl) {
                synchronize_function_list();
                continue;
            }
            unit.impls.emplace_back(*impl);
            continue;
        }

        if(check(token_kind::kw_operator)) {
            auto function = parse_operator_function(function_syntax_kind::free_function, export_span);
            if(not function) {
                synchronize_function_list();
                continue;
            }
            unit.functions.emplace_back(*function);
            continue;
        }

        if (
            exported
            and peek().kind != token_kind::identifier
        ) {
            report_current(
                diagnostic_kind::expected_token,
                std::format(
                    "expected function name after export, got {}",
                    token_name(peek().kind)
                )
            );
            synchronize_function_list();
            continue;
        }

        if(looks_like_extern_function()) {
            auto function = parse_extern_function();
            if(not function) {
                synchronize_function_list();
                continue;
            }

            if(exported) {
                arena.node(*function).exported = true;
                arena.node(*function).full_span = combine_spans(*export_span, arena.node(*function).full_span);
            }
            unit.functions.emplace_back(*function);
            continue;
        }

        if(not starts_function_definition()) {
            report_current(
                diagnostic_kind::unexpected_token,
                std::format(
                    "unexpected top-level token {}",
                    token_name(peek().kind)
                )
            );
            synchronize_function_list();
            continue;
        }

        auto function = parse_function();
        if(not function) {
            synchronize_function_list();
            continue;
        }

        if(exported) {
            arena.node(*function).exported = true;
            arena.node(*function).full_span = combine_spans(*export_span, arena.node(*function).full_span);
        }
        unit.functions.emplace_back(*function);
    }

    unit.functions.insert(unit.functions.end(), synthetic_functions.begin(), synthetic_functions.end());
    unit.full_span = combine_spans(start, peek().span);
    return std::move(unit);
}

auto parser::parse_module_header() -> std::optional<module_header_syntax>
{
    auto export_kw = expect(token_kind::kw_export);
    auto module_kw = expect(token_kind::kw_module);
    auto name = parse_module_name();
    auto semicolon = expect(token_kind::semicolon);
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

auto parser::parse_import_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<import_syntax>
{
    auto import_kw = expect(token_kind::kw_import);
    auto name = parse_module_name();
    auto semicolon = expect(token_kind::semicolon);
    if(not import_kw or not name or not semicolon) {
        return std::nullopt;
    }
    return import_syntax {
        .full_span = combine_spans(export_span.value_or(import_kw->span), semicolon->span),
        .exported = exported,
        .name = std::move(*name),
    };
}

auto parser::parse_module_name() -> std::optional<module_name_syntax>
{
    auto result = module_name_syntax{};
    auto first = expect_identifier("module name");
    if(not first) {
        return std::nullopt;
    }

    result.components.emplace_back(first->span);
    result.full_span = first->span;

    while(check(token_kind::dot)) {
        consume();
        auto next = expect_identifier("module name component");
        if(not next) {
            return std::nullopt;
        }
        result.components.emplace_back(next->span);
        result.full_span = combine_spans(result.full_span, next->span);
    }
    return result;
}

auto parser::starts_function_definition() const -> bool
{
    return check(token_kind::identifier);
}

auto parser::starts_top_level_item() const -> bool
{
    return (
        check(token_kind::identifier)
        or check(token_kind::kw_struct)
        or check_contextual("variant")
        or check(token_kind::kw_impl)
        or check(token_kind::kw_concept)
        or check(token_kind::kw_operator)
        or check(token_kind::kw_export)
    );
}

auto parser::looks_like_extern_function(std::size_t lookahead) const -> bool
{
    return check_contextual("extern", lookahead) and peek(lookahead + 1uz).kind == token_kind::string_literal;
}

auto parser::check_contextual(std::string_view text, std::size_t lookahead) const -> bool
{
    auto token = peek(lookahead);
    return token.kind == token_kind::identifier and token.text == text;
}

auto parser::starts_parameter() const -> bool
{
    return check(token_kind::identifier) or check(token_kind::kw_const);
}

auto parser::expect_parameter_start() -> bool
{
    if(starts_parameter()) {
        return true;
    }

    report_current(
        diagnostic_kind::expected_identifier,
        std::format("expected parameter, got {}", token_name(peek().kind))
    );
    return false;
}

auto parser::parse_extern_function() -> std::optional<function_id>
{
    auto extern_token = consume();
    auto abi = expect(token_kind::string_literal);
    if(not abi) {
        return std::nullopt;
    }

    return parse_function(function_syntax_kind::free_function, extern_token.span, abi->span);
}

auto parser::parse_function(function_syntax_kind kind, std::optional<source_span> extern_span, std::optional<source_span> extern_abi) -> std::optional<function_id>
{
    auto name = expect_identifier("function name");
    auto generic_parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::less)) {
        auto parsed = parse_generic_parameter_list();
        if(not parsed) {
            return std::nullopt;
        }
        generic_parameters = std::move(*parsed);
    }

    auto l_paren = expect(token_kind::l_paren);
    if(not name or not l_paren) {
        return std::nullopt;
    }

    auto parameters = parse_parameter_list();
    if(not parameters) {
        return std::nullopt;
    }

    auto r_paren = expect(token_kind::r_paren);
    if(not r_paren) {
        return std::nullopt;
    }

    auto return_type = std::optional<type_id>{};
    if(check(token_kind::arrow)) {
        consume();
        if(not expect_type_start()) {
            return std::nullopt;
        }
        return_type = parse_type();
        if(not return_type) {
            return std::nullopt;
        }
    }

    auto requires_clause = std::optional<concept_requires_syntax>{};
    if(check_contextual("requires")) {
        requires_clause = parse_requires_clause_until(token_kind::l_brace);
        if(not requires_clause) {
            return std::nullopt;
        }
    }

    auto body = stmt_id{};
    auto has_body = true;
    auto end_span = source_span{};
    if(extern_abi and check(token_kind::semicolon)) {
        auto semicolon = consume();
        has_body = false;
        end_span = semicolon.span;
    } else {
        auto parsed_body = parse_block_statement();
        if(not parsed_body) {
            return std::nullopt;
        }
        body = *parsed_body;
        end_span = arena.span(body);
    }

    auto function = function_syntax {
        .full_span = combine_spans(extern_span.value_or(name->span), end_span),
        .kind = kind,
        .has_body = has_body,
        .extern_abi = extern_abi,
        .name = name->span,
        .generic_parameters = std::move(generic_parameters),
        .parameters = std::move(*parameters),
        .return_type = return_type,
        .requires_clause = std::move(requires_clause),
        .body = body,
    };
    return arena.add(std::move(function));
}

auto parser::parse_operator_function(function_syntax_kind kind, std::optional<source_span> export_span) -> std::optional<function_id>
{
    auto operator_kw = expect(token_kind::kw_operator);
    auto operator_kind = parse_overload_operator();
    auto l_paren = expect(token_kind::l_paren);
    if(not operator_kw or not operator_kind or not l_paren) {
        return std::nullopt;
    }

    auto parameters = parse_parameter_list();
    if(not parameters) {
        return std::nullopt;
    }

    auto r_paren = expect(token_kind::r_paren);
    if(not r_paren) {
        return std::nullopt;
    }

    auto return_type = std::optional<type_id>{};
    if(check(token_kind::arrow)) {
        consume();
        return_type = parse_expected_type();
        if(not return_type) {
            return std::nullopt;
        }
    }

    auto requires_clause = std::optional<concept_requires_syntax>{};
    if(check_contextual("requires")) {
        requires_clause = parse_requires_clause_until(token_kind::l_brace);
        if(not requires_clause) {
            return std::nullopt;
        }
    }

    auto body = parse_block_statement();
    if(not body) {
        return std::nullopt;
    }

    auto full_span = combine_spans(operator_kw->span, arena.span(*body));
    if(export_span) {
        full_span = combine_spans(*export_span, full_span);
    }
    return arena.add(function_syntax {
        .full_span = full_span,
        .kind = kind,
        .exported = static_cast<bool>(export_span),
        .overload_operator = operator_kind->first,
        .name = operator_kind->second,
        .parameters = std::move(*parameters),
        .return_type = return_type,
        .requires_clause = std::move(requires_clause),
        .body = *body,
    });
}

auto parser::parse_overload_operator() -> std::optional<std::pair<overload_operator_kind, source_span>>
{
    using enum token_kind;
    auto current = peek();
    auto consume_operator = [&](overload_operator_kind kind) {
        return std::pair{ kind, consume().span };
    };

    switch(current.kind) {
        case plus: { return consume_operator(overload_operator_kind::plus); }
        case minus: { return consume_operator(overload_operator_kind::minus); }
        case star: { return consume_operator(overload_operator_kind::star); }
        case slash: { return consume_operator(overload_operator_kind::slash); }
        case percent: { return consume_operator(overload_operator_kind::percent); }
        case amp: { return consume_operator(overload_operator_kind::amp); }
        case pipe: { return consume_operator(overload_operator_kind::pipe); }
        case caret: { return consume_operator(overload_operator_kind::caret); }
        case less_less: { return consume_operator(overload_operator_kind::less_less); }
        case greater_greater: { return consume_operator(overload_operator_kind::greater_greater); }
        case tilde: { return consume_operator(overload_operator_kind::tilde); }
        case equal_equal: { return consume_operator(overload_operator_kind::equal_equal); }
        case bang_equal: { return consume_operator(overload_operator_kind::bang_equal); }
        case less: { return consume_operator(overload_operator_kind::less); }
        case less_equal: { return consume_operator(overload_operator_kind::less_equal); }
        case greater: { return consume_operator(overload_operator_kind::greater); }
        case greater_equal: { return consume_operator(overload_operator_kind::greater_equal); }
        case equal: { return consume_operator(overload_operator_kind::equal); }
        case plus_equal: { return consume_operator(overload_operator_kind::plus_equal); }
        case minus_equal: { return consume_operator(overload_operator_kind::minus_equal); }
        case star_equal: { return consume_operator(overload_operator_kind::star_equal); }
        case slash_equal: { return consume_operator(overload_operator_kind::slash_equal); }
        case percent_equal: { return consume_operator(overload_operator_kind::percent_equal); }
        case amp_equal: { return consume_operator(overload_operator_kind::amp_equal); }
        case pipe_equal: { return consume_operator(overload_operator_kind::pipe_equal); }
        case caret_equal: { return consume_operator(overload_operator_kind::caret_equal); }
        case less_less_equal: { return consume_operator(overload_operator_kind::less_less_equal); }
        case greater_greater_equal: { return consume_operator(overload_operator_kind::greater_greater_equal); }
        case l_bracket: {
            auto open = consume();
            auto close = expect(token_kind::r_bracket);
            if(not close) {
                return std::nullopt;
            }
            return std::pair{ overload_operator_kind::subscript, combine_spans(open.span, close->span) };
        }
        default:
            report_current(
                diagnostic_kind::unexpected_token,
                std::format("expected overloadable operator, got {}", token_name(current.kind))
            );
            return std::nullopt;
    }
}

auto parser::parse_struct_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<struct_id>
{
    auto struct_kw = expect(token_kind::kw_struct);
    auto name = expect_identifier("struct name");
    auto generic_parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::less)) {
        auto parsed = parse_generic_parameter_list();
        if(not parsed) {
            return std::nullopt;
        }
        generic_parameters = std::move(*parsed);
    }

    auto open = expect(token_kind::l_brace);
    if(not struct_kw or not name or not open) {
        return std::nullopt;
    }

    auto fields = std::vector<struct_field_syntax>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        auto field = parse_struct_field();
        if(not field) {
            synchronize_statement();
            continue;
        }
        fields.emplace_back(std::move(*field));
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    auto declaration = struct_syntax {
        .full_span = combine_spans(export_span.value_or(struct_kw->span), close->span),
        .exported = exported,
        .name = name->span,
        .generic_parameters = std::move(generic_parameters),
        .fields = std::move(fields),
    };
    return arena.add(std::move(declaration));
}

auto parser::parse_struct_field() -> std::optional<struct_field_syntax>
{
    auto name = expect_identifier("field name");
    auto colon = expect(token_kind::colon);
    if(not name or not colon) {
        return std::nullopt;
    }
    auto type = parse_expected_type();
    auto semicolon = expect(token_kind::semicolon);
    if(not type or not semicolon) {
        return std::nullopt;
    }
    return struct_field_syntax {
        .full_span = combine_spans(name->span, semicolon->span),
        .name = name->span,
        .type = *type,
    };
}

auto parser::parse_variant_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<variant_id>
{
    auto variant_kw = expect_identifier("variant");
    auto name = expect_identifier("variant name");
    auto generic_parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::less)) {
        auto parsed = parse_generic_parameter_list();
        if(not parsed) {
            return std::nullopt;
        }
        generic_parameters = std::move(*parsed);
    }

    auto open = expect(token_kind::l_brace);
    if(not variant_kw or not name or not open) {
        return std::nullopt;
    }

    auto cases = std::vector<variant_case_syntax>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        auto variant_case = parse_variant_case();
        if(not variant_case) {
            synchronize_statement();
            continue;
        }
        cases.emplace_back(std::move(*variant_case));
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    return arena.add(variant_syntax {
        .full_span = combine_spans(export_span.value_or(variant_kw->span), close->span),
        .exported = exported,
        .name = name->span,
        .generic_parameters = std::move(generic_parameters),
        .cases = std::move(cases),
    });
}

auto parser::parse_variant_case() -> std::optional<variant_case_syntax>
{
    auto name = expect_identifier("variant case name");
    if(not name) {
        return std::nullopt;
    }

    auto payloads = std::vector<type_id>{};
    if(check(token_kind::l_paren)) {
        consume();
        if(check(token_kind::r_paren)) {
            report_current(diagnostic_kind::expected_type, "variant case payload cannot be empty");
            return std::nullopt;
        }

        auto first = parse_expected_type();
        if(not first) {
            return std::nullopt;
        }
        payloads.emplace_back(*first);
        while(check(token_kind::comma)) {
            consume();
            auto payload = parse_expected_type();
            if(not payload) {
                return std::nullopt;
            }
            payloads.emplace_back(*payload);
        }

        auto close = expect(token_kind::r_paren);
        if(not close) {
            return std::nullopt;
        }
    }

    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }

    return variant_case_syntax {
        .full_span = combine_spans(name->span, semicolon->span),
        .name = name->span,
        .payloads = std::move(payloads),
    };
}

auto parser::parse_concept_declaration(bool exported, std::optional<source_span> export_span) -> std::optional<concept_id>
{
    auto concept_kw = expect(token_kind::kw_concept);
    auto name = expect_identifier("concept name");
    auto open = expect(token_kind::l_brace);
    if(not concept_kw or not name or not open) {
        return std::nullopt;
    }

    auto items = std::vector<concept_item_syntax>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        auto item = parse_concept_item();
        if(not item) {
            synchronize_statement();
            continue;
        }
        items.emplace_back(std::move(*item));
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    return arena.add(concept_syntax {
        .full_span = combine_spans(export_span.value_or(concept_kw->span), close->span),
        .exported = exported,
        .name = name->span,
        .items = std::move(items),
    });
}

auto parser::parse_concept_item() -> std::optional<concept_item_syntax>
{
    if(check_contextual("requires")) {
        auto requirement = parse_concept_requires();
        if(not requirement) {
            return std::nullopt;
        }
        return concept_item_syntax { std::move(*requirement) };
    }

    if(check_contextual("type")) {
        auto alias = parse_type_alias();
        if(not alias) {
            return std::nullopt;
        }
        return concept_item_syntax { std::move(*alias) };
    }

    auto function = parse_concept_function_requirement();
    if(not function) {
        return std::nullopt;
    }
    return concept_item_syntax { std::move(*function) };
}

auto parser::parse_concept_requires() -> std::optional<concept_requires_syntax>
{
    auto start = expect_identifier("requires");
    if(not start) {
        return std::nullopt;
    }

    auto constraints = parse_concept_requires_constraints_until(token_kind::semicolon);
    auto semicolon = expect(token_kind::semicolon);
    if(not constraints or not semicolon) {
        return std::nullopt;
    }

    return concept_requires_syntax {
        .full_span = combine_spans(start->span, semicolon->span),
        .constraints = std::move(*constraints),
    };
}

auto parser::parse_requires_clause_until(token_kind end) -> std::optional<concept_requires_syntax>
{
    auto start = expect_identifier("requires");
    if(not start) {
        return std::nullopt;
    }

    auto constraints = parse_concept_requires_constraints_until(end);
    if(not constraints) {
        return std::nullopt;
    }

    return concept_requires_syntax {
        .full_span = combine_spans(start->span, peek().span),
        .constraints = std::move(*constraints),
    };
}

auto parser::parse_concept_requires_constraints_until(token_kind end)
    -> std::optional<std::vector<concept_requires_constraint_syntax>>
{
    auto constraints = std::vector<concept_requires_constraint_syntax>{};
    while(not check_any({ end, token_kind::eof })) {
        if(check(token_kind::l_paren)) {
            consume();
            auto nested = parse_concept_requires_constraints_until(token_kind::r_paren);
            auto close = expect(token_kind::r_paren);
            if(not nested or not close) {
                return std::nullopt;
            }
            constraints.insert(
                constraints.end(),
                std::make_move_iterator(nested->begin()),
                std::make_move_iterator(nested->end())
            );
            if(not check(token_kind::kw_and)) {
                break;
            }
            consume();
            continue;
        }

        auto primary = parse_concept_requires_primary();
        if(not primary) {
            return std::nullopt;
        }
        constraints.emplace_back(std::move(*primary));
        if(not check(token_kind::kw_and)) {
            break;
        }
        consume();
    }
    return constraints;
}

auto parser::parse_concept_requires_primary() -> std::optional<concept_requires_constraint_syntax>
{
    if(not check(token_kind::identifier)) {
        report_current(
            diagnostic_kind::expected_identifier,
            std::format("expected requires constraint, got {}", token_name(peek().kind))
        );
        return std::nullopt;
    }

    if (
        peek(1uz).kind != token_kind::colon
        and peek(1uz).kind != token_kind::colon_colon
        and peek(1uz).kind != token_kind::equal_equal
        and not (
            peek(1uz).kind == token_kind::dot
            and peek(2uz).kind == token_kind::dot
            and peek(3uz).kind == token_kind::dot
        )
    ) {
        auto name = consume();
        return concept_requires_constraint_syntax {
            concept_parent_constraint_syntax {
                .full_span = name.span,
                .name = name.span,
            }
        };
    }

    auto left = parse_type();
    if(not left) {
        return std::nullopt;
    }

    auto is_pack = false;
    if(consume_ellipsis()) {
        is_pack = true;
    }

    if(check(token_kind::colon)) {
        consume();
        auto bounds = std::vector<source_span>{};
        auto bound = expect_identifier("concept bound");
        if(not bound) {
            return std::nullopt;
        }
        bounds.emplace_back(bound->span);
        auto end = bound->span;
        while (
            check(token_kind::kw_and)
            and peek(1uz).kind == token_kind::identifier
            and peek(2uz).kind != token_kind::colon
            and peek(2uz).kind != token_kind::colon_colon
            and peek(2uz).kind != token_kind::equal_equal
            and not (
                peek(2uz).kind == token_kind::dot
                and peek(3uz).kind == token_kind::dot
                and peek(4uz).kind == token_kind::dot
            )
        ) {
            consume();
            auto next = expect_identifier("concept bound");
            if(not next) {
                return std::nullopt;
            }
            bounds.emplace_back(next->span);
            end = next->span;
        }
        return concept_requires_constraint_syntax {
            concept_type_bound_constraint_syntax {
                .full_span = combine_spans(arena.span(*left), end),
                .type = *left,
                .is_pack = is_pack,
                .concept_bounds = std::move(bounds),
            }
        };
    }

    if(is_pack) {
        report_current(diagnostic_kind::expected_token, "type pack constraints must use ':'");
        return std::nullopt;
    }

    auto equal = expect(token_kind::equal_equal);
    auto right = parse_expected_type();
    if(not equal or not right) {
        return std::nullopt;
    }
    return concept_requires_constraint_syntax {
        concept_type_equality_constraint_syntax {
            .full_span = combine_spans(arena.span(*left), arena.span(*right)),
            .left = *left,
            .right = *right,
        }
    };
}

auto parser::parse_type_alias() -> std::optional<type_alias_syntax>
{
    auto type_kw = expect_identifier("type");
    auto name = expect_identifier("type alias name");
    if(not type_kw or not name) {
        return std::nullopt;
    }

    auto value = std::optional<type_id>{};
    if(check(token_kind::equal)) {
        consume();
        value = parse_expected_type();
        if(not value) {
            return std::nullopt;
        }
    }

    auto semicolon = expect(token_kind::semicolon);
    if(not semicolon) {
        return std::nullopt;
    }
    return type_alias_syntax {
        .full_span = combine_spans(type_kw->span, semicolon->span),
        .name = name->span,
        .value = value,
    };
}

auto parser::parse_concept_function_requirement() -> std::optional<concept_function_requirement_syntax>
{
    auto name = expect_identifier("concept function name");
    auto l_paren = expect(token_kind::l_paren);
    if(not name or not l_paren) {
        return std::nullopt;
    }

    auto parameters = parse_parameter_list();
    auto r_paren = expect(token_kind::r_paren);
    if(not parameters or not r_paren) {
        return std::nullopt;
    }

    auto return_type = std::optional<type_id>{};
    if(check(token_kind::arrow)) {
        consume();
        return_type = parse_expected_type();
        if(not return_type) {
            return std::nullopt;
        }
    }

    if(check(token_kind::semicolon)) {
        auto semicolon = consume();
        return concept_function_requirement_syntax {
            .full_span = combine_spans(name->span, semicolon.span),
            .name = name->span,
            .parameters = std::move(*parameters),
            .return_type = return_type,
        };
    }

    auto body = parse_block_statement();
    if(not body) {
        return std::nullopt;
    }

    auto default_function = arena.add(function_syntax {
        .full_span = combine_spans(name->span, arena.span(*body)),
        .kind = function_syntax_kind::impl_function,
        .name = name->span,
        .parameters = *parameters,
        .return_type = return_type,
        .body = *body,
    });
    return concept_function_requirement_syntax {
        .full_span = combine_spans(name->span, arena.span(*body)),
        .name = name->span,
        .parameters = std::move(*parameters),
        .return_type = return_type,
        .default_function = default_function,
    };
}

auto parser::parse_impl_block() -> std::optional<impl_id>
{
    auto impl_kw = expect(token_kind::kw_impl);
    auto type = parse_expected_type();
    auto requires_clause = std::optional<concept_requires_syntax>{};
    if(check_contextual("requires")) {
        requires_clause = parse_requires_clause_until(token_kind::l_brace);
        if(not requires_clause) {
            return std::nullopt;
        }
    }
    auto open = expect(token_kind::l_brace);
    if(not impl_kw or not type or not open) {
        return std::nullopt;
    }

    auto type_aliases = std::vector<type_alias_syntax>{};
    auto functions = std::vector<function_id>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        if(check_contextual("type")) {
            auto alias = parse_type_alias();
            if(not alias) {
                synchronize_function_list();
                continue;
            }
            type_aliases.emplace_back(std::move(*alias));
            continue;
        }

        auto function = parse_impl_item(arena.node(*type));
        if(not function) {
            synchronize_function_list();
            continue;
        }
        functions.emplace_back(*function);
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    return arena.add(impl_syntax {
        .full_span = combine_spans(impl_kw->span, close->span),
        .type = *type,
        .requires_clause = std::move(requires_clause),
        .type_aliases = std::move(type_aliases),
        .functions = std::move(functions),
    });
}

auto parser::parse_concept_impl_block() -> std::optional<concept_impl_id>
{
    auto impl_kw = expect(token_kind::kw_impl);
    auto concept_name = expect_identifier("concept name");
    auto for_kw = expect(token_kind::kw_for);
    auto target_type = parse_expected_type();
    auto requires_clause = std::optional<concept_requires_syntax>{};
    if(check_contextual("requires")) {
        requires_clause = parse_requires_clause_until(token_kind::l_brace);
        if(not requires_clause) {
            return std::nullopt;
        }
    }
    auto open = expect(token_kind::l_brace);
    if(not impl_kw or not concept_name or not for_kw or not target_type or not open) {
        return std::nullopt;
    }

    auto type_aliases = std::vector<type_alias_syntax>{};
    auto functions = std::vector<function_id>{};
    while(not check_any({ token_kind::r_brace, token_kind::eof })) {
        if(check_contextual("type")) {
            auto alias = parse_type_alias();
            if(not alias) {
                synchronize_function_list();
                continue;
            }
            type_aliases.emplace_back(std::move(*alias));
            continue;
        }

        auto function = parse_function(function_syntax_kind::impl_function);
        if(not function) {
            synchronize_function_list();
            continue;
        }
        functions.emplace_back(*function);
    }

    auto close = expect(token_kind::r_brace);
    if(not close) {
        return std::nullopt;
    }

    return arena.add(concept_impl_syntax {
        .full_span = combine_spans(impl_kw->span, close->span),
        .concept_name = concept_name->span,
        .target_type = *target_type,
        .requires_clause = std::move(requires_clause),
        .type_aliases = std::move(type_aliases),
        .functions = std::move(functions),
    });
}

auto parser::parse_impl_item(type_syntax const& impl_type) -> std::optional<function_id>
{
    if(check(token_kind::kw_operator)) {
        static_cast<void>(impl_type);
        return parse_operator_function(function_syntax_kind::impl_function);
    }

    if(check(token_kind::tilde)) {
        auto start = consume();
        auto name = expect_identifier("destructor name");
        auto l_paren = expect(token_kind::l_paren);
        auto r_paren = expect(token_kind::r_paren);
        auto body = parse_block_statement();
        if(not name or not l_paren or not r_paren or not body) {
            return std::nullopt;
        }
        return arena.add(function_syntax {
            .full_span = combine_spans(start.span, arena.span(*body)),
            .kind = function_syntax_kind::destructor,
            .name = name->span,
            .body = *body,
        });
    }

    auto name = expect_identifier("impl item name");
    if(not name) {
        return std::nullopt;
    }

    auto generic_parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::less)) {
        auto parsed = parse_generic_parameter_list();
        if(not parsed) {
            return std::nullopt;
        }
        generic_parameters = std::move(*parsed);
    }

    auto l_paren = expect(token_kind::l_paren);
    if(not l_paren) {
        return std::nullopt;
    }

    auto parameters = parse_parameter_list();
    auto r_paren = expect(token_kind::r_paren);
    if(not parameters or not r_paren) {
        return std::nullopt;
    }

    static_cast<void>(impl_type);
    if(check(token_kind::equal)) {
        return parse_default_constructor(name->span, name->span, std::move(*parameters));
    }

    auto return_type = std::optional<type_id>{};
    if(check(token_kind::arrow)) {
        consume();
        return_type = parse_expected_type();
        if(not return_type) {
            return std::nullopt;
        }
    }

    auto requires_clause = std::optional<concept_requires_syntax>{};
    if(check_contextual("requires")) {
        requires_clause = parse_requires_clause_until(token_kind::l_brace);
        if(not requires_clause) {
            return std::nullopt;
        }
    }

    auto body = parse_block_statement();
    if(not body) {
        return std::nullopt;
    }

    return arena.add(function_syntax {
        .full_span = combine_spans(name->span, arena.span(*body)),
        .kind = function_syntax_kind::impl_function,
        .name = name->span,
        .generic_parameters = std::move(generic_parameters),
        .parameters = std::move(*parameters),
        .return_type = return_type,
        .requires_clause = std::move(requires_clause),
        .body = *body,
    });
}

auto parser::parse_default_constructor(source_span start, source_span name, std::vector<parameter_syntax> parameters) -> std::optional<function_id>
{
    auto equal = expect(token_kind::equal);
    auto default_name = expect_identifier("default");
    auto semicolon = expect(token_kind::semicolon);
    if(not equal or not default_name or not semicolon) {
        return std::nullopt;
    }
    return arena.add(function_syntax {
        .full_span = combine_spans(start, semicolon->span),
        .kind = function_syntax_kind::constructor,
        .defaulted = true,
        .name = name,
        .parameters = std::move(parameters),
        .default_marker = default_name->span,
    });
}

auto parser::parse_parameter_list() -> std::optional<std::vector<parameter_syntax>>
{
    auto parameters = std::vector<parameter_syntax>{};
    if(check(token_kind::r_paren)) {
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

    while(check(token_kind::comma)) {
        consume();
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

auto parser::parse_parameter() -> std::optional<parameter_syntax>
{
    auto is_const = false;
    auto const_span = std::optional<source_span>{};
    if(check(token_kind::kw_const)) {
        is_const = true;
        const_span = consume().span;
    }

    auto name = expect_identifier("parameter name");
    if(not name) {
        return std::nullopt;
    }

    if(name->text == "self" and not check(token_kind::colon)) {
        auto is_reference = false;
        auto end = name->span;
        if(check(token_kind::kw_const)) {
            is_const = true;
            end = consume().span;
            auto amp = expect(token_kind::amp);
            if(not amp) {
                return std::nullopt;
            }
            is_reference = true;
            end = amp->span;
        } else if(check(token_kind::amp)) {
            is_reference = true;
            end = consume().span;
        }
        return parameter_syntax {
            .full_span = combine_spans(const_span.value_or(name->span), end),
            .is_const = is_const,
            .is_self_receiver = true,
            .self_is_reference = is_reference,
            .name = name->span,
        };
    }

    auto colon = expect(token_kind::colon);
    if(not colon) {
        return std::nullopt;
    }

    if(not expect_type_start()) {
        return std::nullopt;
    }
    auto type = parse_type();
    if(not type) {
        return std::nullopt;
    }
    auto is_pack = false;
    auto end = arena.span(*type);
    if(auto ellipsis = consume_ellipsis()) {
        is_pack = true;
        end = *ellipsis;
    }
    auto full_start = const_span.value_or(name->span);
    return parameter_syntax {
        .full_span = combine_spans(full_start, end),
        .is_const = is_const,
        .is_pack = is_pack,
        .name = name->span,
        .type = *type,
    };
}

auto parser::parse_lambda_parameter_list() -> std::optional<std::vector<parameter_syntax>>
{
    auto parameters = std::vector<parameter_syntax>{};
    if(check(token_kind::r_paren)) {
        return parameters;
    }

    if(not expect_parameter_start()) {
        return std::nullopt;
    }
    auto parameter = parse_lambda_parameter();
    if(not parameter) {
        return std::nullopt;
    }
    parameters.emplace_back(std::move(*parameter));

    while(check(token_kind::comma)) {
        consume();
        if(not expect_parameter_start()) {
            return std::nullopt;
        }
        auto next = parse_lambda_parameter();
        if(not next) {
            return std::nullopt;
        }
        parameters.emplace_back(std::move(*next));
    }

    return parameters;
}

auto parser::parse_lambda_parameter() -> std::optional<parameter_syntax>
{
    auto is_const = false;
    auto const_span = std::optional<source_span>{};
    if(check(token_kind::kw_const)) {
        is_const = true;
        const_span = consume().span;
    }

    auto name = expect_identifier("lambda parameter name");
    if(not name) {
        return std::nullopt;
    }

    auto type = std::optional<type_id>{};
    auto is_pack = false;
    auto end = name->span;
    if(check(token_kind::colon)) {
        consume();
        if(not expect_type_start()) {
            return std::nullopt;
        }
        type = parse_type();
        if(not type) {
            return std::nullopt;
        }
        end = arena.span(*type);
        if(auto ellipsis = consume_ellipsis()) {
            is_pack = true;
            end = *ellipsis;
        }
    }

    auto full_start = const_span.value_or(name->span);
    return parameter_syntax {
        .full_span = combine_spans(full_start, end),
        .is_const = is_const,
        .is_pack = is_pack,
        .name = name->span,
        .type = type,
    };
}

auto parser::parse_generic_parameter_list() -> std::optional<std::vector<generic_parameter_syntax>>
{
    auto open = expect(token_kind::less);
    if(not open) {
        return std::nullopt;
    }

    auto parameters = std::vector<generic_parameter_syntax>{};
    if(check(token_kind::greater)) {
        report_current(diagnostic_kind::expected_identifier, "generic parameter list cannot be empty");
        return std::nullopt;
    }

    auto first = parse_generic_parameter();
    if(not first) {
        return std::nullopt;
    }
    parameters.emplace_back(std::move(*first));

    while(check(token_kind::comma)) {
        consume();
        auto next = parse_generic_parameter();
        if(not next) {
            return std::nullopt;
        }
        parameters.emplace_back(std::move(*next));
    }

    auto close = expect_closing_angle();
    if(not close) {
        return std::nullopt;
    }
    return parameters;
}

auto parser::parse_generic_parameter() -> std::optional<generic_parameter_syntax>
{
    auto name = expect_identifier("generic parameter name");
    if(not name) {
        return std::nullopt;
    }

    auto is_pack = false;
    auto end = name->span;
    if(auto ellipsis = consume_ellipsis()) {
        is_pack = true;
        end = *ellipsis;
    }

    auto bounds = std::vector<source_span>{};
    if(check(token_kind::colon)) {
        consume();
        auto bound = expect_identifier("concept bound");
        if(not bound) {
            return std::nullopt;
        }
        bounds.emplace_back(bound->span);
        end = bound->span;
        while(check(token_kind::kw_and)) {
            consume();
            auto next = expect_identifier("concept bound");
            if(not next) {
                return std::nullopt;
            }
            bounds.emplace_back(next->span);
            end = next->span;
        }
    }

    return generic_parameter_syntax {
        .full_span = combine_spans(name->span, end),
        .name = name->span,
        .is_pack = is_pack,
        .concept_bounds = std::move(bounds),
    };
}

auto parser::consume_ellipsis() -> std::optional<source_span>
{
    if(not check(token_kind::dot) or peek(1uz).kind != token_kind::dot or peek(2uz).kind != token_kind::dot) {
        return std::nullopt;
    }

    auto first = consume();
    consume();
    auto third = consume();
    return combine_spans(first.span, third.span);
}
