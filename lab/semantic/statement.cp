export module semantic.statement;

import std;
import source;
import diagnostic;
import parser.ast;
import semantic.type;
import semantic.symbol;
import semantic.state;
import semantic.expression;

impl semantic_analyzer {
    bind_parameter(self&, parameter: parameter_syntax, function: function_id) -> void
    {
        let name = source_text(parameter.name);
        let existing = find_in_current_scope(name);
        if(existing.valid()) {
            report(diagnostic_kind::duplicate_parameter, parameter.name);
            return;
        }
        bind_to_current_scope(semantic_symbol{
            .kind = semantic_symbol_kind::parameter,
            .name = string{name},
            .span = parameter.name,
            .type = semantic_type_kind::int_type,
            .function = function,
            .parameter_count = 0 as usize
        });
    }

    bind_local(self&, statement: stmt_id, item: var_decl_item) -> void
    {
        let name = source_text(item.name);
        let existing = find_in_current_scope(name);
        if(existing.valid()) {
            report(diagnostic_kind::duplicate_local, item.name);
            return;
        }
        let symbol = bind_to_current_scope(semantic_symbol{
            .kind = semantic_symbol_kind::local,
            .name = string{name},
            .span = item.name,
            .type = semantic_type_kind::int_type,
            .function = current_function,
            .parameter_count = 0 as usize
        });
        semantic_record_statement_binding(result, statement, symbol);
    }

    check_block(self&, id: stmt_id, scoped: bool) -> void
    {
        if(scoped) {
            enter_scope();
        }

        let syntax = (*parsed).ast.statements[id.value];
        match syntax {
            .block(value) => {
                let index: usize = 0;
                while(index < value.statements.size()) {
                    check_statement(value.statements[index]);
                    index += 1;
                }
            },
            .var_decl(value) => {},
            .assign(value) => {},
            .call(value) => {},
            .if_stmt(value) => {},
            .while_stmt(value) => {},
            .return_stmt(value) => {},
        };

        if(scoped) {
            leave_scope();
        }
    }

    check_var_decl(self&, id: stmt_id, value: var_decl_statement) -> void
    {
        let index: usize = 0;
        while(index < value.declarations.size()) {
            let item = value.declarations[index];
            if(item.initializer.has_value()) {
                check_expression(*item.initializer);
            }
            bind_local(id, item);
            index += 1;
        }
    }

    check_assign(self&, value: assign_statement) -> void
    {
        let target = find_visible_variable(source_text(value.name));
        if(not target.valid()) {
            report(diagnostic_kind::undeclared_variable, value.name);
        }
        check_expression(value.value);
    }

    check_call_statement(self&, value: call_statement) -> void
    {
        check_call_expression(value.callee, value.arguments, value.full_span, false);
    }

    check_return_statement(self&, value: return_statement) -> void
    {
        if(current_return_type == semantic_type_kind::void_type) {
            if(value.value.has_value()) {
                check_expression(*value.value);
                report(diagnostic_kind::void_return_value, value.full_span);
            }
            return;
        }

        if(not value.value.has_value()) {
            report(diagnostic_kind::int_return_value_missing, value.full_span);
            return;
        }

        let info = check_expression(*value.value);
        if(info.type != semantic_type_kind::error) {
            current_saw_value_return = true;
        }
    }

    check_statement(self&, id: stmt_id) -> void
    {
        let syntax = (*parsed).ast.statements[id.value];
        match syntax {
            .block(value) => {
                check_block(id, true);
            },
            .var_decl(value) => {
                check_var_decl(id, value);
            },
            .assign(value) => {
                check_assign(value);
            },
            .call(value) => {
                check_call_statement(value);
            },
            .if_stmt(value) => {
                check_expression(value.condition);
                check_statement(value.then_branch);
                if(value.else_branch.has_value()) {
                    check_statement(*value.else_branch);
                }
            },
            .while_stmt(value) => {
                check_expression(value.condition);
                check_statement(value.body);
            },
            .return_stmt(value) => {
                check_return_statement(value);
            },
        };
    }
}
