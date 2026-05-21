export module parser.program;

import std;
import source;
import lexer.token;
import diagnostic;
import parser.ast;
import parser.state;
import parser.statement;

impl parser {
    parse_program(self&) -> program_id
    {
        trace_enter("Program");
        trace_select("Program", "Program -> FunctionList EOF");
        let first = peek();
        let start = first.span;
        let functions = vector<function_id>{};
        while(not check(token_kind::eof)) {
            if(not starts_function()) {
                trace_error("FunctionList", "expected function or EOF");
                report_current(diagnostic_kind::unexpected_token);
                consume();
                synchronize_function();
                continue;
            }

            trace_select("FunctionList", "FunctionList -> Function FunctionList");
            let function = parse_function();
            if(function.has_value()) {
                functions.push_back(*function);
            } else {
                synchronize_function();
            }
        }
        trace_select("FunctionList", "FunctionList -> epsilon");
        let end = peek();
        let syntax = program_syntax{
            .full_span = combine(start, end.span),
            .functions = move functions
        };
        let result = arena.add_program(syntax);
        trace_exit("Program", true);
        return result;
    }

    parse_function(self&) -> optional<function_id>
    {
        trace_enter("Function");
        trace_select("Function", "Function -> ReturnType identifier \"(\" ParamListOpt \")\" Block");
        let first = peek();
        let start = first.span;
        let return_type = parse_return_type();
        let name = expect_identifier();
        let open = expect(token_kind::l_paren);
        if(not name.has_value() or not open.has_value()) {
            trace_exit("Function", false);
            return optional<function_id>::none;
        }
        let parameters = parse_parameter_list();
        let close = expect(token_kind::r_paren);
        if(not close.has_value()) {
            trace_exit("Function", false);
            return optional<function_id>::none;
        }

        let body = parse_block();
        if(not body.has_value()) {
            trace_exit("Function", false);
            return optional<function_id>::none;
        }
        let syntax = function_syntax{
            .full_span = combine(start, arena.stmt_span(*body)),
            .return_type = return_type,
            .name = (*name).span,
            .parameters = move parameters,
            .body = *body
        };
        let result = arena.add_function(syntax);
        trace_exit("Function", true);
        return optional<function_id>::some(result);
    }

    parse_return_type(self&) -> return_type_kind
    {
        trace_enter("ReturnType");
        if(check(token_kind::kw_int)) {
            trace_select("ReturnType", "ReturnType -> \"int\"");
            let item = consume();
            trace_match(item);
            trace_exit("ReturnType", true);
            return return_type_kind::int_type;
        }
        assert(check(token_kind::kw_void), "parser requires function return type");
        trace_select("ReturnType", "ReturnType -> \"void\"");
        let item = consume();
        trace_match(item);
        trace_exit("ReturnType", true);
        return return_type_kind::void_type;
    }

    parse_parameter_list(self&) -> vector<parameter_syntax>
    {
        trace_enter("ParamListOpt");
        let parameters = vector<parameter_syntax>{};
        if(check(token_kind::r_paren)) {
            trace_select("ParamListOpt", "ParamListOpt -> epsilon");
            trace_exit("ParamListOpt", true);
            return parameters;
        }

        trace_select("ParamListOpt", "ParamListOpt -> ParamList");
        while(true) {
            let parameter = parse_parameter();
            if(not parameter.has_value()) {
                trace_exit("ParamListOpt", false);
                return parameters;
            }
            parameters.push_back(*parameter);
            if(not check(token_kind::comma)) {
                trace_select("ParamListTail", "ParamListTail -> epsilon");
                trace_exit("ParamListOpt", true);
                return parameters;
            }
            trace_select("ParamListTail", "ParamListTail -> \",\" Param ParamListTail");
            let comma = consume();
            trace_match(comma);
        }
        trace_exit("ParamListOpt", true);
        return parameters;
    }

    parse_parameter(self&) -> optional<parameter_syntax>
    {
        trace_enter("Param");
        trace_select("Param", "Param -> \"int\" identifier");
        let start = expect(token_kind::kw_int);
        let name = expect_identifier();
        if(not start.has_value() or not name.has_value()) {
            trace_exit("Param", false);
            return optional<parameter_syntax>::none;
        }
        let syntax = parameter_syntax{
            .full_span = combine((*start).span, (*name).span),
            .name = (*name).span
        };
        trace_exit("Param", true);
        return optional<parameter_syntax>::some(syntax);
    }
}
