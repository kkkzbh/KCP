module semantic.check:statements;

import std;
import parser;
import semantic.ty;
import semantic.symbol;
import semantic.diagnostic;
import semantic.result.keys;
import semantic.check.keys;
import :state;
import :context;
import :types;
import :expressions;

struct statement_checker
{
    explicit statement_checker(semantic_context& context) :
        context(context),
        types(context),
        expressions(context) {}

    auto check_function(ast_arena const& ast, function_id id) -> void
    {
        auto const& function = ast.function(id);
        auto signature_id = context.result.signature_of(context.current_unit, id);
        if(not signature_id.valid()) {
            return;
        }

        context.scopes.clear();
        context.loops.clear();
        context.scopes.emplace_back(context.units[context.current_unit].visible_functions);
        context.scopes.emplace_back();

        auto& signature = context.result.signatures[signature_id.value];
        auto returns = return_state{};
        if(not is_error(signature.returns)) {
            returns.declared_return = signature.returns;
        }

        for(auto index = 0uz; index < function.parameters.size(); ++index) {
            auto const& parameter = function.parameters[index];
            auto name = std::string{ context.ast_source.slice(parameter.name) };
            auto symbol = (
                context.bind_symbol (semantic_symbol {
                    .kind = symbol_kind::parameter,
                    .name = name,
                    .span = parameter.name,
                    .type = signature.parameters[index],
                    .is_const = parameter.is_const,
                    .unit_index = context.current_unit,
                    .function = id,
                })
            );
            if(symbol.valid()) {
                context.parameter_symbols[parameter_binding_key{context.current_unit, parameter.name.start}] = symbol;
            }
        }

        check_statement(ast, function.body, returns);

    }

    auto check_statement(ast_arena const& ast, stmt_id id, return_state& returns) -> void
    {
        auto const& statement = ast.statement(id);
        std::visit (
            overloaded {
                [&](block_statement_syntax const& node) {
                    context.scopes.emplace_back();
                    for(auto child : node.statements) {
                        check_statement(ast, child, returns);
                    }
                    context.scopes.pop_back();
                },
                [&](declaration_statement_syntax const& node) {
                    auto expected = std::optional<semantic_type_id>{};
                    if(node.declared_type) {
                        expected = types.lower_type(ast, *node.declared_type);
                    }

                    auto initializer = expressions.check_expression(ast, node.initializer, expected);
                    auto declared_type = expected.value_or(initializer.type);
                    if(expected and not types.can_implicitly_convert(initializer, *expected)) {
                        context.report (
                            semantic_diagnostic_code::type_mismatch,
                            node.full_span,
                            "initializer type does not match declared type"
                        );
                    }

                    auto symbol = (
                        context.bind_symbol (semantic_symbol {
                            .kind = symbol_kind::local,
                            .name = std::string{ context.ast_source.slice(node.name) },
                            .span = node.name,
                            .type = declared_type,
                            .is_const = node.is_const,
                            .unit_index = context.current_unit,
                        })
                    );
                    if(symbol.valid()) {
                        context.result.statement_bindings[semantic_node_key{context.current_unit, id}] = symbol;
                    }
                },
                [&](if_statement_syntax const& node) {
                    check_condition(ast, node.condition);
                    check_statement(ast, node.then_branch, returns);
                    if(node.else_branch) {
                        check_statement(ast, *node.else_branch, returns);
                    }
                },
                [&](while_statement_syntax const& node) {
                    check_condition(ast, node.condition);
                    context.loops.emplace_back();
                    check_statement(ast, node.body, returns);
                    context.loops.pop_back();
                },
                [&](do_while_statement_syntax const& node) {
                    context.loops.emplace_back();
                    check_statement(ast, node.body, returns);
                    context.loops.pop_back();
                    check_condition(ast, node.condition);
                },
                [&](for_statement_syntax const& node) {
                    auto range = expressions.check_expression(ast, node.range, std::nullopt);
                    auto element = types.range_element_type(range.type);
                    if(not element.valid()) {
                        context.report (
                            semantic_diagnostic_code::invalid_range,
                            node.full_span,
                            "for range must be array<T,N> or sequence<T,N>"
                        );
                        element = semantic_type_ids::error;
                    }

                    auto label = loop_label{};
                    if(node.label) {
                        label = loop_label {
                            .name = std::string{ context.ast_source.slice(*node.label) },
                            .span = *node.label,
                        };
                    }
                    context.loops.emplace_back(label);
                    context.scopes.emplace_back();
                    auto symbol = (
                        context.bind_symbol (semantic_symbol {
                            .kind = symbol_kind::local,
                            .name = std::string{ context.ast_source.slice(node.name) },
                            .span = node.name,
                            .type = element,
                            .is_const = node.is_const,
                            .unit_index = context.current_unit,
                        })
                    );
                    if(symbol.valid()) {
                        context.result.statement_bindings[semantic_node_key{context.current_unit, id}] = symbol;
                    }
                    check_statement(ast, node.body, returns);
                    context.scopes.pop_back();
                    context.loops.pop_back();
                },
                [&](break_statement_syntax const& node) {
                    check_loop_jump(node.full_span, node.label, true);
                },
                [&](continue_statement_syntax const& node) {
                    check_loop_jump(node.full_span, node.label, false);
                },
                [&](return_statement_syntax const& node) {
                    auto returned = semantic_type_ids::unit;
                    if(node.value) {
                        if(returns.declared_return) {
                            returned = expressions.check_expression(ast, *node.value, returns.declared_return).type;
                        } else {
                            returned = expressions.check_expression(ast, *node.value, std::nullopt).type;
                        }
                    }

                    if(returns.declared_return and not types.can_implicitly_convert(returned, *returns.declared_return)) {
                        context.report (
                            semantic_diagnostic_code::return_type_mismatch,
                            node.full_span,
                            "return type does not match function return type"
                        );
                    }
                },
                [&](expression_statement_syntax const& node) {
                    expressions.check_expression(ast, node.expression, std::nullopt);
                },
            },
            statement
        );
    }

    auto check_condition(ast_arena const& ast, expr_id condition) -> void
    {
        auto checked = expressions.check_expression(ast, condition, semantic_type_ids::bool_);
        if(not types.can_implicitly_convert(checked, semantic_type_ids::bool_)) {
            context.report (
                semantic_diagnostic_code::condition_not_bool,
                ast.span(condition),
                "condition expression must be bool"
            );
        }
    }

    auto check_loop_jump(source_span span, std::optional<source_span> label, bool is_break) -> void
    {
        if(context.loops.empty()) {
            context.report (
                is_break ? semantic_diagnostic_code::invalid_break : semantic_diagnostic_code::invalid_continue,
                span,
                is_break ? "break must be inside a loop" : "continue must be inside a loop"
            );
            return;
        }

        if(not label) {
            return;
        }

        auto name = context.ast_source.slice(*label);
        auto found = std::ranges::find(context.loops, name, &loop_label::name);
        if(found == context.loops.end()) {
            context.report (
                semantic_diagnostic_code::unknown_label,
                *label,
                std::format("unknown loop label '{}'", name)
            );
        }
    }

    semantic_context& context;
    type_checker types;
    expression_checker expressions;
};
