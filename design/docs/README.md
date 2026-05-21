# docs 内容说明

`docs/` 保存 VitePress 文档站的主要正文。公开网站入口是 `index.md`，本 `README.md` 只作为仓库内维护说明，不参与 VitePress 构建。

## 写作边界

- 面向其他人学习 cp 语言，优先解释语法、静态语义、标准库接口和可运行示例。
- 当前语言设计以本仓库实现和 `std/` 源码为准，不照搬 `实验要求.md` 的参考实现语法。
- 新增语法规则时同步检查对应示例、标准库文档和侧边栏导航。
- 示例代码使用 `cp` 代码块；非代码的 IR、ABI 或 lowering 片段使用 `text` 代码块。

## 页面分组

- `index.md`、`syntax.md`、`stdlib.md`、`examples.md`：读者入口和导览页。
- `type_system.md` 到 `ownership.md`：语言语法和静态语义。
- `iteration.md`、`std_collections.md`、`std_ranges.md`、`std_fs.md`：标准库主题。
- `error_handling.md`、`memory_allocation.md`、`opaque_alias.md`、`extern_c.md`：底层能力和互操作边界。
