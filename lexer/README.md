# Lexer

`lexer/` 实现该语言的词法分析器，负责把源代码切成 `token` 序列，并在出错时产出诊断信息。

## 实现与设计

- 顶层 `source/` 放前端共享的源码管理、切片与行列定位模块。
- `token/` 放词法单元类型、标志与字符串名。
- `diagnostic/` 放词法阶段的错误类型、诊断接收 concept 与默认收集器。
- `charset/` 放 ASCII 字符分类原语。
- `scanner/` 放词法扫描逻辑。
- 顶层 `preprocessor/` 提供注释预处理阶段，负责把注释归一化为空格和换行，并保留原始偏移。
- `docs/` 放设计说明文档。
- `lexer.cppm` 是入口模块，它重新导出 `source`、`token`、`lexer_diagnostic`、`charset`、`scanner`。
- `scanner/scanner.cppm` 是核心扫描器实现：基于预处理后的规范化源码跳过空白，并按最长匹配规则识别关键字、标识符、数字、字符串、字符字面量与运算符。
- 当前词法规则把 `as` 视为保留关键字，支持复合赋值 `+= -= *= /= %= &= |= ^= <<= >>=`。
- 字符串与字符字面量采用常见 C 风格转义，支持简单转义、八进制转义和十六进制转义。
- 本语言不支持 C/C++ 风格的 `\` 加物理换行行拼接；该情况会在词法阶段直接报错。
- 注释预处理保持输入长度不变，因此 `token.span` 仍始终指向原始源码。
- 词法错误不会中止扫描；会生成 `invalid` token，同时通过满足 `lexer_diagnostic_sink` concept 的接收器报告错误。

更细的正规式设计见 [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)。

## 对外暴露的功能接口

通常直接：

```cpp
import lexer;
```

主要接口：

- `source_manager`
  - `add_source(name, text)`：注册源码并返回 `file_id`
  - `slice(source_span)` / `position(byte_pos)`：取词素和位置
- `token_kind` / `token_flags` / `token`
  - 描述 token 类型、附加标记和源码区间
- `lexer_diagnostic_sink`
  - 诊断接收 concept，要求提供 `report(lexer_diagnostic)` 且返回 `void`
- `std::vector<lexer_diagnostic>`
  - 默认收集容器，可通过 `push_back()` 接收诊断并用 `clear()` 清空
- `lexer`
  - `lexer(sources, file, sink)`：构造函数模板受 `lexer_diagnostic_sink` 约束
  - `peek()`：预读一个 token
  - `next()`：读取下一个 token
  - `tokenize_all()`：一次性扫描完整 token 流
  - `reset(file)`：切换到新文件重新扫描

## 阅读顺序

1. [lexer.cppm](/home/kkkzbh/code/cp/lexer/lexer.cppm)
2. [source.cppm](/home/kkkzbh/code/cp/source/source.cppm)
3. [token/token.cppm](/home/kkkzbh/code/cp/lexer/token/token.cppm)
4. [diagnostic/diagnostic.cppm](/home/kkkzbh/code/cp/lexer/diagnostic/diagnostic.cppm)
5. [diagnostic/concepts.cppm](/home/kkkzbh/code/cp/lexer/diagnostic/concepts.cppm)
6. [charset/charset.cppm](/home/kkkzbh/code/cp/lexer/charset/charset.cppm)
7. [scanner/scanner.cppm](/home/kkkzbh/code/cp/lexer/scanner/scanner.cppm)
8. [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)
