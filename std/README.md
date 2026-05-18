# Standard Library

`std/` 放置 cp 标准库源码。标准库使用普通 cp 模块表达，不引入语言关键字特权。

编译器驱动需要提供标准库搜索路径、自动导入策略和最终链接规则。

第一批标准库模块：

- `std.option`: 定义 `optional<T>`。
- `std.iter`: 定义 `iterator` 和 `iterable` 迭代协议，并重导出 `std.option`。
