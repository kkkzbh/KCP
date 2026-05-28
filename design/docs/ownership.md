# 所有权、借用与移动

本文档记录 cp 的所有权前置语义：显式借用、显式移动、`like` const 转发、移动引用、特殊成员函数和能力推导。引用和指针的基础类型规则见 [type_system.md](type_system.md)，结构体构造、析构和成员函数见 [struct.md](struct.md)。

## 设计目标

cp 默认保持普通值语义：

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
- 同一个泛型函数以可写左值、const 左值和右值调用时生成不同实例签名，避免把 `T&`、`T const&` 和 `T move&` 混用。

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
- receiver 是可写时，`T like*` 视为 `T*`，`T like&` 视为 `T&`。
- receiver 是 const 时，`T like*` 视为 `T const*`，`T like&` 视为 `T const&`。
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

- `ref expr` 要求 `expr` 是可写左值，结果类型是 `T&`。
- `const ref expr` 要求 `expr` 是左值，结果类型是 `T const&`。
- `ref expr` 可以匹配 `T&` 参数，也可以退化匹配 `T const&` 参数。
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
- `move expr` 后，原 binding 进入 moved 状态。

进入 moved 状态后：

- 允许析构。
- 允许重新赋值，让 binding 重新获得有效值。
- 不允许读取字段、调用方法、再次 copy 或再次 move。

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

- `forward expr` 只允许作用于当前函数的 `forward&` 参数。
- 如果该参数由左值绑定，`forward value` 表现为普通可写左值。
- 如果该参数由 const 左值绑定，`forward value` 表现为只读左值。
- 如果该参数由临时值、函数返回值或 `move` 绑定，`forward value` 表现为 `move value`。
- 多次转发右值不绕过 moved-state 检查；需要继续转移仍然必须显式写 `forward`。

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

隐式生成规则：

- 默认构造函数：没有任何用户声明构造函数时隐式生成；所有字段可默认初始化时可用，否则隐式 delete。
- 析构函数：默认生成；用户写了 `~T()` 后使用用户版本。
- copy constructor：默认生成；所有字段可 copy construct 时可用，否则隐式 delete；用户声明或删除后不再生成。
- copy assignment：默认生成；所有字段可 copy assign 时可用，否则隐式 delete；用户声明或删除后不再生成。
- move constructor：默认生成；所有字段可 move construct 时可用，否则隐式 delete；语义是逐字段 move；用户声明或删除后不再生成。
- move assignment：默认生成；所有字段可 move assign 时可用，否则隐式 delete；语义是逐字段 move；用户声明或删除后不再生成。

`= delete` 是局部能力开关，不是属性，也不依赖继承。第一版只用于 copy/move 构造、copy/move 赋值和需要显式禁止的特殊 operator。

## copyable、movable 和 move_only

`copyable`、`movable` 和 `move_only` 是由特殊成员可用性推出的语言级 concept，而不是改变类型行为的标记。

建议定义：

```text
copyable:
  copy constructor 可用
  copy assignment 可用

movable:
  move constructor 可用
  move assignment 可用

move_only:
  movable
  not copyable
```

类型是否满足这些 concept 由编译器根据特殊成员函数计算。泛型可以使用这些 concept 约束公共 API：

```cp
push<T: movable>(value: T move&) {
}
```

第一版不支持 `not` 约束表达式时，`move_only` 可以作为编译器内建 concept 暴露。

## 第一版边界

第一版支持：

- 默认 copy。
- `T&`、`T const&`、`T like*`、`T like&`、`T move&` 和 `T forward&`。
- `ref expr`、`const ref expr`、`move expr`、`forward expr` 和 `const expr`。
- `= delete` 用于 copy/move 特殊成员。
- 根据特殊成员可用性推导 `copyable`、`movable` 和 `move_only`。

第一版不支持：

- 引用折叠。
- C++ 风格 `T&&` / `std::forward<T>`。
- `like` 传播 move。
- `T const move&`。
- `T const forward&`。
- `self forward&`。
- 默认非 Copy 类型自动 move。
- 把 `ref expr` 隐式复制成按值参数。
- 以属性或继承语法声明 move-only。

本文档不定义标准库所有权类型、动态缓冲区、容器或底层分配接口。它只给这些设计提供语言前提。
