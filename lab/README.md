# miniC 实验文法

本目录记录后续用 `kcp` 实现编译原理实验课时使用的 miniC 子集文法。

- [minic_grammar.md](minic_grammar.md): miniC 文法、词法约定、表达式优先级、数组/控制流扩展和样例程序。

当前前端按主 cp 编译器的边界组织，LR(1) parser 是 miniC 主线入口：

- `source`: 管理源码文本、`source_span` 和行列定位。
- `preprocessor`: 对源码做等长归一化，当前负责把注释替换为空白并保留换行。
- `lexer`: 消费预处理结果，输出 span-based token 流和词法诊断。
- `parser`: 构造 miniC 规范 LR(1) ACTION/GOTO 表，执行移进/规约分析，并在规约时生成 AST；当前支持固定长度 `int` 数组、数组形参、`for`、`break` 和 `continue`。

实验三选择 miniC 算术表达式子集，并采用要求中的 (a) 路线：在代码中直接写出算符优先关系矩阵，再进行移进/规约分析。

- `parser/op/main.cp`: 单文件实现实验三，包含 token 映射、人工优先关系矩阵、句柄规约、样例表达式接受和错误表达式拒绝。

语义分析和中间代码生成直接接收主线 LR(1) parser 生成的 AST：

- `semantic/type.cp`、`semantic/symbol.cp`、`semantic/result.cp`、`semantic/state.cp`、`semantic/function.cp`、`semantic/statement.cp`、`semantic/expression.cp`、`semantic/program.cp`: 对 parser AST 做函数表、块级作用域、局部变量/数组、函数调用、数组实参类型、循环跳转、返回语句和常量表达式检查，入口为 `analyze_semantics(file, parsed)`。
- `ir/quad.cp`、`ir/result.cp`、`ir/state.cp`、`ir/expression.cp`、`ir/statement.cp`、`ir/program.cp`、`ir/format.cp`: 把语义通过的 AST 翻译为四元式，入口为 `emit_quads(file, parsed, semantics)`，格式化入口为 `dump_quads(quads)`；数组相关四元式为 `array_decl`、`array_load`、`array_store`。

这里刻意不提供 `semantic` 或 `ir` 聚合模块；调用方显式导入需要的具体模块。
