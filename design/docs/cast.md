# 类型转换

本文档记录 cp 的显式转换语法。类型转换的可转换性规则见 [type_system.md](type_system.md)。

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

## 函数式转换

函数式转换写作：

```cp
let narrowed = i32(ratio);
let widened = f64(count);
```

语法形式：

```text
FunctionStyleCast -> Type ( Expression )
```

函数式转换只接受一个实参。多个实参不是转换表达式。

## 转换规则

`as` 和函数式转换都进入同一套显式转换检查。

显式转换允许：

- 已经允许隐式转换的类型。
- 数值类型之间的显式转换。

不设计：

- 用户自定义转换。
- `reinterpret_cast` / `static_cast` / `const_cast` 这样的多种 cast 分类。
- 通过构造函数进行隐式转换。
