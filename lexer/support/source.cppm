export module lexer.source;

import std;

export namespace front {

using file_id = std::uint32_t;

struct span {
    file_id file{};
    std::size_t start{};
    std::size_t end{};

    [[nodiscard]] constexpr auto operator==(span const&) const -> bool = default;
};

struct source_position {
    std::size_t line{};
    std::size_t column{};

    [[nodiscard]] constexpr auto operator==(source_position const&) const -> bool = default;
};

class source_manager {
public:
    [[nodiscard]] auto add_source(std::string name, std::string text) -> file_id;
    [[nodiscard]] auto name(file_id id) const -> std::string_view;
    [[nodiscard]] auto text(file_id id) const -> std::string_view;
    [[nodiscard]] auto slice(span value) const -> std::string_view;
    [[nodiscard]] auto position(file_id id, std::size_t offset) const -> source_position;

private:
    struct source_file {
        std::string name;
        std::string text;
        std::vector<std::size_t> line_starts;
    };

    std::vector<source_file> files_;
};

} // namespace front

namespace front {

auto source_manager::add_source(std::string name, std::string text) -> file_id
{
    auto file = source_file{
        .name = std::move(name),
        .text = std::move(text),
        .line_starts = {0},
    };

    for (std::size_t index = 0; index < file.text.size(); ++index) {
        if (file.text[index] == '\n') {
            file.line_starts.push_back(index + 1);
        }
    }

    files_.push_back(std::move(file));
    return static_cast<file_id>(files_.size() - 1);
}

auto source_manager::name(file_id id) const -> std::string_view
{
    return files_.at(id).name;
}

auto source_manager::text(file_id id) const -> std::string_view
{
    return files_.at(id).text;
}

auto source_manager::slice(span value) const -> std::string_view
{
    auto const& file = files_.at(value.file);
    return std::string_view(file.text).substr(value.start, value.end - value.start);
}

auto source_manager::position(file_id id, std::size_t offset) const -> source_position
{
    auto const& file = files_.at(id);
    auto const safe_offset = std::min(offset, file.text.size());
    auto const it = std::upper_bound(file.line_starts.begin(), file.line_starts.end(), safe_offset);
    auto const line_index = static_cast<std::size_t>(std::distance(file.line_starts.begin(), it) - 1);
    auto const line_start = file.line_starts[line_index];

    return source_position{
        .line = line_index + 1,
        .column = safe_offset - line_start + 1,
    };
}

} // namespace front
