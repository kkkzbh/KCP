module semantic:types;

import semantic;

auto semantic_analyzer::lower_type(ast_arena const& ast, type_id id) -> semantic_type_id
{
    auto const& syntax = ast.type(id);
    auto name = ast_source.identifier(syntax.name);
    auto lowered = semantic_type_id{};
    if(auto builtin = is_builtin_name(name)) {
        lowered = semantic_type_ids::builtin(*builtin);
        if(not syntax.arguments.empty()) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "builtin type does not take arguments"
            );
            lowered = semantic_type_ids::error;
        }
    } else if(name == "array" or name == "sequence") {
        lowered = lower_array_or_sequence_type(ast, syntax, name == "array");
    } else if(name == "tuple") {
        lowered = lower_tuple_type(ast, syntax);
    } else {
        report (
            diagnostic_kind::unknown_type,
            syntax.full_span,
            std::format("unknown type '{}'", name)
        );
        lowered = semantic_type_ids::error;
    }

    for(auto suffix : syntax.suffix_operators) {
        if(suffix == token_kind::star) {
            lowered = result.types.intern(pointer_type{ lowered, syntax.is_const });
        } else if(suffix == token_kind::amp) {
            lowered = result.types.intern(reference_type{ lowered, syntax.is_const });
        }
    }
    return lowered;
}

auto semantic_analyzer::range_element_type(semantic_type_id type) -> semantic_type_id
{
    auto value = read_type(type);
    auto const& kind = result.types.get(value);
    if(auto const* array = std::get_if<array_type>(&kind)) {
        return array->element;
    }
    if(auto const* sequence = std::get_if<sequence_type>(&kind)) {
        return sequence->element;
    }
    return {};
}

auto semantic_analyzer::pointer_pointee(semantic_type_id type) -> std::optional<expression_info>
{
    auto const& kind = result.types.get(type);
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return expression_info {
            .type = pointer->pointee,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(pointer->pointee, pointer->is_const),
        };
    }
    if(auto const* reference = std::get_if<reference_type>(&kind)) {
        return expression_info {
            .type = reference->pointee,
            .is_lvalue = true,
            .is_const = terminal_pointee_const(reference->pointee, reference->is_const),
        };
    }
    return std::nullopt;
}

auto semantic_analyzer::target_const(semantic_type_id type, bool lvalue_const) -> bool
{
    auto const& kind = result.types.get(read_type(type));
    if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
        return pointer->is_const;
    }
    if(auto const* reference = std::get_if<reference_type>(&kind)) {
        return reference->is_const;
    }
    return lvalue_const;
}

auto semantic_analyzer::read_type(semantic_type_id type) -> semantic_type_id
{
    auto current = type;
    while(true) {
        auto const& kind = result.types.get(current);
        if(auto const* reference = std::get_if<reference_type>(&kind)) {
            current = reference->pointee;
            continue;
        }
        return current;
    }
}

auto semantic_analyzer::can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool
{
    if(auto const* reference = std::get_if<reference_type>(&result.types.get(to))) {
        return from.is_lvalue and read_type(from.type) == read_type(reference->pointee);
    }
    return can_implicitly_convert(from.type, to);
}

auto semantic_analyzer::can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
{
    if(from == to or is_error(from) or is_error(to)) {
        return true;
    }

    auto source = read_type(from);
    auto target = read_type(to);
    if(source == target) {
        return true;
    }

    auto source_builtin = as_builtin(source);
    auto target_builtin = as_builtin(target);
    if(not source_builtin or not target_builtin) {
        return false;
    }

    if(is_integer(*source_builtin) and is_integer(*target_builtin)) {
        return true;
    }
    if(is_float(*source_builtin) and is_float(*target_builtin)) {
        return float_rank(*source_builtin) <= float_rank(*target_builtin);
    }
    return false;
}

auto semantic_analyzer::can_explicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
{
    if(can_implicitly_convert(from, to)) {
        return true;
    }

    auto source = as_builtin(read_type(from));
    auto target = as_builtin(read_type(to));
    if(source and target) {
        return is_numeric(*source) and is_numeric(*target);
    }
    return false;
}

auto semantic_analyzer::is_numeric_type(semantic_type_id id) -> bool
{
    auto builtin = as_builtin(read_type(id));
    return builtin and is_numeric(*builtin);
}

auto semantic_analyzer::common_numeric_type(semantic_type_id left, semantic_type_id right)
    -> std::optional<semantic_type_id>
{
    auto left_builtin = as_builtin(read_type(left));
    auto right_builtin = as_builtin(read_type(right));
    if(not left_builtin or not right_builtin) {
        return std::nullopt;
    }
    if(is_float(*left_builtin) or is_float(*right_builtin)) {
        auto kind = (
            float_rank(*left_builtin) > float_rank(*right_builtin)
            ? *left_builtin
            : *right_builtin
        );
        if(not is_float(kind)) {
            kind = builtin_type_kind::f64;
        }
        return semantic_type_ids::builtin(kind);
    }
    return common_integer_type(left, right);
}

auto semantic_analyzer::common_integer_type(semantic_type_id left, semantic_type_id right)
    -> std::optional<semantic_type_id>
{
    auto left_builtin = as_builtin(read_type(left));
    auto right_builtin = as_builtin(read_type(right));
    if (
        not left_builtin or not right_builtin
        or not is_integer(*left_builtin) or not is_integer(*right_builtin)
    ) {
        return std::nullopt;
    }

    auto kind = integer_rank(*left_builtin) >= integer_rank(*right_builtin)
        ? *left_builtin
        : *right_builtin;
    return semantic_type_ids::builtin(kind);
}

auto semantic_analyzer::join_same_class_numeric_or_equal(std::vector<semantic_type_id> const& values, source_span span)
    -> semantic_type_id
{
    auto current = read_type(values.front());
    for(auto index = 1uz; index < values.size(); ++index) {
        auto next = read_type(values[index]);
        if(current == next) {
            continue;
        }

        auto left = as_builtin(current);
        auto right = as_builtin(next);
        if(left and right and is_integer(*left) and is_integer(*right)) {
            current = *common_integer_type(current, next);
            continue;
        }
        if(left and right and is_float(*left) and is_float(*right)) {
            current = semantic_type_ids::builtin(float_rank(*left) >= float_rank(*right) ? *left : *right);
            continue;
        }

        report (
            diagnostic_kind::heterogeneous_aggregate,
            span,
            "aggregate elements must have one type family"
        );
        return semantic_type_ids::error;
    }
    return current;
}

auto semantic_analyzer::terminal_pointee_const(semantic_type_id pointee, bool target_const) -> bool
{
    if(not target_const) {
        return false;
    }

    auto const& kind = result.types.get(read_type(pointee));
    return not std::holds_alternative<pointer_type>(kind)
           and not std::holds_alternative<reference_type>(kind);
}

auto semantic_analyzer::lower_array_or_sequence_type(ast_arena const& ast, type_syntax const& syntax, bool is_array)
    -> semantic_type_id
{
    if(syntax.arguments.size() != 2uz) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            is_array ? "array requires <type,length>" : "sequence requires <type,length>"
        );
        return semantic_type_ids::error;
    }

    if (
        not is<type_argument_type_syntax>(syntax.arguments.front())
        or not is<type_argument_literal_syntax>(syntax.arguments.back())
    ) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            is_array ? "array requires <type,length>" : "sequence requires <type,length>"
        );
        return semantic_type_ids::error;
    }

    auto element = lower_type(ast, as<type_argument_type_syntax>(syntax.arguments.front()).type);
    auto length = parse_length(as<type_argument_literal_syntax>(syntax.arguments.back()).literal);
    if(is_array) {
        return result.types.intern (array_type {
            .element = element,
            .length = length,
        });
    }
    return result.types.intern (sequence_type {
        .element = element,
        .length = length,
    });
}

auto semantic_analyzer::lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id
{
    if(syntax.arguments.empty()) {
        report (
            diagnostic_kind::invalid_type_argument,
            syntax.full_span,
            "tuple requires element types"
        );
        return semantic_type_ids::error;
    }

    auto elements = std::vector<semantic_type_id>{};
    for(auto const& argument : syntax.arguments) {
        if(not is<type_argument_type_syntax>(argument)) {
            report (
                diagnostic_kind::invalid_type_argument,
                syntax.full_span,
                "tuple arguments must be types"
            );
            return semantic_type_ids::error;
        }
        elements.emplace_back(lower_type(ast, as<type_argument_type_syntax>(argument).type));
    }
    return result.types.intern(tuple_type{ std::move(elements) });
}

auto semantic_analyzer::parse_length(source_span span) -> std::uint64_t
{
    auto text = ast_source.slice(span);
    auto value = std::uint64_t{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if(parsed.ec != std::errc{}) {
        report(diagnostic_kind::invalid_type_argument, span, "invalid array or sequence length");
        return 0;
    }
    return value;
}

auto semantic_analyzer::literal_type(source_span span) -> semantic_type_id
{
    auto text = ast_source.slice(span);
    if(text == "true" or text == "false") {
        return semantic_type_ids::bool_;
    }
    if(text.starts_with("'")) {
        return semantic_type_ids::char_;
    }
    if(text.starts_with("\"")) {
        return semantic_type_ids::str;
    }
    if(text.contains('.') or text.contains('e') or text.contains('E')) {
        return semantic_type_ids::f64;
    }
    return semantic_type_ids::i32;
}

auto semantic_analyzer::unary_type(token_kind operator_kind, expression_info operand) -> expression_info
{
    auto operand_value = read_type(operand.type);
    using enum token_kind;
    switch(operator_kind) {
        case amp:
            return expression_info {
                .type = intern_type(pointer_type {
                    operand_value,
                    target_const(operand_value, operand.is_const),
                }),
            };
        case star:
            if(auto pointee = pointer_pointee(operand_value)) {
                return *pointee;
            }
            return expression_info{ .type = semantic_type_ids::error };
        case kw_not:
            return expression_info{ .type = semantic_type_ids::bool_ };
        case plus:
        case minus:
        case tilde:
        case plus_plus:
        case minus_minus:
            return expression_info{ .type = operand_value };
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::binary_type(token_kind operator_kind, semantic_type_id left, semantic_type_id right) -> expression_info
{
    auto left_type = read_type(left);
    auto right_type = read_type(right);
    using enum token_kind;
    switch(operator_kind) {
        case kw_and:
        case kw_or:
        case equal_equal:
        case bang_equal:
        case less:
        case less_equal:
        case greater:
        case greater_equal:
            return expression_info{ .type = semantic_type_ids::bool_ };
        case plus:
        case minus:
        case star:
        case slash:
        case percent:
            if(auto common = common_numeric_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        case pipe:
        case caret:
        case amp:
        case less_less:
        case greater_greater:
            if(auto common = common_integer_type(left_type, right_type)) {
                return expression_info{ .type = *common };
            }
            return expression_info{ .type = semantic_type_ids::error };
        default:
            return expression_info{ .type = semantic_type_ids::error };
    }
}

auto semantic_analyzer::function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
    -> std::optional<semantic_type_id>
{
    auto const& callee = ast.expression(node.callee);
    if(not is<name_expr_syntax>(callee)) {
        return std::nullopt;
    }

    auto text = ast_source.identifier(as<name_expr_syntax>(callee).name);
    if(auto builtin = is_builtin_name(text)) {
        return semantic_type_ids::builtin(*builtin);
    }
    return std::nullopt;
}

auto semantic_analyzer::aggregate_context_for(std::optional<semantic_type_id> expected, bool is_array)
    -> std::optional<aggregate_context>
{
    if(not expected) {
        return std::nullopt;
    }
    auto const& type = result.types.get(*expected);
    if(is_array) {
        if(auto const* array = std::get_if<array_type>(&type)) {
            return aggregate_context {
                .element = array->element,
                .length = array->length,
            };
        }
        return std::nullopt;
    }
    if(auto const* sequence = std::get_if<sequence_type>(&type)) {
        return aggregate_context {
            .element = sequence->element,
            .length = sequence->length,
        };
    }
    return std::nullopt;
}

auto semantic_analyzer::join_return_types(std::vector<semantic_type_id> const& values) -> semantic_type_id
{
    if(values.empty()) {
        return semantic_type_ids::unit;
    }

    auto current = read_type(values.front());
    if(is_error(current) or is_inferred(current)) {
        return current;
    }
    for(auto index = 1uz; index < values.size(); ++index) {
        auto next = read_type(values[index]);
        if(is_error(next) or is_inferred(next)) {
            return next;
        }
        if(current == next) {
            continue;
        }

        auto left = as_builtin(current);
        auto right = as_builtin(next);
        if(left and right and is_integer(*left) and is_integer(*right)) {
            current = *common_integer_type(current, next);
            continue;
        }
        if(left and right and is_float(*left) and is_float(*right)) {
            current = semantic_type_ids::builtin(float_rank(*left) >= float_rank(*right) ? *left : *right);
            continue;
        }
        return semantic_type_ids::inferred;
    }
    return current;
}
