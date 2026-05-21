export module parser.ast.expr;

import std;
import source;
import lexer.token;
import parser.ast.ids;

export struct integer_expr {
    full_span: source_span;
}

export struct name_expr {
    full_span: source_span;
    name: source_span;
}

export struct unary_expr {
    full_span: source_span;
    operator_kind: token_kind;
    operand: expr_id;
}

export struct binary_expr {
    full_span: source_span;
    operator_kind: token_kind;
    left: expr_id;
    right: expr_id;
}

export struct call_expr {
    full_span: source_span;
    callee: source_span;
    arguments: vector<expr_id>;
}

export struct grouped_expr {
    full_span: source_span;
    expression: expr_id;
}

export variant expr_syntax {
    integer(integer_expr);
    name(name_expr);
    unary(unary_expr);
    binary(binary_expr);
    call(call_expr);
    grouped(grouped_expr);
}

export expr_syntax_span(value: expr_syntax const&) -> source_span
{
    return match value {
        .integer(item) => item.full_span,
        .name(item) => item.full_span,
        .unary(item) => item.full_span,
        .binary(item) => item.full_span,
        .call(item) => item.full_span,
        .grouped(item) => item.full_span,
    };
}
