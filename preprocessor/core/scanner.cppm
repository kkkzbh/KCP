export module preprocessor.scanner;

import std;
import lexer.source;
import preprocessor.issue;
import preprocessor.output;

/// @brief 预处理扫描器，负责剥离注释并保留源码偏移。
/// @details 扫描器在不改变源文本长度的前提下，把注释位置替换为空格，同时保留换行字符，
/// 这样后续阶段仍能凭借原始偏移映射出正确的行列信息。
export struct preprocessor_scanner
{
    /// @brief 构造预处理扫描器。
    /// @param sources 源文件管理器，用于读取输入文本。
    /// @param file 要预处理的文件编号。
    /// @return 无返回值。
    preprocessor_scanner(source_manager const& sources, file_id file)
        : sources_(sources), file_(file), source_(sources.text(file)), normalized_(std::string{ source_ })
    {}

    /// @brief 执行完整的预处理扫描。
    /// @return 规范化文本与问题列表。
    /// @note 该函数会消耗扫描器内部的中间状态，不应重复调用。
    auto run() -> preprocessed_file
    {
        while(!eof()) {
            auto const ch = current();

            if(ch == '"' || ch == '\'') {
                skip_quoted_literal(ch);
                continue;
            }

            if(ch == '/' && peek_char() == '/') {
                skip_line_comment();
                continue;
            }

            if(ch == '/' && peek_char() == '*') {
                skip_block_comment();
                continue;
            }

            advance();
        }

        return preprocessed_file {
            .normalized_text = std::move(normalized_),
            .issues = std::move(issues_),
        };
    }

private:
    /// @brief 判断扫描位置是否已经到达输入末尾。
    /// @return 若当前偏移已越过或到达输入末尾，则返回 `true`。
    auto eof() const -> bool
    {
        return index_ >= source_.size();
    }

    /// @brief 返回当前位置的字符；到达末尾时返回 `\0`。
    /// @return 当前偏移对应的字符；若已到末尾则返回 `\0`。
    auto current() const -> char
    {
        return eof() ? '\0' : source_[index_];
    }

    /// @brief 返回下一个字符；到达末尾时返回 `\0`。
    /// @return 当前偏移后一个位置的字符；若不存在则返回 `\0`。
    auto peek_char() const -> char
    {
        auto const next = index_ + 1;
        return next < source_.size() ? source_[next] : '\0';
    }

    /// @brief 将扫描位置向前移动指定字符数。
    /// @param count 要前进的字符数，默认为 1。
    /// @return 无返回值。
    auto advance(std::size_t count = 1) -> void
    {
        index_ += count;
    }

    /// @brief 将规范化文本中指定偏移位置替换为空格。
    /// @param offset 要替换的目标偏移。
    /// @return 无返回值。
    auto blank_at(std::size_t offset) -> void
    {
        normalized_[offset] = ' ';
    }

    /// @brief 跳过一个引号包裹的字面量，保持其内部字符不被当作注释。
    /// @param delimiter 字面量使用的引号字符（`"` 或 `'`）。
    /// @return 无返回值。
    /// @note 该函数在遇到换行或文件末尾时提前返回，避免越界和误吞下一行。
    auto skip_quoted_literal(char delimiter) -> void
    {
        advance();

        while(!eof()) {
            auto const ch = current();

            if(ch == '\\') {
                advance();
                if(!eof()) {
                    advance();
                }
                continue;
            }

            if(ch == delimiter) {
                advance();
                return;
            }

            if(ch == '\n') {
                return;
            }

            advance();
        }
    }

    /// @brief 消费一个 `//` 行注释并将其整段置为空格。
    /// @return 无返回值。
    auto skip_line_comment() -> void
    {
        blank_at(index_);
        blank_at(index_ + 1);
        advance(2);

        while(!eof() && current() != '\n') {
            blank_at(index_);
            advance();
        }
    }

    /// @brief 消费一个 `/* */` 块注释；若未闭合则追加一条问题记录。
    /// @return 无返回值。
    /// @note 块注释中的换行会原样保留，以维持源码的行列对齐。
    auto skip_block_comment() -> void
    {
        auto const start = index_;
        blank_at(index_);
        blank_at(index_ + 1);
        advance(2);

        while(!eof()) {
            if(current() == '*' && peek_char() == '/') {
                blank_at(index_);
                blank_at(index_ + 1);
                advance(2);
                return;
            }

            if(current() != '\n') {
                blank_at(index_);
            }
            advance();
        }

        report(preprocess_issue_kind::unterminated_block_comment, start, source_.size());
    }

    /// @brief 向问题列表追加一条预处理问题。
    /// @param kind 问题的具体类别。
    /// @param start 问题片段起始偏移。
    /// @param end 问题片段结束偏移。
    /// @return 无返回值。
    auto report(preprocess_issue_kind kind, std::size_t start, std::size_t end) -> void
    {
        issues_.push_back(preprocess_issue{
            .kind = kind,
            .source_span = span{ .file = file_,.start = start,.end = end },
        });
    }

    source_manager const& sources_;       ///< 源文件管理器。
    file_id file_{};                      ///< 当前正在预处理的文件编号。
    std::string_view source_;             ///< 原始源码只读视图。
    std::string normalized_;              ///< 正在构建的规范化文本，初始化为源码副本。
    std::vector<preprocess_issue> issues_;///< 已记录的问题列表。
    std::size_t index_{};                 ///< 当前扫描偏移。
};

/// @brief 预处理指定文件，剥离注释并收集结构性问题。
/// @param sources 源文件管理器，用于读取输入文本。
/// @param file 要预处理的文件编号。
/// @return 规范化文本与问题列表。
export auto preprocess(source_manager const& sources, file_id file) -> preprocessed_file
{
    return preprocessor_scanner{ sources, file }.run();
}
