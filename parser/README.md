# Parser

`parser/` 挂在 `source -> preprocessor -> lexer` 之后，负责把 token stream 解析成语法树。真实编译器主线使用递归下降 + Pratt 表达式解析，并输出 `ast_arena + translation_unit_id`；实验二 LL(1) 与实验三算符优先作为独立、可测试的实验模块保留。

## 目录

- `parser.cppm`: 统一导出入口
- `ast/`: Rust 风格 AST，使用 typed id、`std::variant` 和 arena 集中存储
- `diagnostic/`: parser 诊断数据
- `syntax/`: 主线语法分析器，负责 token stream 到 AST
- `expression/`: cp 二元运算符优先级事实源
- `ll1/`: 实验二 LL(1) 文法、FIRST/FOLLOW/SELECT 与 LL(1) 判断
- `op/`: 实验三算符优先文法、FIRSTVT/LASTVT、优先矩阵、表驱动分析与步骤 trace
- `lr/`: 实验四 LR 预留边界，当前未实现
- `tool/`: parser demo 与算符优先 demo

## 对外接口

通常直接：

```cpp
import parser;
```

主要导出：

- `parse_result`
- `parse_translation_unit()`
- AST：`ast_arena`、`translation_unit_id`、`function_syntax`、`statement_syntax`、`expr_syntax`、`type_syntax`
- LL(1) 文法分析接口：`cp_ll1_grammar()`、`analyze_grammar()`
- 算符优先实验接口：`cp_expression_op_grammar()`、`build_op_precedence_table()`、`parse_with_op_precedence()`

## 运行

```bash
./build/parser/parser_demo path/to/file.cp
cat path/to/file.cp | ./build/parser/parser_demo -

./build/parser/op_precedence_demo
./build/parser/op_precedence_demo "1 + 2 * 3" --trace
```

## 文档

- [docs/parser_pipeline.md](/home/kkkzbh/code/cp/parser/docs/parser_pipeline.md)
- [docs/parser_theory_to_code_tutorial.md](/home/kkkzbh/code/cp/parser/docs/parser_theory_to_code_tutorial.md)
- [docs/ll1_experiment.md](/home/kkkzbh/code/cp/parser/docs/ll1_experiment.md)
- [docs/op_precedence_experiment.md](/home/kkkzbh/code/cp/parser/docs/op_precedence_experiment.md)
