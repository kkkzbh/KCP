export module parser.op.trace;

import std;

export struct op_trace_step
{
    std::string stack;
    std::string remaining_input;
    std::string action;
};

export [[nodiscard]]
auto format_trace_step(op_trace_step const& value) -> std::string
{
    return std::format("[{}] | [{}] | {}", value.stack, value.remaining_input, value.action);
}
