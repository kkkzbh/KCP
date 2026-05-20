# miniC 实验文法

本目录记录后续用 `kcp` 实现编译原理实验课时使用的 miniC 子集文法。

- [minic_grammar.md](minic_grammar.md): 超轻量级 miniC 文法、词法约定、表达式优先级和样例程序。

当前前端按主 cp 编译器的边界拆成三个基础模块：

- `source`: 管理源码文本、`source_span` 和行列定位。
- `preprocessor`: 对源码做等长归一化，当前负责把注释替换为空白并保留换行。
- `lexer`: 消费预处理结果，输出 span-based token 流和词法诊断。
