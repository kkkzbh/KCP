export module ir.state;

import std;
import source;
import lexer.token;
import parser;
import parser.ast;
import semantic.result;
export import ir.result;
import ir.quad;

export struct loop_labels {
    break_label: string;
    continue_label: string;
}

impl loop_labels {
    loop_labels()
    {
        return loop_labels{ .break_label = string{}, .continue_label = string{} };
    }
}

export struct quad_lowerer {
    file: source_file const*;
    parsed: parse_result const*;
    semantics: semantic_result const*;
    output: quad_emit_result;
    temp_counter: usize;
    label_counter: usize;
    loops: vector<loop_labels>;
}

append_usize(text: string&, value: usize) -> void
{
    if(value >= 10 as usize) {
        append_usize(ref text, value / 10 as usize);
    }
    let ten = 10 as usize;
    let digit = value % ten;
    if(digit == 0 as usize) { text.push_back('0'); return; }
    if(digit == 1 as usize) { text.push_back('1'); return; }
    if(digit == 2 as usize) { text.push_back('2'); return; }
    if(digit == 3 as usize) { text.push_back('3'); return; }
    if(digit == 4 as usize) { text.push_back('4'); return; }
    if(digit == 5 as usize) { text.push_back('5'); return; }
    if(digit == 6 as usize) { text.push_back('6'); return; }
    if(digit == 7 as usize) { text.push_back('7'); return; }
    if(digit == 8 as usize) { text.push_back('8'); return; }
    text.push_back('9');
}

export usize_string(value: usize) -> string
{
    let text = string{};
    append_usize(ref text, value);
    return text;
}

export token_quad_op(kind: token_kind) -> str
{
    if(kind == token_kind::plus) { return "+"; }
    if(kind == token_kind::minus) { return "-"; }
    if(kind == token_kind::star) { return "*"; }
    if(kind == token_kind::slash) { return "/"; }
    if(kind == token_kind::percent) { return "%"; }
    if(kind == token_kind::equal_equal) { return "=="; }
    if(kind == token_kind::bang_equal) { return "!="; }
    if(kind == token_kind::less) { return "<"; }
    if(kind == token_kind::less_equal) { return "<="; }
    if(kind == token_kind::greater) { return ">"; }
    if(kind == token_kind::greater_equal) { return ">="; }
    if(kind == token_kind::amp_amp) { return "&&"; }
    if(kind == token_kind::pipe_pipe) { return "||"; }
    return "?";
}

impl quad_lowerer {
    quad_lowerer(file_value: source_file const&, parsed_value: parse_result const&, semantics_value: semantic_result const&)
    {
        return quad_lowerer{
            .file = &file_value,
            .parsed = &parsed_value,
            .semantics = &semantics_value,
            .output = quad_emit_result{},
            .temp_counter = 0 as usize,
            .label_counter = 0 as usize,
            .loops = vector<loop_labels>{}
        };
    }

    source_text(self const&, span: source_span) -> str
    {
        return (*file).slice(span);
    }

    emit(self&, op: str, arg1: str, arg2: str, result: str) -> void
    {
        output.quads.push_back(quad{
            .op = string{op},
            .arg1 = string{arg1},
            .arg2 = string{arg2},
            .result = string{result}
        });
    }

    next_temp(self&) -> string
    {
        temp_counter += 1;
        let name = string{"t"};
        append_usize(ref name, temp_counter);
        return name;
    }

    next_label(self&) -> string
    {
        label_counter += 1;
        let name = string{"L"};
        append_usize(ref name, label_counter);
        return name;
    }

    push_loop(self&, break_label: string, continue_label: string) -> void
    {
        loops.push_back(loop_labels{ .break_label = move break_label, .continue_label = move continue_label });
    }

    pop_loop(self&) -> void
    {
        loops.pop_back();
    }

    current_loop(self const&) -> loop_labels
    {
        assert(loops.size() != 0 as usize, "loop jump requires active loop");
        return loops[loops.size() - 1];
    }
}
