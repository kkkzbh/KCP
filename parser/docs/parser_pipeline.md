# Parser Pipeline

主线编译路径：

```text
source -> preprocessor -> lexer -> parser -> ast_arena + translation_unit_id
```

`parser` 是真实编译器使用的语法分析器。公开入口集中在 `parser.cppm`，内部按根层模块分区组织声明、语句、类型和 primary / prefix / postfix 表达式，二元表达式由 Pratt 解析处理。对外通过 `import parser;` 使用主线 `parse_result` 和 `parse_translation_unit()`。

AST 采用 typed id + arena + `std::variant`：

- 节点所有权集中在 `ast_arena`。
- `expr_id`、`stmt_id`、`type_id`、`function_id`、`translation_unit_id` 用于跨节点引用。
- 表达式和语句节点使用明确 struct 组成的 `std::variant`，不使用继承和 `virtual`。
- C++26 reflection 暂不进入核心 AST 路径，后续可用于 dump、trace 或 JSON 快照。
