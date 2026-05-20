# 设计

- 语言名: `cp`
- 源文件后缀: `.cp`

`design/` 按内容分为两类:

- `examples/`: 语言示例代码, 用来展示模块、函数、调用等实际写法
- `docs/`: 语言设计文档, 用来记录语法、类型系统、流程控制等规则

标准库源码放在仓库根目录的 `std/` 中。`design/docs/` 记录语言规则，`std/` 记录这些规则之上的普通库模块。

## 示例代码

`examples/` 按主题分组。每个子目录都是独立示例项目，不依赖其他示例目录。

- `examples/basics/main.cp`: 基础表达式示例，展示变量、常量、逻辑运算和两种 cast
- `examples/modules/`: 模块示例，展示同一示例项目内的 `export module`、`import` 和跨文件调用
- `examples/types/main.cp`: 类型示例，展示字面量、`array/tuple`、元组下标与解构、引用、指针和 target const
- `examples/flow/main.cp`: 流程控制示例，展示 `while`、`do while`、范围 `for` 以及带标签的 `break/continue`
- `examples/structs/main.cp`: 结构示例，展示 `struct/impl`、默认构造、构造函数、成员/关联函数、字段访问、块表达式和析构清理
- `examples/operators/main.cp`: 运算符重载示例，展示 `operator +`、`operator +=` 和 `operator []`
- `examples/ownership/main.cp`: 所有权示例，展示表达式级 `ref` / `const ref` / `move`、`self like&` 和 `= delete`
- `examples/concepts/main.cp`: concept 示例，展示关联类型、父 concept、默认实现和 `impl concept for type`

## 设计文档

- `docs/README.md`: 文档总入口
- `docs/cast.md`: 类型转换
- `docs/concept.md`: `concept`、关联类型、默认实现、父 concept 和 `type` 类型别名语句
- `docs/extern_c.md`: `extern "C"` 外部 C 函数声明、C 符号名导出和第一版 ABI 边界
- `docs/enum.md`: 强类型整数枚举、作用域 case 和显式底层整数转换
- `docs/error_handling.md`: `!`、`panic`、`assert`、前置条件和可恢复失败边界
- `docs/flow.md`: 流程控制
- `docs/generic.md`: 泛型函数、泛型 `struct`、泛型 `variant`、泛型 `impl`、参数包、`template for`、强无约束模板语义、`concept` 约束和 `requires` 子句
- `docs/initial.md`: 变量、常量与初始化
- `docs/iteration.md`: `iterator`、`iterable`、`optional<T>` 和范围 `for` 的协议语义
- `docs/std_collections.md`: `std.collections`、`vector`、有序唯一键 `map/set` 和 order-statistics 接口
- `docs/std_ranges.md`: `std.ranges` 基础范围对象和 `iota` 设计
- `docs/lambda.md`: 函数类型、函数指针、普通函数绑定、lambda、自动捕获和闭包边界
- `docs/memory_allocation.md`: 底层内存分配、对象生命周期原语和 runtime ABI
- `docs/module.md`: 模块声明、导入与导出
- `docs/operator.md`: 运算符与第一版 `operator` 重载
- `docs/opaque_alias.md`: `type A = opaque T` 的名义封装和 layout 规则
- `docs/ownership.md`: 默认 copy、显式借用、显式移动、`like` const 转发 / `move&`、特殊成员和能力推导
- `docs/struct.md`: `struct / impl`、构造/析构、成员函数、结构体初始化与块表达式
- `docs/std_fs.md`: `std.fs.file`、`open_options` 和同步文件读写
- `docs/type_system.md`: 类型分类、默认初始化、聚合字面量、`decltype`、返回类型推导与语义检查规则
- `docs/variant.md`: `variant`、case 构造、`match` 和 tagged union 布局
