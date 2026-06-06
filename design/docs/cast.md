# 类型转换

本文档记录 KCP 的显式转换语法。类型转换的可转换性规则见 [type_system.md](type_system.md)。

转换语法只有显式的 `as`。语言仍有一套窄的目标位置隐式转换白名单，覆盖变量初始化、赋值、函数实参、返回语句和 operator 候选匹配等场景；完整列表见 [type_system.md](type_system.md#转换)。本文只记录 `as` 这个表达式语法和它额外放宽的转换集合，不采用宽泛的 C++ 式隐式转换或用户自定义转换。

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

`as` 是表达式中的二元运算形式。当前 parser 把它放在普通二元运算的最高优先级层级：高于 `*` / `/` / `%`、`+` / `-`、位移、比较、相等、位运算和逻辑运算，低于一元运算、调用、字段访问、下标等表达式后缀。连续 `as` 按左结合解析。

因此：

```cp
let a = 1 + 2 as f64;       // 解析为 1 + (2 as f64)
let b = (1 + 2) as f64;     // 整个加法结果再转换
let c = value as i32 as i64; // 解析为 (value as i32) as i64
```

`as` 的目标类型只用于转换检查，不会反向作为左侧表达式的上下文类型。语义阶段会先在没有 expected type 的情况下检查左侧表达式，再判断检查出的源类型能否显式转换到目标类型。因此：

- `nullptr as T*` / `nullptr as f*(...) -> R` 合法，因为 `nullptr` 自身有独立的 `nullptr` 类型，并且右侧提供了具体指针类型。
- 非空数组字面量如 `[1, 2] as [i32; 2]` 可以成立，因为 `[1, 2]` 能先自行推导为 `[i32; 2]`。右侧目标类型不会参与元素检查；`[1, 2.0] as [i32; 2]` 仍会先按无上下文数组字面量报异构元素错误，而不是先把 `2.0` 按 `i32` 检查。
- 空数组或空聚合字面量仍需要上下文，`[] as [i32; 0]` 不会用 `as` 右侧补上下文；应写 `let xs: [i32; 0] = [];` 这类目标位置初始化。

## 转换规则

`as` 进入显式转换检查。它不是构造语法，也不触发用户自定义转换函数。即使目标类型附近声明了构造函数、关联函数、成员函数、`operator` 或约定俗成的 `to_*` 方法，`as` 也不会查找这些候选；`"text" as string` 不会改写成 `string{"text"}`，`value as point` 也不会改写成 `point{value}`。需要构造对象时应直接写目标类型的构造/聚合初始化语法，`as` 只按下面的白名单判断已有源类型和目标类型之间能否转换。

如果转换不在下列允许集合内，语义阶段报告 `invalid_cast`。当前错误恢复会让整个 cast 表达式继续拥有右侧目标类型，用于后续表达式检查和返回类型推导；这只是诊断恢复，不表示转换合法，也不表示非法 cast 有可依赖的运行语义。源表达式或目标类型已经是错误类型时，转换检查会继续传播错误而不额外制造一套 cast 规则；`!` / never 表达式按现有隐式转换规则可以流向任意目标类型，但运行不会继续到使用该值的位置。

`!` 的转换是单向的：`panic("bad") as i32` 这类 never 源表达式可以通过，因为 never 已经能隐式流向任意目标；普通值不能通过 `as !` 变成 never。`1 as !` 会报 `invalid_cast`，和 `let value: ! = 1;` 的普通赋值失败一样，都不表示可以构造一个正常返回的 never 值。

显式转换允许：

- 已经允许隐式转换的类型，包括整数互转、浮点向更高 rank 转换、`nullptr` 到指针、函数类型到完全相同签名的对应函数指针、以及 const / like 资格转换。
- 任意数值类型之间的显式转换，包括整数/浮点互转、浮点窄化、整数窄化和符号变化。这里的数值类型只包含整数和浮点内建类型，不包含 `bool`、`char`、`str` 或 `nullptr`。
- 任意指针类型之间的显式转换，包括不同 pointee 类型的指针互转。它只转换地址类型，不证明目标地址真的满足新 pointee 类型的对象生命周期、对齐或别名规则。
- `enum` 值到它声明的精确底层整数类型。
- opaque alias 与它的底层类型之间的转换，但只允许在声明该 opaque alias 的同一个 translation unit / 源文件内部。

普通 `type name = T` 别名不会创建新的 cast 权限或名义边界。别名在类型检查中等价于目标类型，因此 `value as alias` 等价于 `value as T`。只有 `type name = opaque T` 会形成独立名义类型，并受本文的 opaque 转换权限限制。

指针到指针转换也覆盖函数指针和普通数据指针都属于“指针类型”的情况。语义层只确认两边都是指针类型；它不证明函数指针与数据指针在目标平台上可互换，也不证明转换后的地址能安全调用或解引用。

这条显式指针转换比隐式资格转换更宽。隐式转换只能递归增加 target const，例如 `i32*` 可以流向 `i32 const*`，不能反向去掉 const；但当前 `as` 会把 `T*`、`T const*`、`f*(...) -> R` 以及不同 pointee 的指针都视为可显式互转。因此 `readonly as i32*` 这类从 `i32 const*` 到 `i32*` 的转换在语义层可以通过。它只改变静态地址类型，不证明原对象真的可写；通过转换后的指针写入本来只读的对象属于底层 unsafe 违约。公共 API 不应把这种写法当作普通 const-cast 能力使用。

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

`enum` 只能一步转换到声明时写出的底层类型。`enum flag : u8` 的值可以写 `value as u8`，不能直接写 `value as i32`；需要其它整数类型时应先转到底层类型，再按普通数值转换继续转换，例如 `value as u8 as i32`。

数值转换示例：

```cp
let a: i32 = 1 as i32;
let b: f64 = 1 as f64;      // 整数到浮点必须显式写 as
let c: f32 = 1.5 as f32;    // 浮点窄化必须显式写 as
let d: u8 = (-1) as u8;     // 允许，结果按目标底层数值转换规则
```

当前数值 `as` 的运行结果边界：

- 整数到整数窄化会按目标位宽截断；扩宽当前按 signed 语义扩展。也就是说，`let value: u8 = 255 as u8; value as i32` 当前会得到 `-1`，不是 `255`。
- 整数到浮点当前按 signed integer to float 处理；浮点到整数按 floating point to signed integer 处理；浮点到浮点使用普通浮点转换。
- `bool` 和 `char` 虽然底层也可表示成整数位宽，但语义层不把它们纳入数值类型，因此不能通过 `as` 当作普通整数转换。
- enum 和 opaque alias 的转换先受语义层 enum/opaque 权限检查约束，再按底层类型执行转换。

因此，当前第一版如果需要稳定的无符号扩宽语义，不应依赖 `as` 的运行结果来表达跨位宽 unsigned cast；应先把 API 约束在不触发歧义的整数范围内，或等待语言把 signed/unsigned 数值转换规则收紧后再作为稳定能力使用。

函数和函数指针：

```cp
add(left: i32, right: i32) -> i32
{
    return left + right;
}

let fn: f(i32, i32) -> i32 = add;
let ptr: f*(i32, i32) -> i32 = add;    // 隐式函数到函数指针也允许
let ptr2 = fn as f*(i32, i32) -> i32;  // 显式写 as 同样允许
```

函数类型只能转到完全相同函数签名的函数指针。签名必须逐项相同：形参数量、参数类型、返回类型都不能靠隐式转换、别的调用约定或重排补齐。它不会做参数重排、返回类型转换、调用约定转换或闭包对象到函数指针的转换。有捕获闭包不是函数类型值，不能通过 `as` 变成 `f*`。`nullptr` 只能转到 `f*(...) -> R` 这类函数指针，不能转到非指针的 `f(...) -> R` 函数类型值。

`as` 按源表达式的静态类型做转换检查，结果表达式类型就是右侧目标类型。它不保留源表达式的左值类别、可写性或 NRVO 候选身份，也不是“重新绑定引用”的语法。

从引用值转换时，当前实现按引用的读出类型参与转换，因此转换的是被引用对象的值类型：

```cp
let value = 1;
let ref alias = value;
let out = alias as i64; // i32 -> i64
```

不要把显式借用表达式直接拿去做值转换：

```cp
let value = 1;
let out = (ref value) as i64; // 不作为公开能力
```

普通引用变量如 `alias as i64` 会先按引用读出值，再进行数值转换；但 `(ref value) as i64` 这类显式借用表达式不作为公开能力。第一版公开代码应把显式借用交给引用目标位置使用，或先读出普通值再转换，不应依赖显式借用表达式本身的 `as`。

不要用 `as` 表达借用、移动或生命周期转换。需要引用时使用 `ref` / `const ref`、引用形参或引用返回；需要移动时使用 `move`。

`as T&`、`as T const&`、`as T move&` 和 `as T forward&` 都不是当前公开能力。即使某些引用形状没有在语义阶段被完整拒绝，`as` 也不会创建可绑定引用，cast 结果也不是可写左值。设计上应把它们视为非法/未定义的第一版表面；需要引用时必须写 `ref value`、`const ref value`，或让目标位置是引用形参、引用返回、引用 binding。

opaque alias 的显式转换有 translation unit 边界：

```cp
export module handle_impl;
export type handle = opaque u8*;

export make(raw: u8*) -> handle
{
    return raw as handle; // 合法：和 handle 声明在同一源文件
}

export raw(value: handle) -> u8*
{
    return value as u8*; // 合法：和 handle 声明在同一源文件
}
```

当前实现按声明所在 translation unit 检查这条权限，不按模块名检查。也就是说，另一个文件即使同样声明 `export module handle_impl;`，也不能直接写 `value as u8*` 或 `raw as handle`。需要把转换封装在声明 `handle` 的同一源文件里的导出函数、关联函数或方法中，再让其它文件调用这些 API。

导入 `handle` 的其它模块同样不能直接写 `value as u8*` 或 `raw as handle`，只能通过导出的函数和方法使用。

opaque alias 不会自动继承底层类型的其它转换权限。即使 `handle` 的底层类型是 `u8*`，`nullptr as handle` 也不会先套用 `nullptr -> u8*` 再转 opaque；当前会按 `invalid_cast` 拒绝。需要空 handle 时，应在声明 `handle` 的同一源文件里显式写成两步转换，例如 `(nullptr as u8*) as handle`，并把它封装成构造 / 工厂函数。导入方不能直接完成这两步中的 opaque 转换。

不允许的转换：

- `str` 到 `string`。字符串字面量类型是 `str`，拥有型字符串应写 `string{"text"}` 或使用标准库构造函数，不能写 `"text" as string`。
- 整数到 `enum` 的公共构造。第一版只开放 `enum -> exact underlying`，不开放 `underlying -> enum`。
- `enum` 直接转换到非声明底层类型的其它整数类型。
- `nullptr` 直接转换到 opaque pointer alias；必须在 opaque 声明源文件内先转换到底层指针类型，再转换到 opaque。
- 指针和整数之间的转换。
- 有捕获闭包到函数类型或函数指针。
- 值到引用、引用到其它引用类别、或引用到移动引用/转发引用的公开转换。引用关系由 `ref` / `const ref` / `move` / `forward` 和目标位置匹配表达，不由 `as` 表达。
- `bool`、`char`、`str`、数组、元组、结构体、variant 之间没有本文未列出的通用转换。
- 通过构造函数、成员函数、`operator` 或 opaque alias 外部表示绕过类型边界。

这些写法会失败：

```cp
let text = "cp" as string;     // 错误：as 不是构造
let flag = 1 as bool;          // 错误：数值转换不包含 bool
let mode = 1 as open_flag;     // 错误：不开放 underlying -> enum
let addr = pointer as usize;   // 错误：指针不能转整数
```

不设计：

- 用户自定义转换。
- 函数式转换 `type(value)`。
- `reinterpret_cast` / `static_cast` / `const_cast` 这样的多种 cast 分类。
- 通过构造函数进行隐式转换。
