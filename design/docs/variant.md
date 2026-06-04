# Variant 类型

本文档记录 `variant` 的语言设计。`variant` 用来定义名义和类型，表示若干个 case 中恰好一个。它在语言语义上接近 Rust 的 `enum`，在运行时布局上使用安全 tagged union。

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

泛型 `variant` 可以参与泛型固有 `impl` 和泛型 concept `impl`。条件实现使用普通 `requires` 规则：

```cp
impl comparable for optional<T>
requires
    T: comparable
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

`variant` 支持任意数量 case。实现上每个 case 分配一个内部 tag 编号；case 数量增加只会线性增加 tag 分发表、穷尽检查集合、析构分支和后端 switch 分支。

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
let c = parse_expected::unexpected("invalid number");
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

case pattern 使用点号开头，表示当前 variant 类型下的 case：

```cp
match result {
    .ok(value) => value,
    .error(message) => 0,
}
```

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

`match` 每个分支的表达式结果必须能落到同一个结果类型。当前实现按目标类型检查，而不是做双向 common-type 搜索：

- 如果 `match` 处在有上下文类型的位置，例如 `let value: i64 = match ...`、`return match ...` 且函数已知返回类型为 `i64`、或把 `match` 作为某个 `i64` 参数传入调用，则每个非 `!` arm 都必须能隐式转换到该上下文类型。
- 如果没有上下文类型，第一个非 `!` arm 的表达式类型成为 `match` 结果类型；后续非 `!` arm 必须能隐式转换到这个类型。
- `!` arm 不决定结果类型；所有 arm 都是 `!` 时，整个 `match` 的类型为 `!`。
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

payload pattern 绑定是当前 arm 表达式里的局部名字。它不能带 `ref`、`const`、类型标注或嵌套解构；绑定类型就是该具体 case payload 替换泛型实参后的类型。不同 arm 可以使用同名绑定，互不冲突，也不会捕获或覆盖 arm 外的同名局部变量。

`_` 是通配 arm，不绑定 payload。当前实现按源码顺序检查 arm，并把第一个 `_` 当作兜底分支；后续 arm 仍会被语义检查，但运行时不可达。因此 `_` 应只写一次并放在最后。

如果某个 arm 的表达式类型为 `!`，它不决定 `match` 的结果类型；其它可正常完成的 arm 决定结果类型。所有 arm 都是 `!` 时，整个 `match` 的类型为 `!`。没有可返回值的普通 arm 时，结果类型按 `unit` 处理。

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

`variant` 可以拥有 `impl` 块。规则和 `struct` 的 `impl` 类似。

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

`variant` 不支持用户声明构造函数和析构函数。构造只能通过 case 构造器完成。析构由编译器根据当前 tag 自动生成。

## 内存模型

`variant` 的运行时布局固定为：

```text
tag + payload storage
```

tag 是编译器内部整数，用来记录当前 case。

payload storage 是一块足够容纳最大 case payload 的存储区，对齐取所有 case payload 的最大对齐。

tag 类型固定使用足够容纳实现上限的整数类型，建议使用 `u32`。因此 case 数量的实现上限是不超过 `u32` 可表示范围。后端可以在不改变语言语义的前提下，把小型 `variant` 的 tag 优化成更小整数。

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
    0 means value
    1 means unexpected

payload:
    size  = max(size of i32, size of str)
    align = max(align of i32, align of str)
```

构造 `parse_expected::value(1)` 时：

```text
write tag = 0
write i32 value into payload
```

构造 `parse_expected::unexpected("bad")` 时：

```text
write tag = 1
write str value into payload
```

执行 `match` 时读取 tag，然后按对应 case 的 payload 类型读取数据。

析构时读取 tag，只析构当前活跃 case 的 payload。

泛型 `variant` 的布局按具体实例计算。`optional<i32>`、`optional<str>`、`expected<i32,str>` 分别拥有自己的 tag 和 payload storage；不同实例之间不共享运行时布局要求。

## 和 Union 的关系

`variant` 不依赖用户可写的 `union`。

`variant` 是安全 tagged union。它有 tag，编译器知道当前活跃 case。

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
- 默认初始化。
- 向用户暴露 tag 数值。

推荐实现顺序：

```text
variant declaration
variant case syntax
variant constructor expression
match expression
match arm pattern
variant symbol table
case lookup
constructor argument checking
match exhaustiveness checking
match branch type unification
variant layout
case construction
tag switch
payload load
payload destruction
```
