# Design

- 语言名: `cp`
- 源文件后缀: `.cp`

`design/` 按内容分为两类:

- `examples/`: 语言示例代码, 用来展示模块、函数、调用等实际写法
- `docs/`: 语言设计文档, 用来记录语法、类型系统、流程控制等规则

## 示例代码

- `examples/main.cp`: 主程序示例, 展示导入模块、变量定义、聚合字面量与函数调用
- `examples/math.cp`: 数学模块示例, 展示 `export module`、导出函数、`if` 与基础运算
- `examples/types_demo.cp`: 类型示例, 展示字面量、`sequence/array/tuple`、引用/指针与两种 cast
- `examples/flow_demo.cp`: 流程控制示例, 展示 `while`、`do while`、范围 `for` 以及带标签的 `break/continue`

## 设计文档

- `docs/builtin_type.md`: 内建类型
- `docs/cast.md`: 类型转换
- `docs/flow.md`: 流程控制
- `docs/initial.md`: 变量、常量与初始化
- `docs/module.md`: 模块声明、导入与导出
- `docs/operator.md`: 运算符
- `docs/struct.md`: `struct / impl / trait`
