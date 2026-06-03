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
    state: usize;
    target_state: usize;
    production: usize;
    pop_count: usize;
    goto_state: usize;
    goto_target: usize;
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
            .state = 0 as usize,
            .target_state = 0 as usize,
            .production = 0 as usize,
            .pop_count = 0 as usize,
            .goto_state = 0 as usize,
            .goto_target = 0 as usize,
            .action = string{},
            .subject = string{},
            .detail = string{},
            .lookahead_kind = token_kind::invalid,
            .lookahead_text = string{}
        };
    }
}
