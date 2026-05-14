module semantic.check:expressions;

import std;
import lexer.token;
import parser;
import semantic.ty;
import semantic.symbol;
import semantic.diagnostic;
import semantic.result.keys;
import :state;
import :context;
import :types;

struct expression_checker
{
    explicit expression_checker(semantic_context& context) :
        context(context),
        types(context) {}

    auto check_expression(
        ast_arena const& ast,
        expr_id id,
        std::optional<semantic_type_id> expected
    ) -> expression_info
    {
        auto const& expression = ast.expression(id);
        auto info = (
            std::visit (
                overloaded {
                    [&](name_expr_syntax const& node) {
                        return check_name_expression(node, id);
                    },
                    [&](literal_expr_syntax const& node) {
                        return check_literal_expression(node);
                    },
                    [&](unary_expr_syntax const& node) {
                        return check_unary_expression(ast, node);
                    },
                    [&](binary_expr_syntax const& node) {
                        return check_binary_expression(ast, node);
                    },
                    [&](assignment_expr_syntax const& node) {
                        return check_assignment_expression(ast, node);
                    },
                    [&](call_expr_syntax const& node) {
                        return check_call_expression(ast, node, expected);
                    },
                    [&](cast_expr_syntax const& node) {
                        return check_cast_expression(ast, node);
                    },
                    [&](array_literal_expr_syntax const& node) {
                        return check_array_like_literal(ast, node.full_span, node.elements, expected, true);
                    },
                    [&](sequence_literal_expr_syntax const& node) {
                        return check_array_like_literal(ast, node.full_span, node.elements, expected, false);
                    },
                    [&](tuple_literal_expr_syntax const& node) {
                        return check_tuple_literal(ast, node, expected);
                    },
                    [&](grouped_expr_syntax const& node) {
                        return check_expression(ast, node.expression, expected);
                    },
                },
                expression
            )
        );

        context.result.expression_types[semantic_node_key{context.current_unit, id}] = info.type;
        if(expected and not types.can_implicitly_convert(info, *expected)) {
            context.report (
                semantic_diagnostic_code::type_mismatch,
                ast.span(id),
                "expression type does not match contextual type"
            );
        }
        return info;
    }

    auto check_name_expression(name_expr_syntax const& node, expr_id id) -> expression_info
    {
        auto name = std::string{ context.ast_source.identifier(node.name) };
        auto symbol = context.resolve(name);
        if(not symbol.valid()) {
            context.report (
                semantic_diagnostic_code::unknown_name,
                node.full_span,
                std::format("unknown name '{}'", name)
            );
            return expression_info{ .type = semantic_type_ids::error };
        }

        context.result.expression_symbols[semantic_node_key{context.current_unit, id}] = symbol;
        auto const& value = context.result.symbols[symbol.value];
        return expression_info {
            .type = value.type,
            .is_lvalue = value.kind != symbol_kind::function,
            .is_const = value.kind != symbol_kind::function and value.is_const,
        };
    }

    auto check_literal_expression(literal_expr_syntax const& node) -> expression_info
    {
        auto text = context.ast_source.slice(node.full_span);
        if(text == "true" or text == "false") {
            return expression_info{ .type = semantic_type_ids::bool_ };
        }
        if(text.starts_with("'")) {
            return expression_info{ .type = semantic_type_ids::char_ };
        }
        if(text.starts_with("\"")) {
            return expression_info{ .type = semantic_type_ids::str };
        }
        if(text.contains('.') or text.contains('e') or text.contains('E')) {
            return expression_info{ .type = semantic_type_ids::f64 };
        }
        return expression_info{ .type = semantic_type_ids::i32 };
    }

    auto check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info
    {
        auto operand = check_expression(ast, node.operand, std::nullopt);
        auto operand_value = types.read_type(operand.type);
        using enum token_kind;
        switch(node.operator_kind) {
            case amp:
                if(not operand.is_lvalue) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "address-of operand must be an lvalue"
                    );
                    return expression_info{ .type = semantic_type_ids::error };
                }
                return expression_info {
                    .type = context.result.types.intern(pointer_type {
                        operand_value,
                        types.target_const(operand_value, operand.is_const),
                    }),
                };
            case star:
                if(auto pointee = types.pointer_pointee(operand_value)) {
                    return *pointee;
                }
                context.report (
                    semantic_diagnostic_code::invalid_operator,
                    node.full_span,
                    "cannot dereference non-pointer value"
                );
                return expression_info{ .type = semantic_type_ids::error };
            case kw_not:
                if(types.read_type(operand.type) != semantic_type_ids::bool_) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "'not' operand must be bool"
                    );
                }
                return expression_info{ .type = semantic_type_ids::bool_ };
            case plus:
            case minus:
            case tilde:
                if(not types.is_numeric_type(operand_value)) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "unary operand must be numeric"
                    );
                }
                return expression_info{ .type = operand_value };
            case plus_plus:
            case minus_minus:
                if(not operand.is_lvalue or operand.is_const) {
                    context.report (
                        semantic_diagnostic_code::invalid_assignment_target,
                        node.full_span,
                        "increment operand must be a non-const lvalue"
                    );
                }
                if(not types.is_numeric_type(operand_value)) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "increment operand must be numeric"
                    );
                }
                return expression_info{ .type = operand_value };
            default:
                return expression_info{ .type = semantic_type_ids::error };
        }
    }

    auto check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info
    {
        auto left = check_expression(ast, node.left, std::nullopt);
        auto right = check_expression(ast, node.right, std::nullopt);
        auto left_type = types.read_type(left.type);
        auto right_type = types.read_type(right.type);
        using enum token_kind;
        switch(node.operator_kind) {
            case kw_and:
            case kw_or:
                if(left_type != semantic_type_ids::bool_ or right_type != semantic_type_ids::bool_) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "logical operands must be bool"
                    );
                }
                return expression_info{ .type = semantic_type_ids::bool_ };
            case equal_equal:
            case bang_equal:
            case less:
            case less_equal:
            case greater:
            case greater_equal:
                if(not types.common_numeric_type(left_type, right_type) and left_type != right_type) {
                    context.report (
                        semantic_diagnostic_code::invalid_operator,
                        node.full_span,
                        "comparison operands are incompatible"
                    );
                }
                return expression_info{ .type = semantic_type_ids::bool_ };
            case plus:
            case minus:
            case star:
            case slash:
            case percent:
                if(auto common = types.common_numeric_type(left_type, right_type)) {
                    return expression_info{ .type = *common };
                }
                context.report (
                    semantic_diagnostic_code::invalid_operator,
                    node.full_span,
                    "arithmetic operands must be numeric"
                );
                return expression_info{ .type = semantic_type_ids::error };
            case pipe:
            case caret:
            case amp:
            case less_less:
            case greater_greater:
                if(auto common = types.common_integer_type(left_type, right_type)) {
                    return expression_info{ .type = *common };
                }
                context.report (
                    semantic_diagnostic_code::invalid_operator,
                    node.full_span,
                    "bitwise operands must be integers"
                );
                return expression_info{ .type = semantic_type_ids::error };
            default:
                context.report (
                    semantic_diagnostic_code::invalid_operator,
                    node.full_span,
                    "unsupported binary operator"
                );
                return expression_info{ .type = semantic_type_ids::error };
        }
    }

    auto check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node) -> expression_info
    {
        auto left = check_expression(ast, node.left, std::nullopt);
        auto left_type = types.read_type(left.type);
        if(not left.is_lvalue) {
            context.report (
                semantic_diagnostic_code::invalid_assignment_target,
                node.full_span,
                "assignment target must be an lvalue"
            );
        }
        if(left.is_const) {
            context.report(semantic_diagnostic_code::assign_to_const, node.full_span, "cannot assign to const binding");
        }

        auto right = check_expression(ast, node.right, left_type);
        if(not types.can_implicitly_convert(right, left_type)) {
            context.report (
                semantic_diagnostic_code::type_mismatch,
                node.full_span,
                "assignment value does not match target type"
            );
        }
        return expression_info{ .type = left_type };
    }

    auto check_call_expression(
        ast_arena const& ast,
        call_expr_syntax const& node,
        std::optional<semantic_type_id>
    ) -> expression_info
    {
        if(auto cast_type = function_style_cast_type(ast, node)) {
            if(node.arguments.size() != 1uz) {
                context.report (
                    semantic_diagnostic_code::argument_count_mismatch,
                    node.full_span,
                    "function-style cast expects one argument"
                );
                return expression_info{ .type = *cast_type };
            }
            auto argument = check_expression(ast, node.arguments.front(), std::nullopt);
            if(not types.can_explicitly_convert(argument.type, *cast_type)) {
                context.report(semantic_diagnostic_code::invalid_cast, node.full_span, "invalid function-style cast");
            }
            return expression_info{ .type = *cast_type };
        }

        auto callee = check_expression(ast, node.callee, std::nullopt);
        auto const* callable = std::get_if<function_type>(&context.result.types.get(callee.type));
        if(callable == nullptr) {
            context.report(semantic_diagnostic_code::not_callable, node.full_span, "callee is not callable");
            for(auto argument : node.arguments) {
                check_expression(ast, argument, std::nullopt);
            }
            return expression_info{ .type = semantic_type_ids::error };
        }

        if(callable->parameters.size() != node.arguments.size()) {
            context.report (
                semantic_diagnostic_code::argument_count_mismatch,
                node.full_span,
                "call argument count does not match function signature"
            );
        }

        auto count = std::min(callable->parameters.size(), node.arguments.size());
        for(auto index = 0uz; index < count; ++index) {
            check_expression(ast, node.arguments[index], callable->parameters[index]);
        }
        for(auto index = count; index < node.arguments.size(); ++index) {
            check_expression(ast, node.arguments[index], std::nullopt);
        }
        return expression_info{ .type = callable->returns };
    }

    auto check_cast_expression(ast_arena const& ast, cast_expr_syntax const& node) -> expression_info
    {
        auto target = types.lower_type(ast, node.type);
        auto operand = check_expression(ast, node.operand, std::nullopt);
        if(not types.can_explicitly_convert(operand.type, target)) {
            context.report(semantic_diagnostic_code::invalid_cast, node.full_span, "invalid explicit cast");
        }
        return expression_info{ .type = target };
    }

    auto check_array_like_literal(
        ast_arena const& ast,
        source_span span,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected,
        bool is_array
    ) -> expression_info
    {
        if(auto aggregate = aggregate_context_for(expected, is_array)) {
            if(aggregate->length != elements.size()) {
                context.report (
                    semantic_diagnostic_code::aggregate_length_mismatch,
                    span,
                    "aggregate literal length does not match contextual type"
                );
            }
            for(auto element : elements) {
                check_expression(ast, element, aggregate->element);
            }
            return expression_info{ .type = *expected };
        }

        if(expected) {
            context.report (
                semantic_diagnostic_code::type_mismatch,
                span,
                is_array ? "array literal requires array context" : "sequence literal requires sequence context"
            );
        }

        if(elements.empty()) {
            context.report (
                semantic_diagnostic_code::empty_aggregate_without_context,
                span,
                "empty aggregate literal requires a contextual type"
            );
            return expression_info{ .type = semantic_type_ids::error };
        }

        auto element_types = std::vector<semantic_type_id>{};
        for(auto element : elements) {
            element_types.emplace_back(types.read_type(check_expression(ast, element, std::nullopt).type));
        }

        auto joined = types.join_same_class_numeric_or_equal(element_types, span);
        if(is_array) {
            return expression_info {
                .type = context.result.types.intern (array_type {
                    .element = joined,
                    .length = elements.size(),
                }),
            };
        }
        return expression_info {
            .type = context.result.types.intern (sequence_type {
                .element = joined,
                .length = elements.size(),
            }),
        };
    }

    auto check_tuple_literal(
        ast_arena const& ast,
        tuple_literal_expr_syntax const& node,
        std::optional<semantic_type_id> expected
    ) -> expression_info
    {
        if(expected) {
            if(auto const* tuple = std::get_if<tuple_type>(&context.result.types.get(*expected))) {
                if(tuple->elements.size() != node.elements.size()) {
                    context.report (
                        semantic_diagnostic_code::aggregate_length_mismatch,
                        node.full_span,
                        "tuple literal length does not match contextual type"
                    );
                }
                auto count = std::min(tuple->elements.size(), node.elements.size());
                for(auto index = 0uz; index < count; ++index) {
                    check_expression(ast, node.elements[index], tuple->elements[index]);
                }
                for(auto index = count; index < node.elements.size(); ++index) {
                    check_expression(ast, node.elements[index], std::nullopt);
                }
                return expression_info{ .type = *expected };
            }
            context.report(semantic_diagnostic_code::type_mismatch, node.full_span, "tuple literal requires tuple context");
        }

        auto element_types = std::vector<semantic_type_id>{};
        for(auto element : node.elements) {
            element_types.emplace_back(types.read_type(check_expression(ast, element, std::nullopt).type));
        }
        return expression_info {
            .type = context.result.types.intern(tuple_type{ std::move(element_types) }),
        };
    }

    auto function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
        -> std::optional<semantic_type_id>
    {
        auto const& callee = ast.expression(node.callee);
        if(not is<name_expr_syntax>(callee)) {
            return std::nullopt;
        }

        auto text = context.ast_source.identifier(as<name_expr_syntax>(callee).name);
        if(auto builtin = is_builtin_name(text)) {
            return semantic_type_ids::builtin(*builtin);
        }
        return std::nullopt;
    }

    auto aggregate_context_for(std::optional<semantic_type_id> expected, bool is_array)
        -> std::optional<aggregate_context>
    {
        if(not expected) {
            return std::nullopt;
        }
        auto const& type = context.result.types.get(*expected);
        if(is_array) {
            if(auto const* array = std::get_if<array_type>(&type)) {
                return aggregate_context {
                    .element = array->element,
                    .length = array->length,
                };
            }
            return std::nullopt;
        }
        if(auto const* sequence = std::get_if<sequence_type>(&type)) {
            return aggregate_context {
                .element = sequence->element,
                .length = sequence->length,
            };
        }
        return std::nullopt;
    }

    semantic_context& context;
    type_checker types;
};
