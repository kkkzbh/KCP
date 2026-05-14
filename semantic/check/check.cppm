export module semantic.check;

import std;
import source;
import parser;
import semantic.result;
export import semantic.check.keys;
export import :concepts;
import :analyzer;

export auto analyze_semantics(source_manager const& sources, std::span<parse_result const> units) -> semantic_result
{
    auto analyzer = semantic_analyzer{ sources };
    for(auto const& unit : units) {
        analyzer.add_unit(unit);
    }
    return analyzer.analyze();
}

export template<semantic_parse_result First, semantic_parse_result... Rest>
auto analyze_semantics(source_manager const& sources, First const& first, Rest const&... rest) -> semantic_result
{
    auto analyzer = semantic_analyzer{ sources };
    analyzer.add_unit(first);
    template for(auto const& unit : { rest... }) {
        analyzer.add_unit(unit);
    }
    return analyzer.analyze();
}
