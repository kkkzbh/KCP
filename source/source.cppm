export module source;

import std;

export using file_id = std::uint32_t;

/// @brief 全局字节位置。所有源文件被映射到一个共享的 32 位地址空间，每个文件之间留 1
///        字节哨兵以保证 EOF 位置可被无歧义反查。参见 clang `SourceLocation` /
///        rustc `BytePos`。
export using byte_pos = std::uint32_t;

/// @brief 表示全局地址空间中的半开区间 `[start, end)`。
/// @details `source_span` 已不再携带 `file_id`：跨文件归属由 `source_manager::locate()`
///          反查得到，因此一份 span 仅 8 字节。
export struct source_span
{
    /// @brief 判断两个 `source_span` 是否表示同一段全局范围。
    auto constexpr operator==(source_span const& other) const -> bool = default;

    byte_pos start{}; ///< 全局起点，包含该位置。
    byte_pos end{};   ///< 全局终点，不包含该位置。
};

/// @brief 表示源代码中的行号和列号。
/// @details 行号和列号均从 `1` 开始计数，适合直接用于诊断展示。
export struct source_position
{
    /// @brief 判断两个 `source_position` 是否相等。
    auto constexpr operator==(source_position const& other) const -> bool = default;

    std::size_t line{};   ///< 行号，从 `1` 开始。
    std::size_t column{}; ///< 列号，从 `1` 开始。
};

/// @brief 维护多份源文件并将它们映射到共享的 u32 全局地址空间。
/// @details 每个新加入的文件得到一段连续的全局区间 `[start, start + size)`；相邻文件之间
///          额外保留 1 字节哨兵，用于承载文件 EOF 这一有效坐标，避免与下一个文件的首字节
///          撞车。所有外部公开的 `source_span`/`byte_pos` 都活在这个全局空间里。
export struct source_manager
{
    /// @brief 添加一份源文件并返回它的编号。
    /// @return 新加入源文件对应的 `file_id`。
    /// @note 行起始偏移表仍以 *文件内* 偏移记录，方便快速定位行列。
    auto add_source(std::string name, std::string text) -> file_id
    {
        auto const start = next_offset_;
        auto const size = static_cast<byte_pos>(text.size());
        next_offset_ = start + size + 1; // +1 哨兵位

        files_.emplace_back(std::move(name), std::move(text), start);
        file_starts_.push_back(start);
        return static_cast<file_id>(files_.size() - 1);
    }

    /// @brief 获取指定源文件的名字视图。
    auto name(file_id id) const -> std::string_view
    {
        return files_[id].name;
    }

    /// @brief 获取指定源文件的文本视图。
    auto text(file_id id) const -> std::string_view
    {
        return files_[id].text;
    }

    /// @brief 返回指定文件在全局地址空间中的起点。
    /// @details scanner 把"文件内偏移"翻译成 `byte_pos` 时使用。
    auto file_start(file_id id) const -> byte_pos
    {
        return files_[id].start;
    }

    /// @brief 把全局位置反查为 `(文件编号, 文件内偏移)`。
    /// @details 在文件起点表上做 `upper_bound - 1` 的二分。EOF 哨兵位归属上一个文件。
    auto locate(byte_pos pos) const -> std::pair<file_id, byte_pos>
    {
        auto const it = std::ranges::upper_bound(file_starts_, pos);
        auto const idx = static_cast<file_id>(std::distance(file_starts_.begin(), it) - 1);
        return { idx, pos - files_[idx].start };
    }

    /// @brief 取出 `source_span` 对应的文本片段。
    /// @details 假设 span 落在单个文件内（当前编译期所有 span 都满足）。
    auto slice(source_span value) const -> std::string_view
    {
        auto const [id, local] = locate(value.start);
        auto const length = static_cast<std::size_t>(value.end - value.start);
        auto const text = std::string_view(files_[id].text);
        return { text.data() + local, length };
    }

    /// @brief 将全局位置转换为行列。
    /// @note EOF 哨兵位按该文件文本末尾计算位置。
    auto position(byte_pos pos) const -> source_position
    {
        auto const [id, local] = locate(pos);
        auto const& file = files_[id];
        auto const offset = static_cast<std::size_t>(local);
        auto const it = std::ranges::upper_bound(file.line_starts, offset);
        auto const line_index = static_cast<std::size_t>(std::distance(file.line_starts.begin(), it) - 1);
        auto const line_start = file.line_starts[line_index];

        return source_position {
            .line = line_index + 1,
            .column = offset - line_start + 1,
        };
    }

private:
    /// @brief 单个已加载源文件的内部表示。
    struct source_file
    {
        source_file(std::string name, std::string text, byte_pos start)
            : name(std::move(name)), text(std::move(text)), start(start), line_starts{ 0 }
        {
            for(auto pos = this->text.find('\n'); pos != std::string::npos; pos = this->text.find('\n', pos + 1))
            {
                line_starts.push_back(pos + 1);
            }
        }

        std::string name;                    ///< 源文件名字。
        std::string text;                    ///< 源文件文本内容。
        byte_pos start;                      ///< 该文件在全局地址空间中的起点。
        std::vector<std::size_t> line_starts;///< 每一行起始位置在 `text` 中的*文件内*偏移量。
    };

    std::vector<source_file> files_;     ///< 已加载源文件的顺序存储。
    std::vector<byte_pos> file_starts_;  ///< 与 `files_` 同序的全局起点表，供 `locate()` 二分。
    byte_pos next_offset_ = 0;           ///< 下一个待分配的全局起点。
};
