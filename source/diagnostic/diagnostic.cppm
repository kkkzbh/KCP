export module source.diagnostic;

import std;
import source;
import source.diagnostic.concepts;

export template<diagnostic_enum Severity, diagnostic_enum Code>
struct diagnostic
{
    Severity severity{};
    Code code{};
    std::string message;
    source_span primary_span{};
};
