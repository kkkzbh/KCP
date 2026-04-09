# Lexer

`lexer/` 实现该语言的词法分析器，负责把源代码切成 `token` 序列，并在出错时产出诊断信息。

## 实现与设计

- `core/` 放词法入口与核心扫描逻辑。
- `support/` 放通用基础模块，包括源码管理、token 定义与诊断接口。
- `docs/` 放设计说明文档。
- `core/lexer.cppm` 是入口模块，它重新导出 `source`、`token`、`diagnostic`、`scanner` 四个子模块。
- `support/source.cppm` 管理源码文件、切片与行列位置。
- `support/token.cppm` 定义 `token_kind`、`token_flags` 与 `token`。
- `support/diagnostic.cppm` 定义词法阶段的错误类型与诊断收集接口。
- `core/scanner.cppm` 是核心扫描器实现：跳过空白和注释，按最长匹配规则识别关键字、标识符、数字、字符串、字符字面量与运算符。
- 当前词法规则把 `as` 视为保留关键字，支持复合赋值 `+= -= *= /= %= &= |= ^= <<= >>=`。
- 字符串与字符字面量采用常见 C 风格转义，支持简单转义、八进制转义和十六进制转义。
- 本语言不支持 C/C++ 风格的 `\` 加物理换行行拼接；该情况会在词法阶段直接报错。
- 词法错误不会中止扫描；会生成 `invalid` token，同时通过 `diagnostic_sink` 报告错误。

更细的正规式设计见 [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)。

## 对外暴露的功能接口

通常直接：

```cpp
import lexer;
```

主要接口：

- `front::source_manager`
  - `add_source(name, text)`：注册源码并返回 `file_id`
  - `slice(span)` / `position(file, offset)`：取词素和位置
- `front::token_kind` / `front::token_flags` / `front::token`
  - 描述 token 类型、附加标记和源码区间
- `front::diagnostic_sink`
  - 诊断输出抽象接口
- `front::vector_diagnostic_sink`
  - 默认收集器，实现 `diagnostics()` / `clear()`
- `front::lexer`
  - `lexer(sources, file, sink)`：构造扫描器
  - `peek()`：预读一个 token
  - `next()`：读取下一个 token
  - `tokenize_all()`：一次性扫描完整 token 流
  - `reset(file)`：切换到新文件重新扫描

## 阅读顺序

1. [core/lexer.cppm](/home/kkkzbh/code/cp/lexer/core/lexer.cppm)
2. [support/source.cppm](/home/kkkzbh/code/cp/lexer/support/source.cppm)
3. [support/token.cppm](/home/kkkzbh/code/cp/lexer/support/token.cppm)
4. [support/diagnostic.cppm](/home/kkkzbh/code/cp/lexer/support/diagnostic.cppm)
5. [core/scanner.cppm](/home/kkkzbh/code/cp/lexer/core/scanner.cppm)
6. [docs/regex.md](/home/kkkzbh/code/cp/lexer/docs/regex.md)
