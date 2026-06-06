# 运算符

本文档记录 KCP 运算符表和第一版运算符重载规则。完整类型检查规则见 [type_system.md](type_system.md)。

整体优先级接近 C++，但逻辑运算使用关键字形式。

本章前半部分的逻辑、算术、指针、比较和位运算规则描述的是编译器内建 operator 规则。当前实现允许给内建类型声明可见的扩展 operator，也允许顶层 operator 的参数包含内建类型；因此普通二元表达式会先做用户 operator 查找，再在没有可行候选时使用这些内建规则。需要强制使用内建规则时，写 `builtin(expr)`，见“内建 operator 逃逸”。

## 表达式优先级

当前 parser 的表达式优先级从高到低如下：

| 层级 | 运算/语法 | 结合性 |
| --- | --- | --- |
| 后缀 | 泛型调用实参 `<T...>` 后接调用、`()`、`.`、`::`、`[]`、后置 `++` / `--` | 从左到右 |
| 前缀 | `+`、`-`、`not`、`~`、`&`、`*`、`ref`、`const ref`、`move`、`forward`、`delete`、前置 `++` / `--` | 从右到左 |
| cast | `as Type` | 从左到右 |
| 乘除余 | `*`、`/`、`%` | 从左到右 |
| 加减 | `+`、`-` | 从左到右 |
| 位移 | `<<`、`>>` | 从左到右 |
| 关系/三路比较 | `<`、`<=`、`<=>`、`>`、`>=` | 从左到右 |
| 相等 | `==`、`!=` | 从左到右 |
| 按位与 | `&` | 从左到右 |
| 按位异或 | `^` | 从左到右 |
| 按位或 | `|` | 从左到右 |
| 逻辑与 | `and` | 从左到右 |
| 逻辑或 | `or` | 从左到右 |
| 赋值 | `=`、`+=`、`-=`、`*=`、`/=`、`%=`、`&=`、`|=`、`^=`、`<<=`、`>>=` | 从右到左 |

赋值不属于普通二元 Pratt 运算层级；parser 先解析左侧非赋值表达式，再递归解析右侧赋值表达式。因此 `a = b = c` 解析为 `a = (b = c)`。普通二元运算和 `as` 都是左结合，因此 `a - b - c` 解析为 `(a - b) - c`，`value as i32 as i64` 解析为 `(value as i32) as i64`。

`and` / `or` 是固定的内建逻辑表达式，不进入 operator 重载体系。它们的语义检查会先检查两侧表达式并要求两侧都是 `bool`；因此不能用运行时短路来跳过右侧名字解析或类型检查。运行时求值仍按短路语义执行：`false and rhs` 不求值 `rhs`，`true or rhs` 不求值 `rhs`。编译期 `template if` 条件里的 `and` / `or` 另有常量折叠短路规则，见 [flow.md](flow.md#template-if)。

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

`!`、`&&` 和 `||` 仍是 lexer 能识别的标点 token，主要用于诊断恢复和实验词法覆盖；它们不是表达式语法中的逻辑运算符。`!flag`、`left && right`、`left || right` 不会改写成 `not flag`、`left and right` 或 `left or right`，也不能通过 `operator !`、`operator &&`、`operator ||` 重载打开。需要逻辑运算时必须写关键字形式。

规则：

- `not` 操作数必须是 `bool`，结果为 `bool`。
- `and` / `or` 两侧必须是 `bool`，结果为 `bool`。

## 算术运算

```text
+ - * / %
```

规则：

- 算术运算要求数值类型，或 `+` / `-` 的指针算术形式。
- 这些是内建算术规则；如果当前模块可见的内建类型扩展 operator 或顶层 operator 对同一表达式可行，普通 `left + right`、`left * right` 等二元表达式会先选中用户 operator。用户 operator 体内需要调用原始内建算术时应写 `builtin(left + right)`。
- 整数族内部按 `integer_rank` 统一类型；rank 更高者成为结果类型，rank 相同时保留左操作数类型。
- 浮点族内部按 `float_rank` 统一类型；`f32` 和 `f64` 混合结果为 `f64`。
- 整数和浮点混合运算可用，结果为参与运算的浮点类型；整数在 `float_rank` 中按 rank 0 处理，因此 `i32 + f32` 和 `i64 + f32` 的结果都是 `f32`，`i32 + f64` 的结果为 `f64`。
- `%` 只允许整数类型。
- `*` 和 `/` 只允许数值类型，不支持指针。
- `+` / `-` 也支持指针算术，见“指针运算”。

一元 `+` / `-` 只要求操作数是数值类型，结果类型与操作数读出的类型一致。和二元算术不同，数值类型的一元 `+value` / `-value` 当前直接走内建规则；只有操作数不是数值类型时，才会查找用户 `operator +` / `operator -`。因此不能通过给 `i32`、`f64` 等内建数值类型声明扩展 unary operator 来覆盖普通数值取正或取负。

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
- `p[i]` 要求 `p` 是指针类型，`i` 是整数类型，地址计算等价于 `*(p + i)`。
- `p + q` 不合法，即使两侧都是指针。
- `n - p` 不合法；整数只能出现在指针加法右侧或左侧、指针减整数右侧。
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
- 标准库 `ordering<T>`、`asc<T>`、`desc<T>` 使用 `<=>` 构造 map/set/sort 所需的三路 order object；`partial_ordering` 不作为默认有序容器或排序协议。`ordering<T>` 是标准库保留 concept，不等同于“任意表达式能写成 `order(left, right)`”：当前 concept 判定接受函数/函数指针、lambda/闭包，以及 `struct` / `variant` 自身的 `operator ()`；它不会把只靠当前文件可见顶层 `operator ()` 才能调用的类型当作稳定排序比较器。排序、map、set 的比较器应优先写成带成员 `operator ()(self const&, left: T const&, right: T const&) -> weak_ordering` 的值对象，或直接使用函数 / lambda。
- `== != < <= > >=` 对数值类型按共同数值类型比较；这不包括编译器内建 `<=>`。除 enum 路径外，`<=>` 需要用户或标准库提供可见 operator。
- 除 `str` 外，同一内建标量类型支持 `== != < <= > >=`；例如 `bool == bool`、`char < char` 和 `i32 <= i32` 都走内建规则。`str` 比较由标准库 operator 提供，不走这一条内建路径。
- 导入 `std.compare`、`std.ranges` 或 `std` 后，标准库为 `bool`、整数类型、`isize` / `usize` 和 `char` 提供同类型 `operator <=> -> weak_ordering` 扩展。当前标准库没有给 `f32` / `f64` 提供 `<=>`，也没有给不同整数类型提供跨类型 `<=>`；需要三路比较时先显式转换到同一可比较类型，或提供自定义 operator。
- 同一个 `enum` 类型之间内建支持 `==` / `!=`。
- 同一个 `enum` 类型之间内建支持 `<=>`，按底层整数值比较并返回 `weak_ordering`；该操作要求当前语义输入中存在名为 `weak_ordering` 的 `variant`。实现当前按名字查找这个结果类型，不检查标准库模块身份；公开代码应通过 `import std;` 或 `import std.compare;` 提供标准库版本。
- 同一个 `enum` 类型之间不内建 `< <= > >=`；当前 `enum` 也不是可写 `impl` / `operator` overload 目标。需要这些关系时，使用 `<=>` 配合标准库比较 helper，或先显式转换到底层整数后比较。
- 不同 `enum` 类型不互相比较，即使底层整数类型相同。
- 相同目标类型的指针支持 `==` / `!=`。
- 相同目标类型的指针支持 `< <= > >=`，有定义边界与 C++ 指针有序比较一致。
- 指针可以和 `nullptr` 做 `== != < <= > >=`。当前实现会把 `nullptr` 按另一侧指针类型参与比较；这只是空指针值比较，不证明指针来源、对象边界或有序比较是否满足底层前置条件。
- 裸 `nullptr` 和裸 `nullptr` 之间没有目标指针类型，`nullptr == nullptr`、`nullptr < nullptr` 这类写法不是当前合法内建比较。需要比较空指针值时，至少一侧必须有具体指针类型，例如 `ptr == nullptr` 或 `(nullptr as i32*) == (nullptr as i32*)`。
- 指针不支持内建 `<=>`，即使两侧目标类型相同；需要三路比较时应先显式转换到可比较的整数表示不属于当前公开能力，或提供自己的抽象类型/operator。
- `struct` 不隐式生成比较。需要按字段顺序比较时，必须在固有 `impl` 中显式写 `operator <=> = default;`。
- 除同类型 `enum` 的 `==` / `!=` / `<=>`、同目标指针比较和同类型内建标量比较外，非数值类型不提供通用内建比较。特定类型若需要比较能力，应通过 `operator` 重载、默认三路比较或标准库协议提供。

显式声明的 `operator ==`、`operator !=`、`operator <`、`operator <=`、`operator >`、`operator >=` 优先于任何基于 `<=>` 的派生策略。标准库对 `str` 直接提供这些比较 operator，并以 `operator <=>` 作为三路比较入口；后续若在编译器层接入派生比较，应保持同样的显式 operator 优先规则。

## 位运算

```text
~ & | ^ << >>
```

规则：

- 位运算要求整数类型。
- `~value` 要求整数类型，结果类型是操作数读出类型。
- `& | ^ << >>` 都要求两侧是整数类型。
- 二元位运算结果类型为按 `integer_rank` 统一后的整数类型；rank 相同时保留左操作数类型。
- 当前移位运算不会要求右操作数是无符号或特定宽度整数；它只要求是整数类型。移位位数越界或负数的运行时语义属于底层数值前置条件，第一版不做静态证明。

## 运算符重载

KCP 支持一版受限的 `operator` 重载。它可以覆盖对应内建运算符，但不打开普通函数和成员函数的完整重载体系。

### 语法总览

```text
TopLevelOperator -> export? operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody
                  | export? operator OverloadableOperator ( ParameterList? ) ReturnType? = delete ;
ImplOperator     -> operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody
                  | operator <=> ( self const& , rhs : this const& ) -> weak_ordering = default ;
                  | operator OverloadableOperator ( ParameterList? ) ReturnType? = delete ;

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
- 顶层 operator 至少有一个参数类型必须是 `struct`、`variant`、opaque alias、内建类型，或者这些类型的引用。`enum` 虽然是名义类型，但当前不作为 operator owner；只写 `operator +(left: some_enum, right: some_enum)` 这类声明会报非法 operator。
- 固有 `impl` 内的 operator 至少有一个参数必须是 `self`、`self&`、`self const&` 或 `this` 相关类型。
- 固有 `impl` operator 如果使用 `self` receiver 语法，`self` 必须是第一个参数；`operator +(rhs: i32, self const&)` 这类形状不是合法成员 operator。若不用 receiver 语法，也可以通过普通参数里的当前类型满足“属于当前 impl 目标”的要求，例如 `operator +(left: this const&, right: this const&) -> this`。这种参数只是普通 operator 参数，不启用成员函数 receiver 调用规则。
- 同一个类型命名空间内，相同 operator 可以按参数类型重载。
- 当前可见的顶层 operator 可以按参数类型重载。
- 固有 `impl` operator 当前注册在 `struct`、`variant` 和内建类型的类型命名空间中。opaque alias 和数组类型可以有扩展成员函数，但它们的 `impl operator` 不是当前可用能力；opaque alias 需要 operator 时使用顶层 operator，数组类型当前不能声明可用的用户 operator，应通过包装 `struct` 提供该能力。
- `operator =` 和复合赋值 operator 的第一个参数表示左操作数，必须是可写普通引用。顶层形式不能把左操作数写成值参数、`const&`、`move&` 或 `forward&`；成员形式必须字面以 `self&` 作为第一个参数，`self const&`、值 `self`、`this&` 或把 `self` 放在后续参数中都不是合法成员赋值 operator。
- `operator <=>` 可以在非泛型 `struct` 的固有 `impl` 中写成 `= default`，具体边界见“默认三路比较”。
- 顶层 operator 和固有 `impl` operator 都可以写成 `= delete;` 声明，具体边界见“Deleted operator”。
- 普通函数、普通成员函数和关联函数仍然不支持重载。
- operator 不参与 UFCS，不能写成 `operator +(a, b)` 普通调用。
- operator 参数的默认值不是当前可调用能力。parser 可以记录 `operator ()(self const&, scale: i32 = 1)` 这类默认表达式，语义层也会按普通默认参数规则做形状检查；但 operator 候选匹配要求实参数量与 operator 签名完全一致，不会用默认值补齐。需要可省略实参的调用接口时，应提供普通成员函数或关联函数包装。
- `operator ++` 和 `operator --` 不是合法声明；必须写成 `operator prefix ++`、`operator postfix ++`、`operator prefix --` 或 `operator postfix --`，避免前后置声明歧义。
- `and` / `or` 的短路语义、`ref`、`move`、`delete`、字段访问和生命周期语义不进入 operator 重载体系。

模块可见性分两类：

- 顶层 operator 只有写成 `export operator ...` 时才进入具名模块导出表。未导出的顶层 operator 只在同一个模块 key 内可见；其它文件即使 `import` 了该模块，也不能看到它。
- `impl` 中注册到内建类型的 extension operator 按 extension 成员能力处理。只要它位于具名模块中，就随该模块的 extension operator 导出表对 import 方可见，不在 operator 成员前额外写 `export`。例如 `export module ints; impl i32 { operator +(self, rhs: i32) -> i32 { ... } }` 被 `import ints;` 后，`value + 1` 可以选择这个扩展 operator。未 import 该模块的文件看不到它；通过 `export import ints;` 再导出的模块会继续转发这些 extension operator。`struct` / `variant` 自身的固有 operator 跟类型命名空间绑定，使用者通常需要先能看到该类型名和对应 impl 所在模块；opaque alias 和数组的 `impl operator` 当前不会注册为可用 operator。

### 返回类型与函数体种类

普通源函数体的 operator 可以省略返回类型：

```cp
operator +(left: i32, right: i32)
{
    return builtin(left + right);
}

impl counter {
    operator +=(self&, rhs: this const&)
    {
        value += rhs.value;
        return ref self;
    }
}
```

当前实现按 [type_system.md](type_system.md) 的普通返回类型推导规则处理顶层 operator 和固有 `impl` operator：

- 顶层 operator 有源码函数体且省略 `-> R` 时，从函数体 `return` 推导返回类型。
- 固有 `impl` operator 有源码函数体且省略 `-> R` 时，也从函数体推导返回类型；推导期间 `self`、`this`、隐式字段名和 `rhs.field` 等成员访问按普通固有方法规则可用。
- `variant` 和内建类型的固有 `impl operator` 当前会注册为可用 operator，也可以省略返回类型并按源码函数体推导。
- `struct`、`variant`、内建类型和数组的固有普通成员函数同样可以省略返回类型；但数组 `impl operator` 当前不注册为可用 operator，opaque alias 的 `impl operator` 也不注册为可用 operator。这类声明即使能通过解析和声明收集，也不会参与后续 operator 查找，使用处仍按普通内建/可见顶层 operator 规则报错。
- `return match ...;`、`return` 写在 `match` arm 的块表达式中、以及 `template for` 展开体中的 `return` 都按普通返回推导规则参与 operator 返回类型推导。
- 泛型 operator 的返回类型在具体实例中推导，不为泛型定义本身产生一个全局返回类型。

不做返回类型推导的 operator 形态：

- `= delete;` operator 没有函数体，不推导返回类型；省略返回类型时它的签名返回内部 `unit`。如果要把某个返回类型也作为不可调用形状的一部分，应显式写 `-> R = delete;`。
- `operator <=> = default;` 必须显式写返回类型；该返回类型必须能隐式转换到当前语义输入中解析到的 `weak_ordering`。这只是普通类型等价 / alias / 限定转换层面的接受，不表示可以把 `strong_ordering` 或其它比较分类作为默认比较的公开返回类型；字段比较结果需要更强分类时，通过字段结果的 `to_weak()` 归一化。默认比较不会根据字段比较结果推导返回类型。
- concept 函数要求、concept 默认函数和 `impl Concept for T` 中的 operator 不做普通 operator 的返回推导。省略 `-> R` 表示 `unit`；实现非 `unit` 的 concept operator 要求时必须显式写返回类型。完整边界见 [concept.md](concept.md)。

### Deleted operator

`= delete;` operator 声明用于显式保留一个不可调用的 operator 形状：

```cp
struct token {
    id: i32;
}

impl token {
    operator =(self&, rhs: this const&) = delete;
}

operator +(left: token const&, right: token const&) -> token = delete;
```

deleted operator 仍然是普通 operator 符号：它参与导出/导入、类型命名空间注册、参数数量检查、assignment 左参数可写性检查和重载候选选择。它没有函数体，也不会生成可调用实现。

如果 operator 查找和候选选择最终选中了 deleted operator，语义检查报 `invalid_operator`。它不会因为是 deleted 就从候选集中移除，也不会继续回退到同一优先级之后的内建 operator。只有没有任何可行用户候选时，内建规则才按普通查找流程继续尝试。

`operator <=> = default;` 和 `operator ... = delete;` 是不同能力：前者只用于非泛型 `struct` 固有 `impl` 的默认三路比较，后者用于显式禁止某个 operator 调用形状。默认三路比较展开字段比较时，如果某个字段的 `<=>` 解析到 deleted operator，默认比较声明本身报 `invalid_operator`。

### 默认三路比较

当前实现支持一版受限的默认三路比较。合法形状必须是非泛型 `struct` 的固有 `impl` 中的 `<=>` operator 声明：

```cp
import std.compare;

struct key {
    name: str;
    priority: i32;
}

impl key {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}
```

声明规则：

- 只能默认化 `operator <=>`。`operator == = default;`、`operator < = default;` 或其它 operator 默认化都不是当前能力。
- 必须写成 `= default;` 声明，不能同时提供函数体。
- 必须位于 `struct` 的固有 `impl` 中。`variant`、opaque alias、builtin、数组、顶层 operator、concept `impl` 中的 operator 都不是当前默认比较目标。
- 目标 `impl` 不能是泛型或依赖类型模式；`impl<T> box<T> { operator <=> ... = default; }` 当前报错，即使 `T` 有比较能力。
- 参数必须正好是 `self const&` 和 `rhs: this const&`。`self&`、按值 `self`、`rhs: i32 const&`、多余参数或缺少参数都不合法。
- 返回类型必须显式写出，并且必须能隐式转换到当前语义输入中解析到的 `weak_ordering`。这允许解析后等价于 `weak_ordering` 的类型写法，但不允许把 `strong_ordering`、`partial_ordering` 或用户自定义比较分类作为默认比较的公开返回类型。默认比较当前按名字寻找 `weak_ordering` variant，不校验标准库模块身份；公开代码通常应通过 `import std;` 或 `import std.compare;` 提供标准库版本。

字段比较规则：

- 按 `struct` 字段声明顺序比较，不按初始化器顺序、名字排序或内存布局排序。
- 每个字段以当前对象和 `rhs` 对象中的 const 字段值参与比较；字段类型是 `enum` 时使用 enum 内建 `<=>`，结果为 `weak_ordering`。
- 非 enum 字段按普通 operator 查找解析 `<=>`。字段比较结果如果已经能隐式转换为 `weak_ordering`，直接使用；否则要求结果类型提供可用的 `to_weak()` 成员方法。这里的 `to_weak()` 按结果类型命名空间的方法查找，相当于对比较结果写 `result.to_weak()`；除 receiver 外不能要求额外实参，`to_weak(result)` 自由函数、`Result::to_weak(result)` 关联函数或带额外默认参数的方法都不是默认比较的归一化入口。`to_weak()` 的返回值必须能隐式转换为 `weak_ordering`。
- 字段没有可用 `<=>`、字段 `<=>` 是 deleted、比较 operator 签名无效、或 `to_weak()` 缺失 / 返回类型不对时，默认比较声明报 `invalid_operator`。
- 每个字段比较后都会判断结果是否为 `weak_ordering::equivalent`。遇到第一个非等价结果立即返回；全部字段等价时返回 `weak_ordering::equivalent`。空 `struct` 因为没有字段，会直接返回 `equivalent`。

默认三路比较只生成 `<=>` 本身。它不会自动生成 `==`、`!=`、`<`、`<=`、`>` 或 `>=`，也不会让没有显式 `operator <=> = default;` 的 `struct` 自动可比较。需要这些关系运算时，通过标准库比较协议、显式 operator 或容器的 ordering wrapper 提供。

查找规则：

- 一元 `+value` / `-value` 对数值类型直接走内建规则；只有操作数不是数值类型时，才查找 `value` 类型命名空间或可见 extension 中的 `operator +` / `operator -`。
- 一元 `~value` 对整数类型直接走内建规则；只有操作数不是整数类型时，才查找用户 `operator ~`。
- 解引用 `*value` 对指针类型直接走内建规则；只有操作数不是指针类型时，才查找用户 `operator *`。
- 前置 `++value` / `--value` 和后置 `value++` / `value--` 会先要求操作数是非 const 左值。整数类型直接走内建规则；非整数类型再查找 `operator prefix ++` / `operator postfix ++` / `operator prefix --` / `operator postfix --`。
- 二元运算 `left op right` 先依次查找 `left` 类型命名空间、`right` 类型命名空间、当前可见顶层 `operator op`；没有可行候选时再尝试编译器内建规则。
- 赋值 `left = right` 和复合赋值 `left op= right` 先检查 `left` 必须是可写左值；然后查找 `left` 类型命名空间中的对应 operator；找不到时再查当前可见顶层 operator。
- 普通赋值没有可行用户 `operator =` 时，右侧按左侧读出类型做目标位置检查并写入左侧。
- 复合赋值没有可行用户 `operator op=` 时，只把它降成对应的内建二元运算 `op` 加一次赋回检查；不会再去查找用户 `operator op`。例如用户只声明 `operator +(left: counter, right: counter)` 不能让 `value += other` 自动成立；需要单独声明 `operator +=`，或写成 `value = value + other`。
- 下标 `value[index]` 对 `[T; N]`、`str` 和指针类型直接走内建规则；这些类型的索引类型错误、数组常量越界等错误不会回退到用户 `operator []`。只有目标类型不是数组、`str` 或指针时，才查找 `value` 类型命名空间中的 `operator []`；找不到时再查当前可见顶层 `operator []`。元组字段使用 `.0` / `.1`，不参与 `operator []`。
- 调用 `callee(args...)` 先按普通函数、函数值、lambda 和闭包调用检查；这些规则不成立时，查找 `callee` 类型命名空间中的 `operator ()`，调用参数序列为 `callee, args...`。
- 对二元运算、赋值和 call operator，内建或普通 callable 路径只在对应用户/operator 路径没有可行候选时使用。对上面列出的若干一元运算和内建下标目标，内建路径会先行固定。用户候选选择中只有最高优先级存在多个同分可行候选时才报二义性。

候选选择使用小型重载规则：

1. 参数数量必须一致。
2. 完全类型匹配优先于需要内建转换的匹配。
3. 对精确匹配中的引用绑定，临时值绑定到普通 `T const&` 会比按值精确匹配略低；这避免临时实参优先选中 `const&` 版本。这个惩罚只在候选排序中使用，不改变 `T const&` 能接收临时值的合法性，并且这种精确匹配仍优先于需要普通隐式转换的候选。
4. 非模板候选优先于需要实例化后才确定的泛型候选。
5. 如果最高优先级仍有多个候选，报二义性错误。

不支持 C++ 完整重载体系中的用户自定义隐式转换、模板偏序和 ADL。顶层 operator 只从当前模块和 `import` 引入的可见声明中查找。

`operator ()` 不参与 UFCS，只能通过调用表达式触发；不能写成普通成员调用或顶层函数调用来绕过调用语法。

### 内建 operator 逃逸

`builtin(expr)` 是编译器识别的表达式解析模式，不是普通函数。在 `builtin(expr)` 的整个表达式子树内，所有支持 operator 重载的运算符语法都只走编译器内建规则：

- 不查固有 `impl` operator。
- 不查当前可见顶层 operator。
- 内建规则不成立时直接报错，不回退到用户 operator。

`builtin(expr)` 不改变变量名查找、普通函数调用、成员函数调用、字段访问、显式/隐式类型转换、求值顺序和被调用函数体内部的语义。这里的“普通函数调用”指已解析成函数、函数指针或 lambda/闭包签名的调用；如果某个对象只能靠用户定义的 `operator ()` 被调用，那么这个调用在 `builtin(...)` 内不会回退到该 operator，而会按 callee 不可调用报错。`builtin(...)` 也不执行额外 cast，不改变表达式值类别，不把非法内建运算变成合法。

```cp
operator +(left: i32, right: i32) -> i32
{
    return builtin(left + right);
}

let item = builtin(matrix[i][j] + dx * scale);
```

上例中 `builtin(...)` 内部的 `+`、`*` 和 `[]` 都只使用内建规则。需要局部使用用户 operator 时，应先把那部分表达式绑定到局部变量，再进入 `builtin(...)`。

调用形状也固定：

- 只识别普通名字 callee 的 `builtin(expr)`。`self.builtin(expr)`、`Type::builtin(expr)` 或通过函数值间接调用都不是这个 escape 语法。
- `builtin` 不接受显式类型实参，`builtin<i32>(expr)` 会报 `invalid_type_argument`。
- 必须且只能写一个表达式实参，`builtin()` 和 `builtin(a, b)` 都会报 `argument_count_mismatch`。
- `builtin(expr)` 的结果类型就是 `expr` 在内建 operator-only 模式下检查出的类型；它不会产生额外 wrapper，也不会把表达式强制转换成某个目标类型。
- `expr` 检查时没有来自外层的 expected type。也就是说，`let p: i32* = builtin(nullptr);`、`let xs: [i32; 0] = builtin([]);` 或把省略参数类型的 lambda 包进 `builtin(...)`，都不能靠外层声明、返回类型或函数参数补上下文。需要这类目标类型时，应先在 `builtin(...)` 外建立有明确类型的值，或在内部写出能自行定型的表达式。
- 错误恢复时，`builtin()` 没有可检查的内部表达式，结果按错误类型处理；`builtin(a, b)` 会报告参数数量错误，并只用第一个实参 `a` 的内建 operator-only 检查结果继续后续推导。合法程序不能依赖这些错误调用的恢复类型。
- 这个调用名在普通调用表达式检查时优先于用户函数查找。即使当前作用域有名为 `builtin` 的函数、局部变量或导入符号，直接写 `builtin(expr)` 仍按内建 escape 处理。
- 裸 `builtin` 不是一等函数值。`let f = builtin;` 或 `run(builtin);` 只会按普通名字查找；若没有同名普通 binding，就是未知名字。

## 赋值与复合赋值

```text
= += -= *= /= %= &= |= ^= <<= >>=
```

规则：

- 赋值左侧必须是左值。
- 不能给 `const` binding 重新赋值。
- 这两个左侧检查发生在用户赋值 operator 查找之前；`operator =` / `operator +=` 等不能把临时值、按值结果、只读下标结果或 `const` binding 变成合法赋值目标。比如 `make_vec() = other`、`const_item = other`、返回值不是引用的 `operator []` 结果再赋值，都会先按赋值目标规则报错。
- 简单赋值 `=` 可以由 `operator =` 重载。
- 没有可用 `operator =` 时，简单赋值按左侧类型检查右侧表达式并使用内建赋值。
- `operator =` 的左操作数必须是可写引用；成员形式必须以 `self&` 作为第一个参数。
- 复合赋值可以由对应 `operator +=`、`operator -=` 等重载。
- 没有可用复合赋值 operator 时，只尝试编译器内建二元运算再写回左侧。也就是说，`i32 += i32`、`ptr += integer` 这类内建路径可用；但不会把用户声明的 `operator +` 自动改写成 `operator +=`，也不会调用普通函数做用户自定义转换。
- 内建复合赋值的 fallback 和对应二元内建 operator 完全同源：`+=` / `-=` 可用数值运算和指针加减整数，`*=` / `/=` 可用数值运算，`%=` 只可用整数取余，`&=` / `|=` / `^=` / `<<=` / `>>=` 只可用整数位运算。这里的整数族不包含 `bool`、`char`、`enum` 或 opaque alias；这些类型需要显式声明对应 `operator op=`，或者先转换到底层整数后再运算。
- 指针复合赋值只开放“指针加/减整数”这类能写回原指针类型的内建路径。`ptr += other_ptr`、`ptr -= other_ptr` 或经 `let alias: T*& = ptr; alias += other_ptr;` 这类写法不合法；指针差值 `ptr2 - ptr1` 的结果是 `isize`，不能作为 `-=` 写回 `T*`。
- 当前实现会先为 `=` 和复合赋值尝试用户 operator 候选，并在这一步用“无 expected type”的方式检查右侧表达式；只有没选中用户 operator 的简单 `=` fallback 才按左侧类型重新检查右侧。因此，右侧如果需要目标类型才能成立，例如空数组字面量 `[]`，不要直接依赖赋值左侧补上下文；应先写有显式类型的局部值，或写成带类型名的初始化表达式。
- 内建复合赋值要求 `left op right` 的结果能隐式转换回左侧读出类型，否则报错。
- 内建简单赋值和内建复合赋值表达式的结果类型都是左侧读出类型，但结果本身不是左值，不能继续作为赋值左侧。
- 选中用户定义的 `operator =` 或复合赋值 operator 时，赋值表达式的结果类型由该 operator 返回类型决定；如果返回引用，表达式按引用结果继续作为左值。返回类型可以是内部 `unit`、普通值类型，也可以是左操作数引用类型，例如成员形式的 `this&`。
- 用户定义赋值 operator 的“可写左操作数”约束在声明收集阶段检查，而不是等到调用点。顶层 `operator =(left: vec2&, rhs: vec2 const&)` 是合法形状，`left: vec2 const&`、`left: vec2 move&` 或 `left: vec2` 会直接作为非法 operator 报错；成员 `operator =(self const&, rhs: this const&)` 同样非法，必须写成 `operator =(self&, rhs: this const&)`。

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
- 数组下标结果的左值性跟随数组表达式：可写数组左值产生可写元素左值，const 数组或只读访问产生只读元素左值，临时数组不提供可写左值目标。
- 指针下标结果总是左值；是否可写取决于下标目标表达式的 target const 规则。普通 `p: T*` 和 `const p: T*` 的 `p[i]` 都是可写 `T` 左值，`p: T const*` 的 `p[i]` 是只读 `T` 左值。若下标目标本身是只读引用表达式，例如参数 `p: (T*) const&`，当前实现会让 `p[i]` 只读；这点与显式写 `*(p + i)` 不完全相同，因为指针算术先产生新的指针值。
- `str` 下标结果只读，不是可写左值。
- 数组的常量整数下标会做静态边界检查；负数或 `index >= N` 报 `invalid_operator`。其它整数下标在语义层只做类型检查，运行期越界会触发内建 panic。
- `str` 下标当前只在语义层检查整数类型，不做静态边界证明；运行期越界同样会触发内建 panic。指针下标只做整数类型检查和地址计算，不做静态或运行期边界证明。
- 只有下标目标类型不是 `[T; N]`、`str` 或指针时，才查找 `operator []`。如果目标已经是这些内建下标类型，索引类型错误、数组常量越界或 `str` 只读结果都按内建规则诊断，不会回退到用户 `operator []`。
- `operator []` 可以返回值，也可以返回引用。按值返回时，`value[index]` 只是普通右值，可以读取或参与后续表达式，但不能作为可写左值、不能取 `ref`、不能用 `&value[index]` 取得元素地址，也不能用于 `value[index] = rhs`、`value[index] += rhs`、`++value[index]` 这类需要可写目标的语法。返回 `T&` / `T const&` 时，`value[index]` 的左值性和可写性跟随返回引用；只有返回可写引用时才可以作为赋值目标。
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

- `callee` 可以是普通命名函数、成员函数、关联函数、variant case 构造、函数类型值、函数指针值、闭包类型值或实现 `operator ()` 的对象。
- 调用普通命名函数、成员函数和关联函数时，按对应声明检查参数和返回类型。
- 调用 `f(...) -> R` 函数类型值时，参数必须能按函数类型的形参类型检查，结果类型为 `R`。
- 调用 `f*(...) -> R` 函数指针值时，参数检查和结果类型同函数类型；如果指针为空，属于底层 unsafe 契约。
- 有捕获闭包的调用规则见 [lambda.md](lambda.md)。
- 函数类型中的形参名只用于诊断，不参与调用匹配。
- 名字调用首先识别编译器 builtin 调用名：`alloc`、`free`、`construct_at`、`destroy_at`、`panic`、`assert`、`builtin` 和 `unreachable`。这些名字按 builtin 规则处理，不作为普通用户函数重载。
- 这些 builtin 名只在裸名字作为调用 callee 时触发，不提供可传递的函数值。裸 `panic`、`free` 或 `builtin` 等表达式只按普通名字查找；若当前没有同名普通 binding，会得到未知名字。需要把 builtin 行为传给高阶接口时，用 lambda 或普通包装函数。
- 普通函数、成员函数、关联函数和闭包调用都支持泛型实例化；非泛型目标后面写显式类型实参会报 `invalid_type_argument`。
- 表达式位置的显式泛型调用实参必须紧跟调用括号：`id<i32>(1)`、`value.method<i32>(1)` 和 `Type::make<i32>(1)` 是调用表达式；`let f = id<i32>;`、`run(id<i32>);` 和 `value.method<i32>` 不是当前可用的“已特化函数值”写法。需要把函数作为值保存或传递时，应使用已有的具体函数类型值、函数指针、闭包或一层 lambda 包装。
- parser 只在 `<...>` 后面立即看到 `(...)` 时把它当作泛型调用后缀，因此 `value < id<i32>(4)` 中左侧 `<` 仍是关系运算，右侧 `id<i32>(4)` 才是泛型调用。完整泛型调用边界见 [generic.md](generic.md#表达式位置的显式类型实参调用)。
- 有默认参数的普通函数、成员函数和关联函数允许省略尾部实参；默认表达式按对应参数类型检查。默认值只能出现在尾部参数上，带默认值的参数后面不能再跟没有默认值的普通参数。`self` receiver、推导类型参数、值参数包和 lambda 参数都不能带默认值；这些形状即使能被 parser 记录，也会在语义阶段报错。
- 在固有 `impl` 方法体内，`method(args...)` 可以省略 `self.`；如果当前 `self` 类型有可用方法，按 `self.method(args...)` 检查。
- 第一实参 UFCS 可用：`method(object, args...)` 可以在存在 `object.method(args...)` 可用候选时改写为成员调用。它只用于方法调用，不用于 operator。
- 成员调用也支持普通函数回退：`object.name(args...)` 若找不到成员方法，但当前可见普通函数 `name(object, args...)` 可调用，则按该普通函数检查。这是第一实参 UFCS 的点号形式。
- `Type::case(args...)` 对有 payload 的 variant case 构造对应 variant 值；payload 数量和类型必须匹配。无 payload case 是值，不是可调用对象，写 `Type::case()` 会报错。
- 如果普通函数值、函数指针和闭包调用规则都不成立，编译器才查找 `operator ()`。`operator ()` 的实际参数序列为 `callee, args...`，第一个参数是被调用对象本身。
- 普通表达式调用的 `operator ()` 查找会按 operator lookup 使用目标类型命名空间、当前可见 extension operator 和当前可见顶层 operator；这和 `std.meta` 的 `callable` / `call_result` 查询一致。但 `std.compare` 的 `ordering<T>` 是排序协议专用的保留 concept，判定范围更窄；不要把“表达式可调用”直接推断成“可作为 map/set/sort 的 order object”。
- 如果 callee 已经是闭包、函数类型值或函数指针值，编译器会停在对应 callable 路径上检查参数数量和类型；即使实参数量不匹配，也不会再把同一个 callee 当作普通对象去查 `operator ()`。需要同时支持“函数指针调用”和对象 call operator 的类型时，应在外层写不同包装类型或显式调用对应成员。
- 显式类型实参不传递给 `operator ()`。`callable<i32>(value)` 只有在 `callable` 本身是泛型函数或泛型闭包时才表示泛型调用；如果 `callable` 是普通对象并靠 `operator ()` 可调用，当前语义层会先报告 `invalid_type_argument`，然后只用普通实参按 `operator ()(callable, value)` 做错误恢复检查。需要泛型 call object 时，应让对象的类型本身携带泛型参数，或用泛型函数 / 泛型 lambda 包一层。

默认参数只在调用表达式仍然保留“函数符号”身份时可用：

- 直接命名普通函数、成员函数、关联函数、隐式 `self` 方法调用、第一实参 UFCS 和点号 UFCS 都保留函数符号，可以省略尾部默认实参。
- 通过函数类型值、函数指针值、字段、参数或局部变量调用时，只看 `f(...) -> R` / `f*(...) -> R` 类型本身；函数类型不携带默认参数表，实参数量必须和函数类型形参数量完全一致。
- 闭包和 lambda 调用不使用默认参数，因为 lambda 参数默认值不是当前支持能力。
- `operator ()` 按 operator 候选匹配，当前不把被调对象的 call operator 当作普通函数默认参数调用入口。

查找顺序影响是否进入 UFCS 或 call operator：

- 裸名 `name(args...)` 如果已经解析到局部 binding、普通函数或泛型函数，就按该解析结果检查；即使参数不匹配，也不会再回退成 `self.name(args...)` 或 `args[0].name(...)`。
- 只有裸名没有解析到普通名字时，固有 `impl` 体内才尝试隐式 `self` 方法调用；仍失败且至少有一个普通实参时，才尝试第一实参 UFCS。
- 点号调用 `object.name(args...)` 先查成员方法；只有没有成员方法候选时，才尝试当前可见普通函数 `name(object, args...)`。如果成员方法存在但 receiver、参数、约束或默认参数检查失败，错误停在成员方法路径，不继续改用自由函数。
- `Type::name(args...)` 在 `Type` 是 `variant` 时先查同名 case；如果没有 case，才查对应类型的 associated function。associated call 的 owner 必须是 `struct`、`variant` 或 opaque alias；数组、builtin、tuple、storage、指针、引用、函数类型和函数指针不能作为 `Type::name(...)` associated call owner。
- 只有当前 callee 既不是闭包、也不是函数类型/函数指针值，并且普通 callable 检查失败时，才用 `operator ()` 检查可调用对象。

不支持：

- 普通函数名重载。
- C++ ADL。
- 用 `operator ()(object, args...)` 这种普通函数调用语法直接调用 call operator。
- 把有捕获闭包隐式转换成函数类型或函数指针。

### 控制与诊断 builtin

`panic`、`assert`、`unreachable` 和 `builtin` 是裸 callee 名字触发的编译器 builtin，不是普通标准库函数：

```cp
panic("bad");
assert(index < len, "index out of bounds");
unreachable();
let raw = builtin(left + right);
```

规则：

- `panic(message)` 要求一个 `str` 消息参数，返回类型是 `!`。它会调用 runtime panic 入口并终止当前控制流，因此可以作为 `return panic(...)`、`match` arm 或块表达式的发散值。
- `unreachable()` 不接受参数，返回类型也是 `!`。当前会以固定消息 `"entered unreachable code"` 触发 panic。它表达“此处不应到达”的运行时防御，不是优化承诺。
- `assert(condition)` / `assert(condition, message)` 返回 `unit`。`condition` 必须是 `bool`，可选 `message` 必须是 `str`。checked 构建中条件为 false 时 panic；`--release` 会移除整个 assert 调用，当前不会求值 condition 或 message。因此不要把必须执行的副作用放进 `assert` 实参。
- `builtin(expr)` 要求一个表达式参数，返回类型就是该表达式类型。检查 `expr` 时会进入“只用内建 operator”的模式：内建算术、比较、下标等仍可用，但用户定义 operator、call operator 或其它 operator fallback 不参与。这个模式覆盖 `expr` 的整棵表达式树；需要只绕过某一小段时，应只把那一段包进 `builtin(...)`。
- 这四个 builtin 都不接受显式类型实参。`panic<i32>(...)`、`assert<bool>(...)`、`unreachable<i32>()` 和 `builtin<i32>(...)` 都会报 `invalid_type_argument`，不会回退到同名普通函数。

`alloc`、`free`、`construct_at` 和 `destroy_at` 的内存生命周期规则见 [memory_allocation.md](memory_allocation.md)。

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
- 这两个检查发生在用户自增自减 operator 查找之前；`operator prefix ++` / `operator postfix ++` 不能让临时值、按值返回结果或 const 左值变成合法更新目标。
- 内建路径只允许整数类型，不支持 `bool`、浮点、指针、`array` 或 `tuple`。整数类型会直接使用内建自增自减，不会被用户声明的 `operator prefix ++` / `operator postfix ++` 覆盖；需要自定义更新语义时应使用 `struct` / `variant` 等非整数类型包装。
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

`as` 进入显式转换检查，完整白名单和运行边界见 [cast.md](cast.md)。本页只列 operator 语境下最容易误解的入口规则：

- `as` 是内建语法，不是可重载 operator；不能声明 `operator as`，也不能通过成员函数、关联函数或普通 operator 改变 `value as T` 的含义。
- `as` 左结合，`value as i32 as i64` 按 `(value as i32) as i64` 检查；每一步都必须在显式转换白名单内。
- `as` 的目标必须是类型语法，不是运行时表达式或函数调用；`value as make_type()`、`value as other_value` 都不是当前转换语法。
- 语义检查失败时报 `invalid_cast`，但错误恢复会让整个 cast 表达式继续拥有右侧目标类型，方便后续诊断。合法代码不能依赖非法 cast 的恢复类型或运行行为。
- 当前公开白名单包括：已经允许隐式转换的路径、数值类型显式互转、指针/函数指针之间的显式互转、`enum` 到其精确底层整数类型、以及 opaque alias 声明源文件内部的直接底层互转。反向整数到 enum、跨文件 opaque 底层互转、构造引用、调用构造函数或用户自定义 conversion operator 都不是第一版能力。

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
