# CP Runtime ABI

The compiler lowers raw memory builtins to these C ABI symbols:

```cpp
extern "C" void* cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count);
extern "C" void cp_free(void* ptr);
```

`cp_alloc` returns uninitialized storage for `count` elements. It aborts on integer overflow, invalid alignment, or allocation failure.

`cp_free` releases storage returned by `cp_alloc`. Passing any other pointer is an unsafe contract violation.
