export module std.io.format;

import std.io.raw;
export import std.text.string;

export variant format_error {
    placeholder_too_few;
    argument_too_many;
    invalid_escape;
    unsupported_specifier;
    output_failed;
}

export variant display_result {
    ok;
    error(format_error);
}

export variant format_result {
    ok(string);
    error(format_error);
}

export variant formatter {
    stream(output_stream&);
    buffer(string&);
}

variant scan_result {
    placeholder;
    end;
    error(format_error);
}

export concept display {
    display(self const&, out: formatter&) -> display_result;
}

impl formatter {
    write_char(self&, ch: char) -> display_result
    {
        match self {
            .stream(out) => {
                if(not write_char(ref out, ch)) {
                    return display_result::error(format_error::output_failed);
                }
                return display_result::ok;
            },
            .buffer(text) => {
                text.push_back(ch);
                return display_result::ok;
            },
        };
    }

    write_str(self&, text: str) -> display_result
    {
        match self {
            .stream(out) => {
                if(not write_str(ref out, text)) {
                    return display_result::error(format_error::output_failed);
                }
                return display_result::ok;
            },
            .buffer(storage) => {
                storage.append(text);
                return display_result::ok;
            },
        };
    }

    format<T...: display>(self&, fmt: str, values: T...) -> display_result
    {
        let cursor: usize = 0;
        template for(let value : values...) {
            let scan = scan_to_placeholder(ref self, fmt, ref cursor);
            match scan {
                .placeholder => {},
                .end => {
                    return display_result::error(format_error::argument_too_many);
                },
                .error(error) => {
                    return display_result::error(error);
                },
            };
            let written = value.display(ref self);
            match written {
                .ok => {},
                .error(error) => {
                    return display_result::error(error);
                },
            };
        }

        let tail = scan_to_placeholder(ref self, fmt, ref cursor);
        return match tail {
            .placeholder => display_result::error(format_error::placeholder_too_few),
            .end => display_result::ok,
            .error(error) => display_result::error(error),
        };
    }
}

scan_to_placeholder(out: formatter&, fmt: str, cursor: usize&) -> scan_result
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
                let written = out.write_char('{');
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
                let written = out.write_char('}');
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
            let written = out.write_char(ch);
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

write_u64_digits(out: formatter&, value: u64) -> display_result
{
    if(value >= 10) {
        let prefix = write_u64_digits(ref out, value / 10);
        match prefix {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }
    return out.write_char("0123456789"[(value % 10) as usize]);
}

write_i64_digits_negative(out: formatter&, value: i64) -> display_result
{
    if(value <= -10) {
        let prefix = write_i64_digits_negative(ref out, value / 10);
        match prefix {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }
    return out.write_char("0123456789"[(0 - (value % 10)) as usize]);
}

write_i64(out: formatter&, value: i64) -> display_result
{
    if(value >= 0) {
        return write_u64_digits(ref out, value as u64);
    }
    let sign = out.write_char('-');
    match sign {
        .ok => {},
        .error(error) => {
            return display_result::error(error);
        },
    };
    return write_i64_digits_negative(ref out, value);
}

write_f64(out: formatter&, value: f64) -> display_result
{
    let positive = value;
    if(positive < 0.0) {
        let sign = out.write_char('-');
        match sign {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
        positive = 0.0 - positive;
    }

    let whole = positive as u64;
    let whole_written = write_u64_digits(ref out, whole);
    match whole_written {
        .ok => {},
        .error(error) => {
            return display_result::error(error);
        },
    };

    let dot = out.write_char('.');
    match dot {
        .ok => {},
        .error(error) => {
            return display_result::error(error);
        },
    };

    let fraction = positive - (whole as f64);
    let index: usize = 0;
    while(index < 6) {
        fraction = fraction * 10.0;
        let digit = fraction as u64;
        let written = out.write_char("0123456789"[digit as usize]);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
        fraction = fraction - (digit as f64);
        index += 1;
    }
    return display_result::ok;
}

impl display for str {
    display(self const&, out: formatter&) -> display_result
    {
        return out.write_str(self);
    }
}

impl display for string {
    display(self const&, out: formatter&) -> display_result
    {
        return out.write_str(self.as_str());
    }
}

impl display for char {
    display(self const&, out: formatter&) -> display_result
    {
        return out.write_char(self);
    }
}

impl display for bool {
    display(self const&, out: formatter&) -> display_result
    {
        if(self) {
            return out.write_str("true");
        }
        return out.write_str("false");
    }
}

impl display for i8 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_i64(ref out, self as i64);
    }
}

impl display for i16 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_i64(ref out, self as i64);
    }
}

impl display for i32 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_i64(ref out, self as i64);
    }
}

impl display for i64 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_i64(ref out, self);
    }
}

impl display for isize {
    display(self const&, out: formatter&) -> display_result
    {
        return write_i64(ref out, self as i64);
    }
}

impl display for u8 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_u64_digits(ref out, self as u64);
    }
}

impl display for u16 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_u64_digits(ref out, self as u64);
    }
}

impl display for u32 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_u64_digits(ref out, self as u64);
    }
}

impl display for u64 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_u64_digits(ref out, self);
    }
}

impl display for usize {
    display(self const&, out: formatter&) -> display_result
    {
        return write_u64_digits(ref out, self as u64);
    }
}

impl display for f32 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_f64(ref out, self as f64);
    }
}

impl display for f64 {
    display(self const&, out: formatter&) -> display_result
    {
        return write_f64(ref out, self);
    }
}

export format<T...: display>(fmt: str, values: T...) -> format_result
{
    let text = string{};
    let out = formatter::buffer(ref text);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref out, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return format_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return format_result::error(error);
            },
        };
        let written = value.display(ref out);
        match written {
            .ok => {},
            .error(error) => {
                return format_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref out, fmt, ref cursor);
    match tail {
        .placeholder => {
            return format_result::error(format_error::placeholder_too_few);
        },
        .end => {},
        .error(error) => {
            return format_result::error(error);
        },
    };
    return format_result::ok(move text);
}

export format_to<T...: display>(out: output_stream&, fmt: str, values: T...) -> display_result
{
    let writer = formatter::stream(ref out);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref writer, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return display_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return display_result::error(error);
            },
        };
        let written = value.display(ref writer);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref writer, fmt, ref cursor);
    return match tail {
        .placeholder => display_result::error(format_error::placeholder_too_few),
        .end => display_result::ok,
        .error(error) => display_result::error(error),
    };
}

export print<T...: display>(fmt: str, values: T...) -> display_result
{
    let out = stdout();
    let writer = formatter::stream(ref out);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref writer, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return display_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return display_result::error(error);
            },
        };
        let written = value.display(ref writer);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref writer, fmt, ref cursor);
    return match tail {
        .placeholder => display_result::error(format_error::placeholder_too_few),
        .end => display_result::ok,
        .error(error) => display_result::error(error),
    };
}

export println<T...: display>(fmt: str, values: T...) -> display_result
{
    let out = stdout();
    let writer = formatter::stream(ref out);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref writer, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return display_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return display_result::error(error);
            },
        };
        let written = value.display(ref writer);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref writer, fmt, ref cursor);
    match tail {
        .placeholder => {
            return display_result::error(format_error::placeholder_too_few);
        },
        .end => {},
        .error(error) => {
            return display_result::error(error);
        },
    };
    return writer.write_char('\n');
}

export eprint<T...: display>(fmt: str, values: T...) -> display_result
{
    let out = stderr();
    let writer = formatter::stream(ref out);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref writer, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return display_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return display_result::error(error);
            },
        };
        let written = value.display(ref writer);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref writer, fmt, ref cursor);
    return match tail {
        .placeholder => display_result::error(format_error::placeholder_too_few),
        .end => display_result::ok,
        .error(error) => display_result::error(error),
    };
}

export eprintln<T...: display>(fmt: str, values: T...) -> display_result
{
    let out = stderr();
    let writer = formatter::stream(ref out);
    let cursor: usize = 0;
    template for(let value : values...) {
        let scan = scan_to_placeholder(ref writer, fmt, ref cursor);
        match scan {
            .placeholder => {},
            .end => {
                return display_result::error(format_error::argument_too_many);
            },
            .error(error) => {
                return display_result::error(error);
            },
        };
        let written = value.display(ref writer);
        match written {
            .ok => {},
            .error(error) => {
                return display_result::error(error);
            },
        };
    }

    let tail = scan_to_placeholder(ref writer, fmt, ref cursor);
    match tail {
        .placeholder => {
            return display_result::error(format_error::placeholder_too_few);
        },
        .end => {},
        .error(error) => {
            return display_result::error(error);
        },
    };
    return writer.write_char('\n');
}
