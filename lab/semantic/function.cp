export module semantic.function;

import std;
import source;
import diagnostic;
import parser.ast;
import semantic.type;
import semantic.symbol;
import semantic.state;
import semantic.statement;

return_type_from_syntax(kind: return_type_kind) -> semantic_type_kind
{
    if(kind == return_type_kind::void_type) {
        return semantic_type_kind::void_type;
    }
    return semantic_type_kind::int_type;
}

impl semantic_analyzer {
    collect_function(self&, id: function_id) -> void
    {
        let function = (*parsed).ast.functions[id.value];
        let name = source_text(function.name);
        let existing = semantic_find_function(result, name);
        if(existing.valid()) {
            report(diagnostic_kind::duplicate_function, function.name);
            return;
        }
        let symbol = add_symbol(semantic_symbol{
            .kind = semantic_symbol_kind::function,
            .name = string{name},
            .span = function.name,
            .type = return_type_from_syntax(function.return_type),
            .function = id,
            .parameter_count = function.parameters.size()
        });
        result.functions.push_back(symbol);
    }

    collect_functions(self&) -> void
    {
        let program = (*parsed).ast.programs[(*parsed).root.value];
        let index: usize = 0;
        while(index < program.functions.size()) {
            collect_function(program.functions[index]);
            index += 1;
        }
    }

    check_main_function(self&) -> void
    {
        let main_symbol = semantic_find_function(result, "main");
        if(not main_symbol.valid()) {
            let program = (*parsed).ast.programs[(*parsed).root.value];
            report(diagnostic_kind::missing_main, program.full_span);
        }
    }

    check_function(self&, id: function_id) -> void
    {
        let function = (*parsed).ast.functions[id.value];
        current_function = id;
        current_return_type = return_type_from_syntax(function.return_type);
        current_function_span = function.full_span;
        current_saw_value_return = false;

        enter_scope();
        let parameter_index: usize = 0;
        while(parameter_index < function.parameters.size()) {
            bind_parameter(function.parameters[parameter_index], id);
            parameter_index += 1;
        }

        check_block(function.body, false);
        if(current_return_type == semantic_type_kind::int_type and not current_saw_value_return) {
            report(diagnostic_kind::int_return_value_missing, function.full_span);
        }
        leave_scope();
    }

    check_functions(self&) -> void
    {
        let program = (*parsed).ast.programs[(*parsed).root.value];
        let index: usize = 0;
        while(index < program.functions.size()) {
            check_function(program.functions[index]);
            index += 1;
        }
    }
}
