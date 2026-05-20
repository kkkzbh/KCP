# CP Runtime ABI

The compiler lowers raw memory builtins to these C ABI symbols:

```cpp
extern "C" void* cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count);
extern "C" void cp_free(void* ptr);
extern "C" void cp_bounds_fail();
extern "C" std::int32_t cp_io_write_char(std::int32_t stream, char ch);
```

`cp_alloc` returns uninitialized storage for `count` elements. It aborts on integer overflow, invalid alignment, or allocation failure.

`cp_free` releases storage returned by `cp_alloc`. Passing any other pointer is an unsafe contract violation.

`cp_bounds_fail` aborts after a runtime bounds check fails.

`cp_io_write_char` writes one byte to stdout when `stream == 1`, and stderr when `stream == 2`.
It returns `0` on success and `-1` on write failure.
