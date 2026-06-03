export module parser.ast.item;

import std;
import source;
import parser.ast.ids;

export enum return_type_kind : u8 {
    int_type = 0;
    void_type = 1;
}

export variant parameter_data {
    scalar;
    array;
}

export struct parameter_syntax {
    full_span: source_span;
    name: source_span;
    data: parameter_data = parameter_data::scalar;
}

export parameter_is_array(parameter: parameter_syntax const&) -> bool
{
    return match parameter.data {
        .array => true,
        _ => false,
    };
}

export struct function_syntax {
    full_span: source_span;
    return_type: return_type_kind;
    name: source_span;
    parameters: vector<parameter_syntax>;
    body: stmt_id;
}

export struct program_syntax {
    full_span: source_span;
    functions: vector<function_id>;
}
