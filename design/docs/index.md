# 学习路线

这里是 KCP 语言文档的读者入口。文档分成两层：先读导览页建立整体认识，再按需要进入参考页查细节。

本文档按当前编译器实现写作：已经可解析、可语义检查、可运行或标准库已经提供的能力会写成可用规则；尚未接入的语法、库接口或运行时能力会明确标成不支持或当前限制。遇到“设计直觉”和页面细则冲突时，以对应参考页的可做/不可做边界为准。

## 推荐顺序

1. 读 [语法导览](syntax.md)，了解 KCP 程序的基本形状。
2. 读 [标准库导览](stdlib.md)，知道 `std` 里已经有哪些可用模块。
3. 对照 [示例导览](examples.md)，选择一个主题示例阅读。
4. 遇到具体规则时进入下面的参考页。

## 语言语法

- [类型系统](type_system.md)：类型分类、默认初始化、聚合字面量、`decltype`、返回类型推导、引用和指针规则。
- [初始化](initial.md)：变量、常量、引用 binding 和初始化表达式。
- [模块](module.md)：模块声明、导入、重导出、导出和名字冲突。
- [流程控制](flow.md)：`if`、循环、标签、`break` / `continue`、`return`、表达式语句、块表达式和正常完成判断。
- [结构](struct.md)：`struct / impl`、构造、析构、成员函数、关联函数、UFCS 和字段访问。
- [Enum](enum.md)：强类型整数枚举、作用域 case 和显式底层整数转换。
- [Variant](variant.md)：名义类型、case 构造、`match`、tag + payload storage 布局和 payload 析构限制。
- [运算符](operator.md)：运算符表、内建运算符和第一版 `operator` 重载。
- [类型转换](cast.md)：`as` 显式转换。
- [Lambda 与函数值](lambda.md)：函数类型、函数指针、普通函数绑定、lambda、捕获和闭包边界。
- [泛型](generic.md)：泛型函数、泛型类型、泛型 `impl`、参数包、`template for`、约束和实例化。
- [元编程与反射基础](meta.md)：类型查询、`call_result` 和 `callable`。
- [Concept](concept.md)：静态协议、关联类型、默认实现、父 concept 和 `impl Concept for Type`。
- [所有权、借用与移动](ownership.md)：默认 copy、显式 `ref` / `move`、`like` const 转发 / `move&`、特殊成员和 concept 边界。

## 标准库

- [标准库导览](stdlib.md)：标准库模块分层、导入方式和常见类型。
- [标准库 core](std_core.md)：`optional`、`expected`、`iterator` / `iterable` 和指针 iterator。
- [迭代](iteration.md)：range-for 的语言入口、`iterator` / `iterable` 协议要求和内建类型入口。
- [标准库 memory](std_memory.md)：`raw_buffer`、`span` 和 `contiguous_mutable_range` 的底层连续区间边界。
- [标准库 text](std_text.md)：`str` 扩展、拥有型 `string`、迭代、失效和字符串限制。
- [标准库 string](std_string.md)：拥有型 `string` 字符容器、`str` 视图桥接、追加、迭代和失效规则。
- [标准库 collections](std_collections.md)：集合模块导览和容器选择。
- [标准库 vector](std_vector.md)：连续动态数组、容量管理、下标访问、迭代和算法入口。
- [标准库 map](std_map.md)：有序唯一键映射、node 引用、插入结果、按序访问和 rank。
- [标准库 set](std_set.md)：有序唯一键集合、重复插入语义、按序访问和 rank。
- [标准库 compare](std_compare.md)：三路比较分类、保留 concept、`asc` / `desc` 和默认排序协议。
- [标准库 ranges](std_ranges.md)：`std.ranges` 的 sources、lazy adapters、terminals、UFCS 组合和非重载命名规则。
- [标准库 algorithm](std_algorithm.md)：`sort` / `stable_sort` 的连续可写 range、比较器和稳定性边界。
- [标准库 IO](std_io.md)：`format` / `print` / `display` 协议、stdout/stderr 输出和格式错误边界。
- [标准库 fs](std_fs.md)：`std.fs.file`、`open_options`、同步读写和 runtime 文件 ABI。

## 底层与互操作

- [错误处理](error_handling.md)：`!`、`panic`、`assert`、前置条件和可恢复失败边界。
- [底层内存分配](memory_allocation.md)：裸分配、对象生命周期原语和 runtime ABI。
- [Opaque alias](opaque_alias.md)：`type A = opaque T` 的名义封装、layout 规则和显式转换边界。
- [`extern "C"`](extern_c.md)：外部 C 函数声明、C 符号名导出和第一版 ABI 边界。
