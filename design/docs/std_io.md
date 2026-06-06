# 标准库 IO

本文档记录 `std.io` 第一版公共接口。当前 IO 只覆盖格式化输出和 stdout / stderr 风格输出流，不提供标准输入、任意文件描述符抽象或文件 IO；文件读写见 [std_fs.md](std_fs.md)。

`std.io` 聚合模块当前重导出：

```text
std.io.raw
std.io.format
```

## Raw 输出流

`std.io.raw` 提供最底层输出流：

```cp
struct output_stream {
    fd: i32;
}

stdout() -> output_stream;
stderr() -> output_stream;
write_char(out: output_stream&, ch: char) -> bool;
write_str(out: output_stream&, text: str) -> bool;
```

规则：

- `stdout()` 返回 `output_stream{ .fd = 1 }`。
- `stderr()` 返回 `output_stream{ .fd = 2 }`。
- `write_char(out, ch)` 直接调用 runtime 的 `cp_io_write_char(out.fd, ch)`，返回是否成功。
- 当前 runtime 只有 `fd == 2` 写 stderr，其它 fd 都写 stdout。`output_stream{ .fd = 3 }` 这类手工构造值不会获得真实 fd 3 输出能力。
- `write_str(out, text)` 按 `str` 迭代顺序逐字符调用 `write_char`；遇到第一个失败立即返回 `false`，不会继续写剩余字符。它不是一次 bulk write，也不暴露已经成功写出的字符数。
- `write_str` 按 `str.len` 处理文本，中间 `'\0'` 是普通字符，不按 C 字符串提前截断。
- 这些接口都要求可写 `output_stream&`，因为调用形状上把 stream 当作可变输出目标处理。

`std.io.raw` 本身不提供格式化、缓冲、flush、stdin、文件打开或错误分类；它只是格式化层和 stdout/stderr 输出的底层目标。

## 格式化入口

`std.io.format` 提供格式化结果类型、`formatter`、`display` concept 和常用输出函数。它还会重导出 `std.text.string`，因此只导入 `std.io.format` 时也能看到 `string`。

`std.io.format` 只是普通导入 `std.io.raw`，不会把 raw 输出流名字继续重导出给用户。只写 `import std.io.format;` 时可以直接调用 `format`、`print`、`println`、`eprint` 和 `eprintln`，但看不到 `output_stream`、`stdout()`、`stderr()`、`write_char` 或 `write_str`。需要自己构造 stream 并调用 `format_to(ref out, ...)` 时，应导入 `std.io`，或同时导入 `std.io.format` 和 `std.io.raw`。

公开类型：

```cp
variant format_error {
    placeholder_too_few;
    argument_too_many;
    invalid_escape;
    unsupported_specifier;
    output_failed;
}

variant display_result {
    ok;
    error(format_error);
}

variant format_result {
    ok(string);
    error(format_error);
}

variant formatter {
    stream(output_stream&);
    buffer(string&);
}

concept display {
    display(self const&, out: formatter&) -> display_result;
}
```

公开函数：

```cp
format<T...: display>(fmt: str, values: T...) -> format_result;
format_to<T...: display>(out: output_stream&, fmt: str, values: T...) -> display_result;
print<T...: display>(fmt: str, values: T...) -> display_result;
println<T...: display>(fmt: str, values: T...) -> display_result;
eprint<T...: display>(fmt: str, values: T...) -> display_result;
eprintln<T...: display>(fmt: str, values: T...) -> display_result;
```

`format` 写入内部 `string` buffer，成功时返回 `format_result::ok(string)`。其它入口写到 stream，返回 `display_result`。

## 格式字符串

格式字符串在运行时按 `str` 扫描，不做编译期字面量解析或占位符数量检查。泛型约束 `T...: display` 只证明每个值参数都能调用 `display`；它不会证明 `{}` 的数量和参数数量相等，也不会提前拒绝 `"{:"`、`"{x"` 或单独的 `"}"`。因此 `fmt` 可以来自变量或函数返回值，所有转义、占位符和参数数量错误都通过 `format_result::error(...)` / `display_result::error(...)` 在调用时报告。

支持的格式片段：

- `{}`：消耗一个值参数，并调用该值的 `display` 实现。
- `&#123;&#123;`：输出字面量 `{`。
- `&#125;&#125;`：输出字面量 `}`。

错误分类：

- `placeholder_too_few`：格式串中还有 `{}`，但值参数已经用完。
- `argument_too_many`：值参数还没用完，格式串已经结束。
- `invalid_escape`：出现单独的 `{`、单独的 `}` 或未知花括号转义，例如 `{x`。
- `unsupported_specifier`：出现 `{:...}` 形式。第一版不支持任何格式说明符；`"{:"` 和 `"{:}"` 都按这个错误处理。
- `output_failed`：底层输出目标写入失败。自定义 `display` 实现可以返回任意 `format_error`，格式化入口会原样传播；如果它显式返回 `output_failed`，调用者也会看到这一错误。

不支持宽度、精度、对齐、进制选择、命名参数、位置参数、locale、自定义格式说明符或字符串插值。普通字符串字面量不会因为包含花括号而自动格式化；只有传给 `format` / `print` / `formatter.format` 这类入口时才扫描占位符。

格式化是边扫描边写出。已经写出的普通文本和参数不会在后续错误时回滚：

- `format(...)` 失败时返回 `error(...)`，不返回内部部分 buffer。
- `print` / `println` / `eprint` / `eprintln` / `format_to` 写 stream 时，失败前的字符已经发送到底层 stream。自定义 `display` 中途返回错误时也是同样规则：该实现已经写出的内容不会回滚，后续格式片段不会继续处理。
- `println` / `eprintln` 只有在格式串和所有参数都成功后才追加最后的 `'\n'`；最终换行写入失败同样返回 `output_failed`。

## formatter

`formatter` 是格式化实现使用的公开输出目标：

```cp
formatter::stream(ref out)
formatter::buffer(ref text)
```

成员接口：

```cp
write_char(self&, ch: char) -> display_result;
write_str(self&, text: str) -> display_result;
format<T...: display>(self&, fmt: str, values: T...) -> display_result;
```

规则：

- `stream` case 包装已有 `output_stream&`，写入失败会返回 `display_result::error(output_failed)`。
- `buffer` case 包装已有 `string&`，`write_char` 使用 `push_back`，`write_str` 使用 `append`。
- `format(...)` 在已有 formatter 上继续扫描格式串并写入同一个目标；它使用和顶层 `format` / `print` 相同的占位符扫描规则，少参数、多参数、非法花括号、未支持格式说明符和自定义 `display` 返回的错误都会以 `display_result::error(...)` 传播。
- 自定义 `display` 实现应使用传入的 `formatter&` 写出内容并传播错误，不应直接调用 `print` / `println`。直接打印会绕过传入目标，导致 `format(...)` 无法收集内容。

## display

`display` 是普通标准库 concept：

```cp
display(self const&, out: formatter&) -> display_result;
```

它不是自动派生能力，也不是编译器对所有类型的内建格式化。

内置实现覆盖：

- `bool`：输出小写 `true` / `false`。
- 所有内建整数、`isize`、`usize`：十进制输出。
- `f32` / `f64`：先输出整数部分，再输出 `.` 和 6 位小数；不四舍五入，不处理科学计数、NaN / Inf 专门拼写或宽度。
- `char`：直接输出字符本身，不加引号、不转义。
- `str`：按 `str.len` 写出完整视图区间。
- `string`：先借出 `as_str()` 再写出。

没有内置 display 的常见类型包括数组、tuple、指针、`optional<T>`、`expected<T,E>`、`vector<T>`、`map<K,V>`、`set<K>`、用户自定义 `struct` 和 `variant`。把这些值传给 `{}` 会在语义检查阶段因为不满足 `display` 约束而失败。

自定义类型示例：

```cp
impl display for point {
    display(self const&, out: formatter&) -> display_result
    {
        return out.format("point({}, {})", x, y);
    }
}
```

`display` receiver 是 `self const&`，因此格式化协议不提供可写 receiver。需要在显示时消费或修改对象的类型，应先在调用点生成要显示的普通值或字符串。

## 输出入口差异

```cp
let result = format("value = {}", 42);

let out = stdout();
format_to(ref out, "value = {}", 42);

print("value = {}", 42);
println("value = {}", 42);
eprint("value = {}", 42);
eprintln("value = {}", 42);
```

规则：

- `format` 不写 stdout/stderr；它返回 `format_result`。成功时是 `format_result::ok(string)`，失败时是 `format_result::error(format_error)`。当前没有隐式 unwrap，也没有 `format(...).value()` 这类 helper；需要拿到 `string` 时必须 `match` `.ok(text)`。
- `format_to` 第一个参数是 `output_stream&`，必须传入可写 stream 左值；`format_to(stdout(), "...")` 不是当前能力。它只在调用期间使用这个 stream 引用，不保存 stream 或格式化参数。只导入 `std.io.format` 时虽然能看到 `format_to` 函数本身，但看不到 `stdout()` 或 `output_stream` 类型；需要额外导入 `std.io.raw` 或使用聚合入口 `std.io`。
- `print` / `println` 在函数内部构造 stdout stream。
- `eprint` / `eprintln` 在函数内部构造 stderr stream。
- `println` / `eprintln` 只比对应无换行版本多写一个末尾 `'\n'`。

## 不支持内容

第一版 `std.io` 不提供：

- 标准输入、读取一行、读取字符或扫描输入。
- 任意文件描述符输出抽象、flush、buffered writer 或 stderr/stdout 以外的真实 stream。
- 编译期格式串检查。
- 宽度、精度、对齐、进制、命名/位置参数、locale 或自定义格式说明符。
- 自动显示数组、tuple、容器、指针、variant 或用户类型。
- 把 `str` 当 C string 写出的 runtime 入口；`write_str` 始终按 `str` 长度逐字符输出。
