# Preprocessor

`preprocessor/` 实现词法分析之前的注释剥离阶段，负责把源代码中的 `//` 和 `/* */` 注释规范化为空白，同时保留原始偏移与行列信息。

## 实现与设计

- `diagnostic.cppm` 放预处理阶段的诊断类型。
- `preprocessed.cppm` 放预处理产物结构。
- `preprocessor.cppm` 放预处理扫描逻辑和对外 `preprocess` 函数。
- `docs/` 放设计说明文档。
- `preprocessor.cppm` 是入口模块，它重新导出 `diagnostic` 与 `preprocessed` 两个类型模块，并直接实现核心扫描器：按顺序跳过引号字面量并把注释位置替换为空格，同时原样保留换行。
- `diagnostic.cppm` 定义 `preprocess_diagnostic_kind` 与 `preprocess_diagnostic`，并提供 `to_string`；同时导出兼容别名 `preprocess_issue_kind` / `preprocess_issue`。
- `preprocessed.cppm` 定义 `preprocessed_file` 结构和按偏移查询诊断的方法。
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
- `preprocess(sources, file)`
  - 执行完整的预处理扫描并返回 `preprocessed_file`

## 阅读顺序

1. [preprocessor.cppm](/home/kkkzbh/code/cp/preprocessor/preprocessor.cppm)
2. [diagnostic.cppm](/home/kkkzbh/code/cp/preprocessor/diagnostic.cppm)
3. [preprocessed.cppm](/home/kkkzbh/code/cp/preprocessor/preprocessed.cppm)
4. [docs/pipeline.md](/home/kkkzbh/code/cp/preprocessor/docs/pipeline.md)
