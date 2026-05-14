module semantic.check:types;

import std;
import lexer.token;
import parser;
import semantic.ty;
import semantic.diagnostic;
import :state;
import :context;

struct type_checker
{
    explicit type_checker(semantic_context& context) :
        context(context) {}

    auto lower_type(ast_arena const& ast, type_id id) -> semantic_type_id
    {
        auto const& syntax = ast.type(id);
        auto name = context.ast_source.identifier(syntax.name);
        auto lowered = semantic_type_id{};
        if(auto builtin = is_builtin_name(name)) {
            lowered = semantic_type_ids::builtin(*builtin);
            if(not syntax.arguments.empty()) {
                context.report (
                    semantic_diagnostic_code::invalid_type_argument,
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
            context.report (
                semantic_diagnostic_code::unknown_type,
                syntax.full_span,
                std::format("unknown type '{}'", name)
            );
            lowered = semantic_type_ids::error;
        }

        for(auto suffix : syntax.suffix_operators) {
            if(suffix == token_kind::star) {
                lowered = context.result.types.intern(pointer_type{ lowered, syntax.is_const });
            } else if(suffix == token_kind::amp) {
                lowered = context.result.types.intern(reference_type{ lowered, syntax.is_const });
            }
        }
        return lowered;
    }

    auto range_element_type(semantic_type_id type) -> semantic_type_id
    {
        auto value = read_type(type);
        auto const& kind = context.result.types.get(value);
        if(auto const* array = std::get_if<array_type>(&kind)) {
            return array->element;
        }
        if(auto const* sequence = std::get_if<sequence_type>(&kind)) {
            return sequence->element;
        }
        return {};
    }

    auto pointer_pointee(semantic_type_id type) -> std::optional<expression_info>
    {
        auto const& kind = context.result.types.get(type);
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

    auto target_const(semantic_type_id type, bool lvalue_const) -> bool
    {
        auto const& kind = context.result.types.get(read_type(type));
        if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
            return pointer->is_const;
        }
        if(auto const* reference = std::get_if<reference_type>(&kind)) {
            return reference->is_const;
        }
        return lvalue_const;
    }

    auto read_type(semantic_type_id type) -> semantic_type_id
    {
        auto current = type;
        while(true) {
            auto const& kind = context.result.types.get(current);
            if(auto const* reference = std::get_if<reference_type>(&kind)) {
                current = reference->pointee;
                continue;
            }
            return current;
        }
    }

    auto can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool
    {
        if(auto const* reference = std::get_if<reference_type>(&context.result.types.get(to))) {
            return from.is_lvalue and read_type(from.type) == read_type(reference->pointee);
        }
        return can_implicitly_convert(from.type, to);
    }

    auto can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
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

    auto can_explicitly_convert(semantic_type_id from, semantic_type_id to) -> bool
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

    auto is_numeric_type(semantic_type_id id) -> bool
    {
        auto builtin = as_builtin(read_type(id));
        return builtin and is_numeric(*builtin);
    }

    auto common_numeric_type(semantic_type_id left, semantic_type_id right)
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

    auto common_integer_type(semantic_type_id left, semantic_type_id right)
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

    auto join_same_class_numeric_or_equal(std::vector<semantic_type_id> const& values, source_span span)
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

            context.report (
                semantic_diagnostic_code::heterogeneous_aggregate,
                span,
                "aggregate elements must have one type family"
            );
            return semantic_type_ids::error;
        }
        return current;
    }

private:
    auto terminal_pointee_const(semantic_type_id pointee, bool target_const) -> bool
    {
        if(not target_const) {
            return false;
        }

        auto const& kind = context.result.types.get(read_type(pointee));
        return not std::holds_alternative<pointer_type>(kind)
               and not std::holds_alternative<reference_type>(kind);
    }

    auto lower_array_or_sequence_type(ast_arena const& ast, type_syntax const& syntax, bool is_array)
        -> semantic_type_id
    {
        if(syntax.arguments.size() != 2uz) {
            context.report (
                semantic_diagnostic_code::invalid_type_argument,
                syntax.full_span,
                is_array ? "array requires <type,length>" : "sequence requires <type,length>"
            );
            return semantic_type_ids::error;
        }

        if (
            not is<type_argument_type_syntax>(syntax.arguments.front())
            or not is<type_argument_literal_syntax>(syntax.arguments.back())
        ) {
            context.report (
                semantic_diagnostic_code::invalid_type_argument,
                syntax.full_span,
                is_array ? "array requires <type,length>" : "sequence requires <type,length>"
            );
            return semantic_type_ids::error;
        }

        auto element = lower_type(ast, as<type_argument_type_syntax>(syntax.arguments.front()).type);
        auto length = parse_length(as<type_argument_literal_syntax>(syntax.arguments.back()).literal);
        if(is_array) {
            return context.result.types.intern (array_type {
                .element = element,
                .length = length,
            });
        }
        return context.result.types.intern (sequence_type {
            .element = element,
            .length = length,
        });
    }

    auto lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id
    {
        if(syntax.arguments.empty()) {
            context.report (
                semantic_diagnostic_code::invalid_type_argument,
                syntax.full_span,
                "tuple requires element types"
            );
            return semantic_type_ids::error;
        }

        auto elements = std::vector<semantic_type_id>{};
        for(auto const& argument : syntax.arguments) {
            if(not is<type_argument_type_syntax>(argument)) {
                context.report (
                    semantic_diagnostic_code::invalid_type_argument,
                    syntax.full_span,
                    "tuple arguments must be types"
                );
                return semantic_type_ids::error;
            }
            elements.emplace_back(lower_type(ast, as<type_argument_type_syntax>(argument).type));
        }
        return context.result.types.intern(tuple_type{ std::move(elements) });
    }

    auto parse_length(source_span span) -> std::uint64_t
    {
        auto text = context.ast_source.slice(span);
        auto value = std::uint64_t{};
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if(parsed.ec != std::errc{}) {
            context.report(semantic_diagnostic_code::invalid_type_argument, span, "invalid array or sequence length");
            return 0;
        }
        return value;
    }

    semantic_context& context;
};
