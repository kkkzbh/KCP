# 初始化

完整类型推导与聚合字面量规则见 [type_system.md](type_system.md)。

## 变量
let name: type = 初始化表达式
let name = 初始化表达式 表示类型可推导

## 常量
把 `let` 换为 `const` 即可。这里的 `const` 是 binding 属性，表示名字不能被重新赋值。

const name: type = 初始化表达式

binding const 和类型里的 target const 相互独立：

```cp
const p: i32 const* = value;
```

上例中，`const p` 表示 `p` 这个名字不能重新绑定；`i32 const*` 表示 `p` 最终指向的 `i32` 不可写。裸 `type const` 不是合法类型，类型中的 `const` 必须跟随 `*` 或 `&` 使用。

## 初始化表达式
可以是sequence, 匿名变量, 字面量, 以及返回这些的表达式

## 标识符name
ASCII，与C++一致
