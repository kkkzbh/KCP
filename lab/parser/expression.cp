export module parser.expression;

import std;
import source;
import lexer.token;
import diagnostic;
import parser.ast;
import parser.state;

impl parser {
    parse_expression(self&) -> optional<expr_id>
    {
        trace_enter("Expr");
        trace_select("Expr", "Expr -> LogicalOr");
        let result = parse_logical_or();
        trace_exit("Expr", result.has_value());
        return result;
    }

    parse_logical_or(self&) -> optional<expr_id>
    {
        trace_enter("LogicalOr");
        trace_select("LogicalOr", "LogicalOr -> LogicalAnd LogicalOrTail");
        let parsed = parse_logical_and();
        if(not parsed.has_value()) {
            trace_exit("LogicalOr", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(check(token_kind::pipe_pipe)) {
            trace_select("LogicalOrTail", "LogicalOrTail -> \"||\" LogicalAnd LogicalOrTail");
            let op = consume();
            trace_match(op);
            let right = parse_logical_and();
            if(not right.has_value()) {
                trace_exit("LogicalOr", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("LogicalOrTail", "LogicalOrTail -> epsilon");
        trace_exit("LogicalOr", true);
        return optional<expr_id>::some(left);
    }

    parse_logical_and(self&) -> optional<expr_id>
    {
        trace_enter("LogicalAnd");
        trace_select("LogicalAnd", "LogicalAnd -> Equality LogicalAndTail");
        let parsed = parse_equality();
        if(not parsed.has_value()) {
            trace_exit("LogicalAnd", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(check(token_kind::amp_amp)) {
            trace_select("LogicalAndTail", "LogicalAndTail -> \"&&\" Equality LogicalAndTail");
            let op = consume();
            trace_match(op);
            let right = parse_equality();
            if(not right.has_value()) {
                trace_exit("LogicalAnd", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("LogicalAndTail", "LogicalAndTail -> epsilon");
        trace_exit("LogicalAnd", true);
        return optional<expr_id>::some(left);
    }

    parse_equality(self&) -> optional<expr_id>
    {
        trace_enter("Equality");
        trace_select("Equality", "Equality -> Relational EqualityTail");
        let parsed = parse_relational();
        if(not parsed.has_value()) {
            trace_exit("Equality", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(check_any(token_kind::equal_equal, token_kind::bang_equal)) {
            if(check(token_kind::equal_equal)) {
                trace_select("EqualityTail", "EqualityTail -> \"==\" Relational EqualityTail");
            } else {
                trace_select("EqualityTail", "EqualityTail -> \"!=\" Relational EqualityTail");
            }
            let op = consume();
            trace_match(op);
            let right = parse_relational();
            if(not right.has_value()) {
                trace_exit("Equality", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("EqualityTail", "EqualityTail -> epsilon");
        trace_exit("Equality", true);
        return optional<expr_id>::some(left);
    }

    parse_relational(self&) -> optional<expr_id>
    {
        trace_enter("Relational");
        trace_select("Relational", "Relational -> Additive RelationalTail");
        let parsed = parse_additive();
        if(not parsed.has_value()) {
            trace_exit("Relational", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(
            check(token_kind::less)
            or check(token_kind::less_equal)
            or check(token_kind::greater)
            or check(token_kind::greater_equal)
        ) {
            if(check(token_kind::less)) {
                trace_select("RelationalTail", "RelationalTail -> \"<\" Additive RelationalTail");
            } else if(check(token_kind::less_equal)) {
                trace_select("RelationalTail", "RelationalTail -> \"<=\" Additive RelationalTail");
            } else if(check(token_kind::greater)) {
                trace_select("RelationalTail", "RelationalTail -> \">\" Additive RelationalTail");
            } else {
                trace_select("RelationalTail", "RelationalTail -> \">=\" Additive RelationalTail");
            }
            let op = consume();
            trace_match(op);
            let right = parse_additive();
            if(not right.has_value()) {
                trace_exit("Relational", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("RelationalTail", "RelationalTail -> epsilon");
        trace_exit("Relational", true);
        return optional<expr_id>::some(left);
    }

    parse_additive(self&) -> optional<expr_id>
    {
        trace_enter("Additive");
        trace_select("Additive", "Additive -> Multiplicative AdditiveTail");
        let parsed = parse_multiplicative();
        if(not parsed.has_value()) {
            trace_exit("Additive", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(check_any(token_kind::plus, token_kind::minus)) {
            if(check(token_kind::plus)) {
                trace_select("AdditiveTail", "AdditiveTail -> \"+\" Multiplicative AdditiveTail");
            } else {
                trace_select("AdditiveTail", "AdditiveTail -> \"-\" Multiplicative AdditiveTail");
            }
            let op = consume();
            trace_match(op);
            let right = parse_multiplicative();
            if(not right.has_value()) {
                trace_exit("Additive", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("AdditiveTail", "AdditiveTail -> epsilon");
        trace_exit("Additive", true);
        return optional<expr_id>::some(left);
    }

    parse_multiplicative(self&) -> optional<expr_id>
    {
        trace_enter("Multiplicative");
        trace_select("Multiplicative", "Multiplicative -> Unary MultiplicativeTail");
        let parsed = parse_unary();
        if(not parsed.has_value()) {
            trace_exit("Multiplicative", false);
            return optional<expr_id>::none;
        }
        let left = *parsed;
        while(check(token_kind::star) or check(token_kind::slash) or check(token_kind::percent)) {
            if(check(token_kind::star)) {
                trace_select("MultiplicativeTail", "MultiplicativeTail -> \"*\" Unary MultiplicativeTail");
            } else if(check(token_kind::slash)) {
                trace_select("MultiplicativeTail", "MultiplicativeTail -> \"/\" Unary MultiplicativeTail");
            } else {
                trace_select("MultiplicativeTail", "MultiplicativeTail -> \"%\" Unary MultiplicativeTail");
            }
            let op = consume();
            trace_match(op);
            let right = parse_unary();
            if(not right.has_value()) {
                trace_exit("Multiplicative", false);
                return optional<expr_id>::none;
            }
            left = add_binary(op.kind, left, *right);
        }
        trace_select("MultiplicativeTail", "MultiplicativeTail -> epsilon");
        trace_exit("Multiplicative", true);
        return optional<expr_id>::some(left);
    }

    parse_unary(self&) -> optional<expr_id>
    {
        trace_enter("Unary");
        if(check(token_kind::plus) or check(token_kind::minus)) {
            if(check(token_kind::plus)) {
                trace_select("Unary", "Unary -> \"+\" Unary");
            } else {
                trace_select("Unary", "Unary -> \"-\" Unary");
            }
            let op = consume();
            trace_match(op);
            let operand = parse_unary();
            if(not operand.has_value()) {
                trace_exit("Unary", false);
                return optional<expr_id>::none;
            }
            let syntax = unary_expr{
                .full_span = combine(op.span, arena.expr_span(*operand)),
                .operator_kind = op.kind,
                .operand = *operand
            };
            let result = arena.add_expr(expr_syntax::unary(syntax));
            trace_exit("Unary", true);
            return optional<expr_id>::some(result);
        }
        trace_select("Unary", "Unary -> Primary");
        let result = parse_primary();
        trace_exit("Unary", result.has_value());
        return result;
    }

    parse_primary(self&) -> optional<expr_id>
    {
        trace_enter("Primary");
        if(
            not check(token_kind::integer_literal)
            and not check(token_kind::identifier)
            and not check(token_kind::l_paren)
        ) {
            trace_error("Primary", "expected expression");
            report_current(diagnostic_kind::expected_expression);
            trace_exit("Primary", false);
            return optional<expr_id>::none;
        }

        if(check(token_kind::integer_literal)) {
            trace_select("Primary", "Primary -> integer");
            let item = consume();
            trace_match(item);
            let syntax = integer_expr{ .full_span = item.span };
            let result = arena.add_expr(expr_syntax::integer(syntax));
            trace_exit("Primary", true);
            return optional<expr_id>::some(result);
        }

        if(check(token_kind::identifier)) {
            trace_select("Primary", "Primary -> identifier PrimarySuffix");
            let name = consume();
            trace_match(name);
            if(check(token_kind::l_paren)) {
                trace_select("PrimarySuffix", "PrimarySuffix -> CallSuffix");
                trace_select("CallSuffix", "CallSuffix -> \"(\" ArgListOpt \")\"");
                let open = consume();
                trace_match(open);
                let arguments = parse_argument_list();
                let close = expect(token_kind::r_paren);
                if(not close.has_value()) {
                    trace_exit("Primary", false);
                    return optional<expr_id>::none;
                }
                let syntax = call_expr{
                    .full_span = combine(name.span, (*close).span),
                    .callee = name.span,
                    .arguments = move arguments
                };
                let result = arena.add_expr(expr_syntax::call(syntax));
                trace_exit("Primary", true);
                return optional<expr_id>::some(result);
            }
            trace_select("PrimarySuffix", "PrimarySuffix -> epsilon");
            let syntax = name_expr{ .full_span = name.span, .name = name.span };
            let result = arena.add_expr(expr_syntax::name(syntax));
            trace_exit("Primary", true);
            return optional<expr_id>::some(result);
        }

        if(check(token_kind::l_paren)) {
            trace_select("Primary", "Primary -> \"(\" Expr \")\"");
            let open = consume();
            trace_match(open);
            let expression = parse_expression();
            let close = expect(token_kind::r_paren);
            if(not expression.has_value() or not close.has_value()) {
                trace_exit("Primary", false);
                return optional<expr_id>::none;
            }
            let syntax = grouped_expr{
                .full_span = combine(open.span, (*close).span),
                .expression = *expression
            };
            let result = arena.add_expr(expr_syntax::grouped(syntax));
            trace_exit("Primary", true);
            return optional<expr_id>::some(result);
        }

        trace_exit("Primary", false);
        return optional<expr_id>::none;
    }

    parse_argument_list(self&) -> vector<expr_id>
    {
        trace_enter("ArgListOpt");
        let arguments = vector<expr_id>{};
        if(check(token_kind::r_paren)) {
            trace_select("ArgListOpt", "ArgListOpt -> epsilon");
            trace_exit("ArgListOpt", true);
            return arguments;
        }

        trace_select("ArgListOpt", "ArgListOpt -> ArgList");
        while(true) {
            let argument = parse_expression();
            if(not argument.has_value()) {
                trace_exit("ArgListOpt", false);
                return arguments;
            }
            arguments.push_back(*argument);
            if(not check(token_kind::comma)) {
                trace_select("ArgListTail", "ArgListTail -> epsilon");
                trace_exit("ArgListOpt", true);
                return arguments;
            }
            trace_select("ArgListTail", "ArgListTail -> \",\" Expr ArgListTail");
            let comma = consume();
            trace_match(comma);
        }
        trace_exit("ArgListOpt", true);
        return arguments;
    }

    add_binary(self&, kind: token_kind, left: expr_id, right: expr_id) -> expr_id
    {
        let left_span = arena.expr_span(left);
        let right_span = arena.expr_span(right);
        let syntax = binary_expr{
            .full_span = combine(left_span, right_span),
            .operator_kind = kind,
            .left = left,
            .right = right
        };
        return arena.add_expr(expr_syntax::binary(syntax));
    }
}
