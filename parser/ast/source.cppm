export module parser.ast.source;

import std;
import source;
import parser.ast.name;

export struct ast_source_view
{
    explicit ast_source_view(source_manager const& sources) :
        sources(sources) {}

    auto slice(source_span span) const -> std::string_view
    {
        return sources.slice(span);
    }

    auto identifier(source_span span) const -> std::string_view
    {
        return slice(span);
    }

    auto module_name(module_name_syntax const& syntax) const -> std::string
    {
        return (
            syntax.components
            | std::views::transform([&](source_span component) {
                return slice(component);
            })
            | std::views::join_with('.')
            | std::ranges::to<std::string>()
        );
    }

    source_manager const& sources;
};
