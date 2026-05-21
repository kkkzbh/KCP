# Lexer

`lexer/` 实现该语言的词法分析器，负责把预处理后的单文件源码视图切成 `token` 序列，并在出错时产出词法诊断。

## 实现与设计

- 顶层 `source/` 放前端共享的源码管理、切片与行列定位模块。
- 顶层 `diagnostic/` 放统一的阶段诊断类型。
- `token.cppm` 放词法单元类型、标志与字符串名。
- `charset.cppm` 放 ASCII 字符分类原语。
- `cursor.cppm` 放扫描游标、局部 offset 与全局 `source_span` 的映射。
- `lexer.cppm` 放词法扫描逻辑和对外 `lex(...)` 入口。
- 顶层 `preprocessor/` 提供注释预处理阶段，负责把注释归一化为空格和换行，并保留原始偏移。
- `docs/` 放设计说明文档。
- `lexer.cppm` 是入口模块，它重新导出 `token`、`diagnostic` 和字符分类原语，并导出一次性 `lex(...)` 词法处理函数；内部扫描状态机基于 `preprocessed_file.normalized_text` 跳过空白，并按最长匹配规则识别关键字、标识符、数字、字符串、字符字面量与运算符。
- 当前词法规则把 `as` 视为保留关键字，支持复合赋值 `+= -= *= /= %= &= |= ^= <<= >>=`。
- 字符串与字符字面量采用常见 C 风格转义，支持简单转义、八进制转义和十六进制转义。
- 本语言不支持 C/C++ 风格的 `\` 加物理换行行拼接；该情况会在词法阶段直接报错。
- 注释预处理保持输入长度不变，因此 `token.span` 仍始终指向原始源码。
- 词法错误不会中止扫描；会生成 `invalid` token，同时写入 `lex_result.diagnostics`。

更细的正规式设计见 [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)。

## 对外暴露的功能接口

通常直接：

```cpp
import source;
import preprocessor;
import lexer;
```

主要接口：

- `preprocessed_file`（来自 `preprocessor` 模块）
  - `normalized_text`：词法阶段扫描的等长源码视图
  - `file_start`：用于把 token 局部 offset 映射回原始源码坐标
- `token_kind` / `token_flags` / `token`
  - 描述 token 类型、附加标记和源码区间
- `lex_result`
  - `tokens`：完整 token 流，包含末尾 `eof`
  - `diagnostics`：词法诊断列表
- `lex(preprocessed_file const&)`
  - 借用指定预处理文件并执行完整词法处理，返回 `lex_result`

## 阅读顺序

1. [lexer.cppm](/home/kkkzbh/code/cp/lexer/lexer.cppm)
2. [source.cppm](/home/kkkzbh/code/cp/source/source.cppm)
3. [token.cppm](/home/kkkzbh/code/cp/lexer/token.cppm)
4. [diagnostic.cppm](/home/kkkzbh/code/cp/diagnostic/diagnostic.cppm)
5. [charset.cppm](/home/kkkzbh/code/cp/lexer/charset.cppm)
6. [cursor.cppm](/home/kkkzbh/code/cp/lexer/cursor.cppm)
7. [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)
