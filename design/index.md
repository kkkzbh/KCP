---
layout: home

hero:
  name: cp
  text: 编译原理实验语言
  tagline: 从语法、类型系统、标准库到可执行示例，系统学习这门实验语言的设计。
  actions:
    - theme: brand
      text: 开始学习
      link: /docs/
    - theme: alt
      text: 语法导览
      link: /docs/syntax
    - theme: alt
      text: 标准库
      link: /docs/stdlib

features:
  - title: 语法先行
    details: 以模块、函数、变量、控制流、结构体、泛型和所有权为主线，先建立可读写 cp 程序的基本能力。
  - title: 标准库导向
    details: 通过 std.core、std.memory、std.collections、std.text、std.ranges、std.io 和 std.fs 理解普通模块如何组成库。
  - title: 示例可对照
    details: design/examples 按主题保存独立示例项目，适合把文档规则和真实代码放在一起阅读。
---

## 文档边界

`design/` 是 cp 语言的学习与设计入口：

- `docs/` 面向读者解释语法、静态语义和标准库接口。
- `examples/` 按主题提供独立 `.cp` 示例项目。
- 仓库根目录的 `std/` 保存标准库源码，文档中的标准库说明以这些普通 cp 模块为准。

cp 的源文件后缀是 `.cp`。这套文档以当前实验语言为准，不照搬 `实验要求.md` 中的参考实现语法。
