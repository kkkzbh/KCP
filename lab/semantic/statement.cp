export module semantic.statement;

import std;
import source;
import diagnostic;
import parser.ast;
import semantic.type;
import semantic.symbol;
import semantic.state;
import semantic.expression;

array_size_digit(ch: char) -> usize
{
    if(ch == '0') { return 0 as usize; }
    if(ch == '1') { return 1 as usize; }
    if(ch == '2') { return 2 as usize; }
    if(ch == '3') { return 3 as usize; }
    if(ch == '4') { return 4 as usize; }
    if(ch == '5') { return 5 as usize; }
    if(ch == '6') { return 6 as usize; }
    if(ch == '7') { return 7 as usize; }
    if(ch == '8') { return 8 as usize; }
    return 9 as usize;
}

array_size_value(text: str) -> usize
{
    let value: usize = 0;
    let index: usize = 0;
    while(index < text.size()) {
        value = value * 10 as usize + array_size_digit(text[index]);
        index += 1;
    }
    return value;
}

declaration_type(item: var_decl_item) -> semantic_type_kind
{
    if(item.array_size.has_value()) {
        return semantic_type_kind::int_array_type;
    }
    return semantic_type_kind::int_type;
}

parameter_type(parameter: parameter_syntax) -> semantic_type_kind
{
    if(parameter.is_array) {
        return semantic_type_kind::int_array_type;
    }
    return semantic_type_kind::int_type;
}

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
            .type = parameter_type(parameter),
            .function = function,
            .parameter_count = 0 as usize,
            .parameter_types = vector<semantic_parameter_type>{}
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
            .type = declaration_type(item),
            .function = current_function,
            .parameter_count = 0 as usize,
            .parameter_types = vector<semantic_parameter_type>{}
        });
        semantic_record_statement_binding(result, statement, symbol);
    }

    require_int_expression(self&, expression: expr_id, span: source_span) -> semantic_expr_info
    {
        let info = check_expression(expression);
        if(info.type == semantic_type_kind::int_array_type) {
            report(diagnostic_kind::array_value_required, span);
            return semantic_expr_info{ .type = semantic_type_kind::error, .constant = optional<i32>::none };
        }
        return info;
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
            .for_stmt(value) => {},
            .break_stmt(value) => {},
            .continue_stmt(value) => {},
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
                require_int_expression(*item.initializer, (*parsed).ast.expr_span(*item.initializer));
            }
            if(item.array_size.has_value()) {
                let size = array_size_value(source_text(*item.array_size));
                if(size == 0 as usize) {
                    report(diagnostic_kind::invalid_array_size, *item.array_size);
                }
                if(item.array_initializers.size() > size) {
                    report(diagnostic_kind::array_initializer_too_many, item.full_span);
                }
                let init_index: usize = 0;
                while(init_index < item.array_initializers.size()) {
                    let expression = item.array_initializers[init_index];
                    require_int_expression(expression, (*parsed).ast.expr_span(expression));
                    init_index += 1;
                }
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
            check_expression(value.value);
            if(value.index.has_value()) {
                check_expression(*value.index);
            }
            return;
        }

        let symbol = result.symbols[target.index()];
        if(value.index.has_value()) {
            if(symbol.type != semantic_type_kind::int_array_type) {
                report(diagnostic_kind::non_array_index, value.name);
            }
            require_int_expression(*value.index, (*parsed).ast.expr_span(*value.index));
            require_int_expression(value.value, (*parsed).ast.expr_span(value.value));
            return;
        }

        if(symbol.type == semantic_type_kind::int_array_type) {
            report(diagnostic_kind::array_value_required, value.name);
        }
        require_int_expression(value.value, (*parsed).ast.expr_span(value.value));
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

        let info = require_int_expression(*value.value, (*parsed).ast.expr_span(*value.value));
        if(info.type != semantic_type_kind::error) {
            current_saw_value_return = true;
        }
    }

    check_for_statement(self&, value: for_statement) -> void
    {
        enter_scope();
        if(value.initializer.has_value()) {
            check_statement(*value.initializer);
        }
        require_int_expression(value.condition, (*parsed).ast.expr_span(value.condition));
        loop_depth += 1;
        check_statement(value.body);
        loop_depth -= 1;
        if(value.step.has_value()) {
            check_statement(*value.step);
        }
        leave_scope();
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
                require_int_expression(value.condition, (*parsed).ast.expr_span(value.condition));
                check_statement(value.then_branch);
                if(value.else_branch.has_value()) {
                    check_statement(*value.else_branch);
                }
            },
            .while_stmt(value) => {
                require_int_expression(value.condition, (*parsed).ast.expr_span(value.condition));
                loop_depth += 1;
                check_statement(value.body);
                loop_depth -= 1;
            },
            .for_stmt(value) => {
                check_for_statement(value);
            },
            .break_stmt(value) => {
                if(loop_depth == 0 as usize) {
                    report(diagnostic_kind::break_outside_loop, value.full_span);
                }
            },
            .continue_stmt(value) => {
                if(loop_depth == 0 as usize) {
                    report(diagnostic_kind::continue_outside_loop, value.full_span);
                }
            },
            .return_stmt(value) => {
                check_return_statement(value);
            },
        };
    }
}
