export module semantic;

export import :result;

import std;
import source;
import parser;
import parser.ast;
import lexer.token;
export import diagnostic;
export import semantic.type;
export import semantic.symbol;

struct loop_label
{
    std::string name{};
    source_span span{};
};

struct return_state
{
    std::optional<semantic_type_id> declared_return{};
};

struct expression_info
{
    semantic_type_id type{};
    bool is_lvalue{};
    bool is_const{};
};

struct aggregate_context
{
    semantic_type_id element{};
    std::size_t length{};
};

struct semantic_unit_state
{
    semantic_unit_state(ast_arena const& ast, translation_unit_syntax const& root) :
        ast(ast),
        root(root) {}

    ast_arena const& ast;
    translation_unit_syntax const& root;
    std::string module_name{};
    std::string module_key{};
    bool named_module{};
    std::map<std::string, symbol_id> visible_functions{};
};

struct return_inference_key
{
    constexpr return_inference_key() = default;

    explicit constexpr return_inference_key(std::size_t unit, function_id id) :
        unit_index(unit),
        function_id_value(id.value) {}

    auto constexpr operator==(return_inference_key const&) const -> bool = default;
    auto constexpr operator<=>(return_inference_key const&) const = default;

    std::size_t unit_index{};
    std::uint32_t function_id_value{};
};

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
        ast_source(sources) {}

    auto analyze() -> semantic_result
    {
        build_module_index();
        infer_return_types();
        check_bodies();
        return finish();
    }

    auto add_unit(ast_arena const& ast, translation_unit_syntax const& root) -> void
    {
        units.emplace_back(ast, root);
    }

private:
    auto add_symbol(semantic_symbol symbol) -> symbol_id
    {
        auto id = symbol_id{ static_cast<std::uint32_t>(result.symbols.size()) };
        result.symbols.emplace_back(std::move(symbol));
        return id;
    }

    auto add_signature(function_signature signature) -> function_signature_id
    {
        auto id = function_signature_id{ static_cast<std::uint32_t>(result.signatures.size()) };
        result.signatures.emplace_back(std::move(signature));
        return id;
    }

    template<typename Type>
    auto intern_type(Type type) -> semantic_type_id
    {
        return result.types.intern(std::move(type));
    }

    auto report(diagnostic_kind kind, source_span span, std::string message) -> void
    {
        diagnostics.report(kind, std::move(message), span);
    }

    auto record_expression(expr_id id, std::size_t unit, semantic_expression_info info) -> void
    {
        auto key = semantic_node_key{unit, id};
        result.expression_types[key] = info.type;
        result.expression_infos[key] = info;
    }

    auto finish() -> semantic_result
    {
        result.diagnostics = diagnostics.take();
        return std::move(result);
    }

    auto build_module_index() -> void;
    auto collect_modules() -> void;
    auto collect_declarations() -> void;
    auto resolve_imports() -> void;
    auto collect_function_declaration(std::size_t unit_index, ast_arena const& ast, function_id id) -> void;

    auto lower_type(ast_arena const& ast, type_id id) -> semantic_type_id;
    auto range_element_type(semantic_type_id type) -> semantic_type_id;
    auto pointer_pointee(semantic_type_id type) -> std::optional<expression_info>;
    auto target_const(semantic_type_id type, bool lvalue_const) -> bool;
    auto read_type(semantic_type_id type) -> semantic_type_id;
    auto can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool;
    auto can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool;
    auto can_explicitly_convert(semantic_type_id from, semantic_type_id to) -> bool;
    auto is_numeric_type(semantic_type_id id) -> bool;
    auto common_numeric_type(semantic_type_id left, semantic_type_id right) -> std::optional<semantic_type_id>;
    auto common_integer_type(semantic_type_id left, semantic_type_id right) -> std::optional<semantic_type_id>;
    auto join_same_class_numeric_or_equal(std::vector<semantic_type_id> const& values, source_span span)
        -> semantic_type_id;
    auto terminal_pointee_const(semantic_type_id pointee, bool target_const) -> bool;
    auto lower_array_or_sequence_type(ast_arena const& ast, type_syntax const& syntax, bool is_array)
        -> semantic_type_id;
    auto lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id;
    auto parse_length(source_span span) -> std::uint64_t;

    auto literal_type(source_span span) -> semantic_type_id;
    auto unary_type(token_kind operator_kind, expression_info operand) -> expression_info;
    auto binary_type(token_kind operator_kind, semantic_type_id left, semantic_type_id right) -> expression_info;
    auto function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
        -> std::optional<semantic_type_id>;
    auto aggregate_context_for(std::optional<semantic_type_id> expected, bool is_array)
        -> std::optional<aggregate_context>;
    auto join_return_types(std::vector<semantic_type_id> const& values) -> semantic_type_id;

    auto infer_return_types() -> void;
    auto infer_function_return(std::size_t unit_index, function_id id) -> semantic_type_id;
    auto ensure_function_return_inferred(std::size_t unit_index, function_id id) -> void;
    auto infer_function_body_return(std::size_t unit_index, function_id id) -> semantic_type_id;
    auto infer_statement_returns(ast_arena const& ast, stmt_id id, std::vector<semantic_type_id>& observed) -> void;
    auto infer_expression_type(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected)
        -> expression_info;
    auto infer_name_expression(name_expr_syntax const& node) -> expression_info;
    auto infer_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info;
    auto infer_array_like_literal(
        ast_arena const& ast,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected,
        bool is_array
    ) -> expression_info;
    auto infer_tuple_literal(
        ast_arena const& ast,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected
    ) -> expression_info;
    auto infer_callable_return(symbol_id symbol) -> semantic_type_id;
    auto callee_function_symbol(ast_arena const& ast, expr_id id) -> std::optional<symbol_id>;
    auto resolve_binding(std::string_view name) const -> std::optional<return_inference_binding>;
    auto resolve_visible_function(std::string_view name) const -> std::optional<symbol_id>;
    auto update_inferred_function_return(std::size_t unit_index, function_id id, semantic_type_id type) -> void;
    auto report_cannot_infer_return_type(std::size_t unit_index, function_id id) -> void;

    auto check_bodies() -> void;
    auto check_function(std::size_t unit_index, function_id id) -> void;
    auto enter_function_scope() -> void;
    auto bind_symbol(semantic_symbol symbol) -> symbol_id;
    auto resolve(std::string_view name) const -> symbol_id;
    auto push_scope() -> void;
    auto pop_scope() -> void;
    auto push_loop(loop_label label = {}) -> void;
    auto pop_loop() -> void;
    auto check_statement(stmt_id id, return_state& returns) -> void;
    auto check_condition(expr_id condition) -> void;
    auto check_loop_jump(source_span span, std::optional<source_span> label, bool is_break) -> void;

    auto check_expression(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected) -> expression_info;
    auto check_name_expression(name_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_literal_expression(literal_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info;
    auto check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info;
    auto check_binary_operator(
        token_kind operator_kind,
        semantic_type_id left_type,
        semantic_type_id right_type,
        source_span span
    ) -> expression_info;
    auto check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node) -> expression_info;
    auto compound_assignment_operator(token_kind operator_kind) -> std::optional<token_kind>;
    auto check_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info;
    auto check_cast_expression(ast_arena const& ast, cast_expr_syntax const& node) -> expression_info;
    auto check_array_like_literal(
        ast_arena const& ast,
        source_span span,
        std::vector<expr_id> const& elements,
        std::optional<semantic_type_id> expected,
        bool is_array
    ) -> expression_info;
    auto check_tuple_literal(
        ast_arena const& ast,
        tuple_literal_expr_syntax const& node,
        std::optional<semantic_type_id> expected
    ) -> expression_info;
    auto parse_integer_literal(std::string_view text) -> std::int64_t;
    auto parse_float_literal(std::string_view text) -> double;
    auto parse_char_literal(std::string_view text) -> char;
    auto parse_string_literal(std::string_view text) -> std::string;

    ast_source_view ast_source;
    diagnostic_collector diagnostics{};
    semantic_result result{};
    std::vector<semantic_unit_state> units{};
    std::map<std::string, std::map<std::string, symbol_id>> module_functions{};
    std::map<std::string, std::map<std::string, symbol_id>> module_exports{};
    std::size_t active_unit_index{};
    semantic_unit_state const* active_unit{};
    ast_arena const* active_ast{};
    function_id active_function{};
    std::vector<std::map<std::string, symbol_id>> scopes{};
    std::vector<loop_label> loops{};
    std::map<return_inference_key, return_inference_state> return_states{};
    std::vector<std::map<std::string, return_inference_binding>> return_scopes{};
    std::size_t return_unit{};
};

export auto analyze_semantics(source_manager const& sources, std::span<parse_result const> units) -> semantic_result
{
    auto analyzer = semantic_analyzer{ sources };
    for(auto const& parsed : units) {
        contract_assert(parsed.accepted and parsed.root);
        analyzer.add_unit(parsed.ast, *parsed.root);
    }
    return analyzer.analyze();
}
