# 类型转换

本文档记录 KCP 的显式转换语法。类型转换的可转换性规则见 [type_system.md](type_system.md)。

只设计显式转换，不采用宽泛的 C++ 式隐式转换。

## as 转换

`as` 转换写作：

```cp
let widened = count as f64;
let narrowed = ratio as i32;
```

语法形式：

```text
AsCast -> Expression as Type
```

`as` 是表达式中的二元运算形式，优先级由 parser 的运算符表定义。

## 转换规则

`as` 进入显式转换检查。它不是构造语法，也不触发用户自定义转换函数。

显式转换允许：

- 已经允许隐式转换的类型。
- 任意数值类型之间的显式转换，包括整数/浮点互转、窄化和符号变化。
- 任意指针类型之间的显式转换。
- `enum` 值到它声明的底层整数类型。
- opaque alias 与它的底层类型之间的转换，但只允许在定义该 opaque alias 的模块内部。

示例：

```cp
let int_value = 1.5 as i32;
let float_value = 1 as f64;

let raw: u8* = nullptr as u8*;
let typed = raw as i32*;

enum open_flag : u8 {
    read = 1;
}

let bits: u8 = open_flag::read as u8;
```

`as` 会先按表达式读出规则处理源表达式，因此从引用值转换时，转换的是被引用对象的值类型：

```cp
let value = 1;
let ref alias = value;
let out = alias as i64; // i32 -> i64
```

opaque alias 的显式转换有模块边界：

```cp
module handle_impl;

export type handle = opaque u8*;

export make(raw: u8*) -> handle
{
    return raw as handle; // 合法：定义模块内部
}

export raw(value: handle) -> u8*
{
    return value as u8*; // 合法：定义模块内部
}
```

导入 `handle` 的其它模块不能直接写 `value as u8*` 或 `raw as handle`，只能通过导出的函数和方法使用。

不允许的转换：

- `str` 到 `string`。字符串字面量类型是 `str`，拥有型字符串应写 `string{"text"}` 或使用标准库构造函数，不能写 `"text" as string`。
- 整数到 `enum` 的公共构造。第一版只开放 `enum -> underlying`，不开放 `underlying -> enum`。
- 指针和整数之间的转换。
- `bool`、`char`、`str`、数组、元组、结构体、variant 之间没有本文未列出的通用转换。
- 通过构造函数、成员函数、`operator` 或 opaque alias 外部表示绕过类型边界。

不设计：

- 用户自定义转换。
- 函数式转换 `type(value)`。
- `reinterpret_cast` / `static_cast` / `const_cast` 这样的多种 cast 分类。
- 通过构造函数进行隐式转换。
