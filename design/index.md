# 学习路线

这里是 cp 语言文档的读者入口。文档分成两层：先读导览页建立整体认识，再按需要进入参考页查细节。

## 推荐顺序

1. 读 [语法导览](/docs/syntax)，了解 cp 程序的基本形状。
2. 读 [标准库导览](/docs/stdlib)，知道 `std` 里已经有哪些可用模块。
3. 对照 [示例导览](/docs/examples)，选择一个主题示例阅读。
4. 遇到具体规则时进入下面的参考页。

## 语言语法

- [类型系统](/docs/type_system)：类型分类、默认初始化、聚合字面量、`decltype`、返回类型推导、引用和指针规则。
- [初始化](/docs/initial)：变量、常量、引用 binding 和初始化表达式。
- [模块](/docs/module)：模块声明、导入、重导出、导出和名字冲突。
- [流程控制](/docs/flow)：`if`、循环、标签、`break` / `continue` 和 `return`。
- [结构](/docs/struct)：`struct / impl`、构造、析构、成员函数、关联函数、UFCS、字段访问和块表达式。
- [Enum](/docs/enum)：强类型整数枚举、作用域 case 和显式底层整数转换。
- [Variant 与 match](/docs/variant)：名义和类型、case 构造、`match`、默认初始化和 tagged union 布局。
- [运算符](/docs/operator)：运算符表、内建运算符和第一版 `operator` 重载。
- [类型转换](/docs/cast)：`as` 显式转换。
- [Lambda 与函数值](/docs/lambda)：函数类型、函数指针、普通函数绑定、lambda、捕获和闭包边界。
- [泛型](/docs/generic)：泛型函数、泛型类型、泛型 `impl`、参数包、`template for`、约束和实例化。
- [Concept](/docs/concept)：静态协议、关联类型、默认实现、父 concept 和 `impl concept for Type`。
- [所有权、借用与移动](/docs/ownership)：默认 copy、显式 `ref` / `move`、`like` const 转发 / `move&`、特殊成员和能力推导。

## 标准库

- [标准库导览](/docs/stdlib)：标准库模块分层、导入方式和常见类型。
- [迭代](/docs/iteration)：range-for 的语言入口、`iterator` / `iterable` 协议要求和内建类型入口。
- [标准库 collections](/docs/std_collections)：`vector`、有序唯一键 `map/set`、node 语义和 order-statistics 接口。
- [标准库 ranges](/docs/std_ranges)：`std.ranges` 的范围对象、`iota` 入口和非重载命名规则。
- [标准库 fs](/docs/std_fs)：`std.fs.file`、`open_options`、同步读写和 runtime 文件 ABI。

## 底层与互操作

- [错误处理](/docs/error_handling)：`!`、`panic`、`assert`、前置条件和可恢复失败边界。
- [底层内存分配](/docs/memory_allocation)：裸分配、对象生命周期原语和 runtime ABI。
- [Opaque alias](/docs/opaque_alias)：`type A = opaque T` 的名义封装、layout 规则和显式转换边界。
- [`extern "C"`](/docs/extern_c)：外部 C 函数声明、C 符号名导出和第一版 ABI 边界。
