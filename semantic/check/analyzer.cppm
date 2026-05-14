module semantic.check:analyzer;

import std;
import source;
import lexer.token;
import parser;
import semantic.ty;
import semantic.symbol;
import semantic.diagnostic;
import semantic.result;
import semantic.check.keys;
import :context;
import :types;
import :statements;

enum class return_inference_state : std::uint8_t
{
    pending,
    visiting,
    resolved,
    failed,
};

struct return_inference_binding
{
    semantic_type_id type{};
    bool is_const{};
};

struct semantic_analyzer
{
    explicit semantic_analyzer(source_manager const& sources) :
        context(sources) {}

    auto analyze() -> semantic_result
    {
        collect_modules();
        collect_declarations();
        resolve_imports();
        infer_return_types();
        check_bodies();
        return std::move(context.result);
    }

    auto add_unit(parse_result const& parsed) -> void
        pre(parsed.accepted and parsed.root)
    {
        context.units.emplace_back(parsed);
    }

    auto collect_modules() -> void
    {
        for(auto& state : context.units) {
            auto const& parsed = state.parsed;
            auto const& syntax = *parsed.root;
            if(syntax.module_header) {
                state.module_name = context.ast_source.module_name(syntax.module_header->name);
            }
        }
    }

    auto collect_declarations() -> void
    {
        for(auto unit_index : std::views::iota(0uz, context.units.size())) {
            auto const& unit = context.units[unit_index];
            auto const& ast = unit.parsed.ast;
            auto const& syntax = *unit.parsed.root;
            for(auto function_id : syntax.functions) {
                collect_function_declaration(unit_index, ast, function_id);
            }
        }
    }

    auto collect_function_declaration(std::size_t unit_index,ast_arena const& ast,function_id id) -> void
    {
        auto const& function = ast.function(id);
        auto name = std::string{ context.ast_source.identifier(function.name) };
        auto const& module_name = context.units[unit_index].module_name;
        auto& local_names = context.module_functions[module_name];
        if(local_names.contains(name)) {
            context.report (
                semantic_diagnostic_code::duplicate_symbol,
                function.name,
                std::format("duplicate function '{}'", name)
            );
            return;
        }

        auto types = type_checker{ context };
        auto parameter_types = (
            function.parameters
            | std::views::transform([&](parameter_syntax const& parameter) {
                return types.lower_type(ast, parameter.type);
            }) | std::ranges::to<std::vector<semantic_type_id>>()
        );
        auto inferred_return = not function.return_type;
        auto return_type = semantic_type_ids::inferred;
        if(function.return_type) {
            return_type = types.lower_type(ast, *function.return_type);
        }

        auto signature_id = (
            context.add_signature (function_signature {
                .parameters = parameter_types,
                .returns = return_type,
                .inferred_return = inferred_return,
            })
        );
        context.result.function_signatures.emplace(semantic_node_key{unit_index, id}, signature_id);

        auto function_type_id = (
            context.result.types.intern (function_type {
                .parameters = parameter_types,
                .returns = return_type,
            })
        );
        auto symbol = (
            context.add_symbol (semantic_symbol {
                .kind = symbol_kind::function,
                .name = name,
                .span = function.name,
                .type = function_type_id,
                .exported = function.exported,
                .unit_index = unit_index,
                .function = id,
            })
        );

        if(function.exported) {
            auto& exports = context.module_exports[module_name];
            exports.emplace(name, symbol);
        }
        local_names.emplace(std::move(name), symbol);
    }

    auto resolve_imports() -> void
    {
        for(auto& state : context.units) {
            auto const& parsed = state.parsed;

            auto const& syntax = *parsed.root;
            if(auto local = context.module_functions.find(state.module_name); local != context.module_functions.end()) {
                state.visible_functions.insert_range(local->second);
            }

            for(auto const& import : syntax.imports) {
                auto module_name = context.ast_source.module_name(import.name);
                auto found = context.module_exports.find(module_name);
                if(found == context.module_exports.end()) {
                    context.report (
                        semantic_diagnostic_code::unknown_module,
                        import.full_span,
                        std::format("unknown imported module '{}'", module_name)
                    );
                    continue;
                }

                for(auto const& [name, symbol] : found->second) {
                    if(state.visible_functions.contains(name)) {
                        context.report (
                            semantic_diagnostic_code::import_conflict,
                            import.full_span,
                            std::format("imported symbol '{}' conflicts with an existing visible symbol", name)
                        );
                        continue;
                    }
                    state.visible_functions.emplace(name, symbol);
                }
            }
        }
    }

    auto infer_return_types() -> void
    {
        for(auto unit_index : std::views::iota(0uz, context.units.size())) {
            auto const& unit = context.units[unit_index];
            auto const& syntax = *unit.parsed.root;
            for(auto function_id : syntax.functions) {
                auto signature_id = context.result.signature_of(unit_index, function_id);
                if(not signature_id.valid()) {
                    continue;
                }
                auto const& signature = context.result.signatures[signature_id.value];
                if(signature.inferred_return) {
                    ensure_function_return_inferred(unit_index, function_id);
                }
            }
        }
    }

    auto ensure_function_return_inferred(std::size_t unit_index, function_id id) -> void
    {
        auto key = return_inference_key{ unit_index, id };
        auto& state = return_inference_states[key];
        if(state != return_inference_state::pending) {
            return;
        }

        state = return_inference_state::visiting;
        auto inferred = infer_function_body_return(unit_index, id);
        if(is_inferred(inferred)) {
            report_cannot_infer_return_type(unit_index, id);
            inferred = semantic_type_ids::error;
            state = return_inference_state::failed;
        } else if(is_error(inferred)) {
            state = return_inference_state::failed;
        } else {
            state = return_inference_state::resolved;
        }

        update_inferred_function_return(unit_index, id, inferred);
    }

    auto infer_function_return(std::size_t unit_index, function_id id) -> semantic_type_id
    {
        auto key = return_inference_key{ unit_index, id };
        auto& state = return_inference_states[key];
        if(state == return_inference_state::resolved) {
            auto signature_id = context.result.signature_of(unit_index, id);
            return context.result.signatures[signature_id.value].returns;
        }
        if(state == return_inference_state::visiting) {
            return semantic_type_ids::inferred;
        }
        if(state == return_inference_state::failed) {
            return semantic_type_ids::error;
        }

        ensure_function_return_inferred(unit_index, id);
        if(state == return_inference_state::resolved) {
            auto signature_id = context.result.signature_of(unit_index, id);
            return context.result.signatures[signature_id.value].returns;
        }
        if(state == return_inference_state::failed) {
            return semantic_type_ids::error;
        }

        std::unreachable();
    }

    auto infer_function_body_return(std::size_t unit_index, function_id id) -> semantic_type_id
    {
        auto const& unit = context.units[unit_index];
        auto const& ast = unit.parsed.ast;
        auto const& function = ast.function(id);

        auto old_unit = context.current_unit;
        auto old_scopes = std::move(inferred_scopes);
        context.current_unit = unit_index;

        auto observed = std::vector<semantic_type_id>{};
        inferred_scopes.emplace_back();
        auto const& signature = context.result.signatures[context.result.signature_of(unit_index, id).value];
        for(auto index = 0uz; index < function.parameters.size(); ++index) {
            auto const& parameter = function.parameters[index];
            inferred_scopes.back().emplace(
                std::string{ context.ast_source.slice(parameter.name) },
                return_inference_binding {
                    .type = signature.parameters[index],
                    .is_const = parameter.is_const,
                }
            );
        }

        infer_statement_returns(ast, function.body, observed);
        inferred_scopes = std::move(old_scopes);
        context.current_unit = old_unit;
        return join_inferred_returns(observed);
    }

    auto infer_statement_returns(
        ast_arena const& ast,
        stmt_id id,
        std::vector<semantic_type_id>& observed
    ) -> void
    {
        auto const& statement = ast.statement(id);
        std::visit (
            overloaded {
                [&](block_statement_syntax const& node) {
                    inferred_scopes.emplace_back();
                    for(auto child : node.statements) {
                        infer_statement_returns(ast, child, observed);
                    }
                    inferred_scopes.pop_back();
                },
                [&](declaration_statement_syntax const& node) {
                    auto declared = std::optional<semantic_type_id>{};
                    if(node.declared_type) {
                        auto types = type_checker{ context };
                        declared = types.lower_type(ast, *node.declared_type);
                    }
                    auto initializer = infer_expression_type(ast, node.initializer, declared);
                    inferred_scopes.back().emplace (
                        std::string{ context.ast_source.slice(node.name) },
                        return_inference_binding {
                            .type = declared.value_or(initializer.type),
                            .is_const = node.is_const,
                        }
                    );
                },
                [&](if_statement_syntax const& node) {
                    infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
                    infer_statement_returns(ast, node.then_branch, observed);
                    if(node.else_branch) {
                        infer_statement_returns(ast, *node.else_branch, observed);
                    }
                },
                [&](while_statement_syntax const& node) {
                    infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
                    infer_statement_returns(ast, node.body, observed);
                },
                [&](do_while_statement_syntax const& node) {
                    infer_statement_returns(ast, node.body, observed);
                    infer_expression_type(ast, node.condition, semantic_type_ids::bool_);
                },
                [&](for_statement_syntax const& node) {
                    auto range = infer_expression_type(ast, node.range, std::nullopt);
                    auto types = type_checker{ context };
                    auto element = types.range_element_type(range.type);
                    if(not element.valid()) {
                        element = semantic_type_ids::error;
                    }
                    inferred_scopes.emplace_back();
                    inferred_scopes.back().emplace (
                        std::string{ context.ast_source.slice(node.name) },
                        return_inference_binding {
                            .type = element,
                            .is_const = node.is_const,
                        }
                    );
                    infer_statement_returns(ast, node.body, observed);
                    inferred_scopes.pop_back();
                },
                [&](return_statement_syntax const& node) {
                    observed.emplace_back (
                        node.value
                        ? infer_expression_type(ast, *node.value, std::nullopt).type
                        : semantic_type_ids::unit
                    );
                },
                [&](expression_statement_syntax const& node) {
                    infer_expression_type(ast, node.expression, std::nullopt);
                },
                [](break_statement_syntax const&) {},
                [](continue_statement_syntax const&) {},
            },
            statement
        );
    }

    auto infer_expression_type(
        ast_arena const& ast,
        expr_id id,
        std::optional<semantic_type_id> expected
    ) -> expression_info
    {
        auto const& expression = ast.expression(id);
        return std::visit (
            overloaded {
                [&](name_expr_syntax const& node) {
                    return infer_name_expression(node);
                },
                [&](literal_expr_syntax const& node) {
                    return infer_literal_expression(node);
                },
                [&](unary_expr_syntax const& node) {
                    return infer_unary_expression(ast, node);
                },
                [&](binary_expr_syntax const& node) {
                    return infer_binary_expression(ast, node);
                },
                [&](assignment_expr_syntax const& node) {
                    return infer_assignment_expression(ast, node);
                },
                [&](call_expr_syntax const& node) {
                    return infer_call_expression(ast, node);
                },
                [&](cast_expr_syntax const& node) {
                    auto types = type_checker{ context };
                    infer_expression_type(ast, node.operand, std::nullopt);
                    return expression_info{ .type = types.lower_type(ast, node.type) };
                },
                [&](array_literal_expr_syntax const& node) {
                    return infer_array_like_literal(ast, node.elements, expected, true);
                },
                [&](sequence_literal_expr_syntax const& node) {
                    return infer_array_like_literal(ast, node.elements, expected, false);
                },
                [&](tuple_literal_expr_syntax const& node) {
                    return infer_tuple_literal(ast, node.elements, expected);
                },
                [&](grouped_expr_syntax const& node) {
                    return infer_expression_type(ast, node.expression, expected);
                },
            },
            expression
        );
    }

    auto infer_name_expression(name_expr_syntax const& node) -> expression_info
    {
        auto name = std::string{ context.ast_source.identifier(node.name) };
        if(auto binding = resolve_inferred_binding(name)) {
            return expression_info {
                .type = binding->type,
                .is_lvalue = true,
                .is_const = binding->is_const,
            };
        }
        if(auto symbol = resolve_visible_function(name)) {
            return expression_info{ .type = context.result.symbols[symbol->value].type };
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto infer_literal_expression(literal_expr_syntax const& node) -> expression_info
    {
        auto text = context.ast_source.slice(node.full_span);
        if(text == "true" or text == "false") {
            return expression_info{ .type = semantic_type_ids::bool_ };
        }
        if(text.starts_with("'")) {
            return expression_info{ .type = semantic_type_ids::char_ };
        }
        if(text.starts_with("\"")) {
            return expression_info{ .type = semantic_type_ids::str };
        }
        if(text.contains('.') or text.contains('e') or text.contains('E')) {
            return expression_info{ .type = semantic_type_ids::f64 };
        }
        return expression_info{ .type = semantic_type_ids::i32 };
    }

    auto infer_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info
    {
        auto operand = infer_expression_type(ast, node.operand, std::nullopt);
        auto types = type_checker{ context };
        auto operand_value = types.read_type(operand.type);
        using enum token_kind;
        switch(node.operator_kind) {
            case amp:
                return expression_info {
                    .type = context.result.types.intern(pointer_type {
                        operand_value,
                        types.target_const(operand_value, operand.is_const),
                    }),
                };
            case star:
                if(auto pointee = types.pointer_pointee(operand_value)) {
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

    auto infer_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info
    {
        auto types = type_checker{ context };
        auto left = types.read_type(infer_expression_type(ast, node.left, std::nullopt).type);
        auto right = types.read_type(infer_expression_type(ast, node.right, std::nullopt).type);
        using enum token_kind;
        switch(node.operator_kind) {
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
                if(auto common = types.common_numeric_type(left, right)) {
                    return expression_info{ .type = *common };
                }
                return expression_info{ .type = semantic_type_ids::error };
            case pipe:
            case caret:
            case amp:
            case less_less:
            case greater_greater:
                if(auto common = types.common_integer_type(left, right)) {
                    return expression_info{ .type = *common };
                }
                return expression_info{ .type = semantic_type_ids::error };
            default:
                return expression_info{ .type = semantic_type_ids::error };
        }
    }

    auto infer_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node) -> expression_info
    {
        auto types = type_checker{ context };
        auto left = infer_expression_type(ast, node.left, std::nullopt);
        auto left_type = types.read_type(left.type);
        infer_expression_type(ast, node.right, left_type);
        return expression_info{ .type = left_type };
    }

    auto infer_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info
    {
        if(auto cast_type = function_style_cast_type(ast, node)) {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            return expression_info{ .type = *cast_type };
        }

        if(auto function = callee_function_symbol(ast, node.callee)) {
            for(auto argument : node.arguments) {
                infer_expression_type(ast, argument, std::nullopt);
            }
            return expression_info{ .type = infer_callable_return(*function) };
        }

        auto callee = infer_expression_type(ast, node.callee, std::nullopt);
        for(auto argument : node.arguments) {
            infer_expression_type(ast, argument, std::nullopt);
        }
        if(auto const* callable = std::get_if<function_type>(&context.result.types.get(callee.type))) {
            return expression_info{ .type = callable->returns };
        }
        return expression_info{ .type = semantic_type_ids::error };
    }

    auto infer_array_like_literal(
        ast_arena const& ast,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected,
        bool is_array
    ) -> expression_info
    {
        if(expected) {
            for(auto element : elements) {
                infer_expression_type(ast, element, std::nullopt);
            }
            return expression_info{ .type = *expected };
        }
        if(elements.empty()) {
            return expression_info{ .type = semantic_type_ids::error };
        }

        auto values = std::vector<semantic_type_id>{};
        for(auto element : elements) {
            values.emplace_back(infer_expression_type(ast, element, std::nullopt).type);
        }
        auto joined = join_inferred_returns(values);
        if(is_error(joined) or is_inferred(joined)) {
            return expression_info{ .type = joined };
        }
        if(is_array) {
            return expression_info {
                .type = context.result.types.intern (array_type {
                    .element = joined,
                    .length = elements.size(),
                }),
            };
        }
        return expression_info {
            .type = context.result.types.intern (sequence_type {
                .element = joined,
                .length = elements.size(),
            }),
        };
    }

    auto infer_tuple_literal(
        ast_arena const& ast,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected
    ) -> expression_info
    {
        if(expected) {
            for(auto element : elements) {
                infer_expression_type(ast, element, std::nullopt);
            }
            return expression_info{ .type = *expected };
        }

        auto values = std::vector<semantic_type_id>{};
        auto types = type_checker{ context };
        for(auto element : elements) {
            values.emplace_back(types.read_type(infer_expression_type(ast, element, std::nullopt).type));
        }
        return expression_info{ .type = context.result.types.intern(tuple_type{ std::move(values) }) };
    }

    auto join_inferred_returns(std::vector<semantic_type_id> const& values) -> semantic_type_id
    {
        if(values.empty()) {
            return semantic_type_ids::unit;
        }

        auto types = type_checker{ context };
        auto current = types.read_type(values.front());
        if(is_error(current) or is_inferred(current)) {
            return current;
        }
        for(auto index = 1uz; index < values.size(); ++index) {
            auto next = types.read_type(values[index]);
            if(is_error(next) or is_inferred(next)) {
                return next;
            }
            if(current == next) {
                continue;
            }

            auto left = as_builtin(current);
            auto right = as_builtin(next);
            if(left and right and is_integer(*left) and is_integer(*right)) {
                current = *types.common_integer_type(current, next);
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

    auto infer_callable_return(symbol_id symbol) -> semantic_type_id
    {
        auto const& value = context.result.symbols[symbol.value];
        auto signature_id = context.result.signature_of(value.unit_index, value.function);
        if(not signature_id.valid()) {
            return semantic_type_ids::error;
        }
        auto const& signature = context.result.signatures[signature_id.value];
        if(not signature.inferred_return) {
            return signature.returns;
        }
        return infer_function_return(value.unit_index, value.function);
    }

    auto callee_function_symbol(ast_arena const& ast, expr_id id) -> std::optional<symbol_id>
    {
        auto const& expression = ast.expression(id);
        if(not is<name_expr_syntax>(expression)) {
            return std::nullopt;
        }

        auto name = context.ast_source.identifier(as<name_expr_syntax>(expression).name);
        return resolve_visible_function(name);
    }

    auto function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
        -> std::optional<semantic_type_id>
    {
        auto const& callee = ast.expression(node.callee);
        if(not is<name_expr_syntax>(callee)) {
            return std::nullopt;
        }

        auto text = context.ast_source.identifier(as<name_expr_syntax>(callee).name);
        if(auto builtin = is_builtin_name(text)) {
            return semantic_type_ids::builtin(*builtin);
        }
        return std::nullopt;
    }

    auto resolve_inferred_binding(std::string_view name) const -> std::optional<return_inference_binding>
    {
        for(auto scope = inferred_scopes.rbegin(); scope != inferred_scopes.rend(); ++scope) {
            if(auto found = scope->find(std::string{ name }); found != scope->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    auto resolve_visible_function(std::string_view name) const -> std::optional<symbol_id>
    {
        auto const& visible = context.units[context.current_unit].visible_functions;
        if(auto found = visible.find(std::string{ name }); found != visible.end()) {
            return found->second;
        }
        return std::nullopt;
    }

    auto update_inferred_function_return(std::size_t unit_index, function_id id, semantic_type_id type) -> void
    {
        auto signature_id = context.result.signature_of(unit_index, id);
        if(signature_id.valid()) {
            context.result.signatures[signature_id.value].returns = type;
        }

        auto const& unit = context.units[unit_index];
        auto const& function = unit.parsed.ast.function(id);
        auto symbol_id = context.module_functions[unit.module_name][std::string{ context.ast_source.slice(function.name) }];
        auto& symbol = context.result.symbols[symbol_id.value];
        auto const& old_function_type = std::get<function_type>(context.result.types.get(symbol.type));
        symbol.type = context.result.types.intern (function_type {
            .parameters = old_function_type.parameters,
            .returns = type,
        });
    }

    auto report_cannot_infer_return_type(std::size_t unit_index, function_id id) -> void
    {
        auto const& function = context.units[unit_index].parsed.ast.function(id);
        context.report (
            semantic_diagnostic_code::cannot_infer_return_type,
            function.full_span,
            "cannot infer function return type"
        );
    }

    auto check_bodies() -> void
    {
        auto statements = statement_checker{ context };
        for(auto unit_index : std::views::iota(0uz, context.units.size())) {
            context.current_unit = unit_index;
            auto const& state = context.units[unit_index];
            auto const& parsed = state.parsed;

            auto const& ast = parsed.ast;
            auto const& syntax = *parsed.root;
            for(auto function_id : syntax.functions) {
                statements.check_function(ast, function_id);
            }
        }
    }

    semantic_context context;
    std::map<return_inference_key, return_inference_state> return_inference_states{};
    std::vector<std::map<std::string, return_inference_binding>> inferred_scopes{};
};
