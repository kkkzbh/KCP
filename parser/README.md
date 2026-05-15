# Parser

`parser/` 挂在 `source -> preprocessor -> lexer` 之后，负责把 token stream 解析成语法树。真实编译器主线使用递归下降 + Pratt 表达式解析，并输出 `ast_arena + translation_unit_id`。

## 目录

- `parser.cppm`: 统一导出入口与内部状态
- `type.cppm` / `expression.cppm` / `statement.cppm` / `program.cppm`: 主线递归下降实现分区
- `ast/`: Rust 风格 AST，使用 typed id、`std::variant` 和 arena 集中存储

## 对外接口

通常直接：

```cpp
import parser;
```

主要导出：

- `parse_result`
- `parse_translation_unit()`
- AST：`ast_arena`、`translation_unit_id`、`function_syntax`、`statement_syntax`、`expr_syntax`、`type_syntax`

## 文档

- [docs/parser_pipeline.md](/home/kkkzbh/code/cp/parser/docs/parser_pipeline.md)
