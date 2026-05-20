#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace {
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
} // namespace

extern "C" auto cp_alloc(uint64_t elem_size, uint64_t align, uint64_t count) -> void*
{
    auto bytes = checked_allocation_size(elem_size, count);
    auto alignment = normalized_alignment(align);

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
}

extern "C" auto cp_free(void* ptr) -> void
{
    free(ptr);
}

extern "C" auto cp_bounds_fail() -> void
{
    abort();
}

extern "C" auto cp_io_write_char(int32_t stream, char ch) -> int32_t
{
    auto* target = stream == 2 ? stderr : stdout;
    return fputc(static_cast<unsigned char>(ch), target) == EOF ? -1 : 0;
}
