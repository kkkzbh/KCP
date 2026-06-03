export module semantic.result;

import std;
import source;
import diagnostic;
import parser.ast;
import semantic.type;
import semantic.symbol;

export struct semantic_expr_info {
    type: semantic_type_kind;
    constant: optional<i32>;
}

impl semantic_expr_info {
    semantic_expr_info()
    {
        return semantic_expr_info{
            .type = semantic_type_kind::error,
            .constant = optional<i32>::none
        };
    }
}

export struct semantic_expr_record {
    expression: expr_id;
    info: semantic_expr_info;
}

export struct semantic_stmt_binding {
    statement: stmt_id;
    symbol: symbol_id;
}

export struct semantic_result {
    functions: vector<symbol_id>;
    symbols: vector<semantic_symbol>;
    expression_infos: vector<semantic_expr_record>;
    statement_bindings: vector<semantic_stmt_binding>;
    diagnostics: vector<diagnostic>;
}

impl semantic_result {
    accepted(self const&) -> bool
    {
        return diagnostics.size() == 0 as usize;
    }
}

export semantic_add_symbol(result: semantic_result&, symbol: semantic_symbol) -> symbol_id
{
    let id = symbol_id{ .value = result.symbols.size() + 1 };
    result.symbols.push_back(symbol);
    return id;
}

export semantic_record_expression(result: semantic_result&, expression: expr_id, info: semantic_expr_info) -> void
{
    for(let ref item : result.expression_infos) {
        if(item.expression.value == expression.value) {
            item.info = info;
            return;
        }
    }
    result.expression_infos.push_back(semantic_expr_record{ .expression = expression, .info = info });
}

export semantic_expression_info_of(result: semantic_result const&, expression: expr_id) -> semantic_expr_info
{
    for(const ref item : result.expression_infos) {
        if(item.expression.value == expression.value) {
            return item.info;
        }
    }
    return semantic_expr_info{};
}

export semantic_record_statement_binding(result: semantic_result&, statement: stmt_id, symbol: symbol_id) -> void
{
    for(let ref item : result.statement_bindings) {
        if(item.statement.value == statement.value) {
            item.symbol = symbol;
            return;
        }
    }
    result.statement_bindings.push_back(semantic_stmt_binding{ .statement = statement, .symbol = symbol });
}

export semantic_statement_binding_of(result: semantic_result const&, statement: stmt_id) -> symbol_id
{
    for(const ref item : result.statement_bindings) {
        if(item.statement.value == statement.value) {
            return item.symbol;
        }
    }
    return symbol_id{};
}

export semantic_find_function(result: semantic_result const&, name: str) -> symbol_id
{
    for(const ref id : result.functions) {
        const ref symbol = result.symbols[id.index()];
        if(symbol.name.as_str() == name) {
            return id;
        }
    }
    return symbol_id{};
}
