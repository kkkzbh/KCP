export module ir.format;

import std;
import ir.quad;

export dump_quads(quads: vector<quad> const&) -> string
{
    let output = string{};
    for(const ref item : quads) {
        output.append("(");
        output.append(item.op.as_str());
        output.append(", ");
        output.append(item.arg1.as_str());
        output.append(", ");
        output.append(item.arg2.as_str());
        output.append(", ");
        output.append(item.result.as_str());
        output.append(")\n");
    }
    return output;
}
