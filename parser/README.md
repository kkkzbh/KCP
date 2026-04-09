# Parser

`parser/` 实现实验二 2.1 的递归下降 LL(1) 语法分析器，直接挂在现有 `preprocessor -> lexer` 之后。

## 目录

- `support/`: AST、语法诊断、trace 事件
- `core/`: 机器可校验文法、递归下降 parser、聚合入口模块
- `tool/main.cpp`: 命令行 demo，默认输出 `accepted/rejected`，带 `--trace` 时输出分析轨迹

## 对外接口

通常直接：

```cpp
import parser;
```

主要导出：

- `parse_options`
- `parse_result`
- `parse_translation_unit()`
- 最小 AST：`translation_unit_syntax`、`function_syntax`、`statement_syntax`、`expr_syntax`、`type_syntax`
- LL(1) 文法分析接口：`cp_ll1_grammar()`、`analyze_grammar()`

## 运行

```bash
./build/parser/parser_demo path/to/file.cp
./build/parser/parser_demo --trace path/to/file.cp
cat path/to/file.cp | ./build/parser/parser_demo -
```

## 文档

实验 2.1 的文法分析、CFG、左递归/左公因子消除、SELECT 集与 LL(1) 判断见：

- [docs/ll1_experiment.md](/home/kkkzbh/code/cp/parser/docs/ll1_experiment.md)
