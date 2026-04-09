export module parser.trace;

import std;
import lexer.source;
import lexer.token;

export enum class trace_event_kind
{
    enter,
    match,
    fail,
    exit,
};

export struct trace_event
{
    trace_event_kind kind{};
    std::string label;
    token current{};
};

export [[nodiscard]] auto to_string(trace_event_kind kind) -> std::string_view;
export [[nodiscard]] auto format_trace_event(source_manager const& sources, trace_event const& value)
    -> std::string;

auto to_string(trace_event_kind kind) -> std::string_view
{
    using enum trace_event_kind;

    switch(kind) {
    case enter: return "enter";
    case match: return "match";
    case fail: return "fail";
    case exit: return "exit";
    }

    return "unknown";
}

auto format_trace_event(source_manager const& sources, trace_event const& value) -> std::string
{
    auto const lexeme = value.current.source_span.end > value.current.source_span.start
        ? std::string(sources.slice(value.current.source_span))
        : std::string{};

    return std::format(
        "{}\t{}\t{}\t{}",
        to_string(value.kind),
        value.label,
        to_string(value.current.kind),
        lexeme);
}
