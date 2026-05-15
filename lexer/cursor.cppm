/// @brief 词法扫描游标与源码位置映射。
/// @details 该模块只维护当前位置和局部 offset 到全局 source_span 的转换，不参与 token 分类。
export module lexer.cursor;

import std;
import source;
import preprocessor.preprocessed;
import lexer.token;

export struct lexer_cursor
{
    auto reset(preprocessed_file const& preprocessed) -> void
    {
        file_start_ = preprocessed.file_start;
        source_ = preprocessed.normalized_text;
        offset_ = 0;
    }

    auto source() const -> std::string_view
    {
        return source_;
    }

    auto eof() const -> bool
    {
        return offset_ >= source().size();
    }

    auto peek_char(std::size_t distance = 0) const -> char
    {
        auto const index = offset_ + distance;
        auto const text = source();
        return index < text.size() ? text[index] : '\0';
    }

    auto current() const -> char
    {
        return peek_char();
    }

    auto advance(std::size_t count = 1) -> void
    {
        offset_ += count;
    }

    auto offset() const -> std::size_t
    {
        return offset_;
    }

    auto set_offset(std::size_t value) -> void
    {
        offset_ = value;
    }

    auto current_pos() const -> byte_pos
    {
        return to_global(offset_);
    }

    auto to_global(std::size_t local) const -> byte_pos
    {
        return file_start_ + static_cast<byte_pos>(local);
    }

    auto to_local(byte_pos global) const -> std::size_t
    {
        return static_cast<std::size_t>(global - file_start_);
    }

    auto make_span(std::size_t start, std::size_t end) const -> source_span
    {
        return source_span{ .start = to_global(start), .end = to_global(end) };
    }

    auto make_token(token_kind kind, std::size_t start, std::size_t end, token_flags flags) const -> token
    {
        return token {
            .kind = kind,
            .span = make_span(start, end),
            .flags = flags,
        };
    }

private:
    byte_pos file_start_{};
    std::string_view source_;
    std::size_t offset_{};
};
