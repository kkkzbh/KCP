# 标准库源码

`std/` 放置 cp 标准库源码。标准库使用普通 cp 模块表达，不引入语言关键字特权。

编译器驱动需要提供标准库搜索路径、自动导入策略和最终链接规则。

第一批标准库模块：

- `std`: 聚合入口，重导出第一批标准库公共模块。
- `std.option`: 定义 `optional<T>` 和基础查询/回退方法。
- `std.expected`: 定义 `expected<T,E>` 和基础查询/回退方法。
- `std.iter`: 定义 `iterator` 和 `iterable` 迭代协议，并导入 `std.option` 作为签名依赖。
- `std.buffer`: 定义只拥有原始存储的 `buffer<T>`，由上层容器负责元素构造和析构。
- `std.vector`: 定义基于 `buffer<T>` 的动态数组，负责已构造元素数、扩容、元素访问和修改。
- `std.str`: 定义 compiler-recognized `str` 的 `size` / `data` 方法、`char` 迭代器和 `iterable` 实现。
- `std.string`: 定义拥有字符存储的 `string`，维护 trailing `'\0'`，并可通过 `as_str()` 借出 `str`。
- `std.ranges`: ranges 聚合入口，重导出 `std.ranges.*` 的公共范围对象。
- `std.ranges.iota`: 定义可被范围 `for` 消费的半开整数范围 `iota(begin, end)`。
- `std.io`: 第一版格式化输出，提供 `print` / `println` / `eprint` / `eprintln`。

## `std.io` 第一版边界

`std.io` 只做输出，不做标准输入。格式字符串只支持：

- `{}`：消费一个满足 `display` 的参数。
- `{{` / `}}`：输出字面量花括号。

第一版 `display` 实现覆盖 `str`、`char`、`bool`、`i32` 和 `usize`。格式错误和输出错误通过
`print_result` 返回，不做编译期格式字符串检查，也不支持宽度、精度、进制、位置参数、命名参数或 locale。
格式扫描按 `str.size()` 的长度语义进行，字符串中的中间 `'\0'` 不会截断输出。
