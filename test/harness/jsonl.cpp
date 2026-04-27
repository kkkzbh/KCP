#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "jsonl.hpp"

namespace test_support {
namespace {

using json = nlohmann::ordered_json;

[[noreturn]]
auto fail_jsonl(std::filesystem::path const& path, std::size_t line, std::string const& message) -> void
{
    throw std::runtime_error(std::format("{}:{} {}", path.string(), line, message));
}

[[nodiscard]]
auto find_field(jsonl_object const& object, std::string_view key) -> jsonl_value const*
{
    for (auto const& [field_key, value] : object.fields) {
        if (field_key == key) {
            return &value;
        }
    }

    return nullptr;
}

} // namespace

auto read_jsonl(std::filesystem::path const& path, bool required) -> std::vector<jsonl_object>
{
    if (not std::filesystem::exists(path)) {
        if (required) {
            throw std::runtime_error(std::format("missing JSONL expectation file {}", path.string()));
        }
        return {};
    }

    auto stream = std::ifstream{path};
    if (not stream.is_open()) {
        throw std::runtime_error(std::format("failed to open {}", path.string()));
    }

    auto result = std::vector<jsonl_object>{};
    auto line = std::string{};
    auto line_number = 0uz;
    while (std::getline(stream, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        try {
            auto parsed = json::parse(line);
            if (not parsed.is_object()) {
                fail_jsonl(path, line_number, "expected JSON object");
            }

            auto object = jsonl_object{.line = line_number};
            for (auto const& [key, value] : parsed.items()) {
                if (value.is_string()) {
                    object.fields.emplace_back(key, value.get<std::string>());
                    continue;
                }
                if (value.is_number_integer()) {
                    auto const signed_value = value.get<std::int64_t>();
                    if (signed_value < 0) {
                        fail_jsonl(path, line_number, std::format("field '{}' must not be negative", key));
                    }
                    object.fields.emplace_back(key, static_cast<std::size_t>(signed_value));
                    continue;
                }

                fail_jsonl(path, line_number, std::format("field '{}' must be string or integer", key));
            }

            result.push_back(std::move(object));
        } catch (nlohmann::json::exception const& error) {
            fail_jsonl(path, line_number, std::format("invalid JSON: {}", error.what()));
        }
    }

    return result;
}

auto required_string(
    jsonl_object const& object,
    std::filesystem::path const& path,
    std::string_view key) -> std::string
{
    auto const* value = find_field(object, key);
    if (value == nullptr) {
        fail_jsonl(path, object.line, std::format("missing string field '{}'", key));
    }

    if (auto const* string_value = std::get_if<std::string>(value)) {
        return *string_value;
    }

    fail_jsonl(path, object.line, std::format("field '{}' must be a string", key));
}

auto required_size(
    jsonl_object const& object,
    std::filesystem::path const& path,
    std::string_view key) -> std::size_t
{
    auto const* value = find_field(object, key);
    if (value == nullptr) {
        fail_jsonl(path, object.line, std::format("missing integer field '{}'", key));
    }

    if (auto const* size_value = std::get_if<std::size_t>(value)) {
        return *size_value;
    }

    fail_jsonl(path, object.line, std::format("field '{}' must be an integer", key));
}

auto dump_jsonl_record(jsonl_record const& fields) -> std::string
{
    auto record = json{};
    for (auto const& [key, value] : fields) {
        if (auto const* string_value = std::get_if<std::string>(&value)) {
            record[key] = *string_value;
            continue;
        }

        record[key] = std::get<std::size_t>(value);
    }

    return record.dump();
}

auto join_jsonl(std::vector<jsonl_record> const& values) -> std::string
{
    auto result = std::string{};
    for (auto const& value : values) {
        result += dump_jsonl_record(value);
        result += '\n';
    }
    return result;
}

} // namespace test_support
