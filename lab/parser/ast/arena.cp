export module parser.ast.arena;

import std;
import source;
import parser.ast.ids;
import parser.ast.expr;
import parser.ast.stmt;
import parser.ast.item;

export struct ast_arena {
    expressions: vector<expr_syntax>;
    statements: vector<stmt_syntax>;
    functions: vector<function_syntax>;
    programs: vector<program_syntax>;
}

impl ast_arena {
    add_expr(self&, value: expr_syntax) -> expr_id
    {
        let id = expr_id{ .value = expressions.size() };
        expressions.push_back(value);
        return id;
    }

    add_stmt(self&, value: stmt_syntax) -> stmt_id
    {
        let id = stmt_id{ .value = statements.size() };
        statements.push_back(value);
        return id;
    }

    add_function(self&, value: function_syntax) -> function_id
    {
        let id = function_id{ .value = functions.size() };
        functions.push_back(value);
        return id;
    }

    add_program(self&, value: program_syntax) -> program_id
    {
        let id = program_id{ .value = programs.size() };
        programs.push_back(value);
        return id;
    }

    expr_span(self const&, id: expr_id) -> source_span
    {
        return expr_syntax_span(expressions[id.value]);
    }

    stmt_span(self const&, id: stmt_id) -> source_span
    {
        return stmt_syntax_span(statements[id.value]);
    }

    function_span(self const&, id: function_id) -> source_span
    {
        return functions[id.value].full_span;
    }
}
