# 语言设计文档

本目录记录 cp 的语言语法和静态语义规则。这里的内容是编译器前端、语义分析和代码生成需要共同遵守的语言契约。

## 文档目录

- [类型系统](type_system.md)：类型分类、默认初始化、聚合字面量、`decltype`、返回类型推导、引用和指针规则。
- [所有权、借用与移动](ownership.md)：默认 copy、显式 `ref` / `move`、`like` const 转发 / `move&`、特殊成员和能力推导。
- [初始化](initial.md)：变量、常量、引用 binding 和初始化表达式。
- [模块](module.md)：模块声明、导入、重导出、导出和名字冲突。
- [流程控制](flow.md)：`if`、循环、标签、`break` / `continue` 和 `return`。
- [错误处理](error_handling.md)：`!`、`panic`、`assert`、前置条件和可恢复失败边界。
- [结构](struct.md)：`struct / impl`、构造、析构、成员函数、关联函数、UFCS、字段访问和块表达式。
- [Enum](enum.md)：强类型整数枚举、作用域 case 和显式底层整数转换。
- [Variant](variant.md)：名义和类型、case 构造、`match`、默认初始化和 tagged union 布局。
- [Opaque alias](opaque_alias.md)：`type A = opaque T` 的名义封装、layout 规则和显式转换边界。
- [Concept](concept.md)：静态协议、关联类型、默认实现、父 concept 和 `impl concept for Type`。
- [泛型](generic.md)：泛型函数、泛型类型、泛型 `impl`、参数包、`template for`、约束和实例化。
- [Lambda 与函数值](lambda.md)：函数类型、函数指针、普通函数绑定、lambda、捕获和闭包边界。
- [迭代](iteration.md)：range-for 的语言入口、`iterator` / `iterable` 协议要求和内建类型入口。
- [标准库 collections](std_collections.md)：`vector`、有序唯一键 `map/set`、node 语义和 order-statistics 接口。
- [标准库 ranges](std_ranges.md)：`std.ranges` 的范围对象、`iota` 入口和非重载命名规则。
- [标准库 fs](std_fs.md)：`std.fs.file`、`open_options`、同步读写和 runtime 文件 ABI。
- [运算符](operator.md)：运算符表、内建运算符和第一版 `operator` 重载。
- [类型转换](cast.md)：`as` 显式转换。
- [`extern "C"`](extern_c.md)：外部 C 函数声明、C 符号名导出和第一版 ABI 边界。
- [底层内存分配](memory_allocation.md)：裸分配、对象生命周期原语和 runtime ABI。

## 写作规范

语言文档使用同一套结构：

- 顶部用一个一级标题说明主题。
- 开头第一段说明本文档的边界，并链接到相邻主题。
- 有新增语法时使用 `## 语法总览`；如果语法只属于某个大章节，使用该章节下的 `### 语法总览`。产生式统一放在 `text` 代码块中。
- 规则列表统一写在 `规则：` 或更具体的 `检查规则：` 之后。
- 示例代码使用 `cp` 代码块。
- 明确第一版支持和不支持的内容，避免把未来方向写成当前规则。
- 交叉引用使用相对链接，语言文档内部链接直接写文件名。
