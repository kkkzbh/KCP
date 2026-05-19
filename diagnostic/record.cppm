export module diagnostic:record;

import std;
import source;
import :stage;
import :kind;

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
