#pragma once

namespace test_support {

using jsonl_value = std::variant<std::string, std::size_t>;
using jsonl_record = std::vector<std::pair<std::string, jsonl_value>>;

struct jsonl_object
{
    jsonl_record fields;
    std::size_t line{};
};

[[nodiscard]]
auto read_jsonl(std::filesystem::path const& path, bool required) -> std::vector<jsonl_object>;

[[nodiscard]]
auto required_string(jsonl_object const& object, std::filesystem::path const& path, std::string_view key) -> std::string;

[[nodiscard]]
auto required_size(jsonl_object const& object, std::filesystem::path const& path, std::string_view key) -> std::size_t;

[[nodiscard]]
auto dump_jsonl_record(jsonl_record const& fields) -> std::string;

[[nodiscard]]
auto join_jsonl(std::vector<jsonl_record> const& values) -> std::string;

} // namespace test_support
