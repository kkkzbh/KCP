export module std.io.format;

import std.io.raw;

export variant format_error {
    placeholder_too_few;
    argument_too_many;
    invalid_escape;
    unsupported_specifier;
    output_failed;
}

export variant print_result {
    ok;
    error(format_error);
}

variant scan_result {
    placeholder;
    end;
    error(format_error);
}

export concept display {
    write_to(self const&, out: output_stream&) -> print_result;
}

write_output_char(out: output_stream&, ch: char) -> print_result
{
    if(not write_char(ref out, ch)) {
        return print_result::error(format_error::output_failed);
    }
    return print_result::ok;
}

write_output_str(out: output_stream&, text: str) -> print_result
{
    if(not write_str(ref out, text)) {
        return print_result::error(format_error::output_failed);
    }
    return print_result::ok;
}

scan_to_placeholder(out: output_stream&, fmt: str, cursor: usize&) -> scan_result
{
    while(cursor < fmt.size()) {
        let ch = fmt[cursor];
        let next = cursor + 1;
        if(ch == '{') {
            if(next >= fmt.size()) {
                return scan_result::error(format_error::invalid_escape);
            }
            if(fmt[next] == '}') {
                cursor = cursor + 2;
                return scan_result::placeholder;
            }
            if(fmt[next] == '{') {
                let written = write_output_char(ref out, '{');
                match written {
                    .ok => {},
                    .error(error) => {
                        return scan_result::error(error);
                    },
                };
                cursor = cursor + 2;
            } else if(fmt[next] == ':') {
                return scan_result::error(format_error::unsupported_specifier);
            } else {
                return scan_result::error(format_error::invalid_escape);
            }
        } else if(ch == '}') {
            if(next >= fmt.size()) {
                return scan_result::error(format_error::invalid_escape);
            }
            if(fmt[next] == '}') {
                let written = write_output_char(ref out, '}');
                match written {
                    .ok => {},
                    .error(error) => {
                        return scan_result::error(error);
                    },
                };
                cursor = cursor + 2;
            } else {
                return scan_result::error(format_error::invalid_escape);
            }
        } else {
            let written = write_output_char(ref out, ch);
            match written {
                .ok => {},
                .error(error) => {
                    return scan_result::error(error);
                },
            };
            cursor += 1;
        }
    }
    return scan_result::end;
}

write_usize_digits(out: output_stream&, value: usize) -> print_result
{
    if(value >= 10) {
        let prefix = write_usize_digits(ref out, value / 10);
        match prefix {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }
    return write_output_char(ref out, "0123456789"[value % 10]);
}

impl display for str {
    write_to(self const&, out: output_stream&) -> print_result
    {
        return write_output_str(ref out, self);
    }
}

impl display for char {
    write_to(self const&, out: output_stream&) -> print_result
    {
        return write_output_char(ref out, self);
    }
}

impl display for bool {
    write_to(self const&, out: output_stream&) -> print_result
    {
        if(self) {
            return write_output_str(ref out, "true");
        }
        return write_output_str(ref out, "false");
    }
}

impl display for usize {
    write_to(self const&, out: output_stream&) -> print_result
    {
        return write_usize_digits(ref out, self);
    }
}

impl display for i32 {
    write_to(self const&, out: output_stream&) -> print_result
    {
        let value: i64 = self as i64;
        if(value >= 0) {
            return write_usize_digits(ref out, value as usize);
        }
        let sign = write_output_char(ref out, '-');
        match sign {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
        return write_usize_digits(ref out, (0 - value) as usize);
    }
}

export format_to<T...: display>(out: output_stream&, fmt: str, values: T...) -> print_result
{
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return print_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return print_result::error(error);
            },
        };
        let written = value.write_to(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    return match tail {
        .placeholder => print_result::error(format_error::placeholder_too_few),
        .end => print_result::ok,
        .error(error) => print_result::error(error),
    };
}

export print<T...: display>(fmt: str, values: T...) -> print_result
{
    let out = stdout();
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return print_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return print_result::error(error);
            },
        };
        let written = value.write_to(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    return match tail {
        .placeholder => print_result::error(format_error::placeholder_too_few),
        .end => print_result::ok,
        .error(error) => print_result::error(error),
    };
}

export println<T...: display>(fmt: str, values: T...) -> print_result
{
    let out = stdout();
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return print_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return print_result::error(error);
            },
        };
        let written = value.write_to(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    match tail {
        .placeholder => {
            return print_result::error(format_error::placeholder_too_few);
        },
        .end => {},
        .error(error) => {
            return print_result::error(error);
        },
    };
    return write_output_char(ref out, '\n');
}

export eprint<T...: display>(fmt: str, values: T...) -> print_result
{
    let out = stderr();
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return print_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return print_result::error(error);
            },
        };
        let written = value.write_to(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    return match tail {
        .placeholder => print_result::error(format_error::placeholder_too_few),
        .end => print_result::ok,
        .error(error) => print_result::error(error),
    };
}

export eprintln<T...: display>(fmt: str, values: T...) -> print_result
{
    let out = stderr();
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return print_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return print_result::error(error);
            },
        };
        let written = value.write_to(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return print_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    match tail {
        .placeholder => {
            return print_result::error(format_error::placeholder_too_few);
        },
        .end => {},
        .error(error) => {
            return print_result::error(error);
        },
    };
    return write_output_char(ref out, '\n');
}
