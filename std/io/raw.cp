export module std.io.raw;

import std.detail.runtime;
import std.iter;
import std.str;

export struct output_stream {
    fd: i32;
}

export stdout() -> output_stream
{
    return output_stream{ .fd = 1 };
}

export stderr() -> output_stream
{
    return output_stream{ .fd = 2 };
}

export write_char(out: output_stream&, ch: char) -> bool
{
    return cp_io_write_char(out.fd, ch) == 0;
}

export write_str(out: output_stream&, text: str) -> bool
{
    for(let ch : text) {
        if(not write_char(ref out, ch)) {
            return false;
        }
    }
    return true;
}
