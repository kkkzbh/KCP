export module ir.program;

import std;
import source;
import parser;
import parser.ast;
import semantic.result;
import ir.state;
import ir.statement;

impl quad_lowerer {
    lower_function(self&, id: function_id) -> void
    {
        const ref function = (*parsed).ast.functions[id.value];
        let name = source_text(function.name);
        emit("func", "_", "_", name);
        lower_statement(function.body);
        emit("end", "_", "_", name);
    }

    lower_program(self&) -> quad_emit_result
    {
        if(not (*semantics).accepted()) {
            let message = string{"quad emission requires accepted semantic result"};
            output.error = move message;
            return output;
        }

        const ref program = (*parsed).ast.programs[(*parsed).root.value];
        for(const ref function : program.functions) {
            lower_function(function);
        }
        output.accepted = true;
        return output;
    }
}

export emit_quads(file: source_file const&, parsed: parse_result const&, semantics: semantic_result const&) -> quad_emit_result
{
    let lowerer = quad_lowerer{file, parsed, semantics};
    return lowerer.lower_program();
}
