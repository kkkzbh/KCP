export module parser.ast.ids;

import std;

export struct expr_id {
    value: usize;
}

export struct stmt_id {
    value: usize;
}

export struct function_id {
    value: usize;
}

export struct program_id {
    value: usize;
}
