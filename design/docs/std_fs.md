# std.fs

本文档记录第一版同步文件 IO 标准库。底层 C ABI 入口由 runtime 提供，见 [extern_c.md](extern_c.md)。

## 模块边界

```cp
export module std.fs;

export import std.fs.file;
```

`std.fs` 是聚合模块，只负责重导出文件系统子模块。第一版只有文件句柄和同步读写能力，具体实现集中在 `std/fs/file.cp`：

```text
std/fs.cp          -> export module std.fs;
std/fs/file.cp     -> export module std.fs.file;
```

## 类型

```cp
enum io_error : u8 {
    open_failed = 1;
    read_failed = 2;
    write_failed = 3;
    close_failed = 4;
}

enum open_flag : u8 {
    read = 1 << 0;
    write = 1 << 1;
    create = 1 << 2;
    truncate = 1 << 3;
    append = 1 << 4;
}

type open_options = opaque u8;
```

`open_options` 是封装的 bitset，不引入单独的 `flags` 语法。第一版只提供原语设位方法，不定义复杂冲突规则：

```cp
let options = open_options{}.write().create().truncate();
```

## file

`file` 是 RAII 句柄类型：

- copy 删除。
- move 支持。
- 析构自动 close。
- 可以显式 `close()`，重复 close 是成功的 no-op。

接口：

```cp
file::open(path: str, options: open_options) -> expected<file, io_error>
file.read(out: span<u8>) -> expected<usize, io_error>
file.write(data: span<u8> const&) -> expected<usize, io_error>
file.write_str(text: str) -> expected<usize, io_error>
```

`read` 使用 `span<u8>` 表示可写连续区间；`write` 使用 `span<u8> const&` 表示只读连续区间。标准库不引入 `buffer_mut` / `span_mut` 这类名字。

## runtime ABI

```cpp
extern "C" std::uint8_t* cp_file_open(char const* path, std::uint64_t path_len, std::uint8_t flags);
extern "C" std::int32_t cp_file_close(std::uint8_t* handle);
extern "C" std::int32_t cp_file_read(std::uint8_t* handle, std::uint8_t* data, std::uint64_t len, std::uint64_t* out_len);
extern "C" std::int32_t cp_file_write(std::uint8_t* handle, std::uint8_t const* data, std::uint64_t len, std::uint64_t* out_len);
```

`str` 仍然是 `ptr + len`，不是 C string。runtime 在 `cp_file_open` 内部复制路径并补 trailing nul；路径中间含 `'\0'` 时打开失败。
