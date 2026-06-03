export module semantic.state;

import std;
import source;
import diagnostic;
import parser;
import parser.ast;
import semantic.type;
import semantic.symbol;
export import semantic.result;

export struct semantic_scope {
    symbols: vector<symbol_id>;
}

export struct semantic_analyzer {
    file: source_file const*;
    parsed: parse_result const*;
    diagnostics: diagnostic_collector;
    result: semantic_result;
    scopes: vector<semantic_scope>;
    current_function: function_id;
    current_return_type: semantic_type_kind;
    current_function_span: source_span;
    current_saw_value_return: bool;
    loop_depth: usize;
}

impl semantic_analyzer {
    semantic_analyzer(file_value: source_file const&, parsed_value: parse_result const&)
    {
        return semantic_analyzer{
            .file = &file_value,
            .parsed = &parsed_value,
            .diagnostics = diagnostic_collector{},
            .result = semantic_result{},
            .scopes = vector<semantic_scope>{},
            .current_function = function_id{},
            .current_return_type = semantic_type_kind::error,
            .current_function_span = source_span{},
            .current_saw_value_return = false,
            .loop_depth = 0 as usize
        };
    }

    source_text(self const&, span: source_span) -> str
    {
        return (*file).slice(span);
    }

    report(self&, kind: diagnostic_kind, span: source_span) -> void
    {
        diagnostics.report(kind, span);
    }

    add_symbol(self&, symbol: semantic_symbol) -> symbol_id
    {
        return semantic_add_symbol(result, symbol);
    }

    record_expression(self&, expression: expr_id, info: semantic_expr_info) -> void
    {
        semantic_record_expression(result, expression, info);
    }

    expression_info(self const&, expression: expr_id) -> semantic_expr_info
    {
        return semantic_expression_info_of(result, expression);
    }

    enter_scope(self&) -> void
    {
        scopes.push_back(semantic_scope{ .symbols = vector<symbol_id>{} });
    }

    leave_scope(self&) -> void
    {
        scopes.pop_back();
    }

    find_in_current_scope(self const&, name: str) -> symbol_id
    {
        if(scopes.size() == 0 as usize) {
            return symbol_id{};
        }
        const ref scope = scopes[scopes.size() - 1];
        for(const ref id : scope.symbols) {
            const ref symbol = result.symbols[id.index()];
            if(symbol.name.as_str() == name) {
                return id;
            }
        }
        return symbol_id{};
    }

    find_visible_variable(self const&, name: str) -> symbol_id
    {
        let scope_index = scopes.size();
        while(scope_index > 0 as usize) {
            scope_index -= 1;
            const ref scope = scopes[scope_index];
            for(const ref id : scope.symbols) {
                const ref symbol = result.symbols[id.index()];
                if(symbol.name.as_str() == name) {
                    return id;
                }
            }
        }
        return symbol_id{};
    }

    bind_to_current_scope(self&, symbol: semantic_symbol) -> symbol_id
    {
        let id = add_symbol(symbol);
        assert(scopes.size() != 0 as usize, "semantic binding requires scope");
        scopes[scopes.size() - 1].symbols.push_back(id);
        return id;
    }
}
