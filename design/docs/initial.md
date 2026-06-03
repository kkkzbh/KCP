# 初始化

本文档记录变量、常量和初始化表达式规则。类型推导、默认初始化结果和聚合字面量规则见 [type_system.md](type_system.md)，结构体初始化见 [struct.md](struct.md)。

## 变量声明

变量声明必须显式写初始化表达式：

```cp
let name: type = initializer;
let name = initializer;
let ref name = lvalue_initializer;
let (a, b) = tuple_initializer;
let ref (a, b) = tuple_lvalue_initializer;
```

规则：

- 显式类型存在时，初始化表达式必须能转换到声明类型。
- 省略类型时，变量类型从初始化表达式推导。
- `ref` 声明要求初始化表达式是左值，并推导为引用类型。
- 元组解构声明见 [type_system.md](type_system.md) 的元组操作章节。
- 不支持未初始化局部变量声明。
- `static` 局部声明只支持普通值 binding，不支持 `static ref` 或 `static` 解构。

```cp
let count: i32 = 1;
let total = count + 2;
```

如果需要默认值，必须写默认初始化表达式：

```cp
let count = i32{};
let point = vec2{};
```

裸 `{}` 不表示默认初始化。它保留给块表达式。

## static 局部变量

局部声明可以在 `let` / `const` 后写 `static`：

```cp
next() -> i32
{
    let static counter = 0;
    counter += 1;
    return counter;
}
```

规则：

- `let static name = expr` 创建函数内可见、跨调用保留状态的 binding。
- 初始化表达式在运行时第一次执行到该声明时求值，之后再次执行到该声明时跳过初始化并复用已有值。
- `const static name = expr` 也保留跨调用状态，但 binding const 仍然禁止重新赋值。
- 当前 IR 使用一个全局存储和一个 initialized guard 表达该语义。
- static local 不按普通局部变量作用域退出生成析构清理；需要拥有资源的 static 对象时，应由后续 runtime/global-lifetime 设计统一处理。
- 不支持 `let static ref name = expr`、`const static ref name = expr` 或 `let static (a, b) = tuple`。

## 常量声明

常量声明把 `let` 换成 `const`：

```cp
const name: type = initializer;
const name = initializer;
const ref name = lvalue_initializer;
```

这里的 `const` 是 binding const，表示这个名字绑定不可重新赋值：

```cp
const limit: i32 = 10;
```

binding const 和类型里的 target const 相互独立：

```cp
const p: i32 const* = value;
```

上例中：

- `const p` 表示 `p` 这个名字不能重新绑定。
- `i32 const*` 表示 `p` 最终指向的 `i32` 不可写。

裸 `type const` 不是合法类型。类型中的 `const` 必须跟随 `*` 或 `&` 使用，详见 [type_system.md](type_system.md)。

## 初始化表达式

初始化表达式可以是：

- 字面量。
- 名字。
- 函数调用。
- 类型转换。
- 数组或元组字面量。
- 结构体初始化。
- 默认初始化 `Type{}`。
- 块表达式。
- 返回上述值的其他表达式。

默认初始化表达式写作：

```cp
let count = i32{};
let ok = bool{};
let title = str{};
let values = [i32; 4]{};
let point = vec2{};
```

底层代码可以显式创建内联原始存储：

```cp
let slot = storage T{};
let slots = storage [T; 16]{};
```

这仍然是一个完整的初始化表达式：被初始化的是 storage 对象，不是 `T` 对象。它不等价于“未初始化的 `T` 变量”，也不放宽局部变量必须显式初始化的规则。

块表达式可以作为初始化表达式：

```cp
let value = {
    let base = 1;
    base + 2
};
```

如果块没有尾表达式，或者尾表达式后带分号，块表达式类型为内部 `unit`。块表达式规则见 [struct.md](struct.md)。

## 标识符

标识符当前使用 ASCII 规则，整体与 C++ 接近。
