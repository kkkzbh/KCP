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
    std::size_t template_for_depth{};
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

struct constructor_match
{
    constructor_match() = default;

    constructor_match(symbol_id matched_symbol, int match_score) :
        symbol(matched_symbol),
        score(match_score) {}

    symbol_id symbol{};
    int score{};
};

struct operator_match
{
    operator_match() = default;

    operator_match(symbol_id matched_symbol, int conversion_score, int generic_score_value) :
        symbol(matched_symbol),
        score(conversion_score),
        generic_score(generic_score_value) {}

    symbol_id symbol{};
    int score{};
    int generic_score{};
};

struct aggregate_context
{
    semantic_type_id element{};
    std::size_t length{};
};

struct lambda_capture_context
{
    lambda_capture_context() = default;

    explicit lambda_capture_context(function_id lambda_function) :
        function(lambda_function) {}

    function_id function{};
    std::vector<semantic_lambda_capture> captures{};
    std::map<symbol_id, std::uint32_t> capture_indices{};
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
    std::map<overload_operator_kind, std::vector<symbol_id>> visible_operators{};
    std::map<std::string, symbol_id> visible_types{};
    std::map<std::string, symbol_id> visible_concepts{};
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

    template<semantic_node_key_id Id>
    auto node_key(Id id) const -> semantic_node_key
    {
        return semantic_node_key{active_context_index, active_unit_index, id};
    }

    auto parameter_key(source_span name) const -> semantic_parameter_key
    {
        return semantic_parameter_key{active_context_index, active_unit_index, name.start};
    }

    auto record_expression(expr_id id, std::size_t unit, semantic_expression_info info) -> void
    {
        auto key = semantic_node_key{active_context_index, unit, id};
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
    auto validate_requires_clauses() -> void;
    auto collect_concept_declaration(std::size_t unit_index, ast_arena const& ast, concept_id id) -> void;
    auto collect_concept_items(std::size_t unit_index, ast_arena const& ast, concept_id id) -> void;
    auto collect_struct_declaration(std::size_t unit_index, ast_arena const& ast, struct_id id) -> void;
    auto collect_struct_fields(std::size_t unit_index, ast_arena const& ast, struct_id id) -> void;
    auto collect_variant_declaration(std::size_t unit_index, ast_arena const& ast, variant_id id) -> void;
    auto collect_variant_cases(std::size_t unit_index, ast_arena const& ast, variant_id id) -> void;
    auto resolve_imports() -> void;
    auto collect_function_declaration(std::size_t unit_index, ast_arena const& ast, function_id id) -> void;
    auto collect_operator_declaration(std::size_t unit_index, ast_arena const& ast, function_id id) -> void;
    auto validate_extern_c_function(function_syntax const& function, std::span<semantic_type_id const> parameter_types, semantic_type_id return_type) -> void;
    auto is_extern_c_compatible_type(semantic_type_id type, bool allow_unit) const -> bool;
    auto collect_impl_declarations(std::size_t unit_index, ast_arena const& ast, impl_id id) -> void;
    auto collect_impl_function(std::size_t unit_index, ast_arena const& ast, impl_syntax const& impl, semantic_type_id impl_type, std::vector<std::string> const& impl_generic_parameters, function_id id) -> void;
    auto collect_impl_operator(std::size_t unit_index, ast_arena const& ast, impl_syntax const& impl, semantic_type_id impl_type, std::vector<std::string> const& impl_generic_parameters, function_id id) -> void;
    auto collect_concept_impl_declarations(std::size_t unit_index, ast_arena const& ast, concept_impl_id id) -> void;
    auto collect_concept_impl_function(std::size_t unit_index, ast_arena const& ast, concept_impl_syntax const& impl, semantic_type_id target_type, function_id id) -> std::optional<symbol_id>;
    auto validate_concept_impl(semantic_concept_impl const& impl, source_span span) -> void;
    auto validate_parent_concepts(semantic_concept_impl const& impl, semantic_concept const& concept_value, source_span span)
        -> void;
    auto validate_requires_clause(std::size_t unit_index, ast_arena const& ast, concept_requires_syntax const& clause, std::optional<semantic_type_id> self_type = std::nullopt) -> void;
    auto target_implements(symbol_id concept_symbol, semantic_type_id target_type) -> bool;
    auto concept_impl_applies(semantic_concept_impl const& impl, semantic_type_id target_type)
        -> std::optional<std::vector<semantic_type_id>>;
    auto requires_clause_satisfied(std::size_t unit_index, ast_arena const& ast, concept_requires_syntax const& clause, std::map<std::string, semantic_type_id> const& substitutions, std::optional<semantic_type_id> self_type = std::nullopt) -> bool;
    auto requirement_type(std::size_t unit_index, ast_arena const& ast, type_id id, semantic_type_id self_type) -> semantic_type_id;
    auto signatures_match(function_signature const& actual, semantic_concept_function_requirement const& expected, semantic_type_id self_type)
        -> bool;
    auto collect_type_pattern_parameters(ast_arena const& ast, type_id id) -> std::vector<std::string>;
    auto collect_type_pattern_parameters(ast_arena const& ast, type_syntax const& syntax, std::vector<std::string>& names) -> void;
    auto bind_active_generic_parameters(std::vector<std::string> const& names) -> void;
    auto bind_active_function_generic_parameters(std::size_t unit_index, function_id id) -> void;
    auto function_key(std::size_t unit_index, function_id id) const -> return_inference_key;
    auto function_generic_parameter_names(std::size_t unit_index, function_id id) const -> std::vector<std::string>;
    auto function_implicit_generic_count(std::size_t unit_index, function_id id) const -> std::size_t;
    auto function_is_generic(std::size_t unit_index, function_id id) const -> bool;
    auto function_impl_target_pattern(std::size_t unit_index, function_id id) const -> semantic_type_id;
    auto function_signature_for_symbol(symbol_id symbol) const -> function_signature const*;
    auto function_explicit_generic_count(std::size_t unit_index, function_id id) const -> std::size_t;
    auto function_substitution_map(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments)
        -> std::map<std::string, semantic_type_id>;
    auto function_type_pack_substitution_map(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments) -> std::map<std::string, std::vector<semantic_type_id>>;
    auto function_pack_generic_index(std::size_t unit_index, function_id id) const -> std::optional<std::size_t>;
    auto function_value_pack_parameter_index(std::size_t unit_index, function_id id) const -> std::optional<std::size_t>;
    auto validate_function_pack_shape(std::size_t unit_index, function_id id) -> void;
    auto validate_function_type_arguments(std::size_t unit_index, function_id id, std::vector<semantic_type_id> const& type_arguments, source_span span) -> bool;
    auto infer_function_type_arguments_for_call(symbol_id symbol, std::optional<semantic_type_id> receiver_type, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> const& explicit_arguments, source_span span) -> std::optional<std::vector<semantic_type_id>>;
    auto instantiate_function_symbol(symbol_id symbol, std::optional<semantic_type_id> receiver_type, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> explicit_arguments, source_span span) -> semantic_function_instance const*;

    auto lower_type(ast_arena const& ast, type_id id) -> semantic_type_id;
    auto lower_type_with_substitutions(ast_arena const& ast, type_id id, std::map<std::string, semantic_type_id> const& substitutions) -> semantic_type_id;
    auto substitute_type(semantic_type_id type, std::vector<semantic_type_id> const& arguments) -> semantic_type_id;
    auto resolve_type_symbol(std::size_t unit_index, std::string_view name) const -> std::optional<symbol_id>;
    auto resolve_concept_symbol(std::size_t unit_index, std::string_view name) const -> std::optional<symbol_id>;
    auto resolve_exported_type_symbol(std::string_view module_name, std::string_view name, std::set<std::string>& visiting) const -> std::optional<symbol_id>;
    auto resolve_exported_concept_symbol(std::string_view module_name, std::string_view name, std::set<std::string>& visiting) const -> std::optional<symbol_id>;
    auto associated_type(semantic_type_id owner, std::string_view name) -> std::optional<semantic_type_id>;
    auto struct_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>;
    auto variant_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>;
    auto struct_named(std::size_t unit_index, std::string_view name) const -> std::optional<std::uint32_t>;
    auto field_index(std::uint32_t struct_index, std::string_view name) const -> std::optional<std::uint32_t>;
    auto variant_case_payload_types(semantic_type_id type, semantic_variant_case const& variant_case)
        -> std::vector<semantic_type_id>;
    auto is_default_initializable(semantic_type_id type) -> bool;
    auto is_dependent_type(semantic_type_id type) const -> bool;
    auto range_element_type(semantic_type_id type) -> semantic_type_id;
    auto range_info(expression_info range, source_span span) -> semantic_for_range_info;
    auto method_symbol(semantic_type_id owner, std::string_view name) const -> std::optional<symbol_id>;
    auto concrete_method_symbol(symbol_id symbol, semantic_type_id receiver_type, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>;
    auto resolve_operator(overload_operator_kind kind, std::span<semantic_type_id const> owner_types, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>;
    auto choose_operator(std::span<symbol_id const> candidates, std::vector<expression_info> const& arguments, std::optional<semantic_type_id> receiver_type, source_span span) -> std::optional<symbol_id>;
    auto operator_score(symbol_id symbol, std::vector<expression_info> const& arguments, std::optional<semantic_type_id> receiver_type, source_span span) -> std::optional<operator_match>;
    auto operator_expression_info(symbol_id symbol) -> expression_info;
    auto operator_token(overload_operator_kind kind) const -> token_kind;
    auto overload_kind(token_kind kind) const -> std::optional<overload_operator_kind>;
    auto pointer_pointee(semantic_type_id type) -> std::optional<expression_info>;
    auto target_const(semantic_type_id type, bool lvalue_const) -> bool;
    auto lower_parameter_type(ast_arena const& ast, parameter_syntax const& parameter, std::optional<semantic_type_id> self_type = std::nullopt) -> semantic_type_id;
    auto read_type(semantic_type_id type) const -> semantic_type_id;
    auto can_implicitly_convert(expression_info const& from, semantic_type_id to) -> bool;
    auto can_implicitly_convert(semantic_type_id from, semantic_type_id to) -> bool;
    auto can_explicitly_convert(semantic_type_id from, semantic_type_id to) -> bool;
    auto is_numeric_type(semantic_type_id id) -> bool;
    auto common_numeric_type(semantic_type_id left, semantic_type_id right) -> std::optional<semantic_type_id>;
    auto common_integer_type(semantic_type_id left, semantic_type_id right) -> std::optional<semantic_type_id>;
    auto join_same_class_numeric_or_equal(std::vector<semantic_type_id> const& values, source_span span)
        -> semantic_type_id;
    auto terminal_pointee_const(semantic_type_id pointee, bool target_const) -> bool;
    auto lower_array_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id;
    auto lower_tuple_type(ast_arena const& ast, type_syntax const& syntax) -> semantic_type_id;
    auto parse_length(source_span span) -> std::uint64_t;

    auto literal_type(source_span span) -> semantic_type_id;
    auto unary_type(token_kind operator_kind, expression_info operand) -> expression_info;
    auto binary_type(token_kind operator_kind, semantic_type_id left, semantic_type_id right) -> expression_info;
    auto try_builtin_binary_operator(token_kind operator_kind, semantic_type_id left_type, semantic_type_id right_type) -> std::optional<expression_info>;
    auto function_style_cast_type(ast_arena const& ast, call_expr_syntax const& node)
        -> std::optional<semantic_type_id>;
    auto check_builtin_call(ast_arena const& ast, call_expr_syntax const& node, name_expr_syntax const& callee, expr_id id)
        -> std::optional<expression_info>;
    auto check_generic_function_call(ast_arena const& ast, call_expr_syntax const& node, expr_id id)
        -> std::optional<expression_info>;
    auto generic_function_symbol(name_expr_syntax const& callee) const -> std::optional<symbol_id>;
    auto explicit_type_arguments(ast_arena const& ast, call_expr_syntax const& node)
        -> std::optional<std::vector<semantic_type_id>>;
    auto infer_generic_type_arguments(function_signature const& signature, std::vector<expression_info> const& arguments, std::size_t parameter_count) -> std::optional<std::vector<semantic_type_id>>;
    auto infer_type_argument(semantic_type_id pattern, semantic_type_id argument, std::map<std::uint32_t, semantic_type_id>& inferred) -> bool;
    auto generic_substitution_map(function_syntax const& function, std::vector<semantic_type_id> const& type_arguments)
        -> std::map<std::string, semantic_type_id>;
    auto instantiate_function(std::size_t unit_index, function_id id, std::vector<semantic_type_id> type_arguments, source_span span) -> semantic_function_instance const*;
    auto instantiate_lambda(semantic_lambda_info const& lambda, std::vector<expression_info> const& arguments, std::vector<semantic_type_id> explicit_arguments, source_span span) -> semantic_lambda_info;
    auto substitute_signature(function_signature const& signature, std::vector<semantic_type_id> const& type_arguments) -> function_signature;
    auto substitute_signature_for_instance(std::size_t unit_index, function_id id, function_signature const& signature, std::vector<semantic_type_id> const& type_arguments, source_span span) -> function_signature;
    auto substitute_type_for_instance(semantic_type_id type, std::optional<std::size_t> pack_index, std::vector<semantic_type_id> const& type_arguments, std::optional<semantic_type_id> pack_element, source_span span) -> semantic_type_id;
    auto infer_type_argument_with_pack(semantic_type_id pattern, semantic_type_id argument, std::optional<std::size_t> pack_index, std::map<std::uint32_t, semantic_type_id>& inferred, std::optional<semantic_type_id>& pack_element) -> bool;
    auto callable_type(semantic_type_id type) const -> function_type const*;
    auto pointer_value_pointee(semantic_type_id type) const -> std::optional<semantic_type_id>;
    auto aggregate_context_for(std::optional<semantic_type_id> expected) -> std::optional<aggregate_context>;
    auto struct_context_for(std::optional<semantic_type_id> expected) -> std::optional<std::uint32_t>;
    auto choose_constructor(std::uint32_t struct_index, std::vector<expression_info> const& arguments, source_span span) -> std::optional<symbol_id>;
    auto constructor_score(function_signature const& signature, std::vector<expression_info> const& arguments)
        -> std::optional<int>;
    auto join_return_types(std::vector<semantic_type_id> const& values) -> semantic_type_id;

    auto infer_return_types() -> void;
    auto infer_function_return(std::size_t unit_index, function_id id) -> semantic_type_id;
    auto ensure_function_return_inferred(std::size_t unit_index, function_id id) -> void;
    auto infer_function_body_return(std::size_t unit_index, function_id id) -> semantic_type_id;
    auto infer_statement_returns(ast_arena const& ast, stmt_id id, std::vector<semantic_type_id>& observed) -> void;
    auto infer_expression_type(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected)
        -> expression_info;
    auto infer_lambda_return_from_current_return_scope(std::size_t unit_index, function_id id) -> void;
    auto infer_lambda_return_from_current_value_scope(std::size_t unit_index, function_id id) -> void;
    auto infer_lambda_return_with_base_scopes(std::size_t unit_index, function_id id, std::vector<std::map<std::string, return_inference_binding>> base_scopes, std::vector<std::map<std::string, semantic_type_id>> base_type_scopes) -> void;
    auto infer_name_expression(name_expr_syntax const& node) -> expression_info;
    auto infer_call_expression(ast_arena const& ast, call_expr_syntax const& node) -> expression_info;
    auto infer_index_expression(ast_arena const& ast, index_expr_syntax const& node) -> expression_info;
    auto infer_array_literal(ast_arena const& ast, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info;
    auto infer_tuple_literal(ast_arena const& ast, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info;
    auto infer_lambda_expression(lambda_expr_syntax const& node, std::optional<semantic_type_id> expected)
        -> expression_info;
    auto infer_callable_return(symbol_id symbol) -> semantic_type_id;
    auto callee_function_symbol(ast_arena const& ast, expr_id id) -> std::optional<symbol_id>;
    auto resolve_binding(std::string_view name) const -> std::optional<return_inference_binding>;
    auto resolve_visible_function(std::string_view name) const -> std::optional<symbol_id>;
    auto update_inferred_function_return(std::size_t unit_index, function_id id, semantic_type_id type) -> void;
    auto report_cannot_infer_return_type(std::size_t unit_index, function_id id) -> void;

    auto check_bodies() -> void;
    auto check_function(std::size_t unit_index, function_id id) -> void;
    auto check_function_instance(std::size_t instance_index) -> void;
    auto check_function_body(std::size_t unit_index, function_id id, std::size_t context_index, function_signature_id signature_id, symbol_id function_symbol, std::map<std::string, semantic_type_id> const* substitutions, std::map<std::string, std::vector<semantic_type_id>> const* pack_substitutions) -> void;
    auto enter_function_scope() -> void;
    auto bind_symbol(semantic_symbol symbol) -> symbol_id;
    auto bind_type_alias(source_span name, semantic_type_id type) -> void;
    auto resolve(std::string_view name) const -> symbol_id;
    auto resolve_type_alias(std::string_view name) const -> std::optional<semantic_type_id>;
    auto push_scope() -> void;
    auto pop_scope() -> void;
    auto push_loop(loop_label label = {}) -> void;
    auto pop_loop() -> void;
    auto check_statement(stmt_id id, return_state& returns) -> void;
    auto check_template_for_statement(template_for_statement_syntax const& node, stmt_id id, return_state& returns)
        -> void;
    auto check_condition(expr_id condition) -> void;
    auto check_loop_jump(source_span span, std::optional<source_span> label, bool is_break) -> void;
    auto self_symbol() const -> symbol_id;
    auto self_struct_index() const -> std::optional<std::uint32_t>;
    auto self_field(std::string_view name) const -> std::optional<semantic_field_access>;

    auto check_expression(ast_arena const& ast, expr_id id, std::optional<semantic_type_id> expected) -> expression_info;
    auto check_name_expression(name_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_literal_expression(literal_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node) -> expression_info;
    auto check_unary_expression(ast_arena const& ast, unary_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node) -> expression_info;
    auto check_binary_expression(ast_arena const& ast, binary_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_binary_operator(token_kind operator_kind, semantic_type_id left_type, semantic_type_id right_type, source_span span) -> expression_info;
    auto check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node) -> expression_info;
    auto check_assignment_expression(ast_arena const& ast, assignment_expr_syntax const& node, expr_id id) -> expression_info;
    auto compound_assignment_operator(token_kind operator_kind) -> std::optional<token_kind>;
    auto check_call_expression(ast_arena const& ast, call_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_member_call(ast_arena const& ast, call_expr_syntax const& node, member_expr_syntax const& callee)
        -> expression_info;
    auto check_associated_call(ast_arena const& ast, call_expr_syntax const& node, associated_name_expr_syntax const& callee)
        -> expression_info;
    auto check_member_expression(ast_arena const& ast, member_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_index_expression(ast_arena const& ast, index_expr_syntax const& node) -> expression_info;
    auto check_index_expression(ast_arena const& ast, index_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_associated_name_expression(associated_name_expr_syntax const& node, expr_id id) -> expression_info;
    auto check_cast_expression(ast_arena const& ast, cast_expr_syntax const& node) -> expression_info;
    auto check_array_literal(ast_arena const& ast, source_span span, std::vector<expr_id> const& elements, std::optional<semantic_type_id> expected) -> expression_info;
    auto check_tuple_literal(ast_arena const& ast, tuple_literal_expr_syntax const& node, std::optional<semantic_type_id> expected) -> expression_info;
    auto check_struct_initializer(ast_arena const& ast, struct_init_expr_syntax const& node, expr_id id, std::optional<semantic_type_id> expected) -> expression_info;
    auto check_block_expression(ast_arena const& ast, block_expr_syntax const& node) -> expression_info;
    auto check_match_expression(ast_arena const& ast, match_expr_syntax const& node) -> expression_info;
    auto check_lambda_expression(lambda_expr_syntax const& node, expr_id id, std::optional<semantic_type_id> expected)
        -> expression_info;
    auto apply_lambda_parameter_context(lambda_expr_syntax const& node, std::optional<semantic_type_id> expected) -> void;
    auto check_lambda_body(lambda_expr_syntax const& node) -> semantic_lambda_info;
    auto build_generic_lambda_info(lambda_expr_syntax const& node) -> semantic_lambda_info;
    auto collect_generic_lambda_captures(function_id id) -> std::vector<semantic_lambda_capture>;
    auto build_lambda_info(lambda_expr_syntax const& node, std::vector<semantic_lambda_capture> captures, function_type callable, bool force_closure) -> semantic_lambda_info;
    auto record_lambda_capture(symbol_id symbol, expr_id id) -> void;
    auto constant_integer_index(ast_arena const& ast, expr_id id) -> std::optional<std::int64_t>;
    auto parse_integer_literal(std::string_view text) -> std::int64_t;
    auto parse_float_literal(std::string_view text) -> double;
    auto parse_char_literal(std::string_view text) -> char;
    auto parse_string_literal(std::string_view text) -> std::string;

    ast_source_view ast_source; ///< 读取源码切片、标识符文本和模块名的 AST 源码视图。
    diagnostic_collector diagnostics{}; ///< 当前语义分析过程累计的诊断信息。
    semantic_result result{}; ///< 正在构建的语义结果，保存类型、符号、绑定和转换信息。
    std::vector<semantic_unit_state> units{}; ///< 待分析的翻译单元状态列表。
    std::map<std::string, std::map<std::string, symbol_id>> module_functions{}; ///< 按内部模块 key 索引的本模块函数符号表。
    std::map<std::string, std::map<overload_operator_kind, std::vector<symbol_id>>> module_operators{}; ///< 按内部模块 key 索引的本模块 operator 符号表。
    std::map<std::string, std::map<std::string, symbol_id>> module_exports{}; ///< 按模块名索引的导出函数符号表。
    std::map<std::string, std::map<overload_operator_kind, std::vector<symbol_id>>> module_operator_exports{}; ///< 按模块名索引的导出 operator 符号表。
    std::map<std::string, std::map<std::string, symbol_id>> module_types{}; ///< 按内部模块 key 索引的本模块类型符号表。
    std::map<std::string, std::map<std::string, symbol_id>> module_type_exports{}; ///< 按模块名索引的导出类型符号表。
    std::map<std::string, std::map<std::string, symbol_id>> module_concepts{}; ///< 按内部模块 key 索引的本模块 concept 符号表。
    std::map<std::string, std::map<std::string, symbol_id>> module_concept_exports{}; ///< 按模块名索引的导出 concept 符号表。
    std::map<semantic_type_id, std::map<std::string, semantic_type_id>> associated_types{}; ///< 类型的扁平关联类型命名空间。
    std::map<std::pair<symbol_id, semantic_type_id>, std::size_t> concept_impl_index{}; ///< concept + concrete type 的实现事实索引。
    std::vector<std::size_t> generic_concept_impl_indices{}; ///< 目标类型模式含泛型参数的 concept impl。
    std::map<std::size_t, std::vector<std::string>> concept_impl_generic_parameters{}; ///< concept impl 目标类型模式引入的泛型参数。
    std::map<std::size_t, concept_requires_syntax> concept_impl_requires{}; ///< 条件 concept impl 的 requires 子句。
    std::map<return_inference_key, std::vector<std::string>> implicit_function_generic_parameters{}; ///< impl 目标类型模式引入的函数泛型参数。
    std::map<return_inference_key, semantic_type_id> function_impl_target_patterns{}; ///< impl 函数所属目标类型模式，用于从 receiver 推导隐式泛型实参。
    std::map<return_inference_key, concept_requires_syntax> function_impl_requires{}; ///< 条件 impl 的 requires 子句。
    std::map<std::string, semantic_type_id> active_generic_parameters{}; ///< 当前声明中可见的泛型形参。
    std::set<std::string> active_generic_parameter_packs{}; ///< 当前声明中可见的类型参数包名。
    std::map<std::string, semantic_type_id> const* active_type_substitutions{}; ///< lowering 时的泛型类型替换。
    std::map<std::string, std::vector<semantic_type_id>> const* active_type_pack_substitutions{}; ///< lowering 时的类型参数包替换。
    semantic_type_id active_self_type{}; ///< concept / impl 相关类型 lowering 中的 this 替换目标。
    std::map<std::string, semantic_type_id> const* active_type_aliases{}; ///< concept impl 当前可见关联类型。
    std::size_t active_context_index{}; ///< 当前语义 side table 写入的实例上下文；0 表示源程序根上下文。
    std::size_t next_context_index{ 1uz }; ///< 泛型实例和 template for 展开使用的唯一上下文编号源。
    std::size_t active_unit_index{}; ///< 当前函数体检查所在的翻译单元下标。
    semantic_unit_state const* active_unit{}; ///< 当前函数体检查所在的翻译单元状态。
    ast_arena const* active_ast{}; ///< 当前函数体检查使用的 AST arena。
    function_id active_function{}; ///< 当前正在检查的函数语法节点 id。
    symbol_id active_self{}; ///< 当前成员函数中的 self 绑定。
    std::vector<std::map<std::string, symbol_id>> scopes{}; ///< 函数体检查阶段的词法作用域栈。
    std::vector<std::map<std::string, semantic_type_id>> type_scopes{}; ///< 函数体内 type alias 的词法作用域栈。
    std::map<std::string, std::vector<symbol_id>> active_value_packs{}; ///< 当前函数实例中可展开的值参数包。
    std::vector<loop_label> loops{}; ///< 当前嵌套循环标签栈，用于校验跳转语句。
    std::size_t active_template_for_depth{}; ///< 当前 template for 展开深度，用于限制 break/continue 穿透。
    std::vector<lambda_capture_context> lambda_capture_stack{}; ///< 当前正在语义检查的嵌套 lambda 捕获上下文。
    std::map<return_inference_key, return_inference_state> return_states{}; ///< 函数返回类型推断状态表。
    std::vector<std::map<std::string, return_inference_binding>> return_scopes{}; ///< 返回类型推断阶段的词法绑定栈。
    std::size_t return_unit{}; ///< 当前返回类型推断所在的翻译单元下标。
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
