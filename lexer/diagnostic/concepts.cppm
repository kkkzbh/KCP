export module lexer.diagnostic.concepts;

import std;
import lexer.diagnostic;

/// @brief 约束提供 `report(lexer_diagnostic)` 出口的词法诊断接收器。
export template<typename Sink>
concept lexer_diagnostic_report_sink = requires(Sink& sink, lexer_diagnostic value)
{
    { sink.report(std::move(value)) } -> std::same_as<void>;
};

/// @brief 约束可直接追加词法诊断的容器式接收器。
export template<typename Sink>
concept lexer_diagnostic_push_sink = requires(Sink& sink, lexer_diagnostic value)
{
    sink.push_back(std::move(value));
};

/// @brief 约束可作为词法诊断接收器使用的类型。
export template<typename Sink>
concept lexer_diagnostic_sink =
    lexer_diagnostic_report_sink<Sink> or lexer_diagnostic_push_sink<Sink>;
