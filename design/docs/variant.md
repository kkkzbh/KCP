# Variant 类型

本文档记录 `variant` 的语言设计。`variant` 用来定义名义类型，表示若干个 case 中恰好一个。它在语言语义上接近 Rust 的 `enum`，运行时布局是 tag 加重叠 payload storage。当前实现已经支持 case 构造、`match` 分派和 payload 读取，但还没有按 tag 自动析构 payload；资源生命周期边界见后文“Impl”和“内存模型”。

`variant` 不依赖用户可写的 `union`。编译器内部需要支持重叠 payload storage，但这只是布局能力，不暴露为源语言特性。

struct-like case、pattern guard、嵌套解构 pattern、niche optimization 和用户可写 `union` 不支持。

## 语法总览

```text
VariantDecl          -> export? variant identifier GenericParameterList? { VariantCase* }
VariantCase          -> identifier ;
                     | identifier ( TypeList ) ;
TypeList             -> Type ( , Type )*

VariantExpr          -> TypeName :: identifier
                     | TypeName :: identifier ( ArgumentList? )

MatchExpr            -> match Expression { MatchArm* }
MatchArm             -> VariantPattern => Expression ,
VariantPattern       -> . identifier
                     | . identifier ( PatternBindingList )
                     | _
PatternBindingList   -> identifier ( , identifier )*
```

`GenericParameterList` 的完整规则见 [generic.md](generic.md)。`variant` 支持类型参数、整数 const 参数和默认泛型实参；类型参数包不是当前 `variant` 类型构造器的公开能力。

## Variant 定义

`variant` 和 `struct` 一样定义名义类型：

```cp
variant option_i32 {
    none;
    some(i32);
}

variant parse_result {
    ok(i32);
    error(str);
}
```

结构完全相同的 `variant` 仍然是不同类型：

```cp
variant left {
    value(i32);
}

variant right {
    value(i32);
}
```

`left` 和 `right` 不能隐式转换。

`export variant` 导出 variant 类型名，使它可以被其他模块 `import` 后使用：

```cp
export variant option_i32 {
    none;
    some(i32);
}
```

case 名不是独立的顶层导出实体，没有 `export none;` 或“只导出部分 case”的能力。导入方只要能解析到导出的 `variant` 类型，就可以通过 `Type::case` / `Type::case(...)` 使用它的所有 case；如果类型本身未导出，导入方也无法绕过类型可见性单独引用 case。case 名也不会以裸名字进入当前作用域，`some(1)` 或 `none` 不会因为导入了 `option_i32` 就自动解析成 `option_i32::some(1)` / `option_i32::none`。

## 泛型 Variant

`variant` 可以带泛型参数，规则和泛型 `struct` 一致：

```cp
variant optional<T> {
    none;
    some(T);
}

variant expected<T, E> {
    value(T);
    unexpected(E);
}
```

泛型 `variant` 定义一个名义类型构造器。不同类型实参得到不同的具体类型：

```cp
optional<i32>
optional<str>
expected<i32, str>
```

这些具体类型彼此不同，不能因为 case 形状相同而互相隐式转换。

case payload 可以直接使用 `variant` 的泛型参数。泛型参数的作用域覆盖整个 `variant` 体，不覆盖对应的 `impl`。`impl` 需要从自己的目标类型模式重新绑定参数：

```cp
impl optional<T> {
    has_value(self const&) -> bool
    {
        return match self {
            .some(value) => true,
            .none => false,
        };
    }
}
```

泛型 `variant` 支持整数 const 参数和默认泛型实参：

```cp
variant maybe_array<T = i32, N: usize = 2> {
    none;
    some([T; N]);
}
```

不支持对 case 构造器做类型实参推导。构造泛型 `variant` 时，类型名处必须已经能形成具体实例；默认泛型实参可以补齐缺省位置，但 payload 实参不会反推出类型实参：

```cp
let a = optional<i32>::none;
let b = optional<i32>::some(1);
let c = expected<i32, str>::value(1);
let d = maybe_array::some([1, 2]); // T/N 来自默认泛型实参，不来自 payload 推导
```

下面这些形式不合法：

```cp
optional::some(1)
optional<_>::some(1)
expected<i32>::value(1)
```

`_` 没有“占位类型”或“从 payload 推导类型实参”的特殊含义。`optional<_>` 不会让 `some(1)` 反推出 `T = i32`；合法代码必须写出具体类型实参，或让 `variant` 声明自己的默认泛型实参补齐缺省位置。

case 构造器本身也没有独立的显式类型实参列表；类型实参只能写在 `variant` 类型名上：

```cp
optional<i32>::some(1)       // 合法
optional<i32>::some<i64>(1)  // 不合法：case 构造器不接收显式类型实参
```

语言层面不要把 payload case 当成泛型函数使用。当前语义实现还没有对这类调用点类型实参做专门诊断：如果 `variant` 类型名本身已经能具体化，会忽略 case 名后的类型实参，只按已确定的 payload 列表检查实参个数和类型。不要依赖这种恢复行为；公开能力只包括在 `variant` 类型名上写泛型实参或使用默认泛型实参。

泛型 `variant` 可以参与泛型固有 `impl` 和泛型 concept `impl`。条件实现使用普通 `requires` 规则：

```cp
impl equality_comparable<optional<T>> for optional<T>
requires
    T: equality_comparable<T>
{
}
```

约束不满足时，该 `impl` 不为对应具体实例提供 concept 证明。

## Case 规则

case 名在同一个 `variant` 内不能重复。

case 可以不携带 payload。这类 case 称为 unit case：

```cp
variant option_i32 {
    none;
    some(i32);
}

variant status {
    qqq;
    ready;
    failed(str);
}
```

`none`、`qqq`、`ready` 都只是普通 case 名，不是语言内建关键字。它们只在各自的 `variant` 类型命名空间下有意义。

空括号 case 不合法。没有 payload 时直接写 case 名：

```cp
variant option_i32 {
    none;       // 合法
    none();     // 不合法
}
```

payload 类型列表当前不接受尾随逗号。`resize(i32, i32);` 可用，`resize(i32, i32,);` 会在 parser 阶段因为逗号后缺少类型而报错。case 声明之间也不使用逗号分隔；每个 case 都以分号结束。

允许多个 case 携带相同 payload 类型，因为 case 由名字区分，不由 payload 类型区分：

```cp
variant token_value {
    integer(i32);
    character(i32);
}
```

支持两类 case：

```cp
variant event {
    quit;
    key(char);
    resize(i32, i32);
}
```

`resize(i32, i32)` 是多 payload case。多 payload case 不等价于携带一个公开 `(i32, i32)`，而是 `variant` 自己的 case payload 列表。用户在 `match` 中直接按 case payload 绑定：

```cp
match event {
    .resize(width, height) => width * height,
    .key(code) => 0,
    .quit => 0,
}
```

编译器实现可以复用 tuple-like 布局计算，但类型系统不把 `resize(i32, i32)` 视为 `resize((i32, i32))`。

`variant` 支持任意数量 case。case 数量会影响穷尽检查和运行时分派成本；语言层不暴露 case tag 数值。

当前 parser 和语义收集没有拒绝零 case 的 `variant empty { }`，但它不是稳定公开能力。零 case variant 没有任何 case 构造器，不能默认初始化，也没有普通源码路径能构造出一个值；公开代码不要依赖零 case variant 或空 arm `match` 作为可运行特性。需要“不返回”的语义时使用 `!` / `panic` 这类已有机制。

不支持 struct-like case：

```cp
variant event {
    mouse { x: i32; y: i32; }
}
```

struct-like case 会牵扯字段绑定、字段名检查和 pattern 字段匹配，不属于当前 `variant` 设计。

## 构造

case 构造器挂在 variant 类型命名空间下：

```cp
let a = option_i32::none;
let b = option_i32::some(1);
let c = parse_result::error("invalid number");
let d = optional<i32>::some(1);
```

无 payload 的 case 不写括号。带 payload 的 case 必须写括号，参数数量和类型必须匹配 case 声明。

```cp
option_i32::none      // 合法
option_i32::none()    // 不合法

option_i32::some(1)   // 合法
option_i32::some      // 不合法
```

构造表达式的类型为对应的 `variant` 类型。

case 构造名和普通 associated function 共用 `Type::name` 语法，但查找顺序不同：

- `Type::case_name` 这种不带调用括号的 associated name，在 `Type` 是 `variant` 时只表示 unit case。名字不是 case 时报 `unknown_variant_case`，不会再去找同名 associated function。
- `Type::case_name(...)` 这种 associated call，在 `Type` 是 `variant` 时先尝试按 case 构造器查找；如果没有同名 case，再查找 `impl Type { ... }` 中的 associated function。因此 `Type::missing` 报 `unknown_variant_case`，`Type::missing()` 报 `unknown_member`。
- unit case 不能写成调用。`option_i32::none()` 报 `not_callable`。
- 带 payload 的 case 必须写成调用。`option_i32::some` 报 `not_callable`。
- 带 payload 的 case 调用参数数量不匹配时会报错，但编译器仍会检查能对上的前缀实参类型，并继续检查多余实参本身。

因此，如果一个 `variant` 同时有 case `make` 和 associated function `make()`，`Type::make(...)` 优先表示 case 构造；只有没有 `make` case 时才会落到 associated function 查找。不要依赖同名 case/function 共存。

## Match

`match` 是表达式，用于分解 `variant`：

```cp
value_or_zero(x: option_i32) -> i32
{
    return match x {
        .some(v) => v,
        .none => 0,
    };
}
```

`match` 的被匹配表达式必须是 `variant` 类型。

当前 parser 对最常见的 `match name { ... }` 有一个单独入口：`match` 后面紧跟单个 identifier 且下一个 token 是 `{` 时，会直接把这个 identifier 当作被匹配的名字表达式。它不是新的绑定语法，也不会声明 `name`；名字仍按普通表达式名字查找。更复杂的被匹配值仍按普通表达式解析，例如 `match (make_option()) { ... }`、`match item.field { ... }` 或 `match ref value { ... }`。

case pattern 使用点号开头，表示当前 variant 类型下的 case：

```cp
match result {
    .ok(value) => value,
    .error(message) => 0,
}
```

每个 arm 末尾的逗号是当前语法要求的一部分，包括最后一个 arm。`match value { .none => 0 }` 这种省略逗号的形式不作为当前公开语法。

unit case pattern 不写括号。`.none()` 这类空括号 pattern 不合法：

```cp
match value {
    .none => 0,      // 合法
    .none() => 0,    // 不合法
}
```

`match` 必须穷尽。所有 case 都被覆盖，或者存在 `_` 分支时，才是合法的：

```cp
match x {
    .some(v) => v,
    _ => 0,
}
```

同一个 `match` 中不能重复匹配同一个 case。

payload 绑定数量必须和 case payload 数量一致：

```cp
variant pair_result {
    ok(i32, i32);
    error(str);
}

let n = match result {
    .ok(a, b) => a + b,
    .error(msg) => 0,
};
```

payload 绑定是当前 arm 内的普通局部名字。它们只在该 arm 右侧表达式内可见，可以 shadow 外层同名局部；离开 arm 后外层名字恢复可见。同一个 case pattern 里的绑定名不能重复，因为它们进入同一 arm 局部作用域。绑定数量不匹配时会先报参数数量错误；错误恢复只会按 payload 和绑定列表的共同前缀建立可用绑定，多余绑定名不会成为可依赖的局部名字。

通配 arm 可以覆盖剩余所有 case，但不会绑定任何 payload。多 payload case 和 `_` 可以在同一个 `match` 中使用：

```cp
variant event {
    none;
    resize(i32, i32);
    cancelled(str);
}

let area = match event_value {
    .resize(width, height) => width * height,
    _ => 0,
};
```

上例中 `_` 同时覆盖 `none` 和 `cancelled(str)`；`cancelled` 的 `str` payload 在 `_` arm 中不可访问。需要访问 payload 时必须写对应 case pattern。

当前实现不要求 `_` 位于最后，也不把重复 `_` 当作语义错误。所有 arm 仍会继续检查 case 名、payload 绑定和 arm 表达式类型；因此 `_` 后面的具体 case 如果类型不匹配，仍会报错。运行时按源码顺序选择第一个匹配 arm，先出现的 `_` 会遮住后续 arm。公开代码应把 `_` 只写一次并放在最后；不要依赖 `_` 后还有可达 arm。

`match` 每个分支的表达式结果必须能落到同一个结果类型。当前实现按目标类型检查，而不是做双向 common-type 搜索：

- 如果 `match` 处在有上下文类型的位置，例如 `let value: i64 = match ...`、`return match ...` 且函数已知返回类型为 `i64`、或把 `match` 作为某个 `i64` 参数传入调用，则每个非 `!` arm 都必须能隐式转换到该上下文类型。
- 如果没有上下文类型，第一个非 `!` arm 的表达式类型成为 `match` 结果类型；后续非 `!` arm 必须能隐式转换到这个类型。
- `!` arm 不决定普通结果类型；无上下文类型且所有 arm 都是 `!` 时，整个 `match` 的类型为 `!`。如果 `match` 处在已有上下文类型的位置，当前检查器会保留这个上下文类型作为表达式类型，`!` arm 按 never-to-any 的规则满足该上下文。
- 所有可正常完成的 arm 都是 `unit` 时，`match` 的类型为 `unit`。

因此，想让无上下文的 `match` 产生某个更具体的结果类型时，应在左侧声明、函数返回类型或第一个普通 arm 上给出目标：

```cp
let widened: i64 = match value {
    .some(item) => item,
    .none => 0,
};

let explicit = match value {
    .some(item) => item as i64,
    .none => 0,
};
```

不同名义类型、`unit` 和普通值类型、或不能按隐式转换规则落到目标类型的 arm 不能混用：

```cp
match value {
    .some(item) => item,
    .none => true, // 错误：bool 不能作为前一 arm 的 i32 结果
}
```

arm 右侧必须是表达式，不是任意语句列表。可以直接写普通表达式、函数调用、`panic(...)`、另一个 `match`，也可以写块表达式：

```cp
let value = match input {
    .some(item) => {
        let doubled = item * 2;
        doubled
    },
    .none => 0,
};
```

不能把语句直接放在 `=>` 后：

```cp
match value {
    .some(item) => template for(let x : values...) { use(x); }, // 错误：template for 是语句
    .none => return 0,                                          // 错误：return 是语句
}
```

需要多步计算时使用块表达式，并让块的尾表达式成为 arm 值；需要提前从外层函数返回时，把 `return` 写在块表达式内部。

泛型 `variant` 的 `match` 按具体实例检查。编译器先把 case payload 中的类型参数替换为具体类型，再检查 pattern 绑定数量、绑定类型和分支表达式：

```cp
value_or<T>(value: optional<T>, fallback: T) -> T
{
    return match value {
        .some(v) => v,
        .none => fallback,
    };
}
```

泛型 `variant` 和值参数包组合时，每个 `template for` 展开点都按当前 pack 元素的具体实例检查。也就是说，`values: optional<T>...` 中的某个展开元素若是 `optional<i32>`，`.some(item)` 的 `item` 就是 `i32`；另一个展开元素若是 `optional<bool>`，同名 pattern 绑定在那次展开中就是 `bool`。

```cp
variant packet<T> {
    empty;
    pair(T, T);
}

count_pairs<T...>(values: packet<T>...) -> i32
{
    let total = 0;
    template for(let value : values...) {
        total = total + match value {
            .pair(left, right) => 1,
            _ => 0,
        };
    }
    return total;
}
```

上例中，`packet<i32>` 和 `packet<bool>` 可以进入同一个 `packet<T>...` 值参数包，因为它们是同一个名义 `variant` 的不同具体实例；如果把另一个形状相同但声明不同的 `choice<T>` 传进去，则推导失败。pattern 绑定数量、穷尽性、重复 case、未知 case 仍按每次展开后的具体 `variant` 检查。

返回类型推导也发生在具体函数实例上。`template for` 中的 `return match value { .some(item) => item, .none => panic("missing"), };` 会把每次展开的 `item` 类型贡献给当前实例的返回类型；同一实例中如果展开出 `i32` 和 `bool` 这类不能统一的普通返回类型，函数返回类型推导失败。空参数包不会执行展开体，因此展开体内的 `return` 不贡献返回类型。

payload pattern 绑定是当前 arm 表达式里的局部名字。它不能带 `ref`、`const`、类型标注或嵌套解构；绑定类型就是该具体 case payload 替换泛型实参后的类型。同一个 case pattern 内的绑定名共享当前 arm 的局部作用域，因此 `.pair(value, value)` 这类重复绑定名会按普通局部名字重复规则报错；不同 arm 可以使用同名绑定，互不冲突，也不会捕获或覆盖 arm 外的同名局部变量。

`match` 判断被匹配类型时使用读出类型，因此 `match value`、`match ref value` 和 `match const ref value` 都按同一个 `variant` 名义实例检查。当前 pattern 绑定没有源码层 borrow 模式；被匹配表达式是 lvalue 时，payload 绑定可以引用原对象内部 payload；被匹配表达式是临时值或普通返回值时，payload 绑定引用编译器保存后的被匹配值。pattern 名字不是从源码语法上复制出的独立 `let`，但它的静态类型仍是 payload 类型本身；需要稳定保存 payload 到 arm 外时，应在 arm 内显式声明新的局部值。

如果某个 arm 的表达式类型为 `!`，它不决定 `match` 的普通结果类型；其它可正常完成的 arm 决定结果类型。无上下文类型且所有 arm 都是 `!` 时，整个 `match` 的类型为 `!`，这也是省略返回类型推导会观察到的类型。若外层已经给出目标类型，例如 `let value: i32 = match ...` 或显式返回 `-> i32` 的 `return match ...;`，当前语义检查从该目标类型开始，所有 never arm 都可流向这个目标，`match` 表达式类型保留为目标类型。只有错误恢复场景下没有任何普通 arm、也没有任何 `!` arm 时，例如空 `match {}`，结果类型才按 `unit` 处理；这种结果只是为了继续诊断，不是可依赖语义。

当前语义检查器在出错后尽量继续检查 arm 表达式，以便一次报告更多问题：

- 被匹配表达式不是 `variant` 时，`match` 本身报告类型错误；各 arm 右侧表达式仍会被检查，但 case pattern 不会建立 payload 绑定。
- case 名未知时报告未知 variant case；该 arm 右侧表达式仍会在没有 payload 绑定的作用域中检查。
- 重复匹配同一个具体 case 会报告重复 case；重复 arm 仍会按对应 case payload 建立能建立的绑定，并继续检查 arm 表达式。
- payload 绑定数量不匹配时报告参数数量错误；编译器会绑定 `min(payload 数量, 绑定数量)` 个前缀名字，多出的绑定名不会凭空创建。
- 空 `match {}` 面对非空 `variant` 会报告不穷尽；诊断后结果类型按空 arm 错误恢复规则落到 `unit`。不要把这种错误恢复结果当作可依赖语义。

这些错误恢复规则同样适用于 `template for` 展开后的 `match`。如果某次展开中出现重复 case、未知 case 或 payload 绑定数量不匹配，编译器仍会继续检查该 arm 的右侧表达式以及嵌套 `match`，尽量报告后续的名字、类型或返回推导错误。错误恢复不会改变合法程序语义，也不会让错误的 pattern 绑定凭空变成可用名字。

`match` 只能面对 case 名或 `_`，不能匹配内部 tag 数字，也不能依赖 case 声明顺序。tag 编号是编译器内部细节，不进入源语言语义。

## 默认初始化

`variant` 不支持默认初始化。

也就是说，下面不合法：

```cp
let x = option_i32{};
```

必须显式选择 case：

```cp
let x = option_i32::none;
```

不采用“默认构造第一个 case”的规则，因为它会让 case 声明顺序影响程序语义。

## Impl

`variant` 可以拥有 `impl` 块。普通成员函数、关联函数和固有 `impl operator` 都可以注册到 variant 的类型命名空间；调用、UFCS、operator 查找和返回类型推导按普通函数 / operator 规则处理。

```cp
impl option_i32 {
    has_value(self const&) -> bool
    {
        return match self {
            .some(v) => true,
            .none => false,
        };
    }

    zero() -> option_i32
    {
        return option_i32::some(0);
    }
}
```

`variant` 不支持用户声明构造函数和析构函数，也不支持 `operator <=> = default` 这类只面向非泛型 `struct` 的默认三路比较。构造只能通过 case 构造器完成；需要比较、调用或其它自定义运算时，应显式写普通 operator 函数体。

`variant` 没有可直接访问的公共字段，也没有“当前 case payload”这种隐式成员。成员函数体内即使有 `self`，也不能写 `value`、`self.value`、`.some.value` 或按 payload 位置访问字段来读取 case 数据；必须通过 `match self { .case(payload) => ... }` 解开当前 case。`variant` 的 case 名只参与 `Type::case` 构造和 `match` pattern，不会在方法体里变成字段名、局部名字或可调用普通函数。

当前 `variant` 不会按当前 case 自动析构 payload。`variant` 可以携带有析构函数的 `struct` payload，类型检查不会因此拒绝，但局部 `variant` 离开作用域、`destroy_at(ptr_to_variant)` 或 `delete ptr_to_variant` 目前不会自动析构当前活跃 payload。需要拥有资源并依赖自动析构时，应把资源所有权放在有显式析构函数的 `struct` 中，或避免把资源直接放进 `variant` payload。

## 内存模型

`variant` 的运行时布局固定为：

```text
tag + payload storage
```

tag 是编译器内部整数，用来记录当前 case。

payload storage 是一块足够容纳最大 case payload 的存储区，对齐取所有 case payload 的最大对齐。

tag 使用足够容纳当前 case 集合的内部整数表示；源码不能读取、比较或依赖具体 tag 编码。

例如：

```cp
variant parse_result {
    ok(i32);
    error(str);
}
```

概念布局为：

```text
tag:
    0 means ok
    1 means error

payload:
    size  = max(size of i32, size of str)
    align = max(align of i32, align of str)
```

构造 `parse_result::ok(1)` 时：

```text
write tag = 0
write i32 value into payload
```

构造 `parse_result::error("bad")` 时：

```text
write tag = 1
write str value into payload
```

执行 `match` 时读取 tag，然后按对应 case 的 payload 类型读取数据。

`match` 会先求值被匹配表达式，再按 tag 选择第一个匹配 arm。被匹配值是 lvalue 时，payload 绑定可以直接引用原对象内部 payload；被匹配值是 rvalue 时，编译器会先保存被匹配值，再让 arm 内 payload 绑定引用保存后的值。源语言不能观察或依赖中间临时名、block 名或 tag 编号。

没有 `_` arm 时，语义穷尽检查负责保证合法源码覆盖所有 case；运行时出现无效 tag 属于内部状态错误，不是源语言可捕获分支。存在 `_` arm 时，第一个 `_` arm 作为 fallback，且不绑定 payload。

`match` 的结果按普通表达式规则产生：非 `unit` / 非 `!` 的 `match` 需要所有可正常完成 arm 统一出结果类型；`unit` arm 只执行副作用；所有 arm 都是 `!` 时，整个 `match` 为 `!`。泛型 `variant` 或参数包中的 `match` 在具体实例中按替换后的 case payload 类型检查，不共享动态 payload schema。

当前 tag 只参与 case 构造和 `match` 分派；不参与自动析构分派。不要把 `variant` 当作已经具备 RAII payload 析构的资源容器。

泛型 `variant` 的布局按具体实例计算。`optional<i32>`、`optional<str>`、`expected<i32,str>` 分别拥有自己的 tag 和 payload storage；不同实例之间不共享运行时布局要求。

## 和 Union 的关系

`variant` 不依赖用户可写的 `union`。

`variant` 采用 tag 加重叠 payload storage 的布局，并由编译器在 case 构造和 `match` 中维护当前活跃 case。这个 tag 目前只保证 case 分派和 payload 读取，不等同于用户可观察的安全 `union` 抽象，也不提供 payload 析构分派。

如果加入 `union`，它应该是无 tag 的底层特性，主要用于 FFI、内存复用或 bit reinterpretation。`union` 不参与 `variant` 设计。

## 不支持内容

`variant` 不支持：

- 用户可写 `union`。
- niche optimization。
- struct-like case。
- pattern guard。
- 嵌套解构 pattern。
- `ref` / `const` / 类型标注 pattern 绑定。
- 多个 `_` arm 的有意义运行时分派。
- payload 自动析构分发。
- 默认初始化。
- 向用户暴露 tag 数值。

当前已经覆盖声明、case 构造、`match` 穷尽检查、arm 类型统一、运行时分派和 payload 读取。尚未覆盖的核心语义是 payload 自动析构；这也是上面不支持列表中唯一会影响资源生命周期的限制。用户代码如果把需要析构的资源直接放进 `variant` payload，必须理解当前自动清理不会按 case 销毁该 payload。
