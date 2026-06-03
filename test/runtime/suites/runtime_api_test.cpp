import std;

#include "assert.hpp"

extern "C" auto cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count) -> void*;
extern "C" auto cp_free(void* ptr) -> void;
extern "C" auto cp_io_write_char(std::int32_t stream, char ch) -> std::int32_t;
extern "C" auto cp_file_open(char const* path, std::uint64_t path_len, std::uint8_t flags) -> std::uint8_t*;
extern "C" auto cp_file_close(std::uint8_t* handle) -> std::int32_t;
extern "C" auto cp_file_read(std::uint8_t* handle, std::uint8_t* data, std::uint64_t len, std::uint64_t* out_len) -> std::int32_t;
extern "C" auto cp_file_write(std::uint8_t* handle, std::uint8_t const* data, std::uint64_t len, std::uint64_t* out_len) -> std::int32_t;

namespace {

auto constexpr open_read = std::uint8_t{1u << 0u};
auto constexpr open_write = std::uint8_t{1u << 1u};
auto constexpr open_create = std::uint8_t{1u << 2u};
auto constexpr open_truncate = std::uint8_t{1u << 3u};
auto constexpr open_append = std::uint8_t{1u << 4u};

auto check_allocation_runtime() -> void
{
    auto* bytes = cp_alloc(1, 1, 0);
    test_parser::assert_true(bytes != nullptr, "zero-byte allocation should still produce a usable pointer");
    cp_free(bytes);

    auto* aligned = cp_alloc(8, 64, 3);
    test_parser::assert_true(aligned != nullptr, "over-aligned allocation should produce a pointer");
    test_parser::assert_true(
        reinterpret_cast<std::uintptr_t>(aligned) % 64uz == 0uz,
        "over-aligned allocation should respect the requested alignment");
    cp_free(aligned);

    auto* rounded = cp_alloc(1, 24, 1);
    test_parser::assert_true(rounded != nullptr, "non-power-of-two alignment should be rounded up");
    test_parser::assert_true(
        reinterpret_cast<std::uintptr_t>(rounded) % 32uz == 0uz,
        "rounded allocation should use the next power-of-two alignment");
    cp_free(rounded);
}

auto check_file_runtime() -> void
{
    auto const root = std::filesystem::temp_directory_path() / "cp_runtime_api_test";
    std::filesystem::create_directories(root);
    auto const path = root / "data.bin";
    auto const path_text = path.string();

    auto* bad_null = cp_file_open(nullptr, 0, open_read);
    test_parser::assert_true(bad_null == nullptr, "null file path should be rejected");
    auto const nul_path = std::array{ 'b', 'a', 'd', '\0', 'x' };
    auto* bad_nul = cp_file_open(nul_path.data(), nul_path.size(), open_read);
    test_parser::assert_true(bad_nul == nullptr, "interior NUL file path should be rejected");
    auto* bad_mode = cp_file_open(path_text.data(), path_text.size(), open_write);
    test_parser::assert_true(bad_mode == nullptr, "write without create/truncate/append should be rejected");

    auto* write_file = cp_file_open(
        path_text.data(),
        path_text.size(),
        open_write | open_create | open_truncate
    );
    test_parser::assert_true(write_file != nullptr, "write-create-truncate should open a file");
    auto written = std::uint64_t{};
    auto payload = std::array<std::uint8_t, 3>{ 'c', 'p', '\n' };
    test_parser::assert_true(
        cp_file_write(write_file, payload.data(), payload.size(), &written) == 0,
        "runtime file write should succeed");
    test_parser::assert_true(written == payload.size(), "runtime file write should report bytes written");
    test_parser::assert_true(cp_file_close(write_file) == 0, "runtime file close should succeed");

    auto* read_write_file = cp_file_open(path_text.data(), path_text.size(), open_read | open_write);
    test_parser::assert_true(read_write_file != nullptr, "read-write should open an existing file");
    test_parser::assert_true(cp_file_close(read_write_file) == 0, "runtime read-write close should succeed");

    auto* append_file = cp_file_open(path_text.data(), path_text.size(), open_append);
    test_parser::assert_true(append_file != nullptr, "append should open an existing file");
    auto suffix = std::array<std::uint8_t, 1>{ '!' };
    test_parser::assert_true(
        cp_file_write(append_file, suffix.data(), suffix.size(), &written) == 0,
        "runtime append write should succeed");
    test_parser::assert_true(cp_file_close(append_file) == 0, "runtime append close should succeed");

    auto* append_read_file = cp_file_open(path_text.data(), path_text.size(), open_append | open_read);
    test_parser::assert_true(append_read_file != nullptr, "append-read should open an existing file");
    test_parser::assert_true(cp_file_close(append_read_file) == 0, "runtime append-read close should succeed");

    auto* read_file = cp_file_open(path_text.data(), path_text.size(), open_read);
    test_parser::assert_true(read_file != nullptr, "read should open an existing file");
    auto buffer = std::array<std::uint8_t, 8>{};
    auto read = std::uint64_t{};
    test_parser::assert_true(
        cp_file_read(read_file, buffer.data(), buffer.size(), &read) == 0,
        "runtime file read should succeed");
    test_parser::assert_true(read == 4uz, "runtime file read should report bytes read");
    test_parser::assert_true(buffer[0] == 'c' and buffer[3] == '!', "runtime file content should round-trip");
    test_parser::assert_true(cp_file_close(read_file) == 0, "runtime file read close should succeed");

    auto* write_create_file = cp_file_open(path_text.data(), path_text.size(), open_write | open_create);
    test_parser::assert_true(write_create_file != nullptr, "write-create should open a file");
    test_parser::assert_true(cp_file_close(write_create_file) == 0, "runtime write-create close should succeed");

    auto* truncate_read_file = cp_file_open(path_text.data(), path_text.size(), open_truncate | open_read);
    test_parser::assert_true(truncate_read_file != nullptr, "truncate-read should open a file");
    test_parser::assert_true(cp_file_close(truncate_read_file) == 0, "runtime truncate-read close should succeed");

    test_parser::assert_true(cp_file_close(nullptr) == 0, "closing a null handle should be a no-op");
    test_parser::assert_true(cp_file_read(nullptr, buffer.data(), buffer.size(), &read) == -1, "read should reject null handle");
    test_parser::assert_true(cp_file_read(read_file, nullptr, buffer.size(), &read) == -1, "read should reject null data");
    test_parser::assert_true(cp_file_read(read_file, buffer.data(), buffer.size(), nullptr) == -1, "read should reject null length");
    test_parser::assert_true(cp_file_write(nullptr, buffer.data(), buffer.size(), &written) == -1, "write should reject null handle");
    test_parser::assert_true(cp_file_write(read_file, nullptr, buffer.size(), &written) == -1, "write should reject null data");
    test_parser::assert_true(cp_file_write(read_file, buffer.data(), buffer.size(), nullptr) == -1, "write should reject null length");
}

auto check_io_runtime() -> void
{
    test_parser::assert_true(cp_io_write_char(1, '\n') == 0, "stdout char write should succeed");
    test_parser::assert_true(cp_io_write_char(2, '\n') == 0, "stderr char write should succeed");
}

} // namespace

auto main() -> int
{
    check_allocation_runtime();
    check_file_runtime();
    check_io_runtime();
    return 0;
}
