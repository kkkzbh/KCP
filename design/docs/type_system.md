# Type System

本文档记录当前主线编译器已经设计并准备实现的类型规则。范围严格对齐当前 parser 支持的语言面：函数、模块、`let` / `const`、内建类型、`array` / `sequence` / `tuple`、引用、指针、类型转换、流程控制和运算符。

`struct / impl / trait`、闭包、泛型函数暂时只是未来方向，不进入当前语义分析实现。

## 内建类型

- `bool`
- `i8 i16 i32 i64`
- `u8 u16 u32 u64`
- `f32 f64`
- `char`
- `str`

整数字面量默认类型为 `i32`，浮点字面量默认类型为 `f64`。`true` 和 `false` 的类型为 `bool`，字符字面量的类型为 `char`，字符串字面量的类型为 `str`。

内部存在 `unit` 类型，用于表示没有值的返回结果；它 lowered 到 LLVM IR 时是 `void`。`unit` 当前不是用户可写类型。

## 函数返回类型

函数可以显式写返回类型：

```cp
add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

显式返回类型存在时，所有 `return value;` 都必须能转换到声明类型；`return;` 只允许用于 `unit` 返回。

函数也可以省略 `-> type`：

```cp
main()
{
    let x = 1;
}
```

省略返回类型时，语义分析收集函数体中的所有 `return value;` 并统一出返回类型。没有任何带值 `return` 时，函数返回类型推导为内部 `unit`。

## 结构化类型

`array<T,N>` 表示固定长度数组，使用 `[ ... ]` 字面量：

```cp
let data: array<i32,4> = [1, 2, 3, 4];
```

`sequence<T,N>` 表示固定长度序列，使用 `{ ... }` 字面量：

```cp
let seq: sequence<i32,3> = {1, 2, 3};
```

`array` 和 `sequence` 都是同构类型，所有元素必须具有统一元素类型，长度 `N` 必须精确匹配。

`tuple<T...>` 表示元组，使用 `(a,b,c)` 字面量：

```cp
let triple: tuple<i32,f64,char> = (1, 0.5, 'x');
```

元组是异构类型。普通 `(x)` 是分组表达式，不是单元素元组。

## 聚合字面量推导

`[]` 优先对齐数组上下文，`{}` 优先对齐序列上下文：

```cp
let data: array<i32,4> = [1, 2, 3, 4];
let seq: sequence<i32,3> = {1, 2, 3};
```

有上下文时，字面量按目标元素类型逐项检查转换，并检查长度精确匹配。

没有上下文时，非空字面量自行产生类型：

```cp
let data = [1, 2, 3]; // array<i32,3>
let seq = {1, 2, 3};  // sequence<i32,3>
```

没有上下文时，空 `[]` 和空 `{}` 报错，因为无法推导元素类型。

无上下文自行推导时，只允许同类数值提升：整数族内部可以统一，浮点族内部可以统一。整数和浮点数不跨类统一，因此 `[1, 2.0]` 报错。

## 引用、指针与 const

引用与指针写作：

```cp
i32&
i32*
i32**
i32 const&
i32 const*
i32 const**&
i32*&
i32**&
```

`const` 分为两个语法位置，语义不同。

`const name = ...` 或函数参数中的 `const name: T` 是 binding const，表示这个名字绑定不可重新赋值：

```cp
const value = 1;
view(const p: i32 const*) {
}
```

`type const*` / `type const&` 中的 `const` 是 target const，表示指针或引用最终指向的基础值不可写。不管有多少级 `*` / `&`，它都约束最终目标值，而不是中间指针本身。

```cp
update(p: i32*) {
}

view(const p: i32 const*) {
}

borrow(value: i32 const&) {
}
```

类型语法约束为：

```text
Parameter   -> const? identifier : Type
Declaration -> (let | const) identifier (: Type)? = Expression

Type        -> TypeBase TargetConst? TypeSuffix
TargetConst -> const
TypeSuffix  -> *+ &? | &
```

`TargetConst` 只有在 `TypeSuffix` 非空时合法。因此 `i32 const`、`i32* const`、`i32& const` 都不是合法类型写法。当前不支持 C++ 式指针自身 const，也不支持 `volatile` / `restrict`。

## 转换

显式转换有两种写法：

```cp
value as i32
i32(value)
```

两者都进入显式转换检查。

隐式转换只允许本文档明确说明的数值提升和上下文目标转换，不采用宽泛的 C++ 式隐式转换。

## 控制流类型规则

`if`、`while` 和 `do while` 的条件表达式必须是 `bool`。

`for` 当前仅支持范围形式：

```cp
for(let value : values) {
}
```

范围表达式必须是 `array<T,N>` 或 `sequence<T,N>`，循环变量类型为元素类型。`break` 和 `continue` 必须位于循环中；带 label 时，label 必须能解析到外层带标签的 `for`。

## 运算符

运算符优先级由 parser 的共享表定义，整体与 C++ 接近，但逻辑运算使用 `and` / `or` / `not`。

算术运算要求数值类型。整数族内部、浮点族内部可以统一类型；结果类型为统一后的类型。比较运算结果为 `bool`。逻辑运算要求 `bool` 操作数，结果为 `bool`。赋值左侧必须是左值，不能给 `const` binding 重新赋值。
