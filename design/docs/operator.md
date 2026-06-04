# 运算符

本文档记录 KCP 运算符表和第一版运算符重载规则。完整类型检查规则见 [type_system.md](type_system.md)。

整体优先级接近 C++，但逻辑运算使用关键字形式。

## 逻辑运算

KCP 使用：

```cp
not value
left and right
left or right
```

不使用 C/C++ 的：

```text
! && ||
```

规则：

- `not` 操作数必须是 `bool`，结果为 `bool`。
- `and` / `or` 两侧必须是 `bool`，结果为 `bool`。

## 算术运算

```text
+ - * / %
```

规则：

- 算术运算要求数值类型。
- 整数族内部可以统一类型。
- 浮点族内部可以统一类型。
- `%` 只允许整数类型。
- `+` / `-` 也支持指针算术，见“指针运算”。

一元 `+` / `-` 只要求操作数是数值类型，结果类型与操作数读出的类型一致。

## 指针运算

KCP 的裸指针运算保持接近 C++：

```cp
let next = p + 1;
let prev = next - 1;
let distance: isize = next - p;
let value = p[0];
```

规则：

- `p + n`、`n + p`、`p - n` 要求 `p` 是 `T*`，`n` 是整数类型，结果为 `T*`。
- 指针加减以 `T` 为元素步长。
- `p2 - p1` 要求两侧是相同目标类型的指针，结果为 `isize`。
- 指针差值表示元素数量，不是字节数量。
- `p[i]` 要求 `p` 是指针类型，`i` 是整数类型，语义等价于 `*(p + i)`。
- 不支持 `i[p]`。即使 `i` 是整数、`p` 是指针，下标目标也必须写在 `[]` 左侧。
- 有定义的边界与 C++ 保持一致：同一数组对象或同一连续分配对象内的位置，以及 one-past 位置。
- 编译器只做类型检查，不静态证明边界、生命周期或指针来源。

## 比较运算

```text
== != < <= <=> > >=
```

规则：

- `== != < <= > >=` 结果为 `bool`。
- `<=>` 是普通可重载二元运算；标准库约定返回 `partial_ordering`、`weak_ordering` 或 `strong_ordering` 这类比较分类值。
- 标准库 `ordering<T>`、`asc<T>`、`desc<T>` 使用 `<=>` 构造 map/set/sort 所需的三路 order object；`partial_ordering` 不作为默认有序容器或排序协议。
- 数值类型按共同数值类型比较。
- 除 `str` 外，同一内建标量类型支持 `== != < <= > >=`；例如 `bool == bool`、`char < char` 和 `i32 <= i32` 都走内建规则。`str` 比较由标准库 operator 提供，不走这一条内建路径。
- 同一个 `enum` 类型之间内建支持 `==` / `!=`。
- 同一个 `enum` 类型之间内建支持 `<=>`，按底层整数值比较并返回 `weak_ordering`；该操作要求 `weak_ordering` 在当前编译输入中可见，通常通过 `import std;` 或 `import std.compare;` 提供。
- 同一个 `enum` 类型之间不内建 `< <= > >=`；需要这些运算时通过标准库比较协议或显式 operator 提供。
- 不同 `enum` 类型不互相比较，即使底层整数类型相同。
- 相同目标类型的指针支持 `==` / `!=`。
- 相同目标类型的指针支持 `< <= > >=`，有定义边界与 C++ 指针有序比较一致。
- `struct` 不隐式生成比较。需要按字段顺序比较时，必须在固有 `impl` 中显式写 `operator <=> = default;`。
- 除同类型 `enum` 的 `==` / `!=` / `<=>`、同目标指针比较和同类型内建标量比较外，非数值类型不提供通用内建比较。特定类型若需要比较能力，应通过 `operator` 重载、默认三路比较或标准库协议提供。

显式声明的 `operator ==`、`operator !=`、`operator <`、`operator <=`、`operator >`、`operator >=` 优先于任何基于 `<=>` 的派生策略。标准库对 `str` 直接提供这些比较 operator，并以 `operator <=>` 作为三路比较入口；后续若在编译器层接入派生比较，应保持同样的显式 operator 优先规则。

## 位运算

```text
~ & | ^ << >>
```

规则：

- 位运算要求整数类型。
- 结果类型为统一后的整数类型。

## 运算符重载

KCP 支持一版受限的 `operator` 重载。它可以覆盖对应内建运算符，但不打开普通函数和成员函数的完整重载体系。

### 语法总览

```text
TopLevelOperator -> export? operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody
ImplOperator     -> operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody

OverloadableOperator
                 -> UnaryOperator
                  | UpdateOperator
                  | BinaryOperator
                  | ComparisonOperator
                  | AssignmentOperator
                  | SubscriptOperator
                  | CallOperator

UnaryOperator    -> "+" | "-" | "~"
UpdateOperator   -> "prefix" "++" | "postfix" "++" | "prefix" "--" | "postfix" "--"
BinaryOperator   -> "+" | "-" | "*" | "/" | "%" | "&" | "|" | "^" | "<<" | ">>"
ComparisonOperator
                 -> "==" | "!=" | "<" | "<=" | "<=>" | ">" | ">="
AssignmentOperator
                 -> "=" | "+=" | "-=" | "*=" | "/=" | "%="
                  | "&=" | "|=" | "^=" | "<<=" | ">>="
SubscriptOperator
                 -> "[]"
CallOperator    -> "()"
```

实际词法中 `operator` 和后面的运算符 token 可以有空白；文档统一写成 `operator +`、`operator []`、`operator ()`、`operator prefix ++`。

成员 operator 写在固有 `impl` 中，挂到当前类型命名空间：

```cp
impl vec2 {
    operator +(self const&, rhs: this const&) -> this
    {
        return vec2{ .x = x + rhs.x, .y = y + rhs.y };
    }

    operator ==(self const&, rhs: this const&) -> bool
    {
        return x == rhs.x and y == rhs.y;
    }

    operator prefix ++(self&) -> this&
    {
        x += 1;
        y += 1;
        return ref self;
    }

    operator ()(self const&, scale: i32) -> i32
    {
        return x * scale + y;
    }
}
```

顶层 operator 写作普通顶层声明，遵循模块导出和导入规则：

```cp
operator +(left: vec2 const&, right: vec2 const&) -> vec2
{
    return vec2{ .x = left.x + right.x, .y = left.y + right.y };
}
```

声明规则：

- `operator` 是特殊声明，不占用普通函数名空间。
- 顶层 operator 至少有一个参数类型必须是用户定义名义类型、内建类型，或者这些类型的引用。
- 固有 `impl` 内的 operator 至少有一个参数必须是 `self`、`self&`、`self const&` 或 `this` 相关类型。
- 同一个类型命名空间内，相同 operator 可以按参数类型重载。
- 当前可见的顶层 operator 可以按参数类型重载。
- 固有 `impl` operator 当前注册在 `struct`、`variant` 和内建类型的类型命名空间中。opaque alias 和数组类型可以有扩展成员函数，但它们的 `impl operator` 不是当前可用能力；opaque alias 需要 operator 时使用顶层 operator，数组类型当前不能声明可用的用户 operator，应通过包装 `struct` 提供该能力。
- `operator <=>` 可以在非泛型 `struct` 的固有 `impl` 中写成 `= default`，形如 `operator <=>(self const&, rhs: this const&) -> weak_ordering = default;`。它按字段声明顺序逐字段调用 `<=>`，将比较结果通过 `to_weak()` 归一为 `weak_ordering`，遇到非 `equivalent` 立即返回；所有字段都等价时返回 `weak_ordering::equivalent`。字段类型必须能比较，且比较结果必须提供返回 `weak_ordering` 的 `to_weak()`。默认比较要求 `weak_ordering` 可见，因此使用处通常需要导入 `std` 或 `std.compare`。
- 普通函数、普通成员函数和关联函数仍然不支持重载。
- operator 不参与 UFCS，不能写成 `operator +(a, b)` 普通调用。
- `operator ++` 和 `operator --` 不是合法声明；必须写成 `operator prefix ++`、`operator postfix ++`、`operator prefix --` 或 `operator postfix --`，避免前后置声明歧义。
- `and` / `or` 的短路语义、`ref`、`move`、`delete`、字段访问和生命周期语义不进入 operator 重载体系。

查找规则：

- 一元运算 `op value` 先查找 `value` 类型命名空间中的 `operator op`；没有可行候选时再尝试内建规则。
- 前置 `++value` / `--value` 先查找 `operator prefix ++` / `operator prefix --`；没有可行候选时再尝试内建整数规则。
- 后置 `value++` / `value--` 先查找 `operator postfix ++` / `operator postfix --`；没有可行候选时再尝试内建整数规则。
- 二元运算 `left op right` 先依次查找 `left` 类型命名空间、`right` 类型命名空间、当前可见顶层 `operator op`；没有可行候选时再尝试编译器内建规则。
- 赋值 `left = right` 和复合赋值 `left op= right` 先检查 `left` 必须是可写左值；然后查找 `left` 类型命名空间中的对应 operator；找不到时再查当前可见顶层 operator；仍然找不到时才使用允许的内建赋值或报错。
- 下标 `value[index]` 先尝试内建 `[T; N]`、`str` 和指针规则；内建规则不成立时，查找 `value` 类型命名空间中的 `operator []`；找不到时再查当前可见顶层 `operator []`。元组字段使用 `.0` / `.1`，不参与 `operator []`。
- 调用 `callee(args...)` 先按普通函数、函数值、lambda 和闭包调用检查；这些规则不成立时，查找 `callee` 类型命名空间中的 `operator ()`，调用参数序列为 `callee, args...`。
- 内建 operator 是最低优先级候选。可见用户 operator 没有可行候选时，不会阻断其它层级或内建候选；只有最高优先级存在多个同分可行候选时才报二义性。

候选选择使用小型重载规则：

1. 参数数量必须一致。
2. 完全类型匹配优先于需要内建转换的匹配。
3. 非模板候选优先于需要实例化后才确定的泛型候选。
4. 如果最高优先级仍有多个候选，报二义性错误。

不支持 C++ 完整重载体系中的用户自定义隐式转换、模板偏序和 ADL。顶层 operator 只从当前模块和 `import` 引入的可见声明中查找。

`operator ()` 不参与 UFCS，只能通过调用表达式触发；不能写成普通成员调用或顶层函数调用来绕过调用语法。

### 内建 operator 逃逸

`builtin(expr)` 是编译器识别的表达式解析模式，不是普通函数。在 `builtin(expr)` 的整个表达式子树内，所有支持 operator 重载的运算符语法都只走编译器内建规则：

- 不查固有 `impl` operator。
- 不查当前可见顶层 operator。
- 内建规则不成立时直接报错，不回退到用户 operator。

`builtin(expr)` 不改变变量名查找、普通函数调用、成员函数调用、字段访问、显式/隐式类型转换、求值顺序和被调用函数体内部的语义。

```cp
operator +(left: i32, right: i32) -> i32
{
    return builtin(left + right);
}

let item = builtin(matrix[i][j] + dx * scale);
```

上例中 `builtin(...)` 内部的 `+`、`*` 和 `[]` 都只使用内建规则。需要局部使用用户 operator 时，应先把那部分表达式绑定到局部变量，再进入 `builtin(...)`。

## 赋值与复合赋值

```text
= += -= *= /= %= &= |= ^= <<= >>=
```

规则：

- 赋值左侧必须是左值。
- 不能给 `const` binding 重新赋值。
- 简单赋值 `=` 可以由 `operator =` 重载。
- 没有可用 `operator =` 时，简单赋值按左侧类型检查右侧表达式并使用内建赋值。
- `operator =` 的左操作数必须是可写引用；成员形式必须以 `self&` 作为第一个参数。
- 复合赋值可以由对应 `operator +=`、`operator -=` 等重载。
- 没有可用复合赋值 operator 时，只尝试编译器内建二元运算再写回左侧。也就是说，`i32 += i32`、`ptr += integer` 这类内建路径可用；但不会把用户声明的 `operator +` 自动改写成 `operator +=`，也不会调用普通函数做用户自定义转换。
- 内建复合赋值要求 `left op right` 的结果能隐式转换回左侧读出类型，否则报错。
- 复合赋值 operator 的返回类型可以是内部 `unit`，也可以是左操作数引用类型，例如成员形式的 `this&`。

示例：

```cp
impl vec2 {
    operator =(self&, rhs: this const&) -> this&
    {
        x = rhs.x;
        y = rhs.y;
        return self;
    }

    operator +=(self&, rhs: this const&)
    {
        x += rhs.x;
        y += rhs.y;
    }
}
```

## 下标运算

KCP 支持内建下标运算符和 `operator []`：

```cp
value[index]
```

规则：

- 下标运算属于后缀表达式，优先级高于一元前缀运算。
- 内建下标目标支持 `[T; N]`、`str` 和指针。
- `[T; N]` 下标要求 `index` 是整数类型，结果类型为 `T`。
- `str` 下标要求 `index` 是整数类型，结果类型为 `char`。
- `T*` 下标要求 `index` 是整数类型，结果类型为 `T` 左值；`T const*` 下标结果是只读 `T` 左值。
- 指针下标只支持 `p[i]`，不支持 C/C++ 的 `i[p]` 对称写法。
- 如果目标是可写左值且元素本身可写，数组和指针下标结果是可写左值。
- `str` 下标结果只读，不是可写左值。
- 内建下标规则不成立时，可以查找 `operator []`。
- `operator []` 可以返回值，也可以返回引用；返回引用时，`value[index]` 可以作为左值参与赋值。
- 用于可变容器时，推荐同时提供 `self&` 和 `self const&` 两个版本。
- 下标越界规则、常量下标诊断和内建类型的具体左值规则见 [type_system.md](type_system.md)。
- 空 `[]` 在表达式中不是下标运算；数组字面量 `[]` / `[a, b]` 是单独的字面量语法。

示例：

```cp
impl vector<T> {
    operator [](self&, index: usize) -> T&
    {
        return data[index];
    }

    operator [](self const&, index: usize) -> T const&
    {
        return data[index];
    }
}
```

## 函数调用

函数调用写作：

```cp
callee(arg1, arg2)
```

规则：

- `callee` 可以是普通命名函数、成员函数、关联函数、函数类型值、函数指针值或闭包类型值。
- 调用普通命名函数、成员函数和关联函数时，按对应声明检查参数和返回类型。
- 调用 `f(...) -> R` 函数类型值时，参数必须能按函数类型的形参类型检查，结果类型为 `R`。
- 调用 `f*(...) -> R` 函数指针值时，参数检查和结果类型同函数类型；如果指针为空，属于底层 unsafe 契约。
- 有捕获闭包的调用规则见 [lambda.md](lambda.md)。
- 函数类型中的形参名只用于诊断，不参与调用匹配。

## 取址与解引用

KCP 支持内建取址和解引用：

```cp
let p = &value;
let value = *p;
*p = 1;
```

规则：

- `&expr` 要求 `expr` 是左值，结果为指向该值的指针。
- 如果 `expr` 只能只读访问，`&expr` 的结果为 target const 指针。
- `*ptr` 要求 `ptr` 是指针类型，结果为被指向对象的左值。
- `T const*` 解引用得到只读左值。
- 解引用的空指针、悬垂指针、未对齐指针、越界指针和未开始生命周期的对象属于底层 unsafe 契约。

## 自增与自减

KCP 支持前置和后置自增、自减；内建整数路径和用户自定义 operator 使用同一表达式语义：

```cp
++value
--value
value++
value--
```

规则：

- 操作数必须是可写左值。
- 不能作用于 `const` binding 或 const target。
- 内建路径只允许整数类型，不支持 `bool`、浮点、指针、`array` 或 `tuple`。
- 内建自增语义等价于把操作数加 `1` 后写回，自减语义等价于把操作数减 `1` 后写回。
- 非内建类型按表达式位置查找对应 operator：前置 `++value` 查找 `operator prefix ++`，后置 `value++` 查找 `operator postfix ++`；`--` 同理。
- 前置形式先更新，再产生更新后的自身引用；内建前置结果为 `T&`，自定义 operator 推荐返回 `this&`。
- 后置形式先产生旧值，再更新；内建后置结果为 `T`，自定义 operator 推荐返回 `this`。
- 操作数表达式只求值一次。

自定义声明必须显式写出前置或后置位置，裸 `operator ++` / `operator --` 不是合法声明：

```cp
impl counter {
    operator prefix ++(self&) -> this&
    {
        value += 1;
        return ref self;
    }

    operator postfix ++(self&) -> this
    {
        let old = self;
        value += 1;
        return old;
    }
}
```

## 转换运算

```cp
value as type
```

`as` 进入显式转换检查。

## 优先级

具体优先级由 parser 的共享运算符表定义。设计目标是与 C++ 接近，并保留以下差异：

- 逻辑运算使用 `and` / `or` / `not`。
- 下标 `value[index]` 属于后缀表达式，优先级高于一元前缀运算。
- 函数调用 `callee(args...)` 属于后缀表达式，优先级高于一元前缀运算。
- 后置 `value++` / `value--` 属于后缀表达式，优先级高于一元前缀运算。
- 前置 `++value` / `--value` 属于一元前缀运算，和 `not`、一元 `+`、一元 `-`、`~` 同级。
- 一元 `&value` 和 `*ptr` 属于一元前缀运算。
- `<=>` 使用标准库比较分类值表达三路比较，`asc<T>` / `desc<T>` 是排序和有序容器的默认比较入口。
- 不引入 C++ 式多种命名 cast 运算符。
