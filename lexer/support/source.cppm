export module lexer.source;

import std;

export using file_id = std::uint32_t;

/// @brief 表示源代码中的半开区间 `[start, end)`。
/// @details `span` 是词法和语法阶段共享的基础定位类型，约定 `end` 为开区间边界。
export struct span
{
    /// @brief 判断两个 `span` 是否表示同一段源范围。
    /// @param other 待比较的源范围。
    /// @return 若两个 `span` 的文件编号和边界都相同，则返回 `true`。
    constexpr auto operator==(span const& other) const -> bool = default;

    file_id file{};     ///< 所属源文件的编号。
    std::size_t start{};///< 片段起始偏移，包含该位置。
    std::size_t end{};  ///< 片段结束偏移，不包含该位置。
};

/// @brief 表示源代码中的行号和列号。
/// @details 行号和列号均从 `1` 开始计数，适合直接用于诊断展示。
export struct source_position
{
    /// @brief 判断两个 `source_position` 是否相等。
    /// @param other 待比较的源位置。
    /// @return 若两个位置的行号和列号都相同，则返回 `true`。
    constexpr auto operator==(source_position const& other) const -> bool = default;

    std::size_t line{};   ///< 行号，从 `1` 开始。
    std::size_t column{}; ///< 列号，从 `1` 开始。
};

/// @brief 维护多份源文件的名字、文本内容和位置信息。
/// @details `source_manager` 为后续阶段提供统一的文本切片与偏移转行列能力。
export struct source_manager
{
    /// @brief 添加一份源文件并返回它的编号。
    /// @param name 源文件名字。
    /// @param text 源文件完整文本。
    /// @return 新加入源文件对应的 `file_id`。
    /// @note 该函数会预先构建行起始偏移表，以支持后续快速定位。
    auto add_source(std::string name, std::string text) -> file_id
    {
        files_.emplace_back(std::move(name), std::move(text));
        return static_cast<file_id>(files_.size() - 1);
    }

    /// @brief 获取指定源文件的名字视图。
    /// @param id 源文件编号。
    /// @return 指向源文件名字的只读视图。
    auto name(file_id id) const -> std::string_view { return files_.at(id).name; }

    /// @brief 获取指定源文件的文本视图。
    /// @param id 源文件编号。
    /// @return 指向源文件文本的只读视图。
    auto text(file_id id) const -> std::string_view { return files_.at(id).text; }

    /// @brief 取出 `span` 对应的文本片段。
    /// @param value 要切片的源范围。
    /// @return 与该范围对应的只读文本视图。
    auto slice(span const& value) const -> std::string_view
    {
        auto const& file = files_.at(value.file);
        return std::string_view(file.text).substr(value.start, value.end - value.start);
    }

    /// @brief 将偏移量转换为行列位置。
    /// @param id 源文件编号。
    /// @param offset 源文件中的字符偏移量。
    /// @return 该偏移量对应的 `source_position`。
    /// @note 若偏移越过文本末尾，会先被截断到文件末尾再计算位置。
    auto position(file_id id, std::size_t offset) const -> source_position
    {
        auto const& file = files_.at(id);
        auto const safe_offset = std::min(offset, file.text.size());
        auto const it = std::ranges::upper_bound(file.line_starts, safe_offset);
        auto const line_index = static_cast<std::size_t>(std::distance(file.line_starts.begin(), it) - 1);
        auto const line_start = file.line_starts[line_index];

        return source_position{
            .line = line_index + 1,
            .column = safe_offset - line_start + 1,
        };
    }

private:
    /// @brief 单个已加载源文件的内部表示。
    struct source_file
    {
        source_file(std::string name, std::string text) : name(std::move(name)), text(std::move(text)), line_starts{ 0 }
        {
            std::ranges::for_each (
                std::views::iota(0uz, this->text.size())
                | std::views::filter([&](auto const index) {
                    return this->text[index] == '\n';
                }),
                [&](auto const index) {
                    line_starts.push_back(index + 1);
                }
            );
        }

        std::string name;                    ///< 源文件名字。
        std::string text;                    ///< 源文件文本内容。
        std::vector<std::size_t> line_starts;///< 每一行起始位置在 `text` 中的偏移量。
    };

    std::vector<source_file> files_; ///< 已加载源文件的顺序存储。
};
