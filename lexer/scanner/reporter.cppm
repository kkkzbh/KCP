export module lexer.diagnostic_reporter;

import std;
import lexer.diagnostic;

/// @brief 词法诊断接收器的轻量类型擦除包装。
/// @details 只擦除 `report(diagnostic)` 这一条出口，避免 scanner 状态机跟随 sink 类型模板化。
export struct diagnostic_reporter
{
    using report_function = void (*)(void*, diagnostic);

    auto report(diagnostic value) const -> void
    {
        report_fn(object, std::move(value));
    }

    void* object{};
    report_function report_fn{};
};

/// @brief 将任意满足 `diagnostic_sink` 的对象适配为固定 reporter 类型。
export template<diagnostic_sink Sink>
auto make_diagnostic_reporter(Sink& sink) -> diagnostic_reporter
{
    return diagnostic_reporter {
        .object = &sink,
        .report_fn = [](void* object, diagnostic value) -> void {
            static_cast<Sink*>(object)->report(std::move(value));
        },
    };
}
