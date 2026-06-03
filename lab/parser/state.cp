export module parser.state;

import std;
import source;
import diagnostic;
import lexer.token;
import parser.ast;
import parser.trace;
import parser.table;

export struct parser_value {
    token: token = token{};
    span: source_span = source_span{};
    program: program_id = program_id{};
    functions: vector<function_id> = vector<function_id>{};
    function: function_id = function_id{};
    return_type: return_type_kind = return_type_kind::int_type;
    parameters: vector<parameter_syntax> = vector<parameter_syntax>{};
    parameter: parameter_syntax = parameter_syntax{};
    statements: vector<stmt_id> = vector<stmt_id>{};
    statement: stmt_id = stmt_id{};
    optional_statement: optional<stmt_id> = optional<stmt_id>::none;
    declarations: vector<var_decl_item> = vector<var_decl_item>{};
    declaration: var_decl_item = var_decl_item{};
    expression: expr_id = expr_id{};
    arguments: vector<expr_id> = vector<expr_id>{};
    else_branch: optional<stmt_id> = optional<stmt_id>::none;
    return_value: optional<expr_id> = optional<expr_id>::none;
}

export struct parser_state_result {
    accepted: bool;
    ast: ast_arena;
    root: program_id;
    diagnostics: vector<diagnostic>;
    trace: vector<trace_record>;
}

export struct parser_state {
    tokens: vector<token>;
    options: parse_options;
    state_stack: vector<usize>;
    value_stack: vector<parser_value>;
    arena: ast_arena;
    root: program_id;
    diagnostics: diagnostic_collector;
    trace_next_step: usize;
    trace: vector<trace_record>;
}

rhs_value(values: vector<parser_value> const&, index: usize) -> parser_value
{
    return values[values.size() - 1 - index];
}

rhs_token(values: vector<parser_value> const&, index: usize) -> token
{
    let value = rhs_value(values, index);
    return value.token;
}

rhs_functions(values: vector<parser_value> const&, index: usize) -> vector<function_id>
{
    let value = rhs_value(values, index);
    return value.functions;
}

rhs_function(values: vector<parser_value> const&, index: usize) -> function_id
{
    let value = rhs_value(values, index);
    return value.function;
}

rhs_return_type(values: vector<parser_value> const&, index: usize) -> return_type_kind
{
    let value = rhs_value(values, index);
    return value.return_type;
}

rhs_parameters(values: vector<parser_value> const&, index: usize) -> vector<parameter_syntax>
{
    let value = rhs_value(values, index);
    return value.parameters;
}

rhs_parameter(values: vector<parser_value> const&, index: usize) -> parameter_syntax
{
    let value = rhs_value(values, index);
    return value.parameter;
}

rhs_statements(values: vector<parser_value> const&, index: usize) -> vector<stmt_id>
{
    let value = rhs_value(values, index);
    return value.statements;
}

rhs_statement(values: vector<parser_value> const&, index: usize) -> stmt_id
{
    let value = rhs_value(values, index);
    return value.statement;
}

rhs_optional_statement(values: vector<parser_value> const&, index: usize) -> optional<stmt_id>
{
    let value = rhs_value(values, index);
    return value.optional_statement;
}

rhs_declarations(values: vector<parser_value> const&, index: usize) -> vector<var_decl_item>
{
    let value = rhs_value(values, index);
    return value.declarations;
}

rhs_declaration(values: vector<parser_value> const&, index: usize) -> var_decl_item
{
    let value = rhs_value(values, index);
    return value.declaration;
}

rhs_expression(values: vector<parser_value> const&, index: usize) -> expr_id
{
    let value = rhs_value(values, index);
    return value.expression;
}

rhs_arguments(values: vector<parser_value> const&, index: usize) -> vector<expr_id>
{
    let value = rhs_value(values, index);
    return value.arguments;
}

rhs_else_branch(values: vector<parser_value> const&, index: usize) -> optional<stmt_id>
{
    let value = rhs_value(values, index);
    return value.else_branch;
}

rhs_return_value(values: vector<parser_value> const&, index: usize) -> optional<expr_id>
{
    let value = rhs_value(values, index);
    return value.return_value;
}

value_with_token(item: token) -> parser_value
{
    return parser_value{ .token = item, .span = item.span };
}

value_with_program(span: source_span, id: program_id) -> parser_value
{
    return parser_value{ .span = span, .program = id };
}

value_with_functions(functions: vector<function_id>) -> parser_value
{
    return parser_value{ .functions = functions };
}

value_with_function(span: source_span, id: function_id) -> parser_value
{
    return parser_value{ .span = span, .function = id };
}

value_with_return_type(span: source_span, kind: return_type_kind) -> parser_value
{
    return parser_value{ .span = span, .return_type = kind };
}

value_with_parameters(parameters: vector<parameter_syntax>) -> parser_value
{
    return parser_value{ .parameters = parameters };
}

value_with_parameter(span: source_span, parameter: parameter_syntax) -> parser_value
{
    return parser_value{ .span = span, .parameter = parameter };
}

value_with_statements(statements: vector<stmt_id>) -> parser_value
{
    return parser_value{ .statements = statements };
}

value_with_statement(span: source_span, id: stmt_id) -> parser_value
{
    return parser_value{ .span = span, .statement = id };
}

value_with_optional_statement(span: source_span, id: optional<stmt_id>) -> parser_value
{
    return parser_value{ .span = span, .optional_statement = id };
}

value_with_declarations(declarations: vector<var_decl_item>) -> parser_value
{
    return parser_value{ .declarations = declarations };
}

value_with_declaration(span: source_span, declaration: var_decl_item) -> parser_value
{
    return parser_value{ .span = span, .declaration = declaration };
}

value_with_expression(span: source_span, id: expr_id) -> parser_value
{
    return parser_value{ .span = span, .expression = id };
}

value_with_arguments(arguments: vector<expr_id>) -> parser_value
{
    return parser_value{ .arguments = arguments };
}

value_with_arguments_span(span: source_span, arguments: vector<expr_id>) -> parser_value
{
    return parser_value{ .span = span, .arguments = arguments };
}

value_with_else_branch(span: source_span, branch: optional<stmt_id>) -> parser_value
{
    return parser_value{ .span = span, .else_branch = branch };
}

value_with_return_value(span: source_span, value: optional<expr_id>) -> parser_value
{
    return parser_value{ .span = span, .return_value = value };
}

impl parser_state {
    parser_state(items: vector<token>, parse_options_value: parse_options)
    {
        assert(items.size() != 0, "LR(1) parser requires eof token");
        assert(items[items.size() - 1].kind == token_kind::eof, "LR(1) parser requires eof token");
        return parser_state{
            .tokens = move items,
            .options = parse_options_value,
            .state_stack = vector<usize>{},
            .value_stack = vector<parser_value>{},
            .arena = ast_arena{},
            .root = program_id{},
            .diagnostics = diagnostic_collector{},
            .trace_next_step = 1 as usize,
            .trace = vector<trace_record>{}
        };
    }

    current_token(self const&, index: usize) -> token
    {
        if(index >= tokens.size()) {
            return tokens[tokens.size() - 1];
        }
        return tokens[index];
    }

    combine(self const&, first: source_span, second: source_span) -> source_span
    {
        return source_span{ .start = first.start, .end = second.end };
    }

    trace_event_at(self&, action: str, subject: str, detail: str, item: token const&) -> void
    {
        if(not options.trace_enabled) {
            return;
        }
        trace.push_back(trace_record{
            .step = trace_next_step,
            .depth = 0 as usize,
            .action = string{action},
            .subject = string{subject},
            .detail = string{detail},
            .lookahead_kind = item.kind,
            .lookahead_text = item.text
        });
        trace_next_step += 1;
    }

    trace_start(self&) -> void
    {
        let item = current_token(0 as usize);
        trace_event_at("enter", "Program", "LR(1) parse", item);
    }

    trace_shift(self&, item: token const&) -> void
    {
        trace_event_at("match", token_kind_name(item.kind), "shift", item);
    }

    trace_reduce(self&, production: usize, lookahead: token const&) -> void
    {
        if(not options.trace_enabled) {
            return;
        }
        trace_event_at("reduce", "LR(1)", "reduce", lookahead);
    }

    trace_accept(self&, lookahead: token const&) -> void
    {
        trace_event_at("accept", "Program", "accepted", lookahead);
    }

    ensure_root(self&) -> void
    {
        if(arena.programs.size() != 0 as usize) {
            return;
        }
        let end = tokens[tokens.size() - 1].span;
        root = arena.add_program(program_syntax{
            .full_span = end,
            .functions = vector<function_id>{}
        });
    }

    finish(self&, accepted: bool) -> parser_state_result
    {
        ensure_root();
        return parser_state_result{
            .accepted = accepted and diagnostics.empty(),
            .ast = move arena,
            .root = root,
            .diagnostics = diagnostics.take(),
            .trace = move trace
        };
    }

    program_span(self const&, functions: vector<function_id> const&) -> source_span
    {
        if(functions.size() == 0 as usize) {
            return tokens[tokens.size() - 1].span;
        }
        let first = arena.function_span(functions[0 as usize]);
        let last = arena.function_span(functions[functions.size() - 1]);
        return combine(first, last);
    }

    add_binary(self&, kind: token_kind, left: expr_id, right: expr_id) -> expr_id
    {
        let syntax = binary_expr{
            .full_span = combine(arena.expr_span(left), arena.expr_span(right)),
            .operator_kind = kind,
            .left = left,
            .right = right
        };
        return arena.add_expr(expr_syntax::binary(syntax));
    }

    add_assign_statement(self&, name: source_span, index: optional<expr_id>, value: expr_id, end: source_span) -> stmt_id
    {
        let syntax = assign_statement{
            .full_span = combine(name, end),
            .name = name,
            .index = index,
            .value = value
        };
        return arena.add_stmt(stmt_syntax::assign(syntax));
    }

    reduce_program(self&, functions: vector<function_id>) -> parser_value
    {
        let full_span = program_span(functions);
        let id = arena.add_program(program_syntax{
            .full_span = full_span,
            .functions = functions
        });
        root = id;
        return value_with_program(full_span, id);
    }

    reduce_value(self&, production: usize, rhs: vector<parser_value> const&) -> parser_value
    {
        if(production == 0 as usize) {
            return rhs_value(rhs, 0 as usize);
        }
        if(production == 1 as usize) {
            return reduce_program(rhs_functions(rhs, 0 as usize));
        }
        if(production == 2 as usize) {
            let functions = rhs_functions(rhs, 0 as usize);
            functions.push_back(rhs_function(rhs, 1 as usize));
            return value_with_functions(functions);
        }
        if(production == 3 as usize) {
            return value_with_functions(vector<function_id>{});
        }
        if(production == 4 as usize) {
            let return_type = rhs_value(rhs, 0 as usize);
            let name = rhs_token(rhs, 1 as usize);
            let parameters = rhs_parameters(rhs, 3 as usize);
            let body = rhs_statement(rhs, 5 as usize);
            let full_span = combine(return_type.span, arena.stmt_span(body));
            let id = arena.add_function(function_syntax{
                .full_span = full_span,
                .return_type = rhs_return_type(rhs, 0 as usize),
                .name = name.span,
                .parameters = parameters,
                .body = body
            });
            return value_with_function(full_span, id);
        }
        if(production == 5 as usize) {
            let item = rhs_token(rhs, 0 as usize);
            return value_with_return_type(item.span, return_type_kind::int_type);
        }
        if(production == 6 as usize) {
            let item = rhs_token(rhs, 0 as usize);
            return value_with_return_type(item.span, return_type_kind::void_type);
        }
        if(production == 7 as usize) {
            return rhs_value(rhs, 0 as usize);
        }
        if(production == 8 as usize) {
            return value_with_parameters(vector<parameter_syntax>{});
        }
        if(production == 9 as usize) {
            let parameters = rhs_parameters(rhs, 0 as usize);
            parameters.push_back(rhs_parameter(rhs, 2 as usize));
            return value_with_parameters(parameters);
        }
        if(production == 10 as usize) {
            let parameters = vector<parameter_syntax>{};
            parameters.push_back(rhs_parameter(rhs, 0 as usize));
            return value_with_parameters(parameters);
        }
        if(production == 11 as usize) {
            let first = rhs_token(rhs, 0 as usize);
            let name = rhs_token(rhs, 1 as usize);
            let parameter = parameter_syntax{
                .full_span = combine(first.span, name.span),
                .name = name.span,
                .is_array = false
            };
            return value_with_parameter(parameter.full_span, parameter);
        }
        if(production == 12 as usize) {
            let open = rhs_token(rhs, 0 as usize);
            let statements = rhs_statements(rhs, 1 as usize);
            let close = rhs_token(rhs, 2 as usize);
            let syntax = block_statement{
                .full_span = combine(open.span, close.span),
                .statements = statements
            };
            let id = arena.add_stmt(stmt_syntax::block(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 13 as usize) {
            let statements = rhs_statements(rhs, 1 as usize);
            statements.insert(0 as usize, rhs_statement(rhs, 0 as usize));
            return value_with_statements(statements);
        }
        if(production == 14 as usize) {
            return value_with_statements(vector<stmt_id>{});
        }
        if(
            production == 15 as usize
            or production == 16 as usize
            or production == 17 as usize
            or production == 18 as usize
            or production == 19 as usize
            or production == 20 as usize
        ) {
            return rhs_value(rhs, 0 as usize);
        }
        if(production == 21 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let declarations = rhs_declarations(rhs, 1 as usize);
            let semicolon = rhs_token(rhs, 2 as usize);
            let syntax = var_decl_statement{
                .full_span = combine(start.span, semicolon.span),
                .declarations = declarations
            };
            let id = arena.add_stmt(stmt_syntax::var_decl(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 22 as usize) {
            let declarations = rhs_declarations(rhs, 0 as usize);
            declarations.push_back(rhs_declaration(rhs, 2 as usize));
            return value_with_declarations(declarations);
        }
        if(production == 23 as usize) {
            let declarations = vector<var_decl_item>{};
            declarations.push_back(rhs_declaration(rhs, 0 as usize));
            return value_with_declarations(declarations);
        }
        if(production == 24 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let item = var_decl_item{
                .full_span = name.span,
                .name = name.span,
                .initializer = optional<expr_id>::none,
                .array_size = optional<source_span>::none,
                .array_initializers = vector<expr_id>{}
            };
            return value_with_declaration(item.full_span, item);
        }
        if(production == 25 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let expression = rhs_expression(rhs, 2 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, arena.expr_span(expression)),
                .name = name.span,
                .initializer = optional<expr_id>::some(expression),
                .array_size = optional<source_span>::none,
                .array_initializers = vector<expr_id>{}
            };
            return value_with_declaration(item.full_span, item);
        }
        if(production == 26 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let value = rhs_expression(rhs, 2 as usize);
            let semicolon = rhs_token(rhs, 3 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::none, value, semicolon.span);
            return value_with_statement(arena.stmt_span(id), id);
        }
        if(production == 27 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let arguments = rhs_arguments(rhs, 2 as usize);
            let semicolon = rhs_token(rhs, 4 as usize);
            let syntax = call_statement{
                .full_span = combine(name.span, semicolon.span),
                .callee = name.span,
                .arguments = arguments
            };
            let id = arena.add_stmt(stmt_syntax::call(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 28 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let condition = rhs_expression(rhs, 2 as usize);
            let then_branch = rhs_statement(rhs, 4 as usize);
            let else_branch = rhs_else_branch(rhs, 5 as usize);
            let end = arena.stmt_span(then_branch);
            if(else_branch.has_value()) {
                end = arena.stmt_span(*else_branch);
            }
            let syntax = if_statement{
                .full_span = combine(start.span, end),
                .condition = condition,
                .then_branch = then_branch,
                .else_branch = else_branch
            };
            let id = arena.add_stmt(stmt_syntax::if_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 29 as usize) {
            let branch = rhs_statement(rhs, 1 as usize);
            return value_with_else_branch(arena.stmt_span(branch), optional<stmt_id>::some(branch));
        }
        if(production == 30 as usize) {
            return value_with_else_branch(source_span{}, optional<stmt_id>::none);
        }
        if(production == 31 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let condition = rhs_expression(rhs, 2 as usize);
            let body = rhs_statement(rhs, 4 as usize);
            let syntax = while_statement{
                .full_span = combine(start.span, arena.stmt_span(body)),
                .condition = condition,
                .body = body
            };
            let id = arena.add_stmt(stmt_syntax::while_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 32 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let value = rhs_return_value(rhs, 1 as usize);
            let semicolon = rhs_token(rhs, 2 as usize);
            let syntax = return_statement{
                .full_span = combine(start.span, semicolon.span),
                .value = value
            };
            let id = arena.add_stmt(stmt_syntax::return_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 33 as usize) {
            let expression = rhs_expression(rhs, 0 as usize);
            return value_with_return_value(arena.expr_span(expression), optional<expr_id>::some(expression));
        }
        if(production == 34 as usize) {
            return value_with_return_value(source_span{}, optional<expr_id>::none);
        }
        if(
            production == 35 as usize
            or production == 37 as usize
            or production == 39 as usize
            or production == 42 as usize
            or production == 47 as usize
            or production == 50 as usize
            or production == 54 as usize
            or production == 57 as usize
            or production == 62 as usize
        ) {
            return rhs_value(rhs, 0 as usize);
        }
        if(
            production == 36 as usize
            or production == 38 as usize
            or production == 40 as usize
            or production == 41 as usize
            or production == 43 as usize
            or production == 44 as usize
            or production == 45 as usize
            or production == 46 as usize
            or production == 48 as usize
            or production == 49 as usize
            or production == 51 as usize
            or production == 52 as usize
            or production == 53 as usize
        ) {
            let left = rhs_expression(rhs, 0 as usize);
            let op = rhs_token(rhs, 1 as usize);
            let right = rhs_expression(rhs, 2 as usize);
            let id = add_binary(op.kind, left, right);
            return value_with_expression(arena.expr_span(id), id);
        }
        if(production == 55 as usize or production == 56 as usize) {
            let op = rhs_token(rhs, 0 as usize);
            let operand = rhs_expression(rhs, 1 as usize);
            let syntax = unary_expr{
                .full_span = combine(op.span, arena.expr_span(operand)),
                .operator_kind = op.kind,
                .operand = operand
            };
            let id = arena.add_expr(expr_syntax::unary(syntax));
            return value_with_expression(syntax.full_span, id);
        }
        if(production == 58 as usize) {
            let item = rhs_token(rhs, 0 as usize);
            let id = arena.add_expr(expr_syntax::integer(integer_expr{ .full_span = item.span }));
            return value_with_expression(item.span, id);
        }
        if(production == 59 as usize) {
            let item = rhs_token(rhs, 0 as usize);
            let id = arena.add_expr(expr_syntax::name(name_expr{ .full_span = item.span, .name = item.span }));
            return value_with_expression(item.span, id);
        }
        if(production == 60 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let arguments = rhs_arguments(rhs, 2 as usize);
            let close = rhs_token(rhs, 3 as usize);
            let syntax = call_expr{
                .full_span = combine(name.span, close.span),
                .callee = name.span,
                .arguments = arguments
            };
            let id = arena.add_expr(expr_syntax::call(syntax));
            return value_with_expression(syntax.full_span, id);
        }
        if(production == 61 as usize) {
            let open = rhs_token(rhs, 0 as usize);
            let expression = rhs_expression(rhs, 1 as usize);
            let close = rhs_token(rhs, 2 as usize);
            let syntax = grouped_expr{
                .full_span = combine(open.span, close.span),
                .expression = expression
            };
            let id = arena.add_expr(expr_syntax::grouped(syntax));
            return value_with_expression(syntax.full_span, id);
        }
        if(production == 63 as usize) {
            return value_with_arguments(vector<expr_id>{});
        }
        if(production == 64 as usize) {
            let arguments = rhs_arguments(rhs, 0 as usize);
            arguments.push_back(rhs_expression(rhs, 2 as usize));
            return value_with_arguments(arguments);
        }
        if(production == 65 as usize) {
            let arguments = vector<expr_id>{};
            arguments.push_back(rhs_expression(rhs, 0 as usize));
            return value_with_arguments(arguments);
        }
        if(production == 66 as usize) {
            let first = rhs_token(rhs, 0 as usize);
            let name = rhs_token(rhs, 1 as usize);
            let close = rhs_token(rhs, 3 as usize);
            let parameter = parameter_syntax{
                .full_span = combine(first.span, close.span),
                .name = name.span,
                .is_array = true
            };
            return value_with_parameter(parameter.full_span, parameter);
        }
        if(production == 67 as usize or production == 68 as usize or production == 69 as usize) {
            return rhs_value(rhs, 0 as usize);
        }
        if(production == 70 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let size = rhs_token(rhs, 2 as usize);
            let close = rhs_token(rhs, 3 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, close.span),
                .name = name.span,
                .initializer = optional<expr_id>::none,
                .array_size = optional<source_span>::some(size.span),
                .array_initializers = vector<expr_id>{}
            };
            return value_with_declaration(item.full_span, item);
        }
        if(production == 71 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let size = rhs_token(rhs, 2 as usize);
            let initializers = rhs_arguments(rhs, 5 as usize);
            let list = rhs_value(rhs, 5 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, list.span),
                .name = name.span,
                .initializer = optional<expr_id>::none,
                .array_size = optional<source_span>::some(size.span),
                .array_initializers = initializers
            };
            return value_with_declaration(item.full_span, item);
        }
        if(production == 72 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let index = rhs_expression(rhs, 2 as usize);
            let value = rhs_expression(rhs, 5 as usize);
            let semicolon = rhs_token(rhs, 6 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::some(index), value, semicolon.span);
            return value_with_statement(arena.stmt_span(id), id);
        }
        if(production == 73 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let index = rhs_expression(rhs, 2 as usize);
            let close = rhs_token(rhs, 3 as usize);
            let syntax = index_expr{
                .full_span = combine(name.span, close.span),
                .array = name.span,
                .index = index
            };
            let id = arena.add_expr(expr_syntax::index(syntax));
            return value_with_expression(syntax.full_span, id);
        }
        if(production == 74 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let initializer = rhs_optional_statement(rhs, 2 as usize);
            let condition = rhs_expression(rhs, 4 as usize);
            let step = rhs_optional_statement(rhs, 6 as usize);
            let body = rhs_statement(rhs, 8 as usize);
            let syntax = for_statement{
                .full_span = combine(start.span, arena.stmt_span(body)),
                .initializer = initializer,
                .condition = condition,
                .step = step,
                .body = body
            };
            let id = arena.add_stmt(stmt_syntax::for_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 75 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let declarations = rhs_declarations(rhs, 1 as usize);
            let last_index = declarations.size() - 1;
            let syntax = var_decl_statement{
                .full_span = combine(start.span, declarations[last_index].full_span),
                .declarations = declarations
            };
            let id = arena.add_stmt(stmt_syntax::var_decl(syntax));
            return value_with_optional_statement(syntax.full_span, optional<stmt_id>::some(id));
        }
        if(production == 76 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let value = rhs_expression(rhs, 2 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::none, value, arena.expr_span(value));
            return value_with_optional_statement(arena.stmt_span(id), optional<stmt_id>::some(id));
        }
        if(production == 77 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let index = rhs_expression(rhs, 2 as usize);
            let value = rhs_expression(rhs, 5 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::some(index), value, arena.expr_span(value));
            return value_with_optional_statement(arena.stmt_span(id), optional<stmt_id>::some(id));
        }
        if(production == 78 as usize or production == 81 as usize) {
            return value_with_optional_statement(source_span{}, optional<stmt_id>::none);
        }
        if(production == 79 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let value = rhs_expression(rhs, 2 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::none, value, arena.expr_span(value));
            return value_with_optional_statement(arena.stmt_span(id), optional<stmt_id>::some(id));
        }
        if(production == 80 as usize) {
            let name = rhs_token(rhs, 0 as usize);
            let index = rhs_expression(rhs, 2 as usize);
            let value = rhs_expression(rhs, 5 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::some(index), value, arena.expr_span(value));
            return value_with_optional_statement(arena.stmt_span(id), optional<stmt_id>::some(id));
        }
        if(production == 82 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let semicolon = rhs_token(rhs, 1 as usize);
            let syntax = break_statement{ .full_span = combine(start.span, semicolon.span) };
            let id = arena.add_stmt(stmt_syntax::break_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 83 as usize) {
            let start = rhs_token(rhs, 0 as usize);
            let semicolon = rhs_token(rhs, 1 as usize);
            let syntax = continue_statement{ .full_span = combine(start.span, semicolon.span) };
            let id = arena.add_stmt(stmt_syntax::continue_stmt(syntax));
            return value_with_statement(syntax.full_span, id);
        }
        if(production == 84 as usize) {
            let open = rhs_token(rhs, 0 as usize);
            let values = rhs_arguments(rhs, 1 as usize);
            let close = rhs_token(rhs, 2 as usize);
            return value_with_arguments_span(combine(open.span, close.span), values);
        }
        if(production == 85 as usize) {
            return rhs_value(rhs, 0 as usize);
        }
        if(production == 86 as usize) {
            return value_with_arguments(vector<expr_id>{});
        }
        if(production == 87 as usize) {
            let values = rhs_arguments(rhs, 0 as usize);
            values.push_back(rhs_expression(rhs, 2 as usize));
            return value_with_arguments(values);
        }
        if(production == 88 as usize) {
            let values = vector<expr_id>{};
            values.push_back(rhs_expression(rhs, 0 as usize));
            return value_with_arguments(values);
        }

        return parser_value{};
    }

    reduce(self&, action: parser_action, tables: parser_tables const&) -> bool
    {
        let production = &tables.grammar.productions[action.production];
        let rhs = vector<parser_value>{};
        let count: usize = 0;
        while(count < (*production).rhs.size()) {
            assert(state_stack.size() != 0 as usize, "LR(1) state stack underflow");
            assert(value_stack.size() != 0 as usize, "LR(1) value stack underflow");
            state_stack.pop_back();
            rhs.push_back(value_stack.back());
            value_stack.pop_back();
            count += 1;
        }

        let goto_state = state_stack.back();
        let target = tables.goto_table.find(goto_key{ .state = goto_state, .nonterminal = (*production).lhs });
        if(not target.has_value()) {
            diagnostics.report(diagnostic_kind::unexpected_token, tokens[tokens.size() - 1].span);
            return false;
        }

        let value = reduce_value(action.production, rhs);
        value_stack.push_back(value);
        state_stack.push_back(*target);
        return true;
    }

    run(self&, tables: parser_tables const&) -> parser_state_result
    {
        state_stack.push_back(0 as usize);
        trace_start();

        let input_index: usize = 0;
        while(true) {
            let lookahead = current_token(input_index);
            let state = state_stack.back();
            let action = tables.action_table.find(action_key{ .state = state, .terminal = lookahead.kind });
            if(not action.has_value()) {
                diagnostics.report(diagnostic_kind::unexpected_token, lookahead.span);
                return finish(false);
            }

            let current_action = *action;
            if(current_action.kind == parser_action_kind::shift) {
                value_stack.push_back(value_with_token(lookahead));
                state_stack.push_back(current_action.target_state);
                trace_shift(lookahead);
                input_index += 1;
                continue;
            }
            if(current_action.kind == parser_action_kind::reduce) {
                trace_reduce(current_action.production, lookahead);
                if(not reduce(current_action, tables)) {
                    return finish(false);
                }
                continue;
            }
            if(current_action.kind == parser_action_kind::accept) {
                trace_accept(lookahead);
                if(tables.conflicts.size() != 0 as usize) {
                    diagnostics.report(diagnostic_kind::unexpected_token, lookahead.span);
                    return finish(false);
                }
                return finish(true);
            }

            diagnostics.report(diagnostic_kind::unexpected_token, lookahead.span);
            return finish(false);
        }
        return finish(false);
    }
}
