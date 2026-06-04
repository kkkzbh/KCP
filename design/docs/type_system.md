# 类型系统

本文档记录 KCP 的核心类型规则。变量初始化写法见 [initial.md](initial.md)，结构体、构造函数、析构函数和成员函数见 [struct.md](struct.md)，所有权、借用和移动见 [ownership.md](ownership.md)，`concept` 和 `type` 类型别名语句见 [concept.md](concept.md)。

类型系统包含内建标量类型、内部 `unit` 类型、`!` never type、数组、元组、结构体、强类型 `enum`、opaque alias、variant、引用、指针、函数类型、函数指针、函数返回类型推导、默认初始化、显式转换、控制流条件规则和运算符类型规则。

`concept`、泛型、`enum`、opaque alias、`variant`、lambda 和闭包的专门规则分别见 [concept.md](concept.md)、[generic.md](generic.md)、[enum.md](enum.md)、[opaque_alias.md](opaque_alias.md)、[variant.md](variant.md) 和 [lambda.md](lambda.md)。

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
| `nullptr`        | 空指针字面量，需上下文确定为某个 `T*` |

字符串字面量支持 `\n` 等转义字符。`str` 表示字符串视图，语义上接近 C++ 的 `std::string_view`。

#### str 字符串视图

`str` 是编译器认识的标准库字符串视图类型，语义字段等价于 `ptr: char const*` 和 `len: usize`。它非拥有、只读、带运行时长度，不负责分配或释放底层存储；字符串字面量产生的 `str` 指向静态存储。

字符串字面量的底层静态存储可以额外带一个 trailing `'\0'` 以兼容 C ABI，但 `'\0'` 不参与 `str` 的长度语义。`"a\0b".size()` 的结果是 `3`，遍历也会访问中间的 `'\0'`。

`str` 和 `[char; N]` 不等价：

- `[char; N]` 是拥有存储的固定长度数组，长度 `N` 是类型的一部分。
- `str` 是借用视图，长度是运行时值，不在类型中。
- `[char; N]` 的元素在数组可写时可以通过下标写入；`str` 的下标结果只读。

`str` 提供基础内建操作：

```cp
let text: str = "hello";
let count = text.size();
let pointer = text.data();
let first = text[0];
```

规则：

- `s.ptr` 是 `char const*`，指向第一个字符。
- `s.len` 是 `usize`，保存运行时长度。
- `s.size()` 返回 `usize`，等价于 `s.len`。
- `s.data()` 返回 `char const*`，等价于 `s.ptr`。
- `s[i]` 要求 `s` 的类型是 `str`。
- 下标 `i` 必须是整数类型。
- `s[i]` 的结果类型是 `char`。
- `s[i]` 不是左值，不能赋值。
- 越界行为定义为调用者违反前置条件；checked 第一版通过 `assert` 触发 panic。
- 编译期常量下标访问字符串字面量时，如果能确定越界，语义分析可以报错。
- 可以显式构造 `str{ .ptr = p, .len = n }`，调用者负责保证 `[p, p + n)` 指向有效只读字符序列；是否 trailing nul 不是 `str` 的不变量。

`str` 不内建 `empty`、`starts_with`、`contains`、切片或解析等高层能力。它们通过标准库 `impl str`、运算符协议或 [iteration.md](iteration.md) 中的 `iterable` 协议扩展。标准库为 `str` 提供 `operator <=>` 和 `== != < <= > >=`，比较规则是按 `char` 序列做字典序比较；公共前缀相同后，短串为 `less`，长串为 `greater`，长度相同为 `equivalent`。比较始终使用 `str.len`，不会按 C 字符串在中间 `'\0'` 截断。字符串范围遍历依赖 `str` 的 `iterable` 实现，而不是因为 `str` 拥有 `size()` 和 `[]` 自动成立。

#### string 拥有字符串

`string` 是标准库提供的拥有型字符串，底层通过 `string_storage` 管理字符存储和 trailing `'\0'` 物理容量。它不是内建标量类型；编译器只需要理解 `str` 视图和字符串字面量，拥有、扩容和可变操作由标准库 `std.text.string` 实现。

`string` 的第一版语义：

- `string{}` 构造空字符串。
- `string{text}` 从 `str` 构造拥有副本；字符串字面量类型仍是 `str`，因此常用写法是 `string{"text"}`。
- copy 构造和 copy 赋值做深拷贝；move 构造和 move 赋值转移底层存储，并把源对象长度置为 0。
- `str` 不会隐式转换成 `string`，`as` 也不是构造钩子；`"text" as string` 不合法。需要拥有副本时显式写 `string{"text"}` 或 `string{some_str}`。
- `size()` 返回有效字符数，不包含 trailing `'\0'`。
- `capacity()` 返回可存放的有效字符数，不包含 trailing `'\0'` 预留位。
- `data()` 返回底层连续存储指针；普通对象返回 `char*`，const 对象返回 `char const*`。
- 底层存储始终维护一个 trailing `'\0'`，因此 `data()` 可作为 C++11 风格字符串数据入口使用。
- 不提供 `c_str()`；需要裸指针时直接使用 `data()`。
- `as_str()` 返回借用视图 `str`，长度等于 `size()`，不拥有底层存储。
- 下标、`front()` 和 `back()` 使用 `assert` 表达当前长度前置条件；checked 第一版失败时 panic。
- `push_back()`、`pop_back()`、`resize(new_size, ch)`、`append(str)` 和 `clear()` 都保持 trailing `'\0'` 不变量。
- `string` 实现 `iterable` 和 `const_iterable`，可写迭代元素为 `char&`，只读迭代元素为 `char const&`。
- `string` 实现 `contiguous_mutable_range`，可作为标准库需要连续可写字符区间的输入。

`string` 不是 `str` 的替代品：`str` 表达借用和字面量视图，`string` 表达拥有和可变缓冲区。需要传递只读文本时优先使用 `str` 参数；需要保存或修改文本时使用 `string`。

### unit 和 `!`

内部存在 `unit` 类型，用于表示没有值的结果。它 lowered 到 LLVM IR 时是 `void`。

`unit` 不是普通用户可写类型，主要由以下场景产生：

- 没有带值 `return` 的函数。
- `return;`。
- 没有尾表达式的块表达式。
- 尾表达式后带分号的块表达式。

返回类型位置可以写 `void`，它在语义上等同于内部 `unit`：

```cp
touch<T>(value: T) -> void
{
    return;
}
```

`void` 只允许表达“函数没有返回值”，不能作为局部变量类型、字段类型、容器元素类型或泛型实参使用。

`!` 是 never type，表示表达式不会正常产生值。`panic(...)`、`unreachable()` 和只产生发散控制流的表达式使用 `!`。`!` 可以隐式转换到任意类型；任意普通类型不能转换到 `!`。详细错误处理规则见 [error_handling.md](error_handling.md)。

### 结构化类型

`[T; N]` 表示固定长度数组：

```cp
let data: [i32; 4] = [1, 2, 3, 4];
```

固定数组是同构类型，所有元素具有统一元素类型，长度 `N` 是类型的一部分。它内联拥有存储，不退化为指针，编译器 lowering 到后端数组类型。

`[T; N]` 的类型参数规则：

- 第一个参数 `T` 必须是类型。
- 第二个参数 `N` 必须是非负整数编译期常量。
- `N` 是数组类型的一部分，`[i32; 3]` 和 `[i32; 4]` 是不同类型。
- `[T; 0]` 允许存在，但不能读取任何元素。
- 泛型函数可以声明 `N: usize` 或 `N: isize` 这样的整数 const 参数，并在数组类型中写 `[T; N]`。

`storage T` 和 `storage [T; N]` 表示内联原始存储：

```cp
let one = storage i32{};
let many = storage [i32; 4]{};
```

storage 类型只保证大小和对齐，不表示其中已经存在 `T` 对象。它默认初始化时不调用 `T{}`，析构时不调用 `T` 析构函数，也不支持普通数组索引。对象生命周期必须通过 `construct_at(storage.slot(...), value)` 和 `destroy_at(storage.slot(...))` 显式管理。const storage 只能取到 `T const*`，适合读取已经构造的 slot，不能用于构造或析构 slot。

storage 是普通值类型，可以作为字段、局部变量、参数、返回值和泛型实参使用。可写 storage binding 支持整体赋值：

```cp
let slots = storage [i32; 2]{};
slots = storage [i32; 2]{};
```

整体赋值只复制原始 storage 值，不为其中可能存在的对象调用赋值、构造或析构。已经在 storage slot 中手动构造对象的代码，必须在覆盖 storage 前自行维护对象生命周期。

`(T1, T2)` 表示元组类型：

```cp
let triple: (i32, f64, char) = (1, 0.5, 'x');
let single: (i32,) = (1,);
```

元组是异构类型。普通 `(T)` 是类型分组，普通 `(x)` 是分组表达式；一元 tuple 必须写成 `(T,)` / `(x,)`。

`struct` 是名义类型。结构体规则见 [struct.md](struct.md)。

`enum` 是名义整数类型。case 通过 `Type::case` 访问，不隐式转整数；规则见 [enum.md](enum.md)。

`type A = opaque T` 是名义封装类型，layout/ABI 与底层 `T` 一致，但不继承底层操作；规则见 [opaque_alias.md](opaque_alias.md)。

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

函数类型是普通类型表达式，可以出现在局部类型别名、函数返回类型、参数类型、泛型类型实参、`requires` 约束和 `std.meta` 查询中：

```cp
type unary = f(i32) -> i32;
type result = call_result<f(i32, bool) -> i32, i32, bool>;

make<F, Args...>() -> call_result<F, Args...>
requires
    F: callable<Args...>
{
    return 0;
}

main() -> i32
{
    return make<f(i32, bool) -> i32, i32, bool>();
}
```

在类型实参列表里，`f(i32, bool) -> i32` 仍然按完整函数类型解析，`->` 属于函数类型的一部分，不结束外层泛型调用。`f(...) -> R` 作为类型实参只表示类型，不会创建 lambda 或函数值；需要值时仍要传入普通函数名、lambda 或闭包对象。

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

显式借用表达式是例外：`decltype(ref expr)` 和 `decltype(const ref expr)` 保留引用类型。这给成员函数和泛型代码提供查询当前借用形态的方式。

```cp
let x = 1;

type R = decltype(ref x);        // i32&
type C = decltype(const ref x);  // i32 const&
```

`forward` 表达式也是例外：在泛型函数体内，`decltype(forward value)` 保留当前实例中 `forward value` 的引用或移动引用形态。标准库 ranges 使用它区分可写左值、const 左值和右值。

规则：

- `decltype(expr)` 只能出现在类型位置，例如类型别名、变量类型标注、函数返回类型、泛型实例类型实参或 `template for` body 中的类型位置。
- `expr` 只进行语义类型检查，不产生运行时代码，也不会执行副作用。
- `decltype(expr)` 返回表达式的静态读出类型，不保留左值、引用或可写性类别。
- `decltype(ref expr)` 的结果是 `T&`。
- `decltype(const ref expr)` 的结果是 `T const&`。
- `decltype(forward name)` 的结果按当前实例的 forward 绑定类别保留引用或移动引用形态；`name` 必须是当前函数的 `forward&` 参数。
- `self` 在成员函数体中是普通 receiver 变量，因此 `decltype(ref self)` 合法。
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
- `return;` 只允许用于 `void` / 内部 `unit` 返回。

函数可以省略 `-> type`：

```cp
main()
{
    let x = 1;
}
```

省略返回类型时，语义分析收集函数体中的所有 `return` 并统一出返回类型。`return value;` 贡献 `value` 的表达式类型，`return;` 贡献内部 `unit`。函数体中没有任何 `return` 时，返回类型同样推导为 `unit`。

返回类型推导不通过额外括号保留引用。`return (x);` 与 `return x;` 等价，都会按普通表达式读出规则推导返回类型。需要返回引用时，显式写 `return ref x;` 或 `return const ref x;`，借用表达式规则见 [ownership.md](ownership.md)。

当前实现的推导规则：

- 可以从当前函数之后声明的函数调用继续推导；`main() { return f(); } f() { return 1; }` 合法。
- 已显式声明返回类型的递归函数不需要推导；`f() -> i32 { return f(); }` 合法。
- 没有显式返回类型的直接递归或互递归函数不能只靠自身调用推出类型，会报 `cannot_infer_return_type`。
- 多个正常返回观察值必须能统一。相同类型直接统一；整数族按公共整数类型统一；浮点族按更高 rank 的浮点类型统一；不同族、不同名义类型或 `unit` 与普通值类型混用不能统一。
- `return;` 作为 `unit` 参与统一。只有全部正常返回都是 `unit`，或者函数体没有任何 `return` 时，才推导为 `unit`。`if(flag) { return; } return 1;` 不能推导成 `i32`，会报 `cannot_infer_return_type`。
- `if`、`while`、`do while`、范围 `for`、`match`、`template for` 展开体和被选中的 `template if` 分支中的 `return` 都参与推导。
- 未被选中的 `template if` 分支不参与推导，也不要求其中的名字和类型有效。
- `!` never 表达式不决定普通返回类型；例如 `match` 的一个分支返回 `panic(...)`，其它分支返回 `i32` 时，整体可推导为 `i32`。如果所有观察到的返回都是 `!`，则函数返回类型推导为 `!`。
- 关联函数调用和成员访问按普通语义检查后参与推导；例如 `return box::make(1);` 可以推导为 `box`。
- `return ref x;` 和 `return const ref x;` 才能推导引用返回；普通 `return x;` 读取出值类型。

泛型函数省略返回类型时，推导发生在每个具体函数实例上，而不是为泛型定义本身产生一个全局返回类型。也就是说，先确定类型实参、替换参数类型、选择 `template if` 分支并展开 `template for`，再收集该实例中的 `return`。固有 `impl` 或 concept `impl` 中的泛型成员函数同样按具体成员函数实例推导；`impl<T> box<T> { value_or(self const&, fallback: T) { ... } }` 在 `box<i32>` 上调用时推导的是该 `i32` 实例的返回类型。

如果当前实例的返回表达式依赖另一个省略返回类型的泛型函数调用，编译器先推导被调泛型函数的具体实例返回类型，再把这个类型用于当前表达式检查。这个顺序对泛型 `variant` 的 `match` 尤其重要：被调函数可以先推导出 `choice<T>` 这样的具体 variant 类型，随后当前函数的 `match` 才能把 `.case(payload)` 绑定成替换后的 payload 类型，并用 arm 表达式继续推导当前函数返回类型。

参数包相关规则：

- `template for` 展开体中的 `return` 按具体展开后的语句参与推导；`return;` 仍贡献 `unit`。
- 值参数包或类型参数包为空时，展开体执行零次，因此展开体内的 `return` 不贡献返回类型。若函数只有一个空展开且没有其它 `return`，则该实例返回 `unit`；如果后面有 fallback `return 0;`，则按 fallback 推导。
- `match` 出现在 `template for` 内时，先按当前 pack 元素替换泛型 `variant` payload 类型，再按普通 `match` arm 类型统一规则推导表达式类型。
- 同一个具体函数实例中，多个展开产生的返回类型仍必须统一；例如 `first<T...>(values: T...) { template for(let value : values...) { return value; } }` 只有在该实例所有 `value` 的读出类型能统一时才合法。
- 泛型 lambda 当前不做返回类型推导，必须显式写 `-> R`，见 [lambda.md](lambda.md)。

### 返回值消除与目标位置初始化

语言保证一类 NRVO（named return value optimization）：当 `return local;` 满足下面条件时，`local` 直接作为函数返回对象本身，不发生语言级 copy 或 move。普通分组括号会被剥掉，因此 `return (local);` 与 `return local;` 等价。

第一版 NRVO 条件：

- `local` 必须是当前函数、lambda 或构造函数体内的非引用 local binding。
- `local` 不能是参数、`self`、捕获变量、字段、索引结果、函数调用结果或引用别名。
- `return` 表达式不能是 `move local`、`ref local`、`const ref local`，也不能是成员访问、索引、调用或字面量。
- `local` 的读出类型必须等于声明/推导后的函数返回读出类型；如果需要类型转换，则不走 NRVO。
- 同一函数中所有带值 `return` 必须返回同一个 NRVO candidate；否则回退普通 `return` 规则。

`move` 是显式所有权转移。写 `return move local;` 表示用户选择移动，并明确放弃 NRVO。

局部变量声明的初始化另有目标位置直接初始化规则。`let a = func();`、`let b = [i32; 3]{}`、数组/元组/结构体聚合字面量和构造表达式会优先直接写入 `a` / `b` 的存储位置，不构造“临时对象再拷贝到局部变量”。这不是 NRVO，因为它不发生在 `return local;` 上，但同样属于语言级 copy/move 消除。

赋值语句不属于目标位置直接初始化。`a = func();` 仍按普通赋值和转换规则处理。

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
| `[T; N]` | 每个元素按 `T` 默认初始化 |
| `(T1, T2)` | 每个元素按对应元素类型默认初始化 |
| `struct` | 按结构体初始化规则默认初始化 |

如果某个类型不可默认初始化，那么依赖它的默认初始化也失败。例如引用字段没有字段缺省表达式或显式初始化时，包含该字段的结构体不能完成默认初始化。

默认初始化只定义值如何产生，不改变 `let` / `const` 的初始化要求。需要默认值时，仍然必须显式写初始化表达式，例如：

```cp
let value: i32 = i32{};
let point = vec2{};
```

裸 `{}` 不表示默认初始化。裸块的表达式规则见 [struct.md](struct.md) 的块表达式章节。

数组默认初始化写作：

```cp
let values = [i32; 4]{};
```

`[T; N]{}` 创建长度为 `N` 的数组，每个元素按 `T` 的默认初始化规则初始化。如果 `T` 不可默认初始化，则 `[T; N]` 也不可默认初始化。不支持 `[i32; 3]{1, 2, 3}` 作为元素列表构造；元素列表统一使用数组字面量 `[1, 2, 3]`。

## 聚合字面量

数组字面量使用 `[ ... ]`：

```cp
let data: [i32; 4] = [1, 2, 3, 4];
```

有上下文类型时：

- 目标类型必须是 `[T; N]`。
- 字面量长度必须等于 `N`。
- 每个元素按目标元素类型 `T` 做上下文检查。

没有上下文类型时：

- 非空数组字面量自行推导为 `[T; N]`。
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

数组是普通值类型。可写数组 binding 支持整体赋值，右侧必须能转换为完全相同的 `[T; N]`：

```cp
let values = [1, 2];
values = [3, 4];
```

数组长度是类型的一部分，因此 `[i32; 2]` 不能整体赋值为 `[i32; 3]`。数组不会退化为指针；需要指针时显式取址或使用标准库 view/slice。

元组同样暂时保持内建或编译器认识的异构聚合，因为当前用户层还不能表达 pack 字段 `fields: T...`。未来支持 pack field expansion 后，可以再考虑迁移为 compiler-recognized std type。

### 下标访问

数组下标表达式写作：

```cp
let x = values[0];
values[1] = x + 10;
```

规则：

- 内建 `a[i]` 要求 `a` 的类型是 `[T; N]`。
- 下标 `i` 必须是整数类型。
- `a[i]` 的结果类型是 `T`。
- 如果 `a` 是可写左值，则 `a[i]` 是可写左值。
- 如果 `a` 是 const 值或 const 引用，则 `a[i]` 只能读取，不能赋值。
- 编译期常量下标如果不在 `[0, N)` 范围内，语义分析报错。
- 非常量下标的越界行为定义为调用者违反前置条件；checked 第一版通过 `assert` 触发 panic。

单独的 `a[]` 不是表达式。数组访问必须写成带下标的 `a[index]`。用户自定义类型可以通过 `operator []` 支持下标访问，见 [operator.md](operator.md)。

## 元组操作

元组提供基础内建操作：字面量构造、默认初始化、编译期字段访问和简单解构声明。`size`、`front`、`back`、`get<N>`、`apply`、`map`、`zip`、`fold`、比较和 hash 等高层能力由标准库提供。

元组是普通值类型。可写元组 binding 支持整体赋值，右侧必须能转换为相同 arity、逐元素兼容的元组类型：

```cp
let pair = (1, 2);
pair = (3, 4);
```

不同长度的元组不是同一类值；`(i32, i32)` 不能整体赋值为 `(i32, i32, i32)`。

### 编译期字段访问

元组字段访问使用 `.0` / `.1` 语法：

```cp
let pair: (i32, f64) = (1, 2.0);
let first = pair.0;
let second = pair.1;
pair.0 = first + 1;
```

规则：

- `t.N` 要求 `t` 是元组类型。
- 字段编号 `N` 必须在 `[0, tuple_length)` 范围内。
- `t.N` 的结果类型是第 `N` 个元素类型。
- 如果 `t` 是可写左值，则 `t.N` 是可写左值。
- 如果 `t` 是 const 值或 const 引用，则 `t.N` 只能读取，不能赋值。

因为元组是异构类型，元组不支持运行时整数下标。运行时选择异构值应使用 `variant` 或标准库抽象，而不是元组字段访问。

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
- 不支持 `static` 解构声明。
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
i32 like*
i32 like**&
i32 like&
i32 move&
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

表达式位置也可以显式借用：

```cp
foo(ref value);
bar(const ref value);
```

`ref expr` 和 `const ref expr` 的匹配规则见 [ownership.md](ownership.md)。

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

Type            -> TypeBase TargetQualifier? TypeSuffix
TargetQualifier -> const | like | forward
TypeSuffix      -> *+ &? | & | move & | forward &
```

`TargetQualifier` 只有在 `TypeSuffix` 非空时合法。因此 `i32 const`、`i32 like`、`i32* const`、`i32& const` 都不是合法类型写法。

`like` 是 receiver-const 转发限定符，可以写作 `T like*`、`T like**`、`T like*&` 或 `T like&`。它只负责把当前 `self like&` receiver 的 constness 转发到对应指针/引用目标，不表达 move，也不改变基础类型。`move &` 只允许写作 `T move&`，表示移动引用；`forward &` 只允许写作依赖类型上的 `T forward&`，表示泛型转发引用。第一版不允许 `T const move&`、`T like move&` 或 `T const forward&`。具体规则见 [ownership.md](ownership.md)。当前不支持 C++ 式指针自身 const，也不支持 `volatile` / `restrict`。

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

显式转换只有 `as` 一种写法：

```cp
value as i32
```

显式转换的完整白名单见 [cast.md](cast.md)：已可隐式转换的类型、数值互转、指针到指针、`enum` 到底层整数，以及 opaque alias 定义模块内部的底层互转。

隐式转换只允许本文档明确说明的数值提升和上下文目标转换，不采用宽泛的 C++ 式隐式转换。

## 控制流类型规则

`if`、`while` 和 `do while` 的条件表达式必须是 `bool`。

`for` 当前仅支持范围形式：

```cp
for(let value : values) {
}
```

范围表达式的目标语义基于 [iteration.md](iteration.md)：表达式必须是内建数组、实现 `iterable`，或在只读上下文中实现 `const_iterable`。实现 `iterator` 的游标本身不能直接作为 range-for 的范围表达式。

range-for 的绑定推导与声明绑定保持同一方向：没有 `ref` 时按值绑定，`const` 只约束循环变量本身；显式 `ref` 时才保留 iterator item 的引用语义。也就是说 `for(const value : values)` 的 `value` 是 `read_type(iter_item)` 的 const 值，而 `for(const ref value : values)` 才是只读引用。

`break` 和 `continue` 必须位于循环中；带 label 时，label 必须能解析到外层带标签的 `for`。

## 运算符

运算符优先级由 parser 的共享表定义，整体与 C++ 接近，但逻辑运算使用 `and` / `or` / `not`。

算术运算要求数值类型：

- 整数族内部、浮点族内部可以统一类型。
- 结果类型为统一后的类型。

比较运算结果为 `bool`。逻辑运算要求 `bool` 操作数，结果为 `bool`。

下标运算 `value[index]` 是后缀表达式。内建下标支持 `[T; N]`、`str` 和指针；用户自定义类型可以通过 `operator []` 提供下标能力，具体规则见 [operator.md](operator.md)。

自增和自减支持前置 `++value` / `--value` 与后置 `value++` / `value--`：

- 操作数必须是可写左值。
- 不能作用于 `const` binding 或 const target。
- 内建路径只允许整数类型。
- 非内建类型通过 `operator prefix ++` / `operator postfix ++`、`operator prefix --` / `operator postfix --` 参与重载解析；声明时必须写出 `prefix` 或 `postfix`。
- 前置形式先更新并产生更新后的自身引用；内建前置结果为 `T&`。
- 后置形式先产生旧值再更新；内建后置结果为 `T`，结果本身不是左值。

赋值规则：

- 赋值左侧必须是左值。
- 不能给 `const` binding 重新赋值。
- 复合赋值优先查找对应 `operator +=`、`operator -=` 等；没有用户定义 operator 时，只尝试内建二元运算再写回左侧，例如数值、整数位运算和指针加减整数。用户自定义类型不自动退化为 `left = left op right`。
