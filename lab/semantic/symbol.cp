export module semantic.symbol;

import std;
import source;
import parser.ast;
import semantic.type;

export enum semantic_symbol_kind : u8 {
    function = 0;
    parameter = 1;
    local = 2;
}

export struct symbol_id {
    value: usize;
}

impl symbol_id {
    symbol_id()
    {
        return symbol_id{ .value = 0 as usize };
    }

    valid(self const&) -> bool
    {
        return value != 0 as usize;
    }

    index(self const&) -> usize
    {
        assert(valid(), "invalid symbol id");
        return value - 1;
    }
}

export struct semantic_symbol {
    kind: semantic_symbol_kind;
    name: string;
    span: source_span;
    type: semantic_type_kind;
    function: function_id;
    parameter_count: usize;
}

impl semantic_symbol {
    semantic_symbol()
    {
        return semantic_symbol{
            .kind = semantic_symbol_kind::local,
            .name = string{},
            .span = source_span{},
            .type = semantic_type_kind::error,
            .function = function_id{},
            .parameter_count = 0 as usize
        };
    }
}
