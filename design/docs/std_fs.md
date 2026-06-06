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

当前公开 API 只有打开文件、关闭文件、顺序读取、顺序写入和写入 `str`。第一版没有目录遍历、创建/删除目录、删除/重命名文件、metadata 查询、seek/tell、flush/fsync、异步 IO、非阻塞 IO、内存映射、权限模型、路径规范化或跨平台路径抽象。需要这些能力时应先通过 runtime/标准库扩展新增明确 API，不能从 `file_handle` 或 `open_options` 的底层表示绕过。

`std.fs.file` 普通导入 `std.core.expected`、`std.memory.span` 和 `std.text.str`，但不会把这些名字重导出给用户。只写 `import std.fs.file;` 时可以看到 `file`、`io_error`、`open_options` 等文件 API；如果调用点需要显式写 `expected<...>`、构造 `span<u8>{...}`，或使用 `str` 扩展方法，应额外导入 `std.core`、`std.memory`、`std.text`，或直接导入聚合入口 `std`。

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

type file_handle = opaque u8*;
type open_options = opaque u8;
```

`file_handle` 是底层 runtime 文件句柄的不透明封装。它的公开意义是让 `file` 结构体和模块内函数保存句柄值；导入 `std.fs` 的普通用户代码不能直接把 `u8*` 转成 `file_handle`，也不能从 `file_handle` 转回 `u8*`，除非调用定义在 `std/fs/file.cp` 这个同一 translation unit 内的导出 API。

`open_options` 是封装的 bitset，不引入单独的 `flags` 语法。第一版只提供原语设位方法，不定义复杂冲突规则：

```cp
let options = open_options{}.write().create().truncate();
```

公开方法：

```cp
bits(self) -> u8;
with(self, flag: open_flag) -> open_options;
read(self) -> open_options;
write(self) -> open_options;
create(self) -> open_options;
truncate(self) -> open_options;
append(self) -> open_options;
```

`open_options{}` 的 bitset 值为 0。标准库不在 KCP 层验证 `read` / `write` / `append` / `truncate` 的组合是否合法，也不自动补默认读写模式；调用者应传入 runtime 能解释的组合。

`open_options` 的方法都按值接收 `self`，并返回设位后的新 `open_options`；它们不会原地修改原变量。需要累积 flag 时要使用返回值，例如链式写 `open_options{}.write().create()`，或把返回值重新赋给变量。单独写 `options.write();` 只会丢弃返回的新值，`options.bits()` 仍是调用前的值；应写 `options = options.write();`。`with(flag)` 只是对底层 `u8` 执行 bitwise OR，重复设置同一个 flag 是幂等的。第一版没有 `clear`、`unset`、`toggle`、`contains`、`read_write()` 这类 helper，也没有从 flag 组合自动规范化出唯一高层模式。

由于 `open_options` 是 opaque alias，导入 `std.fs` 的普通用户代码不能直接写 `(raw as open_options)` 来构造任意 bitset；可稳定构造的组合来自 `open_options{}` 和上面这些设位方法。下面的 runtime bit 映射描述的是底层 ABI 如何解释 `u8 flags`，不是鼓励用户绕过公开 API 制造额外 bit。

当前 runtime 把 `open_options.bits()` 映射到 C `fopen` mode。高层用法应优先使用这些组合：

- `open_options{}.read()`：只读打开，文件必须已存在。
- `open_options{}.write().create()`：写入并创建/截断文件。
- `open_options{}.write().create().truncate()`：明确表达创建并截断。
- `open_options{}.read().write()`：读写已存在文件。
- `open_options{}.read().write().create()`：读写并允许创建/截断。
- `open_options{}.append()` / `.append().read()`：追加写，或追加读写。

需要注意的边界：

- `write()` 单独使用没有有效 mode，会返回 `open_failed`。
- `open_options{}` 和单独 `create()` 没有读写方向，也会返回 `open_failed`。
- `read().create()` 不创建文件，只按只读打开。
- `append` 优先于 `truncate`，`truncate` 优先于普通 `read + write`。不要把互相冲突的 flag 组合当作稳定语义。
- `append` 路径使用 C 追加 mode，因此即使没有设置 `create` bit，底层也可能在路径不存在时创建新文件。第一版没有“只追加已存在文件，不允许创建”的单独 API。

## file

`file` 是 RAII 句柄类型：

- copy 删除。
- move 支持。
- 析构自动 close。
- 可以显式 `close()`，重复 close 是成功的 no-op。

接口：

```cp
file(handle: file_handle)
file::open(path: str, options: open_options) -> expected<file, io_error>
file.read(out: span<u8>) -> expected<usize, io_error>
file.write(data: span<u8> const&) -> expected<usize, io_error>
file.write_str(text: str) -> expected<usize, io_error>
file.close(self&) -> expected<usize, io_error>
```

`read` 使用 `span<u8>` 表示可写连续区间；`write` 使用 `span<u8> const&` 表示只读连续区间。标准库不引入 `buffer_mut` / `span_mut` 这类名字。

规则：

- `file_handle` 是 `opaque u8*`，关闭后的文件对象把 handle 置为 null。
- `file` 的 `handle` 字段是公开字段，且 `file_handle{}` 默认初始化为空句柄。因此 `file{}` 或 `file{ .handle = file_handle{} }` 可以构造一个已经 closed/null 的 `file` 值；它只能用于占位、移动赋值目标或显式 `close()` no-op。普通用户代码不能构造非 null `file_handle`，真正可读写的文件应通过 `file::open(...)` 获得。
- `file(handle: file_handle)` 是普通构造函数，不执行打开或复制底层句柄。它只把传入的 opaque handle 存入结构体；由于 opaque 转换权限只在 `std/fs/file.cp` 内部可用，导入方能直接传入的实用值只有 `file_handle{}` 这类空句柄。不要把它当作从整数、指针或路径创建文件对象的公开入口。
- `file` 的 copy 构造和 copy 赋值在源码中声明为删除；稳定用法必须把 `file` 当作独占句柄，不能复制。当前编译器对删除特殊成员的检查还不覆盖所有 copy 形状，例如 `let copy = out;` 这类局部初始化可能仍被接受，但它只会复制底层 handle，两个 `file` 析构时会重复关闭同一个 runtime 句柄并可能触发 double free。公共代码不要依赖这种行为；需要转移句柄时使用 move 构造或 move 赋值。
- move 赋值会先比较当前对象和右侧对象保存的 raw handle。两者不同时，先对当前对象调用 `close()`，再接管右侧 handle 并把右侧置为 null；如果关闭旧 handle 失败，错误不会从 move 赋值返回。两者 raw handle 相同时，当前实现什么都不做。合法代码不应制造两个 `file` 同时拥有同一个非 null handle；`file` 是独占句柄，不是引用计数或共享句柄包装。
- 析构函数调用 `close()`；析构路径无法向调用者返回错误，因此需要观察 close 错误时必须显式调用 `close()`。
- `close`、`read`、`write` 和 `write_str` 都要求 `self&`，不能通过 `file const&` 或 `const file` 调用。即使 `read` 只把字节写入外部 buffer，它也会推进底层文件位置；`write` / `write_str` 和 `close` 也会改变底层句柄状态。需要共享只读访问时应在 API 层传入可写 `file&`，不要把 `file` 当作逻辑 const 的句柄视图。
- `close()` 对 null handle 返回 `expected<usize, io_error>::value(0)`，重复 close 是成功 no-op。非 null handle 会先把对象内的 handle 置为 null，再调用 runtime `cp_file_close(raw)`；因此即使底层 close 失败，对象也已经进入 closed/null 状态，不会在析构时再次尝试关闭同一个 raw handle。
- `file::open(path, options)` 不在 KCP 层规范化路径，也不 special-case 空路径。它直接把 `path.data()`、`path.size()` 和 `options.bits()` 交给 runtime；默认空 `str{}` 的 data 是 null，会返回 `unexpected(open_failed)`。路径中间含 `'\0'`、路径长度无法复制到 runtime 临时 buffer，或 flag 组合映射不到有效 mode 时，同样表现为 `open_failed`。
- `file::open`、`read`、`write` 和 `write_str` 都是同步调用；它们只在调用期间使用传入 `str` / `span` 的指针和长度，不把路径、buffer 或文本视图保存到 `file` 对象里。调用返回后，调用者仍然拥有这些 buffer 的生命周期责任。
- `read` 在 runtime 成功时返回实际读到的字节数；EOF 或短读可以是成功结果，调用者需要根据返回的 `usize` 判断是否填满缓冲区。
- `write` 和 `write_str` 在 runtime 成功时返回实际写入的字节数；当前 runtime 只有完整写入才报告成功，短写会被 KCP 层转换成 `unexpected(write_failed)`，不会把部分写入长度作为成功值暴露出来。
- `write_str` 按 `str.size()` 写入，字符串中间的 `'\0'` 会作为普通字节传给 runtime。
- `read` / `write` / `write_str` 不在 KCP 层 special-case 零长度缓冲区。它们会直接把 `span.data()` 或 `str.data()` 传给 runtime；runtime 先检查指针是否为 null，再执行 `fread` / `fwrite`。因此 `span<u8>{nullptr as u8*, 0}`、`span<u8 const>{nullptr as u8 const*, 0}` 或默认空 `str{}` 这类 null data 的零长度写读会返回失败，而不是成功的 `value(0)`。需要零长度成功语义时，调用者应传入非 null 的有效缓冲区指针，或在调用前自行判断长度为 0。
- `read` / `write` / `write_str` 遇到底层 runtime 非 0 返回值时分别返回 `read_failed` / `write_failed`。如果 handle 已经为 null，这些方法仍会把 null 传给 runtime，并得到对应失败结果；它们不会像 `close()` 一样把 null 当作成功 no-op。

## runtime ABI

KCP 侧 runtime 声明位于 `std.detail.runtime`：

```cp
export extern "C" cp_file_open(path: char const*, path_len: usize, flags: u8) -> u8*;
export extern "C" cp_file_close(handle: u8*) -> i32;
export extern "C" cp_file_read(handle: u8*, data: u8*, len: usize, out_len: usize*) -> i32;
export extern "C" cp_file_write(handle: u8*, data: u8 const*, len: usize, out_len: usize*) -> i32;
```

仓库 runtime 的 C++ 声明当前使用 `std::uint64_t` 承接 KCP 侧的 `usize` 长度参数：

```cpp
extern "C" std::uint8_t* cp_file_open(char const* path, std::uint64_t path_len, std::uint8_t flags);
extern "C" std::int32_t cp_file_close(std::uint8_t* handle);
extern "C" std::int32_t cp_file_read(std::uint8_t* handle, std::uint8_t* data, std::uint64_t len, std::uint64_t* out_len);
extern "C" std::int32_t cp_file_write(std::uint8_t* handle, std::uint8_t const* data, std::uint64_t len, std::uint64_t* out_len);
```

`str` 仍然是 `ptr + len`，不是 C string。runtime 在 `cp_file_open` 内部复制路径并补 trailing nul；路径中间含 `'\0'` 时打开失败。

runtime 入口的空指针处理边界：

- `cp_file_open(path, len, flags)` 要求 `path` 非 null；即使 `len == 0`，null path 也返回 null。路径内部含 `'\0'` 同样返回 null。
- `cp_file_close(nullptr)` 返回 0，和 `file.close()` 的重复 close no-op 语义一致。
- `cp_file_read` / `cp_file_write` 要求 handle、data 和 out_len 都非 null；任一为空时返回非 0，即使 `len == 0` 也一样。KCP `file.read` / `file.write` / `file.write_str` 会把它转换成对应 `read_failed` / `write_failed`。
