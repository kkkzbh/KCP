export module lexer;

import std;
export import source;
import preprocessor;
export import diagnostic;
import lexer.state;
export import lexer.token;

export struct lex_result {
    source: source_file;
    tokens: vector<token>;
    diagnostics: vector<diagnostic>;
}

merge_diagnostics(left: vector<diagnostic> const&, right: vector<diagnostic> const&) -> vector<diagnostic>
{
    let result = vector<diagnostic>{};
    let left_index: usize = 0;
    let right_index: usize = 0;
    while(left_index < left.size() or right_index < right.size()) {
        if(right_index >= right.size()) {
            result.push_back(left[left_index]);
            ++left_index;
        } else if(left_index >= left.size()) {
            result.push_back(right[right_index]);
            ++right_index;
        } else if(left[left_index].primary_span.start <= right[right_index].primary_span.start) {
            result.push_back(left[left_index]);
            ++left_index;
        } else {
            result.push_back(right[right_index]);
            ++right_index;
        }
    }
    return move result;
}

export lex(input: str) -> lex_result
{
    let file = source_file{"<memory>", input};
    let preprocessed = preprocess(file);
    let state = lexer{preprocessed.normalized_text.as_str()};
    let tokens = state.tokenize_all();
    let lexer_diagnostics = state.take_diagnostics();
    let diagnostics = merge_diagnostics(lexer_diagnostics, preprocessed.diagnostics);
    return lex_result{ .source = move file, .tokens = move tokens, .diagnostics = move diagnostics };
}
