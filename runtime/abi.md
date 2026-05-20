# CP Runtime ABI

The compiler lowers raw memory builtins to these C ABI symbols:

```cpp
extern "C" void* cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count);
extern "C" void cp_free(void* ptr);
extern "C" [[noreturn]] void cp_panic(char const* ptr, std::uint64_t len);
extern "C" std::int32_t cp_io_write_char(std::int32_t stream, char ch);
extern "C" std::uint8_t* cp_file_open(char const* path, std::uint64_t path_len, std::uint8_t flags);
extern "C" std::int32_t cp_file_close(std::uint8_t* handle);
extern "C" std::int32_t cp_file_read(std::uint8_t* handle, std::uint8_t* data, std::uint64_t len, std::uint64_t* out_len);
extern "C" std::int32_t cp_file_write(std::uint8_t* handle, std::uint8_t const* data, std::uint64_t len, std::uint64_t* out_len);
```

`cp_alloc` returns uninitialized storage for `count` elements. It aborts on integer overflow, invalid alignment, or allocation failure.

`cp_free` releases storage returned by `cp_alloc`. Passing any other pointer is an unsafe contract violation.

`cp_panic` writes a panic message to stderr and aborts. The frontend uses it for `panic`, checked `assert` failures, and checked indexing contracts.

`cp_io_write_char` writes one byte to stdout when `stream == 1`, and stderr when `stream == 2`.
It returns `0` on success and `-1` on write failure.

`cp_file_open` receives a cp `str` as `ptr + len` rather than a C string. The runtime rejects interior NUL bytes, copies the path to a temporary NUL-terminated buffer, maps the low five option bits to a platform file mode, and returns an opaque handle or null on failure.

`cp_file_read` and `cp_file_write` return `0` on success and store the transferred byte count in `out_len`. They return `-1` on invalid handles or host I/O failure.
