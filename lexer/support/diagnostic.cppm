export module lexer.diagnostic;

import std;
import lexer.source;

export enum class diagnostic_severity
{
    error,
    warning,
};

export enum class diagnostic_code
{
    invalid_character,
    unterminated_string_literal,
    unterminated_char_literal,
    unterminated_block_comment,
    invalid_char_literal,
    invalid_escape_sequence,
    invalid_number_suffix,
};

export struct diagnostic
{
    diagnostic_severity severity{diagnostic_severity::error};
    diagnostic_code code{};
    std::string message;
    span primary_span{};
};

export struct diagnostic_sink
{
    virtual ~diagnostic_sink() = default;
    virtual auto report(diagnostic value) -> void = 0;
};

export struct vector_diagnostic_sink final : diagnostic_sink
{
    auto report(diagnostic value) -> void override;
    [[nodiscard]] auto diagnostics() const -> std::vector<diagnostic> const&;
    auto clear() -> void;

private:
    std::vector<diagnostic> diagnostics_;
};

auto vector_diagnostic_sink::report(diagnostic value) -> void
{
    diagnostics_.push_back(std::move(value));
}

auto vector_diagnostic_sink::diagnostics() const -> std::vector<diagnostic> const&
{
    return diagnostics_;
}

auto vector_diagnostic_sink::clear() -> void
{
    diagnostics_.clear();
}
