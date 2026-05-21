export module parser.state;

import std;
import source;
import diagnostic;
import lexer.token;
import parser.ast;
import parser.trace;

export struct parser {
    tokens: vector<token>;
    index: usize;
    arena: ast_arena;
    diagnostics: diagnostic_collector;
    trace_enabled: bool;
    trace_depth: usize;
    trace_next_step: usize;
    trace: vector<trace_record>;
}

impl parser {
    parser(items: vector<token>)
    {
        return parser{move items, parse_options{}};
    }

    parser(items: vector<token>, options: parse_options)
    {
        assert(items.size() != 0, "parser requires eof token");
        assert(items[items.size() - 1].kind == token_kind::eof, "parser requires eof token");
        return parser{
            .tokens = move items,
            .index = 0 as usize,
            .arena = ast_arena{},
            .diagnostics = diagnostic_collector{},
            .trace_enabled = options.trace_enabled,
            .trace_depth = 0 as usize,
            .trace_next_step = 1 as usize,
            .trace = vector<trace_record>{}
        };
    }

    peek(self const&, distance: usize = 0 as usize) -> token
    {
        let cursor = index + distance;
        if(cursor >= tokens.size()) {
            return tokens[tokens.size() - 1];
        }
        return tokens[cursor];
    }

    check(self const&, kind: token_kind) -> bool
    {
        let current = peek();
        return current.kind == kind;
    }

    check_any(self const&, first: token_kind, second: token_kind) -> bool
    {
        return check(first) or check(second);
    }

    consume(self&) -> token
    {
        let current = peek();
        if(index < tokens.size()) {
            index += 1;
        }
        return current;
    }

    trace_event_at(self&, action: str, subject: str, detail: str, kind: token_kind, text: str) -> void
    {
        if(not trace_enabled) {
            return;
        }

        trace.push_back(trace_record{
            .step = trace_next_step,
            .depth = trace_depth,
            .action = string{action},
            .subject = string{subject},
            .detail = string{detail},
            .lookahead_kind = kind,
            .lookahead_text = string{text}
        });
        trace_next_step += 1;
    }

    trace_event(self&, action: str, subject: str, detail: str) -> void
    {
        let current = peek();
        trace_event_at(action, subject, detail, current.kind, current.text.as_str());
    }

    trace_enter(self&, subject: str) -> void
    {
        if(not trace_enabled) {
            return;
        }

        trace_event("enter", subject, subject);
        trace_depth += 1;
    }

    trace_select(self&, subject: str, detail: str) -> void
    {
        trace_event("select", subject, detail);
    }

    trace_match(self&, item: token const&) -> void
    {
        if(not trace_enabled) {
            return;
        }

        let detail = string{"match "};
        detail.append(token_kind_name(item.kind));
        trace_event_at("match", token_kind_name(item.kind), detail.as_str(), item.kind, item.text.as_str());
    }

    trace_exit(self&, subject: str, accepted: bool) -> void
    {
        if(not trace_enabled) {
            return;
        }

        if(trace_depth > 0 as usize) {
            trace_depth -= 1;
        }
        if(accepted) {
            trace_event("exit", subject, "ok");
        } else {
            trace_event("exit", subject, "fail");
        }
    }

    trace_error(self&, subject: str, detail: str) -> void
    {
        trace_event("error", subject, detail);
    }

    trace_accept(self&) -> void
    {
        trace_event("accept", "Program", "accepted");
    }

    report_current(self&, kind: diagnostic_kind) -> void
    {
        let current = peek();
        diagnostics.report(kind, current.span);
    }

    expect(self&, kind: token_kind) -> optional<token>
    {
        if(not check(kind)) {
            trace_error("expected token", token_kind_name(kind));
            report_current(diagnostic_kind::expected_token);
            return optional<token>::none;
        }

        let current = consume();
        trace_match(current);
        return optional<token>::some(current);
    }

    expect_identifier(self&) -> optional<token>
    {
        if(not check(token_kind::identifier)) {
            trace_error("expected identifier", "identifier");
            report_current(diagnostic_kind::expected_identifier);
            return optional<token>::none;
        }

        let current = consume();
        trace_match(current);
        return optional<token>::some(current);
    }

    combine(self const&, first: source_span, second: source_span) -> source_span
    {
        return source_span{ .start = first.start, .end = second.end };
    }

    starts_function(self const&) -> bool
    {
        return check(token_kind::kw_int) or check(token_kind::kw_void);
    }

    synchronize_statement(self&) -> void
    {
        while(not check(token_kind::eof)) {
            if(check(token_kind::semicolon)) {
                consume();
                return;
            }
            if(check(token_kind::r_brace)) {
                return;
            }
            consume();
        }
    }

    synchronize_function(self&) -> void
    {
        while(not check(token_kind::eof)) {
            if(starts_function()) {
                return;
            }
            consume();
        }
    }
}
