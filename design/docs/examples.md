# 示例导览

`design/examples/` 按主题保存独立示例项目。学习时建议先读示例，再打开对应参考页确认规则。

| 示例 | 主题 |
| --- | --- |
| `examples/basics/main.cp` | 变量、常量、逻辑运算、显式类型转换 |
| `examples/modules/` | `export module`、`import`、跨文件调用 |
| `examples/types/main.cp` | 字面量、数组、元组、引用、指针和 target const |
| `examples/flow/main.cp` | `while`、`do while`、范围 `for`、标签、`break`、`continue` |
| `examples/structs/main.cp` | `struct`、`impl`、构造函数、成员函数、字段访问和析构 |
| `examples/operators/main.cp` | `operator +`、`operator +=`、`operator []`、`operator =`、`operator ()` |
| `examples/ownership/main.cp` | `ref`、`const ref`、`move`、`self like&`、`= delete` |
| `examples/concepts/main.cp` | 关联类型、父 concept、`impl concept for type` |
| `examples/generics/main.cp` | 泛型 `struct`、泛型成员函数、concept 约束、参数包、`template for` |
| `examples/variants/main.cp` | payload case、空 case、case 构造和 `match` |
| `examples/lambdas/main.cp` | 函数类型、函数指针、表达式 lambda、块 lambda 和捕获 |
| `examples/memory/main.cp` | `alloc`、`construct_at`、`destroy_at`、`free` 和析构 |
| `examples/std/main.cp` | `optional`、`expected`、`raw_buffer`、`vector`、`map`、`set`、`string`、`iota`、格式化输出 |
| `examples/interop/main.cp` | 强类型 `enum`、opaque alias、`extern "C"` |
| `examples/errors/main.cp` | `assert`、`panic`、`!`、`optional` 可恢复失败分支 |
| `examples/fs/main.cp` | `std.fs` 文件打开、写入、读取和 `span<u8>` 缓冲区 |

## 阅读建议

先从 `basics`、`modules`、`types`、`flow` 开始，理解普通程序形状。之后读 `structs`、`operators`、`ownership`、`generics` 和 `concepts`，进入类型抽象。最后读 `std`、`fs`、`memory` 和 `interop`，理解标准库和底层边界。

每个示例目录都是独立项目；不要默认它依赖其他示例目录中的模块。
