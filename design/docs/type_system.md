# 类型系统

本文档记录 cp 的核心类型规则。变量初始化写法见 [initial.md](initial.md)，结构体、构造函数、析构函数和成员函数见 [struct.md](struct.md)，`concept` 和 `type` 类型别名语句见 [concept.md](concept.md)。

类型系统包含内建标量类型、内部 `unit` 类型、数组、元组、结构体、variant、引用、指针、函数类型、函数指针、函数返回类型推导、默认初始化、显式转换、控制流条件规则和运算符类型规则。

`concept`、泛型、`variant`、lambda 和闭包的专门规则分别见 [concept.md](concept.md)、[generic.md](generic.md)、[variant.md](variant.md) 和 [lambda.md](lambda.md)。

## 类型分类

### 内建标量类型

```text
bool
i8 i16 i32 i64
u8 u16 u32 u64
isize usize
f32 f64
char
str
```

`bool` 和 `char` 当前按 1 字节基础类型处理。

`usize` 和 `isize` 是编译器内建的目标平台指针宽度整数类型：

- 32 位目标上，`usize` 等价宽度为 `u32`，`isize` 等价宽度为 `i32`。
- 64 位目标上，`usize` 等价宽度为 `u64`，`isize` 等价宽度为 `i64`。
- `usize` 用于对象大小、对齐、索引和地址宽度相关的非负值。
- `isize` 用于指针差值和可能为负的地址宽度相关偏移。
- 它们是不同的内建类型，不是标准库别名；具体宽度由编译目标决定。

字面量默认类型：

| 字面量           | 类型   |
| ---------------- | ------ |
| `true` / `false` | `bool` |
| 整数字面量       | `i32`  |
| 浮点字面量       | `f64`  |
| 字符字面量       | `char` |
| 字符串字面量     | `str`  |

字符串字面量支持 `\n` 等转义字符。`str` 表示字符串视图，语义上接近 C++ 的 `std::string_view`。

#### str 字符串视图

`str` 是内建的非拥有、只读、运行时长度字符序列视图。它不负责分配或释放底层存储，字符串字面量产生的 `str` 指向静态存储。

`str` 和 `array<char,N>` 不等价：

- `array<char,N>` 是拥有存储的固定长度数组，长度 `N` 是类型的一部分。
- `str` 是借用视图，长度是运行时值，不在类型中。
- `array<char,N>` 的元素在数组可写时可以通过下标写入；`str` 的下标结果只读。

`str` 提供基础内建操作：

```cp
let text: str = "hello";
let count = text.size();
let first = text[0];
```

规则：

- `s.size()` 返回 `u64`。
- `s[i]` 要求 `s` 的类型是 `str`。
- 下标 `i` 必须是整数类型。
- `s[i]` 的结果类型是 `char`。
- `s[i]` 不是左值，不能赋值。
- 越界行为定义为运行时错误；后端通过插入 bounds check 实现。
- 编译期常量下标访问字符串字面量时，如果能确定越界，语义分析可以报错。

`str` 不内建相等比较、大小比较、`empty`、`starts_with`、`contains`、切片或解析等高层能力。它们通过标准库 `impl str`、运算符协议或 [iteration.md](iteration.md) 中的 `iterable` 协议扩展。字符串范围遍历依赖 `str` 的 `iterable` 实现，而不是因为 `str` 拥有 `size()` 和 `[]` 自动成立。

### unit

内部存在 `unit` 类型，用于表示没有值的结果。它 lowered 到 LLVM IR 时是 `void`。

`unit` 当前不是用户可写类型，主要由以下场景产生：

- 没有带值 `return` 的函数。
- `return;`。
- 没有尾表达式的块表达式。
- 尾表达式后带分号的块表达式。

### 结构化类型

`array<T,N>` 表示固定长度数组：

```cp
let data: array<i32,4> = [1, 2, 3, 4];
```

`array` 是同构类型，所有元素具有统一元素类型，长度 `N` 是类型的一部分。

`array<T,N>` 直接作为公开数组类型，同时也是编译器 lowering 到后端数组类型的原语类型。不额外引入 `T[N]` 或 `int[N]` 这类底层数组语法。`array<T,N>` 始终保持同一个类型，标准库可通过普通 `impl array<T,N>` 提供高层成员能力。

`array<T,N>` 的类型参数规则：

- 第一个参数 `T` 必须是类型。
- 第二个参数 `N` 必须是非负整数编译期常量。
- `N` 是数组类型的一部分，`array<i32,3>` 和 `array<i32,4>` 是不同类型。
- `array<T,0>` 允许存在，但不能读取任何元素。

`tuple<T...>` 表示元组：

```cp
let triple: tuple<i32,f64,char> = (1, 0.5, 'x');
```

元组是异构类型。普通 `(x)` 是分组表达式，不是单元素元组。

`struct` 是名义类型。结构体规则见 [struct.md](struct.md)。

`variant` 是名义和类型，表示若干个 case 中恰好一个。variant 规则见 [variant.md](variant.md)。

### 函数类型与函数指针

函数类型写作：

```cp
f(i32, i32) -> i32
f(left: i32, right: i32) -> i32
```

`f(...) -> R` 表示非空函数值。普通命名函数和无捕获 lambda 可以绑定到函数类型。

函数指针类型写作：

```cp
f*(i32, i32) -> i32
f*(left: i32, right: i32) -> i32
```

`f*(...) -> R` 表示运行时函数地址，主要用于 C ABI 和底层回调。它接近 C/C++ 函数指针，可以默认初始化为空函数指针；调用空函数指针属于底层 unsafe 契约。

函数类型参数可以只写类型，也可以写参数名。参数名由 parser 识别并保存在 AST 中，用于诊断、文档和可读性；它不参与类型等价、ABI 或重载选择。

lambda、普通函数绑定、捕获和闭包规则见 [lambda.md](lambda.md)。

### decltype 类型表达式

`decltype(expr)` 是类型位置表达式，用于取得表达式的静态类型：

```cp
let x = 1;
type X = decltype(x);      // i32
type Y = decltype(x + 1);  // i32
```

`decltype(expr)` 返回 `expr` 被读取后的值类型，不保留 C++ 的 value category 细节。

```cp
let x = 1;
let ref r = x;
let p: i32* = &x;

type A = decltype(x);   // i32
type B = decltype(r);   // i32
type C = decltype(*p);  // i32
```

规则：

- `decltype(expr)` 只能出现在类型位置，例如类型别名、变量类型标注、函数返回类型、泛型实例类型实参或 `template for` body 中的类型位置。
- `expr` 只进行语义类型检查，不产生运行时代码，也不会执行副作用。
- `decltype(expr)` 返回表达式的静态读出类型，不保留左值、引用或可写性类别。
- 如果 `expr` 依赖泛型参数，`decltype(expr)` 可以形成依赖类型，等实例化后再确定。
- 不支持 C++ 的 `decltype(auto)`。
- 不支持 `decltype((x))` 这类通过额外括号保留引用类别的特殊规则。

`decltype` 与参数包配合时，可以取得 `template for` 当前展开值的类型：

```cp
debug<T...>(values: T...)
{
    template for(let value : values...) {
        type U = decltype(value);
        write_type_name<U>();
        write(value);
    }
}
```

## 函数返回类型

函数声明和定义写作：

```cp
add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

显式返回类型存在时：

- 所有 `return value;` 都必须能转换到声明返回类型。
- `return;` 只允许用于 `unit` 返回。

函数可以省略 `-> type`：

```cp
main()
{
    let x = 1;
}
```

省略返回类型时，语义分析收集函数体中的所有 `return value;` 并统一出返回类型。没有任何带值 `return` 时，函数返回类型推导为内部 `unit`。

构造函数不参与普通函数返回类型推导；构造函数返回类型固定为当前结构体类型，见 [struct.md](struct.md)。

## 默认初始化

默认初始化用于 `Type{}`、聚合初始化缺省字段、数组和元组元素递归初始化等场景。

| 类型 | 默认初始化结果 |
| --- | --- |
| `bool` | `false` |
| `i8 i16 i32 i64` | `0` |
| `u8 u16 u32 u64` | `0` |
| `isize` | `0` |
| `usize` | `0` |
| `f32 f64` | `0.0` |
| `char` | `'\0'` |
| `str` | `""` |
| `T*` | 空指针值 |
| `T&` | 不可默认初始化 |
| `f(...) -> R` | 不可默认初始化 |
| `f*(...) -> R` | 空函数指针值 |
| `array<T,N>` | 每个元素按 `T` 默认初始化 |
| `tuple<T...>` | 每个元素按对应元素类型默认初始化 |
| `struct` | 按结构体初始化规则默认初始化 |

如果某个类型不可默认初始化，那么依赖它的默认初始化也失败。例如引用字段没有显式初始化时，包含该字段的结构体不能完成默认初始化。

默认初始化只定义值如何产生，不改变 `let` / `const` 的初始化要求。需要默认值时，仍然必须显式写初始化表达式，例如：

```cp
let value: i32 = i32{};
let point = vec2{};
```

裸 `{}` 不表示默认初始化。裸块的表达式规则见 [struct.md](struct.md) 的块表达式章节。

数组默认初始化写作：

```cp
let values = array<i32,4>{};
```

`array<T,N>{}` 创建长度为 `N` 的数组，每个元素按 `T` 的默认初始化规则初始化。如果 `T` 不可默认初始化，则 `array<T,N>` 也不可默认初始化。不支持 `array<i32,3>{1, 2, 3}` 作为元素列表构造；元素列表统一使用数组字面量 `[1, 2, 3]`。

## 聚合字面量

数组字面量使用 `[ ... ]`：

```cp
let data: array<i32,4> = [1, 2, 3, 4];
```

有上下文类型时：

- 目标类型必须是 `array<T,N>`。
- 字面量长度必须等于 `N`。
- 每个元素按目标元素类型 `T` 做上下文检查。

没有上下文类型时：

- 非空数组字面量自行推导为 `array<T,N>`。
- `N` 为元素个数。
- `T` 由元素类型统一得到。
- 空 `[]` 报错，因为无法推导元素类型。

无上下文自行推导时，只允许同类数值提升：

- 整数族内部可以统一。
- 浮点族内部可以统一。
- 整数和浮点数不跨类统一，因此 `[1, 2.0]` 报错。

元组字面量使用 `(a,b,c)`。普通 `(x)` 是分组表达式。

结构体初始化使用 `type_name{ ... }`，见 [struct.md](struct.md)。它和数组字面量是不同语法，不引入 C++ 式 `initializer_list` 特权。

## 数组操作

数组提供基础内建操作：下标访问、默认初始化和范围遍历。`size`、`front`、`back`、`data`、`as_slice`、`fill`、`map`、`iter` 等高层能力由标准库提供。

### 下标访问

数组下标表达式写作：

```cp
let x = values[0];
values[1] = x + 10;
```

规则：

- 内建 `a[i]` 要求 `a` 的类型是 `array<T,N>`。
- 下标 `i` 必须是整数类型。
- `a[i]` 的结果类型是 `T`。
- 如果 `a` 是可写左值，则 `a[i]` 是可写左值。
- 如果 `a` 是 const 值或 const 引用，则 `a[i]` 只能读取，不能赋值。
- 编译期常量下标如果不在 `[0, N)` 范围内，语义分析报错。
- 非常量下标的越界行为定义为运行时错误；后端通过插入 bounds check 实现。

单独的 `a[]` 不是表达式。数组访问必须写成带下标的 `a[index]`。用户自定义类型可以通过 `operator []` 支持下标访问，见 [operator.md](operator.md)。

## 元组操作

元组提供基础内建操作：字面量构造、默认初始化、编译期下标访问和简单解构声明。`size`、`front`、`back`、`get<N>`、`apply`、`map`、`zip`、`fold`、比较和 hash 等高层能力由标准库提供。

### 编译期下标访问

元组下标表达式复用 `[]` 语法：

```cp
let pair: tuple<i32,f64> = (1, 2.0);
let first = pair[0];
let second = pair[1];
pair[0] = first + 1;
```

规则：

- `t[i]` 要求 `t` 的类型是 `tuple<T...>`。
- 下标 `i` 必须是编译期整数常量。
- `i` 必须在 `[0, tuple_length)` 范围内。
- `t[i]` 的结果类型是第 `i` 个元素类型。
- 如果 `t` 是可写左值，则 `t[i]` 是可写左值。
- 如果 `t` 是 const 值或 const 引用，则 `t[i]` 只能读取，不能赋值。

因为元组是异构类型，元组不支持运行时整数下标。运行时选择异构值应使用 `variant` 或标准库抽象，而不是元组下标。

### 简单解构声明

元组可以在 `let` / `const` 声明中做简单解构：

```cp
let pair = (1, 2.0);
let (count, ratio) = pair;
const (x, y, z) = (1, 2, 3);
let ref (first, second) = pair;
const ref (readonly_first, readonly_second) = pair;
```

规则：

- 解构目标必须是元组类型。
- 绑定个数必须和元组元素个数完全一致。
- 每个绑定的类型由对应元素类型推导。
- `let` 解构产生可重新赋值的 binding；`const` 解构产生不可重新赋值的 binding。
- 没有 `ref` 时，解构绑定是新 binding，不是引用别名。
- 带 `ref` 时，解构要求初始化表达式是左值，并为每个元素产生引用 binding。
- `let ref` 解构保留元素的 target const：可写元素得到 `T&`，只读元素得到 `T const&`。
- `const ref` 解构为每个元素产生只读引用，结果类型为 `T const&`。
- 只支持简单标识符列表，不支持嵌套解构、忽略绑定、剩余绑定、字段重命名或 pattern guard。

## 引用、指针与 const

引用和指针类型写作：

```cp
i32&
i32*
i32**
i32 const&
i32 const*
i32 const**&
i32*&
i32**&
```

`const` 分为 binding const 和 target const。

### binding const

`const name = ...` 或函数参数中的 `const name: T` 是 binding const，表示这个名字绑定不可重新赋值：

```cp
const value = 1;

view(const p: i32 const*) {
}
```

局部声明可以在 `let` / `const` 后添加 `ref`，表示引用 binding：

```cp
let value = 1;
let ref writable = value;
const ref readonly = value;
```

`let ref name = expr` 要求 `expr` 是左值，并按表达式的可写性推导为 `T&` 或 `T const&`。`const ref name = expr` 同样要求 `expr` 是左值，但结果总是只读引用 `T const&`。

### target const

`type const*` / `type const&` 中的 `const` 是 target const，表示指针或引用最终指向的基础值不可写。不管有多少级 `*` / `&`，它都约束最终目标值，而不是中间指针本身。

```cp
update(p: i32*) {
}

view(const p: i32 const*) {
}

borrow(value: i32 const&) {
}
```

类型语法约束为：

```text
Parameter   -> const? identifier : Type
Declaration -> (let | const) RefMode? BindingPattern (: Type)? = Expression
RefMode     -> ref
BindingPattern -> identifier | ( identifier (, identifier)* )

Type        -> TypeBase TargetConst? TypeSuffix
TargetConst -> const
TypeSuffix  -> *+ &? | &
```

`TargetConst` 只有在 `TypeSuffix` 非空时合法。因此 `i32 const`、`i32* const`、`i32& const` 都不是合法类型写法。

当前不支持 C++ 式指针自身 const，也不支持 `volatile` / `restrict`。

### 指针运算与解引用

指针算术、比较和解引用语义保持接近 C++。裸指针是底层 unsafe 能力，编译器只做类型检查，不完整证明对象生命周期、边界、悬垂或别名安全。

取址和解引用：

```cp
let value = 1;
let p: i32* = &value;
let x = *p;
*p = 2;
```

规则：

- `&expr` 要求 `expr` 是左值，结果类型为 `T*`。如果 `expr` 只能只读访问，结果类型为 `T const*`。
- `*ptr` 要求 `ptr` 是 `T*` 或 `T const*`。
- `*ptr` 的结果是左值。`T*` 解引用得到可写 `T` 左值；`T const*` 解引用得到只读 `T` 左值。
- 解引用空指针、悬垂指针、未对齐指针、未开始对象生命周期的存储或不满足目标类型别名规则的地址，属于底层 unsafe 契约。

指针算术：

```cp
let next = p + 1;
let prev = next - 1;
let distance: isize = next - p;
let item = p[0];
```

规则：

- `p + n`、`n + p` 和 `p - n` 要求 `p` 是 `T*`，`n` 是整数类型，结果为 `T*`。
- 指针加减的步长是 `T` 的元素大小，不是字节大小。
- `p2 - p1` 要求两侧都是相同目标类型的指针，结果类型为 `isize`，表示元素距离。
- `p[i]` 要求 `p` 是 `T*` 或 `T const*`，`i` 是整数类型，语义等价于 `*(p + i)`。
- `T*` 下标得到可写 `T` 左值；`T const*` 下标得到只读 `T` 左值。
- 不支持 `i[p]`。整数表达式不能作为指针下标目标。
- 指针差值只对同一数组对象或同一连续分配对象内的两个元素位置，以及 one-past 位置有定义。
- 产生或使用越出同一数组对象允许范围的指针值，行为按 C++ 指针算术边界处理；编译器不尝试静态证明。

指针比较：

- `p == q`、`p != q` 可比较相同目标类型的指针，结果为 `bool`。
- `< <= > >=` 可比较相同目标类型的指针，结果为 `bool`。
- 指针有序比较只对同一数组对象或同一连续分配对象内的位置有定义；其它来源的指针有序比较属于底层 unsafe 契约。

## 转换

显式转换有两种写法：

```cp
value as i32
i32(value)
```

两者都进入显式转换检查。

隐式转换只允许本文档明确说明的数值提升和上下文目标转换，不采用宽泛的 C++ 式隐式转换。

## 控制流类型规则

`if`、`while` 和 `do while` 的条件表达式必须是 `bool`。

`for` 当前仅支持范围形式：

```cp
for(let value : values) {
}
```

范围表达式的目标语义基于 [iteration.md](iteration.md)：表达式必须实现 `iterable` 或本身实现 `iterator`，循环变量类型为对应 `iter_item`。

`break` 和 `continue` 必须位于循环中；带 label 时，label 必须能解析到外层带标签的 `for`。

## 运算符

运算符优先级由 parser 的共享表定义，整体与 C++ 接近，但逻辑运算使用 `and` / `or` / `not`。

算术运算要求数值类型：

- 整数族内部、浮点族内部可以统一类型。
- 结果类型为统一后的类型。

比较运算结果为 `bool`。逻辑运算要求 `bool` 操作数，结果为 `bool`。

下标运算 `value[index]` 是后缀表达式。内建下标支持 `array<T,N>`、`tuple<T...>`、`str` 和指针；用户自定义类型可以通过 `operator []` 提供下标能力，具体规则见 [operator.md](operator.md)。

自增和自减支持前置 `++value` / `--value` 与后置 `value++` / `value--`：

- 操作数必须是可写左值。
- 不能作用于 `const` binding 或 const target。
- 只允许整数类型。
- 前置形式先更新并产生更新后的值，后置形式先产生旧值再更新。
- 表达式结果类型与操作数读出的类型一致，结果本身不是左值。

赋值规则：

- 赋值左侧必须是左值。
- 不能给 `const` binding 重新赋值。
- 复合赋值优先查找对应 `operator +=`、`operator -=` 等；没有用户定义 operator 时，内建数值、整数等类型按内建复合赋值规则检查，用户自定义类型不自动退化为 `left = left op right`。
