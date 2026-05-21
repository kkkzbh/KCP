export module ir.quad;

import std;

export struct quad {
    op: string;
    arg1: string;
    arg2: string;
    result: string;
}

impl quad {
    quad()
    {
        return quad{
            .op = string{},
            .arg1 = string{},
            .arg2 = string{},
            .result = string{}
        };
    }
}
