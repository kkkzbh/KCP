export module diagnostic:stage;

import std;

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

export auto stage_name(diagnostic_stage stage) -> std::string_view
{
    using enum diagnostic_stage;
    switch(stage) {
        case preprocessor:
            return "preprocessor";
        case lexer:
            return "lexer";
        case parser:
            return "parser";
        case semantic:
            return "semantic";
    }

    std::unreachable();
}

export auto severity_name(diagnostic_severity severity) -> std::string_view
{
    using enum diagnostic_severity;
    switch(severity) {
        case error:
            return "error";
        case warning:
            return "warning";
    }

    std::unreachable();
}
