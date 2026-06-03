import std;
import parser.ast;
import semantic;

#include "assert.hpp"

namespace {

auto check_result_lookup_api() -> void
{
    auto result = semantic_result{};
    auto expr = expr_id{ .value = 1 };
    auto stmt = stmt_id{ .value = 2 };
    auto function = function_id{ .value = 3 };
    auto unit = 4uz;
    auto context = 5uz;
    auto span = source_span{ .start = 7, .end = 11 };
    auto symbol = symbol_id{ 0 };
    auto second_symbol = symbol_id{ 1 };
    auto signature = function_signature_id{ 0 };
    auto i64 = semantic_type_ids::builtin(builtin_type_kind::i64);

    auto function_symbol = semantic_symbol {
        .kind = symbol_kind::function,
        .name = "fn",
        .type = result.types.intern(function_type {
            .parameters = { semantic_type_ids::i32 },
            .returns = semantic_type_ids::i32,
        }),
        .unit_index = unit,
        .function = function,
    };
    result.symbols.emplace_back(std::move(function_symbol));
    auto local_symbol = semantic_symbol {
        .kind = symbol_kind::local,
        .name = "value",
        .type = semantic_type_ids::i32,
    };
    result.symbols.emplace_back(std::move(local_symbol));
    auto function_signature_value = function_signature {
        .parameters = { semantic_type_ids::i32 },
        .returns = semantic_type_ids::i32,
    };
    result.signatures.emplace_back(std::move(function_signature_value));

    auto context_expr_key = semantic_node_key{ context, unit, expr };
    auto context_stmt_key = semantic_node_key{ context, unit, stmt };
    auto context_function_key = semantic_node_key{ context, unit, function };
    auto unit_expr_key = semantic_node_key{ unit, expr };
    auto unit_stmt_key = semantic_node_key{ unit, stmt };
    auto unit_function_key = semantic_node_key{ unit, function };
    result.expression_types[context_expr_key] = semantic_type_ids::i32;
    result.expression_types[unit_expr_key] = semantic_type_ids::bool_;
    result.expression_symbols[context_expr_key] = symbol;
    result.expression_symbols[unit_expr_key] = second_symbol;
    result.expression_operators[context_expr_key] = symbol;
    result.expression_operators[unit_expr_key] = second_symbol;
    result.first_argument_ufcs_calls.insert(context_expr_key);
    result.function_signatures[context_function_key] = signature;
    result.function_signatures[unit_function_key] = signature;
    result.statement_bindings[context_stmt_key] = symbol;
    result.statement_bindings[unit_stmt_key] = second_symbol;
    result.local_bindings[semantic_parameter_key{ context, unit, span.start }] = symbol;
    result.local_bindings[semantic_parameter_key{ unit, span.start }] = second_symbol;
    result.function_symbols[context_function_key] = symbol;
    result.function_symbols[unit_function_key] = second_symbol;
    result.parameter_bindings[semantic_parameter_key{ context, unit, span.start }] = symbol;
    result.parameter_bindings[semantic_parameter_key{ unit, span.start }] = second_symbol;
    result.parameter_pack_bindings[semantic_parameter_key{ context, unit, span.start }] = { symbol, second_symbol };
    result.expression_infos[context_expr_key] = semantic_expression_info {
        .type = semantic_type_ids::i32,
        .read_type = semantic_type_ids::i32,
        .is_lvalue = true,
    };
    result.expression_infos[unit_expr_key] = semantic_expression_info {
        .type = semantic_type_ids::bool_,
        .read_type = semantic_type_ids::bool_,
    };
    result.expression_conversions[context_expr_key] = i64;
    result.expression_conversions[unit_expr_key] = semantic_type_ids::i32;
    result.function_nrvo_candidates[context_function_key] = symbol;
    result.function_nrvo_candidates[unit_function_key] = second_symbol;
    result.return_nrvo_candidates[context_stmt_key] = symbol;
    result.return_nrvo_candidates[unit_stmt_key] = second_symbol;
    result.direct_initializers[context_stmt_key] = true;
    result.direct_initializers[unit_stmt_key] = true;
    result.literal_values[context_expr_key] = semantic_literal_value{ .value = std::int64_t{ 42 } };
    result.literal_values[unit_expr_key] = semantic_literal_value{ .value = true };
    result.builtin_calls[context_expr_key] = semantic_builtin_call {
        .kind = semantic_builtin_call_kind::alloc,
        .type = semantic_type_ids::i32,
    };
    result.builtin_calls[unit_expr_key] = semantic_builtin_call {
        .kind = semantic_builtin_call_kind::free,
        .type = semantic_type_ids::unit,
    };
    result.expression_fields[context_expr_key] = semantic_field_access {
        .struct_index = 3,
        .field_index = 1,
    };
    result.expression_fields[unit_expr_key] = semantic_field_access {
        .struct_index = 4,
        .field_index = 2,
    };
    result.expression_variant_cases[context_expr_key] = semantic_variant_case_access {
        .variant_index = 1,
        .case_index = 2,
    };
    result.expression_variant_cases[unit_expr_key] = semantic_variant_case_access {
        .variant_index = 3,
        .case_index = 4,
    };
    result.expression_enum_cases[context_expr_key] = semantic_enum_case_access {
        .enum_index = 5,
        .case_index = 6,
    };
    result.expression_enum_cases[unit_expr_key] = semantic_enum_case_access {
        .enum_index = 7,
        .case_index = 8,
    };
    auto closure_type = result.types.intern(struct_type{ .index = 9 });
    auto lambda_info = semantic_lambda_info {
        .function = function,
        .function_symbol = symbol,
        .closure_type = closure_type,
        .closure_struct_index = 9,
    };
    result.lambda_infos[context_function_key] = lambda_info;
    result.lambda_call_infos[context_expr_key] = lambda_info;
    result.closure_lambda_infos[9] = context_function_key;
    result.lambda_capture_accesses[context_expr_key] = semantic_lambda_capture_access {
        .function = function,
        .field_index = 1,
    };
    result.pattern_bindings[semantic_parameter_key{ context, unit, span.start }] = symbol;
    result.pattern_bindings[semantic_parameter_key{ unit, span.start }] = second_symbol;
    result.for_ranges[context_stmt_key] = semantic_for_range_info {
        .kind = semantic_for_range_kind::array,
        .element_type = semantic_type_ids::i32,
    };
    result.template_for_expansions[context_stmt_key] = {
        semantic_template_for_expansion {
            .kind = semantic_template_for_expansion_kind::type,
            .context_index = context + 1uz,
            .binding_symbol = symbol,
            .bound_type = semantic_type_ids::i32,
        },
    };
    result.template_if_selections[context_stmt_key] = semantic_template_if_selection {
        .context_index = context + 2uz,
        .body = stmt,
        .has_body = true,
    };
    result.function_generic_parameter_counts[semantic_node_key{ unit, function }] = 2uz;
    result.function_instances.emplace_back(
        semantic_function_instance_key{ unit, function, { semantic_type_ids::i32 } },
        context,
        symbol,
        signature,
        std::map<std::string, semantic_type_id>{},
        std::map<std::string, std::vector<semantic_type_id>>{}
    );
    result.function_instance_by_symbol[symbol] = 0uz;
    result.function_parameter_defaults[context_function_key] = { true, false };

    test_parser::assert_true(result.accepted(), "empty diagnostics should be accepted");
    test_parser::assert_true(result.type_of(unit, expr) == semantic_type_ids::bool_, "unit type lookup should work");
    test_parser::assert_true(result.type_of(context, unit, expr) == semantic_type_ids::i32, "context type lookup should work");
    test_parser::assert_true(result.resolved_name(unit, expr) == second_symbol, "unit symbol lookup should work");
    test_parser::assert_true(result.resolved_name(context, unit, expr) == symbol, "context symbol lookup should work");
    test_parser::assert_true(result.selected_operator(unit, expr) == second_symbol, "unit operator lookup should work");
    test_parser::assert_true(result.selected_operator(context, unit, expr) == symbol, "context operator lookup should work");
    test_parser::assert_true(result.first_argument_ufcs_call(context, unit, expr), "UFCS side table lookup should work");
    test_parser::assert_true(result.signature_of(unit, function) == signature, "unit signature lookup should work");
    test_parser::assert_true(result.signature_of(context, unit, function) == signature, "context signature lookup should work");
    test_parser::assert_true(result.binding_of(unit, stmt) == second_symbol, "unit binding lookup should work");
    test_parser::assert_true(result.binding_of(context, unit, stmt) == symbol, "context binding lookup should work");
    test_parser::assert_true(result.local_binding_of(unit, span) == second_symbol, "unit local lookup should work");
    test_parser::assert_true(result.local_binding_of(context, unit, span) == symbol, "context local lookup should work");
    test_parser::assert_true(result.function_symbol_of(unit, function) == second_symbol, "unit function symbol lookup should work");
    test_parser::assert_true(result.function_symbol_of(context, unit, function) == symbol, "context function symbol lookup should work");
    test_parser::assert_true(result.parameter_binding_of(unit, span) == second_symbol, "unit parameter lookup should work");
    test_parser::assert_true(result.parameter_binding_of(context, unit, span) == symbol, "context parameter lookup should work");
    test_parser::assert_true(result.parameter_pack_bindings_of(context, unit, span).size() == 2uz, "parameter pack lookup should work");
    test_parser::assert_true(result.info_of(unit, expr).type == semantic_type_ids::bool_, "unit expression info lookup should work");
    test_parser::assert_true(result.info_of(context, unit, expr).is_lvalue, "context expression info lookup should work");
    test_parser::assert_true(result.conversion_of(unit, expr) == semantic_type_ids::i32, "unit conversion lookup should work");
    test_parser::assert_true(result.conversion_of(context, unit, expr) == i64, "context conversion lookup should work");
    test_parser::assert_true(result.nrvo_candidate_of(unit, function) == second_symbol, "unit NRVO lookup should work");
    test_parser::assert_true(result.nrvo_candidate_of(context, unit, function) == symbol, "context NRVO lookup should work");
    test_parser::assert_true(result.nrvo_return_of(unit, stmt) == second_symbol, "unit return NRVO lookup should work");
    test_parser::assert_true(result.nrvo_return_of(context, unit, stmt) == symbol, "context return NRVO lookup should work");
    test_parser::assert_true(result.direct_initializer_of(unit, stmt), "unit direct initializer lookup should work");
    test_parser::assert_true(result.direct_initializer_of(context, unit, stmt), "context direct initializer lookup should work");
    test_parser::assert_true(std::get<bool>(result.literal_of(unit, expr).value), "unit literal lookup should work");
    test_parser::assert_true(std::get<std::int64_t>(result.literal_of(context, unit, expr).value) == 42, "context literal lookup should work");
    test_parser::assert_true(result.builtin_call_of(unit, expr).kind == semantic_builtin_call_kind::free, "unit builtin lookup should work");
    test_parser::assert_true(result.builtin_call_of(context, unit, expr).kind == semantic_builtin_call_kind::alloc, "context builtin lookup should work");
    test_parser::assert_true(result.field_access_of(unit, expr).field_index == 2, "unit field lookup should work");
    test_parser::assert_true(result.field_access_of(context, unit, expr).field_index == 1, "context field lookup should work");
    test_parser::assert_true(result.variant_case_of(unit, expr).case_index == 4, "unit variant lookup should work");
    test_parser::assert_true(result.variant_case_of(context, unit, expr).case_index == 2, "context variant lookup should work");
    test_parser::assert_true(result.enum_case_of(unit, expr).case_index == 8, "unit enum lookup should work");
    test_parser::assert_true(result.enum_case_of(context, unit, expr).case_index == 6, "context enum lookup should work");
    test_parser::assert_true(result.lambda_of(context, unit, function).valid(), "lambda lookup should work");
    test_parser::assert_true(result.lambda_call_of(context, unit, expr).valid(), "lambda call lookup should work");
    test_parser::assert_true(result.lambda_of_closure(closure_type).valid(), "closure lambda lookup should work");
    test_parser::assert_true(not result.lambda_of_closure({}).valid(), "invalid closure type should not resolve");
    test_parser::assert_true(result.lambda_capture_of(context, unit, expr).valid(), "lambda capture lookup should work");
    test_parser::assert_true(result.pattern_binding_of(unit, span) == second_symbol, "unit pattern binding lookup should work");
    test_parser::assert_true(result.pattern_binding_of(context, unit, span) == symbol, "context pattern binding lookup should work");
    test_parser::assert_true(result.function_instance_of(symbol) != nullptr, "function instance lookup should work");
    test_parser::assert_true(result.function_instance_of(second_symbol) == nullptr, "missing function instance lookup should return null");
    test_parser::assert_true(result.for_range_of(context, unit, stmt).valid(), "for range lookup should work");
    test_parser::assert_true(result.template_for_expansions_of(context, unit, stmt).size() == 1uz, "template for lookup should work");
    test_parser::assert_true(result.template_if_selection_of(context, unit, stmt).has_body, "template if lookup should work");
    test_parser::assert_true(result.generic_parameter_count_of(unit, function) == 2uz, "generic parameter count lookup should work");
    test_parser::assert_true(result.generic_parameter_count_of(0uz, function_id{}) == 0uz, "missing generic parameter count should be zero");
    test_parser::assert_true(result.parameter_defaults_of(symbol)->front(), "instance parameter defaults should resolve by context");
    test_parser::assert_true(result.parameter_defaults_of(symbol_id{}) == nullptr, "invalid symbol should not have defaults");
}

} // namespace

auto main() -> int
{
    check_result_lookup_api();
    return 0;
}
