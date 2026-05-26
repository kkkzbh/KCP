export module semantic.expression;

import std;
import source;
import diagnostic;
import lexer.token;
import parser.ast;
import semantic.type;
import semantic.symbol;
import semantic.state;

digit_value(ch: char) -> i32
{
    if(ch == '0') { return 0; }
    if(ch == '1') { return 1; }
    if(ch == '2') { return 2; }
    if(ch == '3') { return 3; }
    if(ch == '4') { return 4; }
    if(ch == '5') { return 5; }
    if(ch == '6') { return 6; }
    if(ch == '7') { return 7; }
    if(ch == '8') { return 8; }
    return 9;
}

bool_value(value: bool) -> i32
{
    if(value) {
        return 1;
    }
    return 0;
}

parse_i32_literal(text: str) -> i32
{
    let value: i32 = 0;
    let index: usize = 0;
    while(index < text.size()) {
        value = value * 10 + digit_value(text[index]);
        index += 1;
    }
    return value;
}

int_info(value: optional<i32>) -> semantic_expr_info
{
    return semantic_expr_info{ .type = semantic_type_kind::int_type, .constant = value };
}

error_info() -> semantic_expr_info
{
    return semantic_expr_info{ .type = semantic_type_kind::error, .constant = optional<i32>::none };
}

is_int_value(info: semantic_expr_info) -> bool
{
    return info.type == semantic_type_kind::int_type;
}

constant_binary(analyzer: semantic_analyzer&, kind: token_kind, left: optional<i32>, right: optional<i32>, span: source_span) -> optional<i32>
{
    if(not left.has_value() or not right.has_value()) {
        return optional<i32>::none;
    }

    let lhs = *left;
    let rhs = *right;
    if(kind == token_kind::plus) { return optional<i32>::some(lhs + rhs); }
    if(kind == token_kind::minus) { return optional<i32>::some(lhs - rhs); }
    if(kind == token_kind::star) { return optional<i32>::some(lhs * rhs); }
    if(kind == token_kind::slash) {
        if(rhs == 0) {
            analyzer.report(diagnostic_kind::constant_divide_by_zero, span);
            return optional<i32>::none;
        }
        return optional<i32>::some(lhs / rhs);
    }
    if(kind == token_kind::percent) {
        if(rhs == 0) {
            analyzer.report(diagnostic_kind::constant_divide_by_zero, span);
            return optional<i32>::none;
        }
        return optional<i32>::some(lhs % rhs);
    }
    if(kind == token_kind::equal_equal) { return optional<i32>::some(bool_value(lhs == rhs)); }
    if(kind == token_kind::bang_equal) { return optional<i32>::some(bool_value(lhs != rhs)); }
    if(kind == token_kind::less) { return optional<i32>::some(bool_value(lhs < rhs)); }
    if(kind == token_kind::less_equal) { return optional<i32>::some(bool_value(lhs <= rhs)); }
    if(kind == token_kind::greater) { return optional<i32>::some(bool_value(lhs > rhs)); }
    if(kind == token_kind::greater_equal) { return optional<i32>::some(bool_value(lhs >= rhs)); }
    if(kind == token_kind::amp_amp) { return optional<i32>::some(bool_value((lhs != 0) and (rhs != 0))); }
    if(kind == token_kind::pipe_pipe) { return optional<i32>::some(bool_value((lhs != 0) or (rhs != 0))); }
    return optional<i32>::none;
}

impl semantic_analyzer {
    check_call_expression(self&, callee: source_span, arguments: vector<expr_id> const&, full_span: source_span, value_required: bool) -> semantic_expr_info
    {
        let argument_types = vector<semantic_parameter_type>{};
        let index: usize = 0;
        while(index < arguments.size()) {
            let argument = check_expression(arguments[index]);
            argument_types.push_back(semantic_parameter_type{ .type = argument.type });
            index += 1;
        }

        let function = semantic_find_function(result, source_text(callee));
        if(not function.valid()) {
            report(diagnostic_kind::undefined_function, callee);
            return error_info();
        }

        let symbol = result.symbols[function.index()];
        if(symbol.parameter_count != arguments.size()) {
            report(diagnostic_kind::argument_count_mismatch, full_span);
            return error_info();
        }
        let parameter_index: usize = 0;
        while(parameter_index < symbol.parameter_types.size()) {
            let argument_type = argument_types[parameter_index].type;
            if(argument_type != semantic_type_kind::error and argument_type != symbol.parameter_types[parameter_index].type) {
                report(diagnostic_kind::argument_type_mismatch, full_span);
                return error_info();
            }
            parameter_index += 1;
        }
        if(value_required and symbol.type == semantic_type_kind::void_type) {
            report(diagnostic_kind::void_value, full_span);
            return error_info();
        }
        return semantic_expr_info{ .type = symbol.type, .constant = optional<i32>::none };
    }

    check_expression(self&, id: expr_id) -> semantic_expr_info
    {
        let syntax = (*parsed).ast.expressions[id.value];
        match syntax {
            .integer(value) => {
                let constant = parse_i32_literal(source_text(value.full_span));
                let info = int_info(optional<i32>::some(constant));
                record_expression(id, info);
                return info;
            },
            .name(value) => {
                let symbol = find_visible_variable(source_text(value.name));
                if(not symbol.valid()) {
                    report(diagnostic_kind::undeclared_variable, value.name);
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let binding = result.symbols[symbol.index()];
                let info = semantic_expr_info{ .type = binding.type, .constant = optional<i32>::none };
                record_expression(id, info);
                return info;
            },
            .unary(value) => {
                let operand = check_expression(value.operand);
                if(operand.type == semantic_type_kind::error) {
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                if(not is_int_value(operand)) {
                    report(diagnostic_kind::array_value_required, value.full_span);
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let constant = optional<i32>::none;
                if(operand.constant.has_value()) {
                    if(value.operator_kind == token_kind::minus) {
                        constant = optional<i32>::some(-(*operand.constant));
                    } else {
                        constant = operand.constant;
                    }
                }
                let info = int_info(constant);
                record_expression(id, info);
                return info;
            },
            .binary(value) => {
                let left = check_expression(value.left);
                let right = check_expression(value.right);
                if(left.type == semantic_type_kind::error or right.type == semantic_type_kind::error) {
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                if(not is_int_value(left) or not is_int_value(right)) {
                    report(diagnostic_kind::array_value_required, value.full_span);
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let constant = constant_binary(ref self, value.operator_kind, left.constant, right.constant, value.full_span);
                let info = int_info(constant);
                record_expression(id, info);
                return info;
            },
            .call(value) => {
                let info = check_call_expression(value.callee, value.arguments, value.full_span, true);
                record_expression(id, info);
                return info;
            },
            .index(value) => {
                let symbol = find_visible_variable(source_text(value.array));
                if(not symbol.valid()) {
                    report(diagnostic_kind::undeclared_variable, value.array);
                    check_expression(value.index);
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let binding = result.symbols[symbol.index()];
                if(binding.type != semantic_type_kind::int_array_type) {
                    report(diagnostic_kind::non_array_index, value.array);
                    check_expression(value.index);
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let index_info = check_expression(value.index);
                if(index_info.type == semantic_type_kind::int_array_type) {
                    report(diagnostic_kind::array_value_required, (*parsed).ast.expr_span(value.index));
                    let info = error_info();
                    record_expression(id, info);
                    return info;
                }
                let info = int_info(optional<i32>::none);
                record_expression(id, info);
                return info;
            },
            .grouped(value) => {
                let inner = check_expression(value.expression);
                record_expression(id, inner);
                return inner;
            },
        };
        return error_info();
    }
}
