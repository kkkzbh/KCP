#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

auto repeat_count(int count) -> int
{
    if(count <= 1024) {
        return 256;
    }
    if(count <= 8192) {
        return 24;
    }
    return 4;
}

auto stable_requested() -> bool
{
    return std::getenv("CP_BENCH_STABLE") != nullptr;
}

auto word_at(int value) -> std::string_view
{
    auto constexpr static words = std::array<std::string_view, 32> {
        "xenon_delta", "amber_flux", "violet_cedar", "cobalt_ember",
        "silver_mint", "orange_slate", "brass_lumen", "indigo_river",
        "pearl_vector", "hazel_matrix", "crimson_node", "opal_kernel",
        "saffron_loop", "teal_packet", "garnet_cache", "ivory_branch",
        "jade_signal", "maroon_stack", "navy_cursor", "olive_thread",
        "plum_object", "quartz_range", "ruby_token", "topaz_scope",
        "umber_arena", "vermilion_ir", "walnut_parse", "yellow_type",
        "zircon_alloc", "azure_merge", "bronze_sort", "coral_span",
    };
    return words[static_cast<std::size_t>(value % static_cast<int>(words.size()))];
}

auto letter_score(char ch) -> std::int64_t
{
    if(ch == '_') {
        return 27;
    }
    return static_cast<std::int64_t>(ch - 'a' + 1);
}

auto word_score(std::string_view value) -> std::int64_t
{
    auto total = std::int64_t{ 0 };
    auto index = std::size_t{ 0 };
    while(index < value.size()) {
        total = (total * 31 + letter_score(value[index])) % 1'000'000'007;
        index += 1;
    }
    return total;
}

auto checked_checksum(std::vector<std::string_view> const& values) -> std::int64_t
{
    auto total = std::int64_t{ 0 };
    auto index = std::size_t{ 0 };
    while(index < values.size()) {
        if(index != 0 and values[index] < values[index - 1]) {
            return -1;
        }
        total = (total + word_score(values[index]) * static_cast<std::int64_t>(index % 251 + 1)) % 1'000'000'007;
        index += 1;
    }
    return total;
}

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    if(count <= 0) {
        return 1;
    }

    auto original = std::vector<std::string_view>{};
    original.reserve(static_cast<std::size_t>(count));
    auto index = 0;
    while(index < count) {
        original.push_back(word_at((index * 4827 + count * 97) % 1'000'003));
        index += 1;
    }

    auto total = std::int64_t{ 0 };
    auto round = 0;
    auto const repeats = repeat_count(count);
    auto const stable = stable_requested();
    while(round < repeats) {
        auto values = original;
        if(stable) {
            std::ranges::stable_sort(values);
        } else {
            std::ranges::sort(values);
        }
        auto const checksum = checked_checksum(values);
        if(checksum < 0) {
            return 2;
        }
        total = (total + checksum) % 1'000'000'007;
        round += 1;
    }

    return static_cast<int>(total % 251);
}
