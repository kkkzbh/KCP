export module ir.result;

import std;
import ir.quad;

export struct quad_emit_result {
    accepted: bool;
    quads: vector<quad>;
    error: string;
}

impl quad_emit_result {
    quad_emit_result()
    {
        return quad_emit_result{
            .accepted = false,
            .quads = vector<quad>{},
            .error = string{}
        };
    }
}
