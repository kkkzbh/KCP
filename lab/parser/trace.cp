export module parser.trace;

import std;
import lexer.token;

export struct parse_options {
    trace_enabled: bool;
}

impl parse_options {
    parse_options()
    {
        return parse_options{ .trace_enabled = false };
    }
}

export struct trace_record {
    step: usize;
    depth: usize;
    action: string;
    subject: string;
    detail: string;
    lookahead_kind: token_kind;
    lookahead_text: string;
}

impl trace_record {
    trace_record()
    {
        return trace_record{
            .step = 0 as usize,
            .depth = 0 as usize,
            .action = string{},
            .subject = string{},
            .detail = string{},
            .lookahead_kind = token_kind::invalid,
            .lookahead_text = string{}
        };
    }
}
