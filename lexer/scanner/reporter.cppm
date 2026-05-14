export module lexer.diagnostic_reporter;

import std;
import lexer.diagnostic;
import lexer.diagnostic.concepts;

/// @brief 词法诊断接收器的轻量类型擦除包装。
/// @details 只擦除 `report(lexer_diagnostic)` 这一条出口，避免 scanner 状态机跟随 sink 类型模板化。
export struct diagnostic_reporter
{
    using report_function = void (*)(void*, lexer_diagnostic);

    auto report(lexer_diagnostic value) const -> void
    {
        report_fn(object, std::move(value));
    }

    void* object{};
    report_function report_fn{};
};

template<typename Sink>
auto report_lexer_diagnostic(Sink& sink, lexer_diagnostic value) -> void
{
    if constexpr(lexer_diagnostic_report_sink<Sink>)
    {
        sink.report(std::move(value));
    }
    else
    {
        sink.push_back(std::move(value));
    }
}

/// @brief 将任意满足 `lexer_diagnostic_sink` 的对象适配为固定 reporter 类型。
export template<lexer_diagnostic_sink Sink>
auto make_diagnostic_reporter(Sink& sink) -> diagnostic_reporter
{
    return diagnostic_reporter {
        .object = &sink,
        .report_fn = [](void* object, lexer_diagnostic value) -> void {
            report_lexer_diagnostic(*static_cast<Sink*>(object), std::move(value));
        },
    };
}
