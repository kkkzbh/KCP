# Preprocessor

`preprocessor/` 实现词法分析之前的注释剥离阶段，负责把源代码中的 `//` 和 `/* */` 注释规范化为空白，同时保留原始偏移与行列信息。

## 实现与设计

- `core/` 放预处理入口与核心扫描逻辑。
- `support/` 放通用基础模块，包括诊断类型与预处理产出结构。
- `docs/` 放设计说明文档。
- `core/preprocessor.cppm` 是入口模块，它重新导出 `diagnostic`、`preprocessed`、`scanner` 三个子模块。
- `support/diagnostic.cppm` 定义 `preprocess_diagnostic_kind` 与 `preprocess_diagnostic`，并提供 `to_string`；同时导出兼容别名 `preprocess_issue_kind` / `preprocess_issue`。
- `support/preprocessed.cppm` 定义 `preprocessed_file` 结构和按偏移查询诊断的方法。
- `core/scanner.cppm` 是核心扫描器实现：按顺序跳过引号字面量并把注释位置替换为空格，同时原样保留换行。
- 规范化文本与原始源码长度一致，后续阶段可以直接复用原始偏移进行诊断定位。
- 未闭合的块注释不会中止扫描；会追加一条 `unterminated_block_comment` 问题，供词法阶段重放为 `invalid` token。

更细的管线说明见 [docs/pipeline.md](/home/kkkzbh/code/cp/preprocessor/docs/pipeline.md)。

## 对外暴露的功能接口

通常直接：

```cpp
import preprocessor;
```

主要接口：

- `preprocess_diagnostic_kind` / `preprocess_diagnostic`（旧名别名：`preprocess_issue_kind` / `preprocess_issue`）
  - 描述预处理阶段记录的诊断类别和源码区间
  - `to_string(preprocess_diagnostic_kind)`：获取诊断类别的稳定字符串名
- `preprocessed_file`
  - `normalized_text`：规范化后的源码副本，长度与原始源码一致
  - `issues`：按起始偏移排列的诊断列表
  - `issue_at(offset)`：在指定偏移查询诊断
- `preprocessor_scanner`
  - `preprocessor_scanner(sources, file)`：绑定源文件并准备扫描
  - `run()`：执行一次完整的预处理扫描并返回 `preprocessed_file`
- `preprocess(sources, file)`
  - 便捷函数：等价于 `preprocessor_scanner{ sources, file }.run()`

## 阅读顺序

1. [core/preprocessor.cppm](/home/kkkzbh/code/cp/preprocessor/core/preprocessor.cppm)
2. [support/diagnostic.cppm](/home/kkkzbh/code/cp/preprocessor/support/diagnostic.cppm)
3. [support/preprocessed.cppm](/home/kkkzbh/code/cp/preprocessor/support/preprocessed.cppm)
4. [core/scanner.cppm](/home/kkkzbh/code/cp/preprocessor/core/scanner.cppm)
5. [docs/pipeline.md](/home/kkkzbh/code/cp/preprocessor/docs/pipeline.md)
