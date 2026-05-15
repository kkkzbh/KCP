/// @brief 统一的编译阶段诊断表示。
export module diagnostic;

import std;
export import source;

using namespace std::string_view_literals;

export enum class diagnostic_stage : std::uint8_t
{
    preprocessor,
    lexer,
    parser,
    semantic,
};

export enum class diagnostic_severity : std::uint8_t
{
    error,
    warning,
};

export enum class diagnostic_kind : std::uint16_t
{
    unterminated_block_comment,
    invalid_character,
    unterminated_string_literal,
    unterminated_char_literal,
    invalid_char_literal,
    invalid_escape_sequence,
    invalid_number_suffix,
    unexpected_token,
    expected_token,
    expected_identifier,
    expected_expression,
    expected_statement,
    expected_type,
    unknown_type,
    invalid_type_argument,
    duplicate_symbol,
    unknown_name,
    unknown_module,
    import_conflict,
    not_callable,
    argument_count_mismatch,
    type_mismatch,
    condition_not_bool,
    invalid_assignment_target,
    assign_to_const,
    invalid_break,
    invalid_continue,
    unknown_label,
    invalid_range,
    empty_aggregate_without_context,
    aggregate_length_mismatch,
    heterogeneous_aggregate,
    invalid_operator,
    invalid_cast,
    return_type_mismatch,
    cannot_infer_return_type,
};


export auto stage_name(diagnostic_stage stage) -> std::string_view
{
    using enum diagnostic_stage;
    switch(stage) {
        case preprocessor:
            return "preprocessor"sv;
        case lexer:
            return "lexer"sv;
        case parser:
            return "parser"sv;
        case semantic:
            return "semantic"sv;
    }

    std::unreachable();
}

export auto severity_name(diagnostic_severity severity) -> std::string_view
{
    using enum diagnostic_severity;
    switch(severity) {
        case error:
            return "error"sv;
        case warning:
            return "warning"sv;
    }

    std::unreachable();
}

export struct diagnostic_spec
{
    diagnostic_stage stage{};
    diagnostic_severity severity{};
    std::string_view code;
    std::string_view message;
};

export struct diagnostic
{
    diagnostic_kind kind{};
    source_span primary_span{};
    std::string message;
};

export auto spec(diagnostic_kind kind) -> diagnostic_spec
{
    using enum diagnostic_kind;
    using enum diagnostic_stage;
    using enum diagnostic_severity;
    switch(kind) {
        case unterminated_block_comment:
            return { preprocessor, error, "unterminated_block_comment"sv, "unterminated block comment"sv };
        case invalid_character:
            return { lexer, error, "invalid_character"sv, "invalid character"sv };
        case unterminated_string_literal:
            return { lexer, error, "unterminated_string_literal"sv, "unterminated string literal"sv };
        case unterminated_char_literal:
            return { lexer, error, "unterminated_char_literal"sv, "unterminated character literal"sv };
        case invalid_char_literal:
            return { lexer, error, "invalid_char_literal"sv, "invalid character literal"sv };
        case invalid_escape_sequence:
            return { lexer, error, "invalid_escape_sequence"sv, "invalid escape sequence"sv };
        case invalid_number_suffix:
            return { lexer, error, "invalid_number_suffix"sv, "invalid number suffix"sv };
        case unexpected_token:
            return { parser, error, "unexpected_token"sv, "unexpected token"sv };
        case expected_token:
            return { parser, error, "expected_token"sv, "expected token"sv };
        case expected_identifier:
            return { parser, error, "expected_identifier"sv, "expected identifier"sv };
        case expected_expression:
            return { parser, error, "expected_expression"sv, "expected expression"sv };
        case expected_statement:
            return { parser, error, "expected_statement"sv, "expected statement"sv };
        case expected_type:
            return { parser, error, "expected_type"sv, "expected type"sv };
        case unknown_type:
            return { semantic, error, "unknown_type"sv, "unknown type"sv };
        case invalid_type_argument:
            return { semantic, error, "invalid_type_argument"sv, "invalid type argument"sv };
        case duplicate_symbol:
            return { semantic, error, "duplicate_symbol"sv, "duplicate symbol"sv };
        case unknown_name:
            return { semantic, error, "unknown_name"sv, "unknown name"sv };
        case unknown_module:
            return { semantic, error, "unknown_module"sv, "unknown module"sv };
        case import_conflict:
            return { semantic, error, "import_conflict"sv, "import conflict"sv };
        case not_callable:
            return { semantic, error, "not_callable"sv, "not callable"sv };
        case argument_count_mismatch:
            return { semantic, error, "argument_count_mismatch"sv, "argument count mismatch"sv };
        case type_mismatch:
            return { semantic, error, "type_mismatch"sv, "type mismatch"sv };
        case condition_not_bool:
            return { semantic, error, "condition_not_bool"sv, "condition is not bool"sv };
        case invalid_assignment_target:
            return { semantic, error, "invalid_assignment_target"sv, "invalid assignment target"sv };
        case assign_to_const:
            return { semantic, error, "assign_to_const"sv, "cannot assign to const binding"sv };
        case invalid_break:
            return { semantic, error, "invalid_break"sv, "invalid break statement"sv };
        case invalid_continue:
            return { semantic, error, "invalid_continue"sv, "invalid continue statement"sv };
        case unknown_label:
            return { semantic, error, "unknown_label"sv, "unknown label"sv };
        case invalid_range:
            return { semantic, error, "invalid_range"sv, "invalid range"sv };
        case empty_aggregate_without_context:
            return { semantic, error, "empty_aggregate_without_context"sv, "empty aggregate requires context"sv };
        case aggregate_length_mismatch:
            return { semantic, error, "aggregate_length_mismatch"sv, "aggregate length mismatch"sv };
        case heterogeneous_aggregate:
            return { semantic, error, "heterogeneous_aggregate"sv, "heterogeneous aggregate"sv };
        case invalid_operator:
            return { semantic, error, "invalid_operator"sv, "invalid operator"sv };
        case invalid_cast:
            return { semantic, error, "invalid_cast"sv, "invalid cast"sv };
        case return_type_mismatch:
            return { semantic, error, "return_type_mismatch"sv, "return type mismatch"sv };
        case cannot_infer_return_type:
            return { semantic, error, "cannot_infer_return_type"sv, "cannot infer return type"sv };
    }

    std::unreachable();
}

export struct diagnostic_collector
{
    auto report(diagnostic_kind kind, source_span span) -> void
    {
        auto const info = spec(kind);
        diagnostics_.emplace_back(kind, span, std::string{ info.message });
    }

    auto report(diagnostic_kind kind, std::string message, source_span span) -> void
    {
        diagnostics_.emplace_back(kind, span, std::move(message));
    }

    auto append(diagnostic value) -> void
    {
        diagnostics_.emplace_back(std::move(value));
    }

    auto append_range(std::ranges::input_range auto&& diagnostics) -> void
    {
        for(auto&& value : diagnostics) {
            append(std::forward<decltype(value)>(value));
        }
    }

    auto clear() -> void
    {
        diagnostics_.clear();
    }

    auto empty() const -> bool
    {
        return diagnostics_.empty();
    }

    auto size() const -> std::size_t
    {
        return diagnostics_.size();
    }

    auto span() const -> std::span<diagnostic const>
    {
        return std::span{ diagnostics_ };
    }

    auto take() -> std::vector<diagnostic>
    {
        return std::exchange(diagnostics_, {});
    }

private:
    std::vector<diagnostic> diagnostics_{};
};
