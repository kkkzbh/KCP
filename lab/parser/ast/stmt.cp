export module parser.ast.stmt;

import std;
import source;
import parser.ast.ids;

export struct block_statement {
    full_span: source_span;
    statements: vector<stmt_id>;
}

export struct var_decl_item {
    full_span: source_span;
    name: source_span;
    initializer: optional<expr_id>;
}

impl var_decl_item {
    var_decl_item()
    {
        return var_decl_item{
            .full_span = source_span{},
            .name = source_span{},
            .initializer = optional<expr_id>::none
        };
    }
}

export struct var_decl_statement {
    full_span: source_span;
    declarations: vector<var_decl_item>;
}

export struct assign_statement {
    full_span: source_span;
    name: source_span;
    value: expr_id;
}

export struct call_statement {
    full_span: source_span;
    callee: source_span;
    arguments: vector<expr_id>;
}

export struct if_statement {
    full_span: source_span;
    condition: expr_id;
    then_branch: stmt_id;
    else_branch: optional<stmt_id>;
}

export struct while_statement {
    full_span: source_span;
    condition: expr_id;
    body: stmt_id;
}

export struct return_statement {
    full_span: source_span;
    value: optional<expr_id>;
}

export variant stmt_syntax {
    block(block_statement);
    var_decl(var_decl_statement);
    assign(assign_statement);
    call(call_statement);
    if_stmt(if_statement);
    while_stmt(while_statement);
    return_stmt(return_statement);
}

export stmt_syntax_span(value: stmt_syntax const&) -> source_span
{
    return match value {
        .block(item) => item.full_span,
        .var_decl(item) => item.full_span,
        .assign(item) => item.full_span,
        .call(item) => item.full_span,
        .if_stmt(item) => item.full_span,
        .while_stmt(item) => item.full_span,
        .return_stmt(item) => item.full_span,
    };
}
