export module parser.ast.name;

import std;
import source;

export auto combine_spans(source_span left, source_span right) -> source_span;

export struct qualified_name_syntax
{
    auto constexpr operator==(qualified_name_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<source_span> components{};
};

auto combine_spans(source_span left, source_span right) -> source_span
{
    return source_span {
        .start = std::min(left.start, right.start),
        .end = std::max(left.end, right.end),
    };
}
