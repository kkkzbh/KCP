export module semantic.program;

import std;
import source;
import parser;
import parser.ast;
import semantic.state;
import semantic.function;

impl semantic_analyzer {
    analyze(self&) -> semantic_result
    {
        assert((*parsed).accepted, "semantic analysis requires accepted parser result");
        collect_functions();
        check_main_function();
        check_functions();
        let collected = diagnostics.take();
        result.diagnostics = move collected;
        return result;
    }
}

export analyze_semantics(file: source_file const&, parsed: parse_result const&) -> semantic_result
{
    let analyzer = semantic_analyzer{file, parsed};
    return analyzer.analyze();
}
