export module ir.expression;

import std;
import source;
import lexer.token;
import parser.ast;
import ir.state;

impl quad_lowerer {
    lower_call(self&, callee: source_span, arguments: vector<expr_id> const&, needs_result: bool) -> string
    {
        for(const ref argument : arguments) {
            let value = lower_expression(argument);
            emit("param", "_", "_", value.as_str());
        }

        let target = string{source_text(callee)};
        let count = usize_string(arguments.size());
        if(needs_result) {
            let temp = next_temp();
            emit("call", target.as_str(), count.as_str(), temp.as_str());
            return temp;
        }
        emit("call", target.as_str(), count.as_str(), "_");
        return string{"_"};
    }

    lower_expression(self&, id: expr_id) -> string
    {
        const ref syntax = (*parsed).ast.expressions[id.value];
        match syntax {
            .integer(value) => {
                return string{source_text(value.full_span)};
            },
            .name(value) => {
                return string{source_text(value.name)};
            },
            .unary(value) => {
                let operand = lower_expression(value.operand);
                if(value.operator_kind == token_kind::plus) {
                    return operand;
                }
                let temp = next_temp();
                emit("uminus", operand.as_str(), "_", temp.as_str());
                return temp;
            },
            .binary(value) => {
                let left = lower_expression(value.left);
                let right = lower_expression(value.right);
                let temp = next_temp();
                emit(token_quad_op(value.operator_kind), left.as_str(), right.as_str(), temp.as_str());
                return temp;
            },
            .call(value) => {
                return lower_call(value.callee, value.arguments, true);
            },
            .index(value) => {
                let index = lower_expression(value.index);
                let temp = next_temp();
                emit("array_load", source_text(value.array), index.as_str(), temp.as_str());
                return temp;
            },
            .grouped(value) => {
                return lower_expression(value.expression);
            },
        };
        return string{"_"};
    }
}
