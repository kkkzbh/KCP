# 语法导览

这页按学习顺序解释 KCP 程序的主要语法。需要查完整规则时，再进入对应参考页。

## 最小程序

KCP 的入口函数通常写成 `main() -> i32`。函数体使用花括号，返回类型放在参数列表之后。

```cp
main() -> i32
{
    return 0;
}
```

变量用 `let`，常量用 `const`。类型可以显式标注，也可以由初始化表达式推导。

```cp
main() -> i32
{
    const answer: i32 = 42;
    let ratio = 0.5;
    let value = (answer as f64 * ratio) as i32;
    return value;
}
```

相关参考：[初始化](initial.md)、[类型系统](type_system.md)、[类型转换](cast.md)。

## 模块

一个文件可以声明命名模块，`export` 决定哪些声明对导入者可见。

```cp
export module sample.math;

export add(left: i32, right: i32) -> i32
{
    return left + right;
}
```

使用模块时通过 `import` 引入：

```cp
import sample.math;

main() -> i32
{
    return add(16, 26);
}
```

相关参考：[模块](module.md)。

## 表达式和控制流

KCP 使用 `if`、`while`、`do while` 和范围 `for`。布尔逻辑使用 `and`、`or`、`not`。

```cp
sum_to(end: i32) -> i32
{
    let total = 0;

    for(let value : iota(0, end)) {
        if(value == 3) {
            continue;
        }
        total += value;
    }

    return total;
}
```

范围 `for` 依赖 `iterable` / `const_iterable` 协议，标准库的 `iota` 提供最常见的半开范围。`.iter()` 返回的是内部游标，不能直接作为范围表达式。

相关参考：[流程控制](flow.md)、[迭代](iteration.md)、[标准库 ranges](std_ranges.md)。

## 结构体和 impl

`struct` 定义数据布局，`impl` 给类型挂接构造函数、成员函数、关联函数和运算符。

```cp
struct point {
    x: i32;
    y: i32;
}

impl point {
    point(x: i32, y: i32)
    {
        return point{ .x = x, .y = y };
    }

    len2(self const&) -> i32
    {
        return x * x + y * y;
    }
}

main() -> i32
{
    let p = point{ 3, 4 };
    return p.len2();
}
```

成员函数显式声明 `self` 的借用方式。`self const&` 表示只读借用，`self&` 表示可写借用。

相关参考：[结构](struct.md)、[所有权、借用与移动](ownership.md)。

## Enum、Variant 和 match

`enum` 表示带底层整数的强类型枚举。`variant` 表示名义和类型，可以拥有空 case 或带 payload 的 case。

```cp
variant result {
    value(i32),
    unexpected(str),
}

read_value(ok: bool) -> result
{
    if(ok) {
        return result::value(1);
    }
    return result::unexpected("failed");
}

main() -> i32
{
    match read_value(true) {
        .value(value) => {
            return value;
        },
        .unexpected(error) => {
            return 1;
        },
    };
}
```

相关参考：[Enum](enum.md)、[Variant](variant.md)、[错误处理](error_handling.md)。

## 泛型和 Concept

泛型使用 `<...>`。Concept 描述静态协议，泛型参数可以用 concept 约束表达所需能力。

```cp
max<T: comparable<T>>(left: T, right: T) -> T
{
    if(left < right) {
        return right;
    }
    return left;
}
```

普通函数参数也可以省略类型，编译器会为每个省略类型的参数引入一个由调用实参推导的隐藏类型参数。需要借用形态时，把引用后缀留在参数名后：

```cp
read(value const&) -> i32
{
    return value;
}

add(left, right) -> i32
{
    return left + right;
}
```

显式类型和省略类型不能混写；`value&: i32` 应写成 `value: i32&` 或 `value&`。

`impl Concept for Type` 给具体类型实现协议；标准库也可以提供由编译器识别的内建 concept。

相关参考：[泛型](generic.md)、[Concept](concept.md)。

## 所有权、借用和移动

KCP 默认按普通 copy 语义传值。需要借用时显式写 `ref` / `const ref`，需要转移所有权时显式写 `move`，需要在泛型中保留实参值类别时写 `forward`。

```cp
take(value: string move&) -> string
{
    return move value;
}

relay<T>(value: T forward&) -> T
{
    return forward value;
}

use_text(text: string const&) -> usize
{
    return text.size();
}
```

显式借用和移动让函数签名直接表达调用者需要保留、读取还是交出对象。

相关参考：[所有权、借用与移动](ownership.md)、[底层内存分配](memory_allocation.md)。
