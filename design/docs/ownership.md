# 所有权、借用与移动

本文档记录 KCP 的所有权前置语义：显式借用、显式移动、`like` const 转发、移动引用、特殊成员函数，以及这些能力当前能否作为 concept 约束使用。引用和指针的基础类型规则见 [type_system.md](type_system.md)，结构体构造、析构和成员函数见 [struct.md](struct.md)。

## 设计目标

KCP 默认保持普通值语义：

```cp
let b = a; // copy
```

资源所有权转移必须显式写出：

```cp
let b = move a;
```

第一版不采用 Rust 的“非 Copy 类型默认 move”，也不照搬 C++ 的 `T&&` / `std::forward<T>` 语法。语言区分四类明确操作：

```text
copy:    x
borrow:  ref x / const ref x
move:    move x
forward: forward x
```

## 语法总览

```text
Type            -> TypeBase TargetQualifier? TypeSuffix
TargetQualifier -> const | like | forward
TypeSuffix      -> *+ &?
                 | &
                 | move &
                 | forward &

BorrowExpr      -> ref Expression
                 | const ref Expression

MoveExpr        -> move Expression
ForwardExpr     -> forward Expression
ConstExpr       -> const Expression

SpecialMember   -> CopyConstructor
                 | CopyAssignment
                 | MoveConstructor
                 | MoveAssignment

DeletedMember   -> SpecialMember = delete ;
DefaultedMember -> Constructor = default ;
```

`move` 和 `ref` 在表达式位置是前缀操作。`ref` 已经用于局部声明中的引用 binding，例如 `let ref alias = value;`；表达式级 `ref value` 是同一词汇的扩展。

## 类型

引用和移动引用类型写作：

```cp
T&
T const&
T like*
T like**
T like*&
T like&
T move&
T forward&
```

含义：

- `T&` 是可写借用。
- `T const&` 是只读借用。
- `T like*`、`T like**`、`T like*&`、`T like&` 是 receiver-const 传播类型，具体由当前 `self like&` receiver 决定。
- `T move&` 是移动借用，表示被调用方可以接管资源。
- `T forward&` 是泛型转发引用，只能用于依赖类型或省略参数类型引入的隐藏类型参数。

`T move&` 是独立于 C++ `T&&` 的简化设计：

- 普通左值不能自动绑定到 `T move&`。
- `move x`、临时值和函数返回值可以绑定到 `T move&`。
- 没有引用折叠。
- 第一版不支持 `T const move&`。

`T forward&` 是独立于 C++ `T&&` 的显式转发设计：

- `T forward&` 可绑定可写左值、const 左值、临时值、函数返回值和 `move x`。
- 命名的 `T forward&` 参数在函数体内按左值使用。
- 继续传递原始值类别必须写 `forward value`。
- `T forward&...` 值参数包展开时，每个 pack 元素独立记录自己的转发类别；`template for` 中绑定到该元素的局部名也可以继续写 `forward name`。
- 同一个泛型函数以可写左值、const 左值和右值调用时生成不同实例签名，避免把 `T&`、`T const&` 和 `T move&` 混用。函数体语义检查使用物化后的参数类型：可写左值调用中的 `T forward&` 参数是 `T&`，const 左值调用中是 `T const&`，临时值、函数返回值和 `move x` 调用中是 `T move&`。但是 `forward value` 的合法性仍来自源码参数是否写成 `forward&`，普通 `T&`、`T const&` 或 `T move&` 参数即使物化形状相同，也不是 forward-capable binding。
- `forward` 只能形成 `T forward&`，不能形成 `T forward*`、`T forward**` 或 `T forward*&`。
- `T forward&` 的被引用类型必须是依赖类型，或来自省略参数类型引入的隐藏类型参数。`i32 forward&`、`[i32; 2] forward&` 这类具体类型转发引用会报 `invalid_type_argument`。
- `self forward&` 当前不支持；即使 parser 能读到这种 receiver 形态，语义层也会报 `self forward& is not supported`。需要转发 receiver 值类别时，当前应改成普通显式泛型参数或按 `self&` / `self const&` / `self move&` 分别声明。

`move&` 和 `forward&` 都不能和 `const` 或 `like` 组合。因此 `T const move&`、`T like move&`、`T const forward&` 和 `T like forward&` 都不是当前合法类型。

`T like` 是为成员函数和 concept requirement 准备的窄规则。它只传播 receiver 的 target constness，不传播 move，不推导基础类型，也不表示任意类型限定：

```cp
operator [](self like&, index: usize) -> T like&
{
    return ref data[index];
}

data(self like&) -> T like*
{
    return ptr;
}
```

上面的声明等价于同时提供：

```cp
operator [](self&, index: usize) -> T&
operator [](self const&, index: usize) -> T const&
```

规则：

- `like` 只允许出现在成员函数、operator 或 concept requirement 中。
- 使用 `like` 的函数必须有且只有一个 `self like&` receiver。
- 同一个函数签名中的所有 `like` 都绑定到这个 receiver。
- `self like&` 可以接受可写 receiver 和 const receiver。
- receiver 是可写时，每个 `like` 后缀按可写目标物化：`T like*` 视为 `T*`，`T like&` 视为 `T&`。
- receiver 是 const 时，每个 `like` 后缀按 const 目标物化：`T like*` 视为 `T const*`，`T like&` 视为 `T const&`。
- 多级指针和指针引用也按同一规则递归物化。`T like**` 在可写 receiver 下是可写的双重指针视图；在 const receiver 下，穿过两级解引用得到的 `T` 是只读目标，`**object.data()` 不能赋值。`T like*&` 返回的是随 receiver constness 物化后的指针字段引用；const receiver 下该引用是只读视图，不能通过它改写字段或最终目标。
- `like` 不接受 `self move&`，也不会产生 `T move&`。
- 函数体按可写 receiver 视图和 const receiver 视图分别检查；如果 const 视图下发生写入，则该 `like` 函数非法。

冒号后继续永远写类型：

```cp
read(value: T const&)
edit(value: T&)
at(self like&, index: usize) -> T like&
data(self like&) -> T like*
take(value: T move&)
```

不引入 `read(const ref value: T)`、`edit(ref value: T)` 或 `take(move value: T)` 这类参数声明语法。这样参数、返回类型、函数类型和类型别名保持同一套类型语法：

```cp
type reader = f(T const&) -> i32;
front(self&) -> T&;
```

## 显式借用表达式

表达式级借用写作：

```cp
ref value
const ref value
```

规则：

- `ref expr` 要求 `expr` 是左值。若 `expr` 是可写左值，结果类型是 `T&`；若 `expr` 是 const 左值，结果类型是 `T const&`。
- `const ref expr` 要求 `expr` 是左值，结果类型是 `T const&`。
- 可写左值上的 `ref expr` 可以匹配 `T&` 参数，也可以退化匹配 `T const&` 参数。
- const 左值上的 `ref expr` 结果已经是 `T const&`，只能匹配只读引用目标。
- `ref expr` 不能匹配按值 `T` 参数。
- `const ref expr` 只能匹配 `T const&` 参数。

示例：

```cp
foo_value(value: i32) {
}

foo_ref(value: i32&) {
}

foo_const_ref(value: i32 const&) {
}

main()
{
    let x = 1;
    foo_value(x);          // copy
    foo_ref(ref x);        // borrow
    foo_const_ref(ref x);  // writable borrow to readonly borrow
    foo_const_ref(const ref x);

    // foo_value(ref x);   // error: explicit borrow does not copy
}
```

对 const 左值写 `ref` 不会去掉只读性：

```cp
const value = 1;
foo_const_ref(ref value); // 合法：结果是 i32 const&
// foo_ref(ref value);    // 错误：不能把 const 左值借成 i32&
```

显式 `ref` 不会隐式读出并复制为值。需要传值时直接写原表达式；需要转移资源时写 `move expr`。

返回引用也使用同一套显式借用表达式：

```cp
front(self&) -> T&
{
    return ref data[0];
}
```

`return (x);` 不表示返回引用；括号只做普通分组。

## 显式移动表达式

移动写作：

```cp
move value
```

规则：

- `move expr` 要求 `expr` 是可写左值。
- `move expr` 的结果类型是 `T move&`。
- `move expr` 可以匹配 `T move&` 参数。
- `move expr` 可以匹配按值 `T` 参数，此时使用 `T` 的 move 构造产生参数值。
- `move expr` 不匹配 `T&` 或 `T const&` 参数。
- `move expr` 本身只改变表达式值类别，不立即调用 move constructor，也不销毁源对象。
  只有当它流向按值局部初始化、按值传参、按值返回、`construct_at` 的 value、
  或其它需要构造一个 `T` 值的目标时，才按目标类型选择 move 构造；流向 `T move&`
  参数时只是绑定移动借用。赋值表达式 `target = move source` 则按 `operator =`
  候选选择 move assignment，找不到可用 operator 时才走当前内建赋值路径。

当前实现的 `move` 是显式值类别标记和 copy/move constructor 选择信号，还没有接入 use-after-move 状态检查。因此：

- 对同一个 binding 多次写 `move value`，语义层不会因为“已经 move 过”而报错。
- `move value` 之后继续读取字段、调用方法或再次按值传递，语义层仍按普通表达式检查。
- 具体类型的 move constructor 可以把源对象置为某个有效但类型自定义的状态；例如标准库 `string` move 后当前运行时测试期望源串为空，但这是该类型实现约定，不是语言级 moved-state 规则。
- 需要在 API 边界禁止赋值，应优先使用 deleted `operator =` 并依赖调用点诊断；需要禁止显式构造，可以删除对应 copy/move constructor。当前 deleted copy/move constructor 还不能完整禁止所有隐式按值 copy/move，详细边界见“特殊成员能力与 concept 边界”。不能依赖当前编译器做通用 use-after-move 分析。

命名的 `T move&` 参数在函数体内按左值使用。如果需要继续转移，必须再次写 `move`：

```cp
take<T>(value: T move&) -> T
{
    return move value;
}
```

这不是完美转发，只是显式继续移动。

## 显式转发表达式

转发写作：

```cp
forward value
```

规则：

- `forward expr` 只允许作用于 forward-capable binding。
- 操作数必须解析到一个 forward-capable binding 的名字本身；允许普通括号分组，但不允许转发字段访问、下标访问、调用结果或任意表达式。
- forward-capable binding 包括当前函数的 `forward&` 参数，以及 `template for` 从 `T forward&...` 值参数包元素建立的局部绑定。
- 普通局部变量、按值参数、`T...` 值参数包展开变量、`T&...` / `T const&...` / `T move&...` 展开变量都不是 forward-capable binding。
- 如果该 binding 由左值绑定，`forward value` 表现为普通可写左值。
- 如果该 binding 由 const 左值绑定，`forward value` 表现为只读左值。
- 如果该 binding 由临时值、函数返回值或 `move` 绑定，`forward value` 表现为 `move value`。
- 当前没有通用 moved-state 检查；多次 `forward value` 的语义约束来自目标参数类型和具体 copy/move 构造可用性。需要继续保留原始右值类别时仍必须显式写 `forward value`。

```cp
relay<T>(value: T forward&)
{
    consume(forward value);
}
```

## 显式 const 表达式

`const value` 是表达式级只读视图：

- 不复制、不移动、不借用，只把表达式视为 const。
- 保留原值类别；左值仍是左值，临时值仍是临时值。
- `const ref value` 仍表示显式 const borrow；`const value` 不是 borrow，可以匹配按值参数。
- `const (move value)` 和 `move (const value)` 非法。

`return local;` 满足 NRVO 条件时直接返回该 local 的存储位置，不是隐式 move。`return move local;` 保持显式移动语义，并明确不触发 NRVO。完整 NRVO 条件见 [type_system.md](type_system.md) 的返回值消除规则。

## 默认 copy 和显式 move

普通赋值和按值传参默认使用 copy：

```cp
let b = a;
consume(a);
```

如果类型不可 copy，以上写法报错。用户必须写：

```cp
let b = move a;
consume(move a);
```

按值参数的匹配规则：

- 实参是普通表达式时，要求目标类型可 copy。
- 实参是 `move expr` 时，要求目标类型可 move。
- 实参是 `ref expr` 或 `const ref expr` 时，不匹配按值参数。

## 特殊成员函数

结构体拥有以下特殊成员：

```text
default constructor
destructor
copy constructor
copy assignment
move constructor
move assignment
```

copy 构造和 move 构造使用构造函数语法表达：

```text
impl handle {
    handle(other: this const&) = delete;
    handle(other: this move&);
}
```

copy 赋值和 move 赋值使用 `operator =` 表达：

```text
impl handle {
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) -> this&;
}
```

当前实现规则：

- 默认构造函数：没有任何用户声明构造函数时，`T{}` 走逐字段默认初始化；所有省略字段都可默认初始化时可用，否则报默认初始化失败。用户写了 `T() = default;` 时也表示同一逐字段默认初始化路径，但它是构造候选中的一个符号，deleted 或签名错误时按普通构造函数规则诊断。
- 析构函数：当前没有为无用户析构函数的 `struct` 生成可调用析构函数符号；这类类型在清理路径上等价于 no-op。用户写了 `~T()` 后使用用户版本。
- copy constructor：当前不会合成一个可枚举的默认 copy constructor 符号。显式构造表达式 `T(value)` 会按普通构造候选打分；如果选中的是 deleted 构造函数，语义层报 `not_callable`，并且不会退回字段聚合初始化。按值初始化、传参、返回、字段写入和 range-for 按值绑定中，如果源和目标的读出类型是同一个 `struct`，且存在非 deleted 的单参数引用构造函数 `T(other: this const&)` / `T(other: this&)`，当前会使用第一个匹配声明；如果源是 const，只能匹配 `this const&`，如果源不是 const，则 `this&` 和 `this const&` 都可匹配，实际使用声明顺序中的第一个非 deleted 匹配项。deleted copy constructor 目前只稳定阻止显式构造候选；隐式按值 copy 可能跳过 deleted 构造函数。如果没有可用用户构造函数，当前走内建同类型值复制。
- move constructor：同样不会合成默认 move constructor 符号。显式 `T(move value)` 选到 deleted move constructor 时按 deleted constructor 报错；但 `move value` 流向按值目标时，只寻找非 deleted 的单参数 `T(other: this move&)` 用户构造函数，并使用第一个匹配声明。deleted move constructor 目前只稳定阻止显式构造候选；隐式按值 move 可能跳过 deleted 构造函数。如果没有可用用户构造函数，当前走内建同类型值写入。`this const move&` 不是合法类型，普通左值也不会隐式选择 move 构造。语言层还没有递归检查“所有字段可 move construct”并据此隐式删除整个类型。
- copy / move assignment：赋值表达式先按普通 operator 重载查找 `operator =`。用户声明的 `operator =(self&, rhs: this const&)` 或 `operator =(self&, rhs: this move&)` 会作为候选参与选择，deleted 候选会让调用点报错。没有可用 `operator =` 时，当前退回到内建赋值：右侧只要能按目标类型隐式转换即可写入左侧；它不是一个可枚举的默认 assignment 符号，也不会逐字段调用用户自定义 assignment。

例如：

```cp
struct box {
    value: i32;
}

impl box {
    box(other: this const&) = delete;
}

main() -> i32
{
    let first = box{ .value = 42 };
    let second = first;  // 当前仍可走内建同类型复制
    return second.value;
}
```

上面这类 deleted copy constructor 只稳定禁止显式选择该构造候选的写法，例如 `let second = box{first};` 会报 deleted constructor 不可调用。它还不能作为“这个类型不可复制”的完整语言承诺；需要阻止 `left = right` 这类赋值时，应删除对应 `operator =`，而不是只删除 copy constructor。

`= delete` 是局部能力开关，不是属性，也不依赖继承。在所有权语义里它主要用于 copy/move 构造、copy/move 赋值和需要显式禁止的赋值 operator。普通 deleted impl 函数见 [struct.md](struct.md)，deleted operator 的候选选择规则见 [operator.md](operator.md)。

## 析构与清理路径

局部变量和按值参数会按作用域形成清理路径。当前实现只为非引用对象中可解析到 `struct` 析构函数的 binding 注册清理；引用 binding、裸指针、`storage T{}` 的原始槽位本身不触发被指向对象或槽内对象析构。

清理规则：

- 普通 block 结束时，按局部对象构造/绑定的逆序调用析构函数。
- `return` 离开当前函数前，会先清理当前作用域中仍然存活的局部对象。
- `break` 和 `continue` 跳出或进入下一轮循环前，会清理从当前位置到目标循环边界之间的局部对象。
- 按值函数参数和按值参数包元素会进入函数入口清理栈；引用参数和 `self&` 这类引用 receiver 不注册清理。
- 解构声明中，按值绑定的每个非引用元素分别拥有自己的清理；`let ref (a, b) = tuple;` 只绑定引用，不析构原 tuple 的元素。
- `for(let value : range)` 的按值循环变量按每一轮迭代注册和清理；`break` / `continue` 会先清理当前迭代中已经注册的对象，再跳到目标 block。`for(let ref value : range)` 不注册循环变量析构。
- `template for` 不是运行时循环；每次展开后的普通语句按当前展开作用域注册清理。`template for(let value : values...)` 的展开变量只是对应参数包元素的当前作用域名字，不额外 copy 构造一个新对象，也不单独注册析构；按值参数包元素的清理由函数入口参数 binding 负责。展开体正常结束时清到该展开开始前；展开体里 `return`、`break` 或 `continue` 按对应目标清理。
- `template if` 只实例化被选中的分支；被选中分支形成一个清理子作用域，未选中分支不注册任何清理。
- NRVO 返回的 local 是函数返回对象本身，callee 中不再对这个 returned local 调用析构；调用方接收后的对象按调用方生命周期析构。
- 手动 `construct_at` 到 `storage` 或裸指针中的对象，必须由用户配对 `destroy_at`。覆盖或离开 `storage` binding 不会自动析构槽内对象。
- 当前自动清理不会递归析构数组、tuple 或 `variant` 内部元素。需要递归析构数组元素时使用 `destroy_at(ptr_to_array)`；`variant` payload 的按 tag 析构分发尚未接入。
- `static` 局部变量不进入普通局部清理栈，也没有程序退出时的析构顺序保证。

## 特殊成员能力与 concept 边界

当前编译器已经在构造、赋值和清理路径中使用 copy/move 特殊成员可用性，但这些能力还没有作为可导入、可约束的公开 concept 暴露。

因此：

- 没有标准库或语言内建的 `copyable`、`movable`、`move_only` concept。
- 语义层不会把 `T: movable` 解释为“检查 move constructor 和 move assignment 可用”。如果没有用户自己声明并导入名为 `movable` 的普通 concept，它就是未知 concept。
- 即使用户声明了同名普通 concept，编译器也不会自动根据特殊成员函数给类型生成实现证明；仍需按普通 concept impl 规则提供 `impl movable for Type`。
- 当前 `requires` 也没有 `not` 约束，因此不能直接用 `movable and not copyable` 表达 move-only。
- 需要在当前实现中禁止赋值时，使用 deleted `operator =`，它会参与 operator 候选选择并在被选中时报错。deleted copy/move 构造函数当前只稳定阻止显式构造候选；隐式按值 copy/move 可能跳过 deleted 构造函数并落回内建同类型复制/写入，所以不要把它当作完整 move-only 类型系统。需要在泛型 API 中约束这种能力时，只能依赖已有普通 concept 或让具体调用点实例化后暴露特殊成员不可用的诊断。

## 第一版边界

第一版支持：

- 默认 copy。
- `T&`、`T const&`、`T like*`、`T like&`、`T move&` 和 `T forward&`。
- `ref expr`、`const ref expr`、`move expr`、`forward expr` 和 `const expr`。
- `= delete` 用于 copy/move 特殊成员，也可用于显式禁止的 impl 函数和 operator；所有权文档只展开特殊成员部分。

第一版不支持：

- 引用折叠。
- C++ 风格 `T&&` / `std::forward<T>`。
- `like` 传播 move。
- 具体类型 forward 引用，例如 `i32 forward&`；`forward&` 要求 pointee 是依赖类型，或来自省略参数类型的隐式泛型参数。
- `T const move&`。
- `T const forward&`。
- `self forward&`。
- 默认非 Copy 类型自动 move。
- 把 `ref expr` 隐式复制成按值参数。
- 以属性或继承语法声明 move-only。

本文档不定义标准库所有权类型、动态缓冲区、容器或底层分配接口。它只给这些设计提供语言前提。
