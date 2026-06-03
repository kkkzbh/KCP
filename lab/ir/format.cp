export module ir.format;

import std;
import ir.quad;

export dump_quads(quads: vector<quad> const&) -> string
{
    let output = string{};
    let index: usize = 0;
    while(index < quads.size()) {
        let item = quads[index];
        output.append("(");
        output.append(item.op.as_str());
        output.append(", ");
        output.append(item.arg1.as_str());
        output.append(", ");
        output.append(item.arg2.as_str());
        output.append(", ");
        output.append(item.result.as_str());
        output.append(")\n");
        index += 1;
    }
    return output;
}
