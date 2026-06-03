export module parser.ast.stmt;

import std;
import source;
import parser.ast.ids;

export struct block_statement {
    full_span: source_span;
    statements: vector<stmt_id>;
}

export variant var_decl_data {
    scalar;
    initialized(expr_id);
    array(source_span);
    array_initialized(source_span, vector<expr_id>);
}

export struct var_decl_item {
    full_span: source_span;
    name: source_span;
    data: var_decl_data = var_decl_data::scalar;
}

impl var_decl_item {
    var_decl_item()
    {
        return var_decl_item{
            .full_span = source_span{},
            .name = source_span{},
            .data = var_decl_data::scalar
        };
    }
}

export var_decl_initializer(item: var_decl_item const&) -> optional<expr_id>
{
    return match item.data {
        .initialized(value) => optional<expr_id>::some(value),
        _ => optional<expr_id>::none,
    };
}

export var_decl_array_size(item: var_decl_item const&) -> optional<source_span>
{
    return match item.data {
        .array(size) => optional<source_span>::some(size),
        .array_initialized(size, initializers) => optional<source_span>::some(size),
        _ => optional<source_span>::none,
    };
}

export var_decl_array_initializers(item: var_decl_item const&) -> vector<expr_id>
{
    return match item.data {
        .array_initialized(size, initializers) => initializers,
        _ => vector<expr_id>{},
    };
}

export struct var_decl_statement {
    full_span: source_span;
    declarations: vector<var_decl_item>;
}

export struct assign_statement {
    full_span: source_span;
    name: source_span;
    index: optional<expr_id>;
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

export struct for_statement {
    full_span: source_span;
    initializer: optional<stmt_id>;
    condition: expr_id;
    step: optional<stmt_id>;
    body: stmt_id;
}

export struct break_statement {
    full_span: source_span;
}

export struct continue_statement {
    full_span: source_span;
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
    for_stmt(for_statement);
    break_stmt(break_statement);
    continue_stmt(continue_statement);
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
        .for_stmt(item) => item.full_span,
        .break_stmt(item) => item.full_span,
        .continue_stmt(item) => item.full_span,
        .return_stmt(item) => item.full_span,
    };
}
