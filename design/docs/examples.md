# 示例导览

`design/examples/` 按主题保存独立示例项目。学习时建议先读示例，再打开对应参考页确认规则。编译器测试会用
`test/fixtures/examples/` 中的镜像文件做语义检查、编译产物检查和二进制运行检查；当前两套文件内容保持一致，但目录之间不是自动同步的，更新示例时需要同时更新镜像。除非特别说明，下表中的路径都以
`design/` 为根。

示例只展示当前编译器已经接受并能运行的主路径，不等价于完整语言规范。语法限制、错误边界和不可做操作以对应参考页为准。
这些目录也不是反例集合；示例中没有出现的写法不自动表示禁止，示例中出现的写法也不能脱离所在上下文推广。拒绝路径以参考页里的诊断说明，以及
`test/lexer/cases/invalid/`、`test/parser/cases/invalid/` 和语义测试中的 negative cases 为准。

| 示例 | 主题 |
| --- | --- |
| `examples/basics/main.cp` | `let`、`const`、基础字面量、逻辑运算和显式数值转换 |
| `examples/modules/` | 双文件模块项目：`export module`、`import`、导出函数和跨文件调用 |
| `examples/types/main.cp` | `bool`、`char`、`str`、数组、嵌套数组、元组、元组字段、解构、引用、指针、指针引用和 target const |
| `examples/flow/main.cp` | `if`、`while`、`do while`、数组范围 `for`、循环标签、带标签 `break`/`continue` |
| `examples/structs/main.cp` | `struct`、默认构造、用户构造、字段初始化、`self` 成员函数、关联函数、块表达式和析构顺序 |
| `examples/operators/main.cp` | 运算符成员重载：`operator +`、`operator +=`、`operator []`、`operator =`、`operator ()` |
| `examples/ownership/main.cp` | `ref`、`const ref`、`move`、`self like&`、返回 `like&`、删除成员和删除赋值 |
| `examples/concepts/main.cp` | 父 concept、关联类型、`impl Concept for Type`、`iter`/`next` 协议和手写迭代循环 |
| `examples/generics/main.cp` | 泛型 `struct`、泛型成员函数、concept 约束、值参数包、类型参数包、`template for` |
| `examples/variants/main.cp` | `variant` 空 case、payload case、case 构造、`match` 表达式和 `optional<T>` |
| `examples/lambdas/main.cp` | 函数类型、函数指针、表达式 lambda、块 lambda、显式返回类型和捕获 |
| `examples/memory/main.cp` | `alloc<T>`、指针算术、`construct_at`、`destroy_at`、`free` 和手动析构 |
| `examples/std/main.cp` | `std` 聚合导入、`optional`、`expected`、`raw_buffer`、`vector`、`map`、`set`、`string`、ranges pipeline、数组 ranges UFCS、`std.meta` 和格式化输出 |
| `examples/interop/main.cp` | `extern "C"` 函数声明、带底层类型的强 `enum`、opaque alias、opaque/enum 显式转换 |
| `examples/errors/main.cp` | `assert`、`panic`、never 类型 `!`、`optional` 和通过 `match` 汇合 never 分支 |
| `examples/fs/main.cp` | `std.fs` 文件打开、写入、显式关闭、读取、`raw_buffer<u8>` 和 `span<u8>` |

## 阅读建议

先从 `basics`、`modules`、`types`、`flow` 开始，理解普通程序形状。之后读 `structs`、`operators`、`ownership`、`generics` 和 `concepts`，进入类型抽象。最后读 `std`、`fs`、`memory` 和 `interop`，理解标准库和底层边界。

每个示例目录都是独立项目；不要默认它依赖其他示例目录中的模块。`modules` 示例需要同时编译 `math.cp` 和
`main.cp`，其余示例入口都是单个 `main.cp`。部分示例依赖标准库模块，例如 `std`、`fs`、`concepts`、`variants` 和
`errors`；测试环境会按源码 `import` 闭包加入对应标准库源文件。手动编译这些示例时，也必须让编译器能找到 `std/` 下的被导入模块；只传 `main.cp` 而不提供标准库搜索路径或 import closure 时，会按普通未知模块 / 未知名字诊断失败。
