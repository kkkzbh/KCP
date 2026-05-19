export module diagnostic:collector;

import std;
import :catalog;

export struct diagnostic_collector
{
    auto report(diagnostic_kind kind, source_span span) -> void
    {
        auto const info = spec(kind);
        diagnostics_.emplace_back(kind, span, std::string{ info.message });
    }

    auto report(diagnostic_kind kind, std::string_view message, source_span span) -> void
    {
        diagnostics_.emplace_back(kind, span, std::string{ message });
    }

    auto report(diagnostic_kind kind, char const* message, source_span span) -> void
    {
        report(kind, std::string_view{ message }, span);
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
