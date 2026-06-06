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

`cp_alloc` returns uninitialized storage for `count` elements. It rounds `align` up to at least `sizeof(void*)` and to the next power of two when needed. A zero-byte request is represented as a one-byte allocation. It aborts on integer overflow, alignment rounding overflow, or allocation failure.

`cp_free` releases storage returned by `cp_alloc`. Passing any other pointer is an unsafe contract violation.

`cp_panic` writes a panic message to stderr and aborts. The frontend uses it for `panic`, checked `assert` failures, and checked indexing contracts.

`cp_io_write_char` writes one byte to stderr when `stream == 2`, and stdout for every other stream value.
It returns `0` on success and `-1` on write failure.

`cp_file_open` receives a cp `str` as `ptr + len` rather than a C string. The path pointer itself must be non-null even when the length is zero. The runtime rejects interior NUL bytes, copies the path to a temporary NUL-terminated buffer, maps the low five option bits to a platform file mode, and returns an opaque handle or null on failure.
Mode selection checks `append`, then `truncate`, then `read + write`, then write-only, then read-only. `append` and `truncate` ignore `create`; write-only requires `create`, `truncate`, or `append`; read-only ignores `create`. Unsupported combinations, size overflow while building the temporary path, and temporary allocation failure all return null.

`cp_file_close` returns `0` for a null handle. For a non-null handle it forwards to `fclose` and returns `0` on success or `-1` on close failure.

`cp_file_read` and `cp_file_write` require non-null `handle`, data pointer, and `out_len`, including zero-length transfers. Passing null for any of them returns `-1`.
`cp_file_read` stores the `fread` byte count in `out_len` and returns `0` when `ferror` is clear, so EOF and short reads are successful transfers with a smaller byte count.
`cp_file_write` stores the `fwrite` byte count in `out_len` but returns `0` only when the full requested length was written; short writes return `-1`.
