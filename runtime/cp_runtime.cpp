#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
auto constexpr open_read = uint8_t{1u << 0u};
auto constexpr open_write = uint8_t{1u << 1u};
auto constexpr open_create = uint8_t{1u << 2u};
auto constexpr open_truncate = uint8_t{1u << 3u};
auto constexpr open_append = uint8_t{1u << 4u};

auto normalized_alignment(uint64_t requested) -> size_t
{
    auto alignment = static_cast<size_t>(requested < sizeof(void*) ? sizeof(void*) : requested);
    if((alignment & (alignment - 1uz)) == 0uz) {
        return alignment;
    }

    auto rounded = sizeof(void*);
    while(rounded < alignment) {
        if(rounded > static_cast<size_t>(-1) / 2uz) {
            abort();
        }
        rounded *= 2uz;
    }
    return rounded;
}

auto checked_allocation_size(uint64_t elem_size, uint64_t count) -> size_t
{
    if(elem_size != 0 and count > UINT64_MAX / elem_size) {
        abort();
    }
    auto bytes = elem_size * count;
    if(bytes > static_cast<uint64_t>(static_cast<size_t>(-1))) {
        abort();
    }
    return static_cast<size_t>(bytes == 0 ? 1 : bytes);
}

auto contains_nul(char const* data, uint64_t len) -> bool
{
    for(auto index = uint64_t{}; index < len; ++index) {
        if(data[index] == '\0') {
            return true;
        }
    }
    return false;
}

auto file_mode(uint8_t flags) -> char const*
{
    auto read = (flags & open_read) != 0u;
    auto write = (flags & open_write) != 0u;
    auto create = (flags & open_create) != 0u;
    auto truncate = (flags & open_truncate) != 0u;
    auto append = (flags & open_append) != 0u;

    if(append) {
        return read ? "a+b" : "ab";
    }
    if(truncate) {
        return read ? "w+b" : "wb";
    }
    if(read and write) {
        return create ? "w+b" : "r+b";
    }
    if(write) {
        return create ? "wb" : nullptr;
    }
    if(read) {
        return "rb";
    }
    return nullptr;
}
} // namespace

extern "C" auto cp_alloc(uint64_t elem_size, uint64_t align, uint64_t count) -> void*
{
    auto bytes = checked_allocation_size(elem_size, count);
    auto alignment = normalized_alignment(align);

#ifdef _WIN32
    auto result = _aligned_malloc(bytes, alignment);
    if(result == nullptr) {
        abort();
    }
    return result;
#else
    if(alignment <= alignof(max_align_t)) {
        auto result = malloc(bytes);
        if(result == nullptr) {
            abort();
        }
        return result;
    }

    auto result = aligned_alloc(alignment, ((bytes + alignment - 1uz) / alignment) * alignment);
    if(result == nullptr) {
        abort();
    }
    return result;
#endif
}

extern "C" auto cp_free(void* ptr) -> void
{
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

extern "C" {
[[noreturn]]
auto cp_panic(char const* ptr, uint64_t len) -> void
{
    fputs("panic: ", stderr);
    if(ptr != nullptr and len != 0u) {
        fwrite(ptr, 1uz, static_cast<size_t>(len), stderr);
    }
    fputc('\n', stderr);
    abort();
}
}

extern "C" auto cp_io_write_char(int32_t stream, char ch) -> int32_t
{
    auto* target = stream == 2 ? stderr : stdout;
    return fputc(static_cast<unsigned char>(ch), target) == EOF ? -1 : 0;
}

extern "C" auto cp_file_open(char const* path, uint64_t path_len, uint8_t flags) -> uint8_t*
{
    if(path == nullptr or contains_nul(path, path_len)) {
        return nullptr;
    }
    auto const* mode = file_mode(flags);
    if(mode == nullptr) {
        return nullptr;
    }

    if(path_len > static_cast<uint64_t>(static_cast<size_t>(-1) - 1uz)) {
        return nullptr;
    }
    auto* owned_path = static_cast<char*>(malloc(static_cast<size_t>(path_len) + 1uz));
    if(owned_path == nullptr) {
        return nullptr;
    }
    memcpy(owned_path, path, static_cast<size_t>(path_len));
    owned_path[path_len] = '\0';
    auto* file = fopen(owned_path, mode);
    free(owned_path);
    return reinterpret_cast<uint8_t*>(file);
}

extern "C" auto cp_file_close(uint8_t* handle) -> int32_t
{
    if(handle == nullptr) {
        return 0;
    }
    return fclose(reinterpret_cast<FILE*>(handle)) == 0 ? 0 : -1;
}

extern "C" auto cp_file_read(uint8_t* handle, uint8_t* data, uint64_t len, uint64_t* out_len) -> int32_t
{
    if(handle == nullptr or data == nullptr or out_len == nullptr) {
        return -1;
    }
    auto* file = reinterpret_cast<FILE*>(handle);
    auto read = fread(data, 1uz, static_cast<size_t>(len), file);
    *out_len = static_cast<uint64_t>(read);
    return ferror(file) == 0 ? 0 : -1;
}

extern "C" auto cp_file_write(uint8_t* handle, uint8_t const* data, uint64_t len, uint64_t* out_len) -> int32_t
{
    if(handle == nullptr or data == nullptr or out_len == nullptr) {
        return -1;
    }
    auto* file = reinterpret_cast<FILE*>(handle);
    auto written = fwrite(data, 1uz, static_cast<size_t>(len), file);
    *out_len = static_cast<uint64_t>(written);
    return written == static_cast<size_t>(len) ? 0 : -1;
}
