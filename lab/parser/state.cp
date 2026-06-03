export module parser.state;

import std;
import source;
import diagnostic;
import lexer.token;
import parser.ast;
import parser.trace;
import parser.table;

export variant value_data {
    empty;
    token(token);
    program(source_span, program_id);
    functions(vector<function_id>);
    function(source_span, function_id);
    return_type(source_span, return_type_kind);
    parameters(vector<parameter_syntax>);
    parameter(source_span, parameter_syntax);
    statements(vector<stmt_id>);
    statement(source_span, stmt_id);
    optional_statement(source_span, optional<stmt_id>);
    declarations(vector<var_decl_item>);
    declaration(source_span, var_decl_item);
    expression(source_span, expr_id);
    arguments(source_span, vector<expr_id>);
    else_branch(source_span, optional<stmt_id>);
    return_value(source_span, optional<expr_id>);
}

export struct parser_value {
    data: value_data = value_data::empty;
}

impl parser_value {
    parser_value(data: value_data)
    {
        return parser_value{ .data = data };
    }
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

struct rhs_view {
    values: vector<parser_value> const&;
}

impl rhs_view {
    rhs_view(source: vector<parser_value> const&)
    {
        return rhs_view{ .values = const ref source };
    }

    value(self const&, index: usize) -> parser_value
    {
        return values[values.size() - 1 - index];
    }

    token(self const&, index: usize) -> token
    {
        return match value(index).data {
            .token(item) => item,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    functions(self const&, index: usize) -> vector<function_id>
    {
        return match value(index).data {
            .functions(items) => items,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    function(self const&, index: usize) -> function_id
    {
        return match value(index).data {
            .function(span, id) => id,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    return_type(self const&, index: usize) -> return_type_kind
    {
        return match value(index).data {
            .return_type(span, kind) => kind,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    parameters(self const&, index: usize) -> vector<parameter_syntax>
    {
        return match value(index).data {
            .parameters(items) => items,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    parameter(self const&, index: usize) -> parameter_syntax
    {
        return match value(index).data {
            .parameter(span, item) => item,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    statements(self const&, index: usize) -> vector<stmt_id>
    {
        return match value(index).data {
            .statements(items) => items,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    statement(self const&, index: usize) -> stmt_id
    {
        return match value(index).data {
            .statement(span, id) => id,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    optional_statement(self const&, index: usize) -> optional<stmt_id>
    {
        return match value(index).data {
            .optional_statement(span, id) => id,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    declarations(self const&, index: usize) -> vector<var_decl_item>
    {
        return match value(index).data {
            .declarations(items) => items,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    declaration(self const&, index: usize) -> var_decl_item
    {
        return match value(index).data {
            .declaration(span, item) => item,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    expression(self const&, index: usize) -> expr_id
    {
        return match value(index).data {
            .expression(span, id) => id,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    arguments(self const&, index: usize) -> vector<expr_id>
    {
        return match value(index).data {
            .arguments(span, items) => items,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    else_branch(self const&, index: usize) -> optional<stmt_id>
    {
        return match value(index).data {
            .else_branch(span, branch) => branch,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    return_value(self const&, index: usize) -> optional<expr_id>
    {
        return match value(index).data {
            .return_value(span, expression) => expression,
            _ => panic("LR(1) semantic value kind mismatch"),
        };
    }

    span(self const&, index: usize) -> source_span
    {
        return match value(index).data {
            .token(item) => item.span,
            .program(span, id) => span,
            .function(span, id) => span,
            .return_type(span, kind) => span,
            .parameter(span, item) => span,
            .statement(span, id) => span,
            .optional_statement(span, id) => span,
            .declaration(span, item) => span,
            .expression(span, id) => span,
            .arguments(span, items) => span,
            .else_branch(span, branch) => span,
            .return_value(span, expression) => span,
            _ => panic("LR(1) semantic value has no span"),
        };
    }
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

    trace_event_at(self&, action: str, subject: str, detail: str, item: token const&, state: usize, target_state: usize, production: usize, pop_count: usize, goto_state: usize, goto_target: usize) -> void
    {
        if(not options.trace_enabled) {
            return;
        }
        trace.push_back(trace_record{
            .step = trace_next_step,
            .depth = 0 as usize,
            .state = state,
            .target_state = target_state,
            .production = production,
            .pop_count = pop_count,
            .goto_state = goto_state,
            .goto_target = goto_target,
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
        trace_event_at("enter", "Program", "LR(1) parse", item, 0 as usize, 0 as usize, 0 as usize, 0 as usize, 0 as usize, 0 as usize);
    }

    trace_shift(self&, state: usize, target_state: usize, item: token const&) -> void
    {
        trace_event_at("match", token_kind_name(item.kind), "shift", item, state, target_state, 0 as usize, 0 as usize, 0 as usize, 0 as usize);
    }

    trace_reduce(self&, state: usize, production: usize, pop_count: usize, goto_state: usize, goto_target: usize, lookahead: token const&) -> void
    {
        if(not options.trace_enabled) {
            return;
        }
        trace_event_at("reduce", "LR(1)", "reduce", lookahead, state, 0 as usize, production, pop_count, goto_state, goto_target);
    }

    trace_accept(self&, state: usize, lookahead: token const&) -> void
    {
        trace_event_at("accept", "Program", "accepted", lookahead, state, 0 as usize, 0 as usize, 0 as usize, 0 as usize, 0 as usize);
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

    add_unary(self&, op: token, operand: expr_id) -> expr_id
    {
        let syntax = unary_expr{
            .full_span = combine(op.span, arena.expr_span(operand)),
            .operator_kind = op.kind,
            .operand = operand
        };
        return arena.add_expr(expr_syntax::unary(syntax));
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
        return parser_value{value_data::program(full_span, id)};
    }

    for_assignment(self&, name: token, index: optional<expr_id>, value: expr_id) -> parser_value
    {
        let id = add_assign_statement(name.span, index, value, arena.expr_span(value));
        return parser_value{value_data::optional_statement(arena.stmt_span(id), optional<stmt_id>::some(id))};
    }

    reduce_unit(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(rule == production_rule::accept) {
            return optional<parser_value>::some(rhs.value(0 as usize));
        }
        if(rule == production_rule::program) {
            return optional<parser_value>::some(reduce_program(rhs.functions(0 as usize)));
        }
        if(rule == production_rule::functions_append) {
            let functions = rhs.functions(0 as usize);
            functions.push_back(rhs.function(1 as usize));
            return optional<parser_value>::some(parser_value{value_data::functions(functions)});
        }
        if(rule == production_rule::functions_empty) {
            return optional<parser_value>::some(parser_value{value_data::functions(vector<function_id>{})});
        }
        if(rule == production_rule::function) {
            let name = rhs.token(1 as usize);
            let parameters = rhs.parameters(3 as usize);
            let body = rhs.statement(5 as usize);
            let full_span = combine(rhs.span(0 as usize), arena.stmt_span(body));
            let id = arena.add_function(function_syntax{
                .full_span = full_span,
                .return_type = rhs.return_type(0 as usize),
                .name = name.span,
                .parameters = parameters,
                .body = body
            });
            return optional<parser_value>::some(parser_value{value_data::function(full_span, id)});
        }
        if(rule == production_rule::return_int) {
            let item = rhs.token(0 as usize);
            return optional<parser_value>::some(parser_value{value_data::return_type(item.span, return_type_kind::int_type)});
        }
        if(rule == production_rule::return_void) {
            let item = rhs.token(0 as usize);
            return optional<parser_value>::some(parser_value{value_data::return_type(item.span, return_type_kind::void_type)});
        }
        if(rule == production_rule::params_some) {
            return optional<parser_value>::some(rhs.value(0 as usize));
        }
        if(rule == production_rule::params_empty) {
            return optional<parser_value>::some(parser_value{value_data::parameters(vector<parameter_syntax>{})});
        }
        if(rule == production_rule::params_append) {
            let parameters = rhs.parameters(0 as usize);
            parameters.push_back(rhs.parameter(2 as usize));
            return optional<parser_value>::some(parser_value{value_data::parameters(parameters)});
        }
        if(rule == production_rule::params_one) {
            let parameters = vector<parameter_syntax>{};
            parameters.push_back(rhs.parameter(0 as usize));
            return optional<parser_value>::some(parser_value{value_data::parameters(parameters)});
        }
        if(rule == production_rule::param_scalar) {
            let first = rhs.token(0 as usize);
            let name = rhs.token(1 as usize);
            let parameter = parameter_syntax{
                .full_span = combine(first.span, name.span),
                .name = name.span,
                .data = parameter_data::scalar
            };
            return optional<parser_value>::some(parser_value{value_data::parameter(parameter.full_span, parameter)});
        }
        if(rule == production_rule::param_array) {
            let first = rhs.token(0 as usize);
            let name = rhs.token(1 as usize);
            let close = rhs.token(3 as usize);
            let parameter = parameter_syntax{
                .full_span = combine(first.span, close.span),
                .name = name.span,
                .data = parameter_data::array
            };
            return optional<parser_value>::some(parser_value{value_data::parameter(parameter.full_span, parameter)});
        }

        return optional<parser_value>::none;
    }

    reduce_block(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(rule == production_rule::block) {
            let open = rhs.token(0 as usize);
            let statements = rhs.statements(1 as usize);
            let close = rhs.token(2 as usize);
            let syntax = block_statement{
                .full_span = combine(open.span, close.span),
                .statements = statements
            };
            let id = arena.add_stmt(stmt_syntax::block(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::stmts_prepend) {
            let statements = rhs.statements(1 as usize);
            statements.insert(0 as usize, rhs.statement(0 as usize));
            return optional<parser_value>::some(parser_value{value_data::statements(statements)});
        }
        if(rule == production_rule::stmts_empty) {
            return optional<parser_value>::some(parser_value{value_data::statements(vector<stmt_id>{})});
        }

        return optional<parser_value>::none;
    }

    reduce_declaration(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(rule == production_rule::var_decl) {
            let start = rhs.token(0 as usize);
            let declarations = rhs.declarations(1 as usize);
            let semicolon = rhs.token(2 as usize);
            let syntax = var_decl_statement{
                .full_span = combine(start.span, semicolon.span),
                .declarations = declarations
            };
            let id = arena.add_stmt(stmt_syntax::var_decl(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::decls_append) {
            let declarations = rhs.declarations(0 as usize);
            declarations.push_back(rhs.declaration(2 as usize));
            return optional<parser_value>::some(parser_value{value_data::declarations(declarations)});
        }
        if(rule == production_rule::decls_one) {
            let declarations = vector<var_decl_item>{};
            declarations.push_back(rhs.declaration(0 as usize));
            return optional<parser_value>::some(parser_value{value_data::declarations(declarations)});
        }
        if(rule == production_rule::decl_name) {
            let name = rhs.token(0 as usize);
            let item = var_decl_item{
                .full_span = name.span,
                .name = name.span,
                .data = var_decl_data::scalar
            };
            return optional<parser_value>::some(parser_value{value_data::declaration(item.full_span, item)});
        }
        if(rule == production_rule::decl_init) {
            let name = rhs.token(0 as usize);
            let expression = rhs.expression(2 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, arena.expr_span(expression)),
                .name = name.span,
                .data = var_decl_data::initialized(expression)
            };
            return optional<parser_value>::some(parser_value{value_data::declaration(item.full_span, item)});
        }
        if(rule == production_rule::decl_array) {
            let name = rhs.token(0 as usize);
            let size = rhs.token(2 as usize);
            let close = rhs.token(3 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, close.span),
                .name = name.span,
                .data = var_decl_data::array(size.span)
            };
            return optional<parser_value>::some(parser_value{value_data::declaration(item.full_span, item)});
        }
        if(rule == production_rule::decl_array_init) {
            let name = rhs.token(0 as usize);
            let size = rhs.token(2 as usize);
            let initializers = rhs.arguments(5 as usize);
            let item = var_decl_item{
                .full_span = combine(name.span, rhs.span(5 as usize)),
                .name = name.span,
                .data = var_decl_data::array_initialized(size.span, initializers)
            };
            return optional<parser_value>::some(parser_value{value_data::declaration(item.full_span, item)});
        }

        return optional<parser_value>::none;
    }

    reduce_statement(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(
            rule == production_rule::stmt_var
            or rule == production_rule::stmt_identifier
            or rule == production_rule::stmt_if
            or rule == production_rule::stmt_while
            or rule == production_rule::stmt_return
            or rule == production_rule::stmt_block
            or rule == production_rule::stmt_for
            or rule == production_rule::stmt_break
            or rule == production_rule::stmt_continue
        ) {
            return optional<parser_value>::some(rhs.value(0 as usize));
        }
        if(rule == production_rule::assign) {
            let name = rhs.token(0 as usize);
            let value = rhs.expression(2 as usize);
            let semicolon = rhs.token(3 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::none, value, semicolon.span);
            return optional<parser_value>::some(parser_value{value_data::statement(arena.stmt_span(id), id)});
        }
        if(rule == production_rule::call_stmt) {
            let name = rhs.token(0 as usize);
            let arguments = rhs.arguments(2 as usize);
            let semicolon = rhs.token(4 as usize);
            let syntax = call_statement{
                .full_span = combine(name.span, semicolon.span),
                .callee = name.span,
                .arguments = arguments
            };
            let id = arena.add_stmt(stmt_syntax::call(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::if_stmt) {
            let start = rhs.token(0 as usize);
            let condition = rhs.expression(2 as usize);
            let then_branch = rhs.statement(4 as usize);
            let else_branch = rhs.else_branch(5 as usize);
            let end = arena.stmt_span(then_branch);
            if(else_branch.has_value()) {
                const ref branch = *else_branch;
                end = arena.stmt_span(branch);
            }
            let syntax = if_statement{
                .full_span = combine(start.span, end),
                .condition = condition,
                .then_branch = then_branch,
                .else_branch = else_branch
            };
            let id = arena.add_stmt(stmt_syntax::if_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::else_block) {
            let branch = rhs.statement(1 as usize);
            return optional<parser_value>::some(parser_value{value_data::else_branch(arena.stmt_span(branch), optional<stmt_id>::some(branch))});
        }
        if(rule == production_rule::else_empty) {
            return optional<parser_value>::some(parser_value{value_data::else_branch(source_span{}, optional<stmt_id>::none)});
        }
        if(rule == production_rule::while_stmt) {
            let start = rhs.token(0 as usize);
            let condition = rhs.expression(2 as usize);
            let body = rhs.statement(4 as usize);
            let syntax = while_statement{
                .full_span = combine(start.span, arena.stmt_span(body)),
                .condition = condition,
                .body = body
            };
            let id = arena.add_stmt(stmt_syntax::while_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::return_stmt) {
            let start = rhs.token(0 as usize);
            let value = rhs.return_value(1 as usize);
            let semicolon = rhs.token(2 as usize);
            let syntax = return_statement{
                .full_span = combine(start.span, semicolon.span),
                .value = value
            };
            let id = arena.add_stmt(stmt_syntax::return_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::return_value) {
            let expression = rhs.expression(0 as usize);
            return optional<parser_value>::some(parser_value{value_data::return_value(arena.expr_span(expression), optional<expr_id>::some(expression))});
        }
        if(rule == production_rule::return_empty) {
            return optional<parser_value>::some(parser_value{value_data::return_value(source_span{}, optional<expr_id>::none)});
        }
        if(rule == production_rule::assign_index) {
            let name = rhs.token(0 as usize);
            let index = rhs.expression(2 as usize);
            let value = rhs.expression(5 as usize);
            let semicolon = rhs.token(6 as usize);
            let id = add_assign_statement(name.span, optional<expr_id>::some(index), value, semicolon.span);
            return optional<parser_value>::some(parser_value{value_data::statement(arena.stmt_span(id), id)});
        }
        if(rule == production_rule::break_stmt) {
            let start = rhs.token(0 as usize);
            let semicolon = rhs.token(1 as usize);
            let syntax = break_statement{ .full_span = combine(start.span, semicolon.span) };
            let id = arena.add_stmt(stmt_syntax::break_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::continue_stmt) {
            let start = rhs.token(0 as usize);
            let semicolon = rhs.token(1 as usize);
            let syntax = continue_statement{ .full_span = combine(start.span, semicolon.span) };
            let id = arena.add_stmt(stmt_syntax::continue_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }

        return optional<parser_value>::none;
    }

    reduce_expression(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(
            rule == production_rule::expr_or
            or rule == production_rule::or_and
            or rule == production_rule::and_equality
            or rule == production_rule::equality_rel
            or rule == production_rule::rel_add
            or rule == production_rule::add_mul
            or rule == production_rule::mul_unary
            or rule == production_rule::unary_primary
            or rule == production_rule::args_some
        ) {
            return optional<parser_value>::some(rhs.value(0 as usize));
        }
        if(
            rule == production_rule::or_binary
            or rule == production_rule::and_binary
            or rule == production_rule::equal_binary
            or rule == production_rule::not_equal_binary
            or rule == production_rule::less_binary
            or rule == production_rule::less_equal_binary
            or rule == production_rule::greater_binary
            or rule == production_rule::greater_equal_binary
            or rule == production_rule::add_binary
            or rule == production_rule::sub_binary
            or rule == production_rule::mul_binary
            or rule == production_rule::div_binary
            or rule == production_rule::mod_binary
        ) {
            let left = rhs.expression(0 as usize);
            let op = rhs.token(1 as usize);
            let right = rhs.expression(2 as usize);
            let id = add_binary(op.kind, left, right);
            return optional<parser_value>::some(parser_value{value_data::expression(arena.expr_span(id), id)});
        }
        if(rule == production_rule::unary_plus or rule == production_rule::unary_minus) {
            let op = rhs.token(0 as usize);
            let operand = rhs.expression(1 as usize);
            let id = add_unary(op, operand);
            return optional<parser_value>::some(parser_value{value_data::expression(arena.expr_span(id), id)});
        }
        if(rule == production_rule::integer) {
            let item = rhs.token(0 as usize);
            let id = arena.add_expr(expr_syntax::integer(integer_expr{ .full_span = item.span }));
            return optional<parser_value>::some(parser_value{value_data::expression(item.span, id)});
        }
        if(rule == production_rule::name) {
            let item = rhs.token(0 as usize);
            let id = arena.add_expr(expr_syntax::name(name_expr{ .full_span = item.span, .name = item.span }));
            return optional<parser_value>::some(parser_value{value_data::expression(item.span, id)});
        }
        if(rule == production_rule::call_expr) {
            let name = rhs.token(0 as usize);
            let arguments = rhs.arguments(2 as usize);
            let close = rhs.token(3 as usize);
            let syntax = call_expr{
                .full_span = combine(name.span, close.span),
                .callee = name.span,
                .arguments = arguments
            };
            let id = arena.add_expr(expr_syntax::call(syntax));
            return optional<parser_value>::some(parser_value{value_data::expression(syntax.full_span, id)});
        }
        if(rule == production_rule::grouped) {
            let open = rhs.token(0 as usize);
            let expression = rhs.expression(1 as usize);
            let close = rhs.token(2 as usize);
            let syntax = grouped_expr{
                .full_span = combine(open.span, close.span),
                .expression = expression
            };
            let id = arena.add_expr(expr_syntax::grouped(syntax));
            return optional<parser_value>::some(parser_value{value_data::expression(syntax.full_span, id)});
        }
        if(rule == production_rule::args_empty) {
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, vector<expr_id>{})});
        }
        if(rule == production_rule::args_append) {
            let arguments = rhs.arguments(0 as usize);
            arguments.push_back(rhs.expression(2 as usize));
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, arguments)});
        }
        if(rule == production_rule::args_one) {
            let arguments = vector<expr_id>{};
            arguments.push_back(rhs.expression(0 as usize));
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, arguments)});
        }
        if(rule == production_rule::index_expr) {
            let name = rhs.token(0 as usize);
            let index = rhs.expression(2 as usize);
            let close = rhs.token(3 as usize);
            let syntax = index_expr{
                .full_span = combine(name.span, close.span),
                .array = name.span,
                .index = index
            };
            let id = arena.add_expr(expr_syntax::index(syntax));
            return optional<parser_value>::some(parser_value{value_data::expression(syntax.full_span, id)});
        }

        return optional<parser_value>::none;
    }

    reduce_loop(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(rule == production_rule::for_stmt) {
            let start = rhs.token(0 as usize);
            let initializer = rhs.optional_statement(2 as usize);
            let condition = rhs.expression(4 as usize);
            let step = rhs.optional_statement(6 as usize);
            let body = rhs.statement(8 as usize);
            let syntax = for_statement{
                .full_span = combine(start.span, arena.stmt_span(body)),
                .initializer = initializer,
                .condition = condition,
                .step = step,
                .body = body
            };
            let id = arena.add_stmt(stmt_syntax::for_stmt(syntax));
            return optional<parser_value>::some(parser_value{value_data::statement(syntax.full_span, id)});
        }
        if(rule == production_rule::for_init_decl) {
            let start = rhs.token(0 as usize);
            let declarations = rhs.declarations(1 as usize);
            let last_index = declarations.size() - 1;
            let syntax = var_decl_statement{
                .full_span = combine(start.span, declarations[last_index].full_span),
                .declarations = declarations
            };
            let id = arena.add_stmt(stmt_syntax::var_decl(syntax));
            return optional<parser_value>::some(parser_value{value_data::optional_statement(syntax.full_span, optional<stmt_id>::some(id))});
        }
        if(rule == production_rule::for_init_assign) {
            let name = rhs.token(0 as usize);
            let value = rhs.expression(2 as usize);
            return optional<parser_value>::some(for_assignment(name, optional<expr_id>::none, value));
        }
        if(rule == production_rule::for_init_index) {
            let name = rhs.token(0 as usize);
            let index = rhs.expression(2 as usize);
            let value = rhs.expression(5 as usize);
            return optional<parser_value>::some(for_assignment(name, optional<expr_id>::some(index), value));
        }
        if(rule == production_rule::for_init_empty or rule == production_rule::for_step_empty) {
            return optional<parser_value>::some(parser_value{value_data::optional_statement(source_span{}, optional<stmt_id>::none)});
        }
        if(rule == production_rule::for_step_assign) {
            let name = rhs.token(0 as usize);
            let value = rhs.expression(2 as usize);
            return optional<parser_value>::some(for_assignment(name, optional<expr_id>::none, value));
        }
        if(rule == production_rule::for_step_index) {
            let name = rhs.token(0 as usize);
            let index = rhs.expression(2 as usize);
            let value = rhs.expression(5 as usize);
            return optional<parser_value>::some(for_assignment(name, optional<expr_id>::some(index), value));
        }

        return optional<parser_value>::none;
    }

    reduce_initializer(self&, rule: production_rule, rhs: rhs_view const&) -> optional<parser_value>
    {
        if(rule == production_rule::init_list) {
            let open = rhs.token(0 as usize);
            let values = rhs.arguments(1 as usize);
            let close = rhs.token(2 as usize);
            return optional<parser_value>::some(parser_value{value_data::arguments(combine(open.span, close.span), values)});
        }
        if(rule == production_rule::init_values_some) {
            return optional<parser_value>::some(rhs.value(0 as usize));
        }
        if(rule == production_rule::init_values_empty) {
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, vector<expr_id>{})});
        }
        if(rule == production_rule::init_values_append) {
            let values = rhs.arguments(0 as usize);
            values.push_back(rhs.expression(2 as usize));
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, values)});
        }
        if(rule == production_rule::init_values_one) {
            let values = vector<expr_id>{};
            values.push_back(rhs.expression(0 as usize));
            return optional<parser_value>::some(parser_value{value_data::arguments(source_span{}, values)});
        }

        return optional<parser_value>::none;
    }

    reduce_value(self&, rule: production_rule, rhs: rhs_view const&) -> parser_value
    {
        let unit = reduce_unit(rule, rhs);
        if(unit.has_value()) {
            return *unit;
        }
        let block = reduce_block(rule, rhs);
        if(block.has_value()) {
            return *block;
        }
        let declaration = reduce_declaration(rule, rhs);
        if(declaration.has_value()) {
            return *declaration;
        }
        let statement = reduce_statement(rule, rhs);
        if(statement.has_value()) {
            return *statement;
        }
        let expression = reduce_expression(rule, rhs);
        if(expression.has_value()) {
            return *expression;
        }
        let loop = reduce_loop(rule, rhs);
        if(loop.has_value()) {
            return *loop;
        }
        let initializer = reduce_initializer(rule, rhs);
        if(initializer.has_value()) {
            return *initializer;
        }

        return panic("missing LR(1) reduction action");
    }

    reduce(self&, production_index: usize, tables: parser_tables const&, lookahead: token const&, action_state: usize) -> bool
    {
        const ref production = tables.grammar.productions[production_index];
        let values = vector<parser_value>{};
        for(const count : iota(0 as usize, production.rhs.size())) {
            assert(state_stack.size() != 0 as usize, "LR(1) state stack underflow");
            assert(value_stack.size() != 0 as usize, "LR(1) value stack underflow");
            state_stack.pop_back();
            values.push_back(value_stack.back());
            value_stack.pop_back();
        }

        let goto_state = state_stack.back();
        let target = tables.goto_table.find(goto_key{ .state = goto_state, .nonterminal = production.lhs });
        if(not target.has_value()) {
            diagnostics.report(diagnostic_kind::unexpected_token, tokens[tokens.size() - 1].span);
            return false;
        }

        let rhs = rhs_view{ .values = const ref values };
        let value = reduce_value(production.rule, rhs);
        value_stack.push_back(value);
        const ref target_state = *target;
        trace_reduce(action_state, production_index, production.rhs.size(), goto_state, target_state, lookahead);
        state_stack.push_back(target_state);
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

            const ref current_action = *action;
            match current_action.data {
                .shift(target_state) => {
                    value_stack.push_back(parser_value{value_data::token(lookahead)});
                    state_stack.push_back(target_state);
                    trace_shift(state, target_state, lookahead);
                    input_index += 1;
                },
                .reduce(production) => {
                    if(not reduce(production, tables, lookahead, state)) {
                        return finish(false);
                    }
                },
                .accept => {
                    trace_accept(state, lookahead);
                    if(tables.conflicts.size() != 0 as usize) {
                        diagnostics.report(diagnostic_kind::unexpected_token, lookahead.span);
                        return finish(false);
                    }
                    return finish(true);
                },
                .error => {
                    diagnostics.report(diagnostic_kind::unexpected_token, lookahead.span);
                    return finish(false);
                },
            };
        }
        return finish(false);
    }
}
