export module ir.statement;

import std;
import parser.ast;
import ir.state;
import ir.expression;

impl quad_lowerer {
    lower_block(self&, id: stmt_id) -> void
    {
        let syntax = (*parsed).ast.statements[id.value];
        match syntax {
            .block(value) => {
                let index: usize = 0;
                while(index < value.statements.size()) {
                    lower_statement(value.statements[index]);
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
    }

    lower_var_decl(self&, value: var_decl_statement) -> void
    {
        let index: usize = 0;
        while(index < value.declarations.size()) {
            let item = value.declarations[index];
            if(item.initializer.has_value()) {
                let init = lower_expression(*item.initializer);
                emit("assign", init.as_str(), "_", source_text(item.name));
            }
            index += 1;
        }
    }

    lower_if(self&, value: if_statement) -> void
    {
        let then_label = next_label();
        let else_label = next_label();
        let end_label = next_label();
        let condition = lower_expression(value.condition);
        emit("jnz", condition.as_str(), "_", then_label.as_str());
        emit("jmp", "_", "_", else_label.as_str());
        emit("label", "_", "_", then_label.as_str());
        lower_statement(value.then_branch);
        emit("jmp", "_", "_", end_label.as_str());
        emit("label", "_", "_", else_label.as_str());
        if(value.else_branch.has_value()) {
            lower_statement(*value.else_branch);
        }
        emit("label", "_", "_", end_label.as_str());
    }

    lower_while(self&, value: while_statement) -> void
    {
        let begin_label = next_label();
        let end_label = next_label();
        emit("label", "_", "_", begin_label.as_str());
        let condition = lower_expression(value.condition);
        emit("jz", condition.as_str(), "_", end_label.as_str());
        lower_statement(value.body);
        emit("jmp", "_", "_", begin_label.as_str());
        emit("label", "_", "_", end_label.as_str());
    }

    lower_statement(self&, id: stmt_id) -> void
    {
        let syntax = (*parsed).ast.statements[id.value];
        match syntax {
            .block(value) => {
                lower_block(id);
            },
            .var_decl(value) => {
                lower_var_decl(value);
            },
            .assign(value) => {
                let rhs = lower_expression(value.value);
                emit("assign", rhs.as_str(), "_", source_text(value.name));
            },
            .call(value) => {
                lower_call(value.callee, value.arguments, false);
            },
            .if_stmt(value) => {
                lower_if(value);
            },
            .while_stmt(value) => {
                lower_while(value);
            },
            .return_stmt(value) => {
                if(value.value.has_value()) {
                    let item = lower_expression(*value.value);
                    emit("ret", item.as_str(), "_", "_");
                } else {
                    emit("ret", "_", "_", "_");
                }
            },
        };
    }
}
