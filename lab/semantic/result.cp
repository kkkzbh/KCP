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
    let index: usize = 0;
    while(index < result.expression_infos.size()) {
        if(result.expression_infos[index].expression.value == expression.value) {
            result.expression_infos[index].info = info;
            return;
        }
        index += 1;
    }
    result.expression_infos.push_back(semantic_expr_record{ .expression = expression, .info = info });
}

export semantic_expression_info_of(result: semantic_result const&, expression: expr_id) -> semantic_expr_info
{
    let index: usize = 0;
    while(index < result.expression_infos.size()) {
        if(result.expression_infos[index].expression.value == expression.value) {
            return result.expression_infos[index].info;
        }
        index += 1;
    }
    return semantic_expr_info{};
}

export semantic_record_statement_binding(result: semantic_result&, statement: stmt_id, symbol: symbol_id) -> void
{
    let index: usize = 0;
    while(index < result.statement_bindings.size()) {
        if(result.statement_bindings[index].statement.value == statement.value) {
            result.statement_bindings[index].symbol = symbol;
            return;
        }
        index += 1;
    }
    result.statement_bindings.push_back(semantic_stmt_binding{ .statement = statement, .symbol = symbol });
}

export semantic_statement_binding_of(result: semantic_result const&, statement: stmt_id) -> symbol_id
{
    let index: usize = 0;
    while(index < result.statement_bindings.size()) {
        if(result.statement_bindings[index].statement.value == statement.value) {
            return result.statement_bindings[index].symbol;
        }
        index += 1;
    }
    return symbol_id{};
}

export semantic_find_function(result: semantic_result const&, name: str) -> symbol_id
{
    let index: usize = 0;
    while(index < result.functions.size()) {
        let id = result.functions[index];
        let symbol = result.symbols[id.index()];
        if(symbol.name.as_str() == name) {
            return id;
        }
        index += 1;
    }
    return symbol_id{};
}
