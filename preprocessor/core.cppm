export module preprocessor.core;

import std;
import lexer.source;

export enum class preprocess_issue_kind
{
    unterminated_block_comment,
};

export struct preprocess_issue
{
    [[nodiscard]] constexpr auto operator==(preprocess_issue const&) const -> bool = default;

    preprocess_issue_kind kind{};
    span source_span{};
};

export struct preprocessed_file
{
    [[nodiscard]] auto issue_at(std::size_t offset) const -> preprocess_issue const*;

    std::string normalized_text;
    std::vector<preprocess_issue> issues;
};

export [[nodiscard]] auto preprocess(source_manager const& sources, file_id file) -> preprocessed_file;

namespace detail {
auto consume_quoted_literal(std::string_view source, std::size_t& index, char delimiter) -> void
{
    ++index;

    while(index < source.size()) {
        if(source[index] == '\\') {
            ++index;
            if(index < source.size()) {
                ++index;
            }
            continue;
        }

        if(source[index] == delimiter) {
            ++index;
            break;
        }

        if(source[index] == '\n') {
            break;
        }

        ++index;
    }
}
} // namespace detail

auto preprocessed_file::issue_at(std::size_t offset) const -> preprocess_issue const*
{
    auto const it = std::ranges::find(issues, offset, [](preprocess_issue const& issue) {
        return issue.source_span.start;
    });
    return it == issues.end() ? nullptr : &*it;
}

auto preprocess(source_manager const& sources, file_id file) -> preprocessed_file
{
    auto const source = sources.text(file);
    auto normalized = std::string{source};
    auto issues = std::vector<preprocess_issue>{};

    for(auto index = std::size_t{0}; index < source.size();) {
        auto const ch = source[index];

        if(ch == '"' || ch == '\'') {
            detail::consume_quoted_literal(source, index, ch);
            continue;
        }

        if(ch == '/' && index + 1 < source.size()) {
            if(source[index + 1] == '/') {
                normalized[index] = ' ';
                normalized[index + 1] = ' ';
                index += 2;

                while(index < source.size() && source[index] != '\n') {
                    normalized[index] = ' ';
                    ++index;
                }

                continue;
            }

            if(source[index + 1] == '*') {
                auto const start = index;
                normalized[index] = ' ';
                normalized[index + 1] = ' ';
                index += 2;

                auto closed = false;
                while(index < source.size()) {
                    if(source[index] == '*' && index + 1 < source.size() && source[index + 1] == '/') {
                        normalized[index] = ' ';
                        normalized[index + 1] = ' ';
                        index += 2;
                        closed = true;
                        break;
                    }

                    if(source[index] != '\n') {
                        normalized[index] = ' ';
                    }
                    ++index;
                }

                if(!closed) {
                    issues.push_back(preprocess_issue{
                        .kind = preprocess_issue_kind::unterminated_block_comment,
                        .source_span = span{ .file = file,.start = start,.end = source.size() },
                    });
                    break;
                }

                continue;
            }
        }

        ++index;
    }

    return preprocessed_file{
        .normalized_text = std::move(normalized),
        .issues = std::move(issues),
    };
}
