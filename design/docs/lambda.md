# Lambda 与函数值

本文档记录 cp 的函数值、函数指针、lambda 和闭包设计。普通函数声明、返回类型推导和块表达式规则分别见 [type_system.md](type_system.md) 与 [struct.md](struct.md)。

## 函数类型

cp 区分函数类型和函数指针类型：

```cp
f(i32, i32) -> i32
f*(i32, i32) -> i32
```

`f(...) -> R` 是函数类型，表示一个非空、可直接调用的函数实现。它可以由普通命名函数或无捕获 lambda 产生。

`f*(...) -> R` 是函数指针类型，表示运行时函数地址。它接近 C/C++ 的函数指针，主要用于 C ABI、底层表驱动和需要显式地址语义的场景。

函数类型参数可以只写类型，也可以写参数名：

```cp
f(i32, i32) -> i32
f(left: i32, right: i32) -> i32
```

参数名由 parser 识别并保存在 AST 中，用于诊断、文档和可读性。它不参与类型等价、不参与 ABI、不参与重载选择，也不会引入可在表达式中引用的名字。上面两个类型完全等价。

函数指针类型同理：

```cp
f*(i32) -> i32
f*(value: i32) -> i32
```

## 普通函数绑定

普通命名函数可以作为值绑定：

```cp
add(x: i32, y: i32) -> i32 {
    return x + y;
}

let op: f(i32, i32) -> i32 = add;
let result = op(1, 2);
```

这里 `add` 在值位置表示一个函数实现。它不是普通变量，也没有可修改状态；绑定到 `f(...) -> R` 后仍表示非空函数值。

普通命名函数也可以在上下文需要函数指针时转换为 `f*(...) -> R`：

```cp
let raw: f*(i32, i32) -> i32 = add;
```

`f(...) -> R` 和 `f*(...) -> R` 的语义边界：

- `f(...) -> R` 是首选回调类型，语义上非空，不暴露指针运算或地址整数转换。
- `f*(...) -> R` 是底层地址值，可以默认初始化为空函数指针。
- 调用空函数指针属于底层 unsafe 契约；编译器只做类型检查。
- 函数值可以在实现中 lower 为函数地址，但语言语义不把它当作裸指针。

## Lambda 语法

lambda 表达式写作：

```text
LambdaExpr -> f ( LambdaParamList? ) ReturnType? LambdaBody
LambdaBody -> Block
            | => Expr
```

参数语法与普通函数参数一致：

```cp
let square = f(x: i32) -> i32 {
    x * x
};
```

返回类型可以省略，由 lambda body 推导：

```cp
let square = f(x: i32) {
    x * x
};
```

当上下文类型已知时，lambda 参数类型也可以省略：

```cp
let square: f(i32) -> i32 = f(x) {
    x * x
};
```

省略参数类型只允许在存在明确上下文函数类型时使用。没有上下文类型时，lambda 参数必须显式标注类型。

## Lambda Body

lambda 支持三种自然写法。

常规函数体风格：

```cp
let abs = f(x: i32) -> i32 {
    if(x < 0) {
        return -x;
    }

    return x;
};
```

块表达式风格：

```cp
let abs = f(x: i32) -> i32 {
    if(x < 0) {
        return -x;
    }

    x
};
```

表达式体风格：

```cp
let inc = f(x: i32) -> i32 => x + 1;
```

`{ ... }` body 不在语法上分成“函数体块”和“块表达式块”。parser 只解析为一个块；语义阶段同时允许 `return` 和尾表达式：

- `return value;` 从当前 lambda 返回，不从外层函数返回。
- `return;` 只允许用于 `unit` 返回 lambda。
- 最后一项是无分号表达式时，它作为正常路径返回值。
- 没有尾表达式，或尾表达式后带分号时，正常路径返回 `unit`。
- 显式返回类型存在时，所有 `return value;` 和尾表达式都必须能转换到该返回类型。
- 省略返回类型时，语义分析统一所有 `return value;` 和尾表达式类型；没有任何带值返回路径时推导为 `unit`。

表达式体：

```cp
f(x: i32) => x + 1
```

等价于：

```cp
f(x: i32) {
    x + 1
}
```

表达式体不需要也不允许在 `=>` 后直接写 `return` 语句；如果需要提前返回，应使用 `{ ... }` body。

## 捕获

lambda 可以自动捕获外层局部变量：

```cp
let bias = 10;

let add_bias = f(x: i32) {
    x + bias
};
```

捕获规则：

- lambda body 中引用外层局部变量时，编译器自动捕获该变量。
- 只读取的变量按只读捕获处理。
- 被赋值、取可写引用或传给需要可写引用的参数时，按可写捕获处理。
- 捕获只发生在普通词法作用域变量上；模块项、普通函数名、类型名和 concept 名不算捕获。

无捕获 lambda 等价于普通命名函数：

```cp
let f: f(i32) -> i32 = f(x: i32) {
    x + 1
};

let p: f*(i32) -> i32 = f(x: i32) {
    x + 1
};
```

有捕获 lambda 生成匿名闭包类型：

```cp
let bias = 10;

let closure = f(x: i32) {
    x + bias
};
```

有捕获 lambda 不能绑定到函数类型或函数指针类型：

```cp
let bad_function: f(i32) -> i32 = f(x: i32) {
    x + bias
}; // error

let bad_pointer: f*(i32) -> i32 = f(x: i32) {
    x + bias
}; // error
```

这条规则避免把需要环境对象的闭包误认为裸代码地址。

## 捕获与逃逸

为了让默认捕获自然，同时避免悬垂引用，逃逸规则按上下文区分：

- lambda 只在当前作用域内调用或传给非逃逸参数时，捕获可以按引用实现。
- lambda 被返回、赋给外层存储、放入结构体字段或传给可能保存它的参数时，视为逃逸。
- 逃逸 lambda 的只读捕获按值保存。
- 逃逸 lambda 的可写捕获报错。

示例：

```cp
make_adder(bias: i32)
{
    return f(x: i32) {
        x + bias
    };
}
```

这里 `bias` 是只读捕获，lambda 逃逸，因此闭包对象保存 `bias` 的值。

```cp
make_counter()
{
    let count = 0;

    return f() {
        count = count + 1;
        count
    };
} // error: escaping lambda mutably captures count
```

## 函数参数中的回调

普通回调优先写函数类型：

```cp
apply(value: i32, cb: f(i32) -> i32) -> i32 {
    return cb(value);
}

inc(x: i32) -> i32 {
    return x + 1;
}

let a = apply(1, inc);
let b = apply(1, f(x: i32) => x + 1);
```

`cb: f(i32) -> i32` 表示调用方必须提供普通函数或无捕获 lambda。实现可以把它 lower 为代码地址，但语言层保证它是非空函数值。

如果 API 明确需要底层地址语义，才写函数指针：

```cp
apply_raw(value: i32, cb: f*(i32) -> i32) -> i32 {
    return cb(value);
}
```

有捕获 lambda 不能传给 `f(...) -> R` 或 `f*(...) -> R` 参数。它需要由类型推导、泛型参数或 `callable` concept 接收。

```cp
let bias = 10;

let closure = f(x: i32) {
    x + bias
};

// apply(1, closure); // error if apply expects f(i32) -> i32
```

`callable` concept 可以让泛型函数接收普通函数、无捕获 lambda 和有捕获闭包：

```cp
map<F>(values: array<i32,4>, callback: F)
requires F: callable
{
    // ...
}
```

## 支持内容

Lambda 支持：

- 函数类型 `f(...) -> R`。
- 函数指针类型 `f*(...) -> R`。
- 普通函数名在值位置绑定为函数值。
- 无捕获 lambda 可绑定为函数类型或函数指针类型。
- 有捕获 lambda 生成匿名闭包类型。
- 有捕获 lambda 不能绑定为函数类型或函数指针类型。
- lambda `{ ... }` body 同时支持 `return` 和尾表达式。
- lambda `=> expr` 表达式体。
- 参数名可出现在函数类型中，但不参与类型等价。

Lambda 不支持：

- 显式捕获列表。
- 捕获生命周期完整证明。
- 闭包类型的命名、结构反射或稳定 ABI。
- 闭包到 `callable` concept 的完整标准库协议。
- 函数重载和函数类型参与重载排序。
