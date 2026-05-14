# Parser Pipeline

主线编译路径：

```text
source -> preprocessor -> lexer -> parser.syntax -> ast_arena + translation_unit_id
```

`parser.syntax` 是真实编译器使用的语法分析器。它按递归下降组织声明、语句、类型和 primary / prefix / postfix 表达式，二元表达式由 Pratt 解析处理。

AST 采用 typed id + arena + `std::variant`：

- 节点所有权集中在 `ast_arena`。
- `expr_id`、`stmt_id`、`type_id`、`function_id`、`translation_unit_id` 用于跨节点引用。
- 表达式和语句节点使用明确 struct 组成的 `std::variant`，不使用继承和 `virtual`。
- C++26 reflection 暂不进入核心 AST 路径，后续可用于 dump、trace 或 JSON 快照。

实验模块不替代主线：

- `parser.ll1`: 实验二模块，描述 cp 子集的 CFG，计算 FIRST/FOLLOW/SELECT，并判断 LL(1) 条件。
- `parser.op`: 实验三模块，针对表达式文法构造 FIRSTVT/LASTVT 和算符优先矩阵，并提供表驱动分析器。
- `parser.expression`: 二元运算符优先级事实源，由主线 Pratt parser 和算符优先实验共享。

实验四 LR 目前只预留 `parser/lr/` 目录，不导出 module，也不参与构建。
