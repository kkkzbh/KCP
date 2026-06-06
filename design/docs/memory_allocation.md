# 底层内存分配

本文档记录 KCP 的底层内存分配与释放设计。语言提供四个最小原语，并在其上提供 `new` / `delete` 语法糖；不引入 `new[]` 或 `delete[]`。

`raw_buffer<T>`、`span<T>`、`vector<T>`、`string` 等内存相关标准库类型建立在这些原语之上。

## 设计目标

底层内存能力分为两件事：

```text
原始内存生命周期: alloc / free
对象生命周期: construct_at / destroy_at
```

`alloc` 和 `free` 只处理字节存储，不负责对象构造和析构。

`construct_at` 和 `destroy_at` 只处理对象生命周期，不负责申请和释放底层存储。

固定大小的内联原始存储使用 `storage T` / `storage [T; N]`。它和 `alloc<T>(count)` 一样只提供存储，不自动构造或析构 `T`。

这样释放内存时不需要区分单个对象和数组，也不会出现 C++ 风格的 `delete` / `delete[]` 二选一问题。

`new` 和 `delete` 只面向“一个对象指针”。`[T; N]` 自身就是一个对象类型，因此 `new [T; N]{...}` 返回的是指向数组对象的指针，`delete` 也只是删除这个数组对象，不需要另一套数组 delete 规则。

## 原语

提供四个编译器内建原语：

```cp
alloc<T>(count) -> T*
free(ptr)

construct_at(ptr, value)
destroy_at(ptr)
```

这些原语语法上表现为全局函数，不使用 `mem::` 前缀。它们不是普通用户库函数，而是编译器识别的 builtin。

类型实参规则：

- `alloc<T>(count)` 必须且只能写一个显式类型实参。
- `alloc` 的类型实参必须是类型，不能是值、名字占位或参数包展开。
- `free`、`construct_at`、`destroy_at` 不接受显式类型实参；目标类型都从指针参数推导。
- 四个原语只按这些 builtin 规则解析，不参与普通函数重载、UFCS 或用户自定义替换。
- `alloc<T>(count)` 的结果类型是 `T*`；`free`、`construct_at`、`destroy_at` 的结果类型都是 `unit`。它们可以出现在 `return` 推导中，但调用本身的参数错误仍会正常报告。

这些 builtin 的识别按裸 callee 名字先行：源码里直接写 `alloc(...)`、`free(...)`、`construct_at(...)` 或 `destroy_at(...)` 时，语义层会先进入内建检查路径，而不是先查找同名局部变量、顶层函数或导入函数。不能通过声明同名函数来替换这些裸调用；如果需要普通函数包装，应换一个名字，例如 `make_raw_buffer(...)`，在包装内部再调用 builtin。

这些 builtin 不是一等函数值。裸 `alloc`、`free`、`construct_at` 或 `destroy_at` 不会解析成可保存或传递的 allocator / deallocator 函数；`let f = free;`、`run(free);` 或 `let make = alloc<i32>;` 这类写法只按普通名字查找处理。需要把它们交给高阶接口时，应写普通包装函数或 lambda，并在包装体内直接调用 builtin。

参数数量或类型实参错误不会让这些名字退回普通函数查找。当前语义检查按 builtin 形状报告错误后，会保留固定结果类型用于后续诊断和返回推导恢复：

- `alloc` 缺少或多写类型实参会报 `invalid_type_argument`；缺少或多写 count 会报 `argument_count_mismatch`。若存在第一个 count 实参，仍检查它是否为整数；额外实参不参与 `alloc` 自身的目标类型检查。
- `free`、`destroy_at` 带显式类型实参会报 `invalid_type_argument`；实参数量不是 1 会报 `argument_count_mismatch`。若存在第一个实参，仍检查它是否为指针，并用 pointee 类型决定后续释放或析构的目标类型。
- `construct_at` 带显式类型实参会报 `invalid_type_argument`；实参数量不是 2 会报 `argument_count_mismatch`。若存在第一个实参，仍检查它是否为指针；若存在第二个实参，仍按指针 pointee 类型检查待构造值。
- 这些恢复结果只用于继续分析错误源码。合法程序不能依赖 `alloc()`、`free()`、`construct_at(ptr)` 或 `destroy_at(ptr, extra)` 这类错误调用的恢复类型。

`free`、`construct_at` 和 `destroy_at` 要求的是指针值本身。普通引用 binding 会先按表达式读出规则读到被引用的指针值，例如 `let ref alias = p; free(alias);`、`construct_at(alias, value);` 或 `destroy_at(alias);` 都按 `p` 的指针类型检查。显式借用表达式不是指针值：`free(ref p)`、`construct_at(ref p, value)`、`destroy_at(const ref p)` 这类写法会被当作引用表达式而拒绝。

### alloc

`alloc<T>(count)` 分配能容纳 `count` 个 `T` 的原始未初始化内存，返回 `T*`。

```cp
let p = alloc<i32>(1);
```

规则：

- `T` 当前只要求能作为类型实参成功解析。语义层没有单独的“完整对象类型”检查；函数类型、`!`、storage 类型等非常规类型也可能通过 `alloc<T>` 的语义检查。这些类型的大小、对齐和运行时含义不应作为稳定公开能力使用。除非是在明确的底层实验代码中，否则应把 `alloc<T>` 的 `T` 限制在实际对象类型上。
- `count` 必须是整数类型；当前实现不要求它一定是 `usize`。
- `count` 只表示元素数量，不是字节数。
- 编译器根据 `T` 自动计算元素大小和对齐。
- 分配时会把元素大小、对齐和 `count` 交给 runtime `cp_alloc`。runtime 会把对齐提升到至少指针大小并按 2 的幂向上取整；总字节数溢出、对齐取整溢出或底层分配失败时直接终止程序。`count == 0` 或 `sizeof(T) == 0` 这类零字节请求不会产生空指针，而是按 runtime 规则申请 1 字节占位存储。
- 返回的内存未初始化，不能在对象生命周期开始前读取。
- 返回类型总是 `T*`，不携带长度、所有权或初始化状态。
- 编译器不证明 `count` 非负、非零或不会溢出底层 allocator 的乘法；这些属于底层 unsafe 契约。
- `alloc` 只是生成一个带有 `T*` 静态类型的地址；它不会证明这个地址之后一定适合按 `T` 解引用、调用或构造，也不会记录运行时元素数量。

### free

`free(ptr)` 释放由 `alloc` 返回的整块原始内存。

```cp
free(p);
```

规则：

- `ptr` 必须是指针类型。
- `ptr` 可以是 `T*`，也可以是 `T const*`；`free` 释放的是原始分配块，不根据 pointee constness 调用对象操作。
- `free` 不调用析构函数。
- `free` 不区分单对象和数组。
- `free` 只按“是不是指针类型”做浅层检查，不区分数据指针和函数指针形状。`f*(...) -> R` 这类函数指针可以通过当前语义检查，但对函数地址或任意非 `alloc` 分配块调用 `free` 都是底层 unsafe 违约。
- 同一块内存只能释放一次。
- `ptr` 必须来自 `alloc` 返回的分配块起始地址。
- `free(nullptr)` 里的裸 `nullptr` 没有目标指针类型，会被拒绝；已经有具体指针类型的空指针变量只通过底层 runtime 契约处理，语言层不把它特判成语义 no-op。

`free` 的类型参数由 `ptr` 的类型推导，用户不需要写 `free<T>(ptr)`。

### construct_at

`construct_at(ptr, value)` 在 `ptr` 指向的位置构造一个对象。

```cp
construct_at(p, 123);
```

规则：

- `ptr` 必须是 `T*` 或 `T const*`。
- `value` 必须能转换到 `T`。
- `value` 会以 `T` 作为 expected type 检查。也就是说，`construct_at(ptr_to_array, [])`、`construct_at(ptr_to_pointer, nullptr)` 这类右侧需要目标类型的表达式可以从 `ptr` 的 pointee 获得上下文；这只影响单个 `T` 对象的构造检查，不会把一个数组字面量拆成多个连续 slot 构造。
- 调用成功后，该地址上的 `T` 对象生命周期开始。
- `construct_at` 不检查该地址之前是否已经有生命周期中的对象，也不会先调用
  `destroy_at`。它不是赋值操作；如果同一 slot 上已经存在 `T` 对象，调用者必须先结束旧对象生命周期，
  再重新 `construct_at`。直接在 live object 上重复构造属于底层 unsafe 违约。
- 传入 `T const*` 时表示在目标位置开始一个 const 对象生命周期；之后普通解引用写入仍受 pointer constness 限制。
- 对标量、指针等 trivial 类型，效果等价于把值写入目标位置。
- 对结构体和拥有资源的类型，先按 `T` 构造右侧值，再把构造结果放入 `ptr` 指向的位置。
- `construct_at` 只构造一个对象；连续存储需要调用者逐个 slot 调用。

### destroy_at

`destroy_at(ptr)` 析构 `ptr` 指向的一个对象。

```cp
destroy_at(p);
```

规则：

- `ptr` 必须是 `T*` 或 `T const*`。
- 该地址上必须已经存在一个生命周期中的 `T` 对象。
- 如果 `T` 是有可解析析构函数的 `struct`，编译器生成析构函数调用；泛型 `struct` 使用当前具体实例的析构函数。
- 如果 `T` 是标量、指针等 trivial 类型，则 `destroy_at` 是 no-op。
- 如果 `T` 是 `[E; N]`，按从后到前的顺序递归析构各元素。
- 当前 `variant` 没有按 tag 自动析构 payload；`destroy_at(ptr_to_variant)` 不会析构活跃 case 的 payload。
- 调用后，该地址上的对象生命周期结束。

`destroy_at` 的类型参数由 `ptr` 的类型推导，用户不需要写 `destroy_at<T>(ptr)`。

## new / delete

`new` 是 `alloc + construct_at` 的便捷语法，只使用类型初始化器的 `{}` 形式：

```cp
let p = new T{args};
```

它概念上等价于：

```cp
let p = alloc<T>(1);
construct_at(p, T{args});
```

`delete` 是 `destroy_at + free` 的便捷语法：

```cp
delete p;
```

它概念上等价于：

```cp
destroy_at(p);
free(p);
```

规则：

- `new T{...}` 返回 `T*`。
- `new` 总是申请 1 个 `T` 对象的存储；动态数组数量不由 `new` 表达式提供。
- `new` 后面必须是类型和 `{}` 初始化，不支持 `new T(...)`。
- `new` 后面的类型按完整类型位置解析，因此可以直接写 `new [T; N]{...}` 或 `new storage [T; N]{}`。这和普通表达式位置的 `Type{}` 启动形状不同；普通表达式里裸 `[T; N]{...}` 不是稳定写法，通常要先取类型别名或使用 `array<T, N>{...}`。
- `new T{...}` 使用普通类型初始化器规则检查 `{...}`。因此 `new !{}`、`new f(...) -> R{}` 这类不可默认初始化类型会被拒绝；`new storage T{}` 和 `new storage [T; N]{}` 可以创建 storage 对象本身，但 initializer 必须为空，不能写元素列表。
- 非 struct 标量、指针、opaque alias 等只支持空的默认初始化形态，例如 `new i32{}` 或 `new callback{}`；`new i32{123}`、`new i32*{nullptr}` 这类带位置项的非专用类型初始化当前会按普通 `Type{...}` 规则拒绝。需要在堆上放一个指定标量值时，使用 `alloc<T>(1)` 后 `construct_at(ptr, value)`，或包一层有构造 / 聚合初始化能力的 `struct`。
- 当前 `new T{expr}` 会先分配 1 个 `T` 的存储，再构造 initializer 值并写入该地址。它不是先在栈上完整构造一个对象再申请内存，也没有在 initializer 内部 `panic` 或发散时自动释放刚分配的存储；依赖失败清理的拥有资源代码应使用库级 RAII 封装，而不是直接把可能失败的构造逻辑放进裸 `new`。
- `delete` 的操作数必须是非 const 目标的指针值，或裸 `nullptr`。这里检查的是指针目标 constness：`T*` 可以，`T const*` 会被拒绝。当前语义层按通用指针类型检查，因此 `f*(...) -> R` 这类函数指针形状也会通过类型检查；合法使用契约仍只面向 `new T{...}` 或等价 `alloc<T>(1)` 得到的对象存储。
- `delete` 不要求指针 binding 本身可重新赋值。`const pointer = new item{...}; delete pointer;` 可以通过，因为 `pointer` 这个名字的 binding const 不等于 `item const*`；`delete` 只拒绝目标类型带 const 的指针。
- `delete nullptr;` 是允许的 no-op。
- `delete` 会先求值一次操作数表达式，得到的同一个指针值再用于析构和释放；上面的 `destroy_at(p); free(p);` 只是生命周期效果等价说明，不表示会把 `p` 这个源码表达式展开并求值两次。
- `delete` 不区分数组和非数组；`new [T; N]{...}` 返回 `[T; N]*`，`delete` 析构并释放这个 `[T; N]` 对象。
- 编译器只检查指针类型和析构调用，不证明指针一定来自 `new` 或 `alloc`。
- 当前语义层不额外拒绝函数指针形状；对函数地址或任意非分配地址执行 `delete` 都属于底层 unsafe 违约。
- `delete` 要消费的是指针值本身。普通引用 binding 会先按表达式读出规则读到被引用的指针值，例如 `let ref alias = p; delete alias;` 按 `p` 的指针类型检查；但 `delete ref p`、`delete const ref p` 等显式借用结果无效，因为它们产生的是借用表达式，不是可删除的指针值。
- `delete` 比 `destroy_at` 更窄：`destroy_at(ptr_to_const)` 可以表达 const 对象生命周期结束，`delete ptr_to_const` 当前会被拒绝。

## 使用示例

单个对象：

```cp
let p = alloc<i32>(1);
construct_at(p, 123);

let value = *p;

destroy_at(p);
free(p);
```

等价便捷写法：

```cp
struct cell {
    value: i32;
}

let p = new cell{ .value = 123 };
let value = (*p).value;
delete p;
```

当前裸标量不能写 `new i32{123}`。`new i32{}` 可以默认初始化一个 `i32` 对象；如果需要指定初值，使用上面的 `alloc<i32>(1); construct_at(p, 123);` 形式或包一层 `struct`。

连续存储：

```cp
let p = alloc<node>(10);

construct_at(p + 0, node{});
construct_at(p + 1, node{});

destroy_at(p + 1);
destroy_at(p + 0);

free(p);
```

数组长度和哪些元素已经构造由调用者保存。裸指针只表示地址，不携带长度、初始化状态或所有权信息。

内联固定存储：

```cp
let one = storage node{};
construct_at(one.slot(), node{});
destroy_at(one.slot());

let many = storage [node; 16]{};
construct_at(many.slot(0), node{});
construct_at(many.data() + 1, node{});
destroy_at(many.slot(1));
destroy_at(many.data());
```

`storage T{}` 创建一个能容纳一个 `T` 的原始存储对象；`storage [T; N]{}` 创建 `N` 个连续 `T` slot 的原始存储对象。`{}` 初始化的是 storage 对象本身，不调用 `T{}`。storage 析构时也不自动析构任何 slot。

`data()` 返回第一个 slot 的指针，`slot(i)` 返回第 `i` 个 slot 的指针；单 slot 的 `storage T` 也可以写 `slot()`。非 const storage 返回 `T*`，const storage 返回 `T const*`。storage 不支持普通 `[]` 索引，读取 slot 前必须由调用者保证该位置已经 `construct_at`。

const storage 的 slot 指针禁止普通写入，例如 `*fixed.slot(0) = value` 无效；底层生命周期原语仍可以用 `T const*` 开始或结束 const 对象生命周期。覆盖或销毁 storage 对象本身前，调用者仍要保证其中已经开始生命周期的 slot 被成对清理。

`data` / `slot` 是编译器识别的 storage 成员 builtin，只在 `value.data()`、`value.slot()`、`value.slot(i)` 这类成员调用语法中生效。它们不是普通库函数，不接受显式类型实参，也不会通过首参 UFCS 把 `data(value)` / `slot(value, i)` 改写成 storage builtin。若当前作用域里真的有同名自由函数，`data(value)` 仍按普通函数调用解析；对 storage 类型上的点号 `data` / `slot` 名字，编译器优先按这些 builtin 检查，而不是进入普通方法重载或点号 UFCS。

storage 成员调用的检查规则：

- 接收者必须是可取地址的 lvalue。`let slots = storage T{}; slots.slot()` 可以，`(storage T{}).slot()` 无效，因为临时 storage 没有稳定 slot 地址；当前报 `invalid_assignment_target`。
- `data()` 不接受参数，语义上使用第 0 个 slot 地址；`data(0)` 报 `argument_count_mismatch`。编译器不额外证明 storage 长度大于 0，解引用或构造仍是调用者契约。`storage [T; 0]{}` 可以形成 storage 对象，`data()` 当前仍会走地址计算路径，但它不表示存在可构造的第 0 个元素；需要空原始区间时应把它只作为长度为 0 的边界值使用，不要解引用或交给 `construct_at`。
- `data` 和 `slot` 都不接受显式类型实参；`item.data<i32>()` 或 `item.slot<i32>(0)` 报 `invalid_type_argument`，但调用仍按 storage builtin 结果类型继续错误恢复。
- `slot()` 不带下标时只允许用于单 slot storage，例如 `storage T{}` 或长度已知为 1 的 `storage [T; 1]{}`。多 slot storage 必须写 `slot(i)`；对 `storage [T; 2]` 写 `slot()` 报 `argument_count_mismatch`。
- `slot(i)` 的 `i` 必须是整数表达式，非整数下标报 `type_mismatch`。若 storage 长度在语义阶段已知，且 `i` 是编译期整数常量，编译器会用 `invalid_operator` 拒绝负数或越界下标；动态下标、泛型长度下标和运行时范围仍由调用者保证。因此 `storage [T; 0]{}.slot(0)` 这类常量下标会被拒绝，但 `slot(index)` 在语义层只检查 `index` 是整数，不生成运行时边界检查。
- `slot` 最多接受一个下标参数，`data` 不接受任何参数；多余参数会作为参数数量错误报告，即 `argument_count_mismatch`。
- 返回指针的 constness 来自 storage 接收者本身：非 const 接收者返回 `T*`，const 接收者返回 `T const*`。普通解引用写入会遵守这个 pointer constness；对象生命周期是否已经开始不由指针类型携带。

`data()` 和 `slot()` 只计算 storage 对象内部 slot 地址；不调用 runtime，不申请内存，也不自动执行 `construct_at` 或 `destroy_at`。

数组对象：

```cp
let p = new [i32; 4]{1, 2, 3, 4};
delete p;
```

`new [T; N]{...}` 使用数组类型初始化器语义：位置元素不能超过 `N`，显式元素按 `T` 检查，缺省尾部元素按 `T` 默认初始化。普通表达式位置若要写同类数组类型初始化器，需要通过类型名或类型别名触发，例如 `type i32x4 = [i32; 4]; i32x4{1, 2}`；`new` 场景可以直接解析 raw array type。`new storage [T; N]{}` 创建的是 raw storage 对象本身，不构造 `T` 元素，也不允许带元素列表。

## 检查边界

语言规则要求：

- `alloc` 后得到的是未初始化内存。
- `construct_at` 后对象生命周期开始。
- `destroy_at` 后对象生命周期结束。
- `free` 只释放原始内存。

编译器只做类型检查：

- `alloc<T>(count)` 检查 `T` 和 `count`。
- `free(ptr)` 检查 `ptr` 是指针，不检查 pointee constness，也不区分数据指针和函数指针形状。
- `construct_at(ptr, value)` 检查指针目标类型和值类型匹配，不把 `T const*` 作为错误。
- `destroy_at(ptr)` 检查 `ptr` 是指针，不把 `T const*` 作为错误。
- `new T{...}` 检查初始化表达式能构造 `T`。
- `delete ptr` 检查 `ptr` 是非 const 目标的指针，或裸 `nullptr`。
- `alloc<T>` 当前不检查 `T` 是否是“可构造对象类型”；这和 `construct_at` / `new` 的初始化检查不同。`alloc<!>(1)` 或函数类型的 `alloc` 可能通过语义检查，但对应的 `new !{}` 或 `new f(...) -> R{}` 会在类型初始化阶段失败。

编译器不完整证明：

- 某个地址是否已经 `construct_at`。
- 某个对象是否重复 `destroy_at`。
- `free` 前是否所有已构造对象都已 `destroy_at`。
- `free` 的指针是否一定来自 `alloc`。
- `free` 的指针是否是分配块起始地址。
- `delete` 的指针是否一定来自 `new`。
- `alloc` 的 `count` 在运行时是否为有效元素数量。

这些属于底层 unsafe 契约。更高层使用通过 `raw_buffer<T>`、`vector<T>`、`string` 等拥有类型封装；`span<T>` 只表达借用区间，不接管释放或元素生命周期。

## Runtime ABI

真正的堆分配不由编译器直接实现，也不直接生成操作系统 syscall。

当前编译器把：

```cp
alloc<T>(count)
free(ptr)
```

实现为 runtime ABI 调用：

```text
declare ptr @cp_alloc(i64 elem_size, i64 align, i64 count)
declare void @cp_free(ptr)
```

runtime 提供 C ABI 符号：

```cpp
extern "C" void* cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count);
extern "C" void cp_free(void* ptr);
```

runtime 可以用普通 C++ `.cpp` 实现，内部调用 `malloc/free`，大于 `max_align_t` 的对齐请求走 `aligned_alloc`。当前实现会把小于 `sizeof(void*)` 的对齐提升到 `sizeof(void*)`，非 2 次幂对齐会向上取整到下一个 2 次幂；零字节请求按 1 字节分配。元素大小乘以数量溢出、对齐向上取整溢出，或底层分配返回 null 时，runtime 直接 `abort()`，不会把失败编码成可检查的空指针返回值。`free(ptr)` 不需要用户传 `count` 或 layout，释放所需的分配块信息由底层 allocator 像 C `free` 一样内部保存。

推荐项目位置：

```text
runtime/
  cp_runtime.cpp
  CMakeLists.txt
  abi.md
```

职责划分：

- `runtime/cp_runtime.cpp` 实现 `cp_alloc` 和 `cp_free`。
- 编译器只需要声明并调用 `@cp_alloc` 和 `@cp_free` 这两个 runtime 入口。
- `compiler/tool/main.cpp` 在链接最终程序时自动带上 runtime 对象或静态库。

`construct_at` 和 `destroy_at` 通常不需要 runtime：

- `construct_at` 在目标地址执行写入、构造或 move 初始化逻辑。
- `destroy_at` 执行数组元素递归析构、`struct` 析构函数调用或 no-op；当前不生成 `variant` payload 析构分发。

## 库封装

在四个原语之上提供库封装：

```text
raw_buffer<T> = 保存 ptr / capacity 的底层原始存储拥有类型，元素构造数量由上层容器维护
span<T>       = 保存 ptr / len 的非拥有借用视图，不释放内存，也不记录初始化状态
vector<T>     = raw_buffer/vector_storage + 多次 construct_at + 多次 destroy_at，维护 len/cap
string        = string_storage/raw_buffer<char> + trailing '\0' 不变量，维护字符 len
```

这些封装按各自层级保存容量、长度、已构造数量和所有权状态。`raw_buffer<T>` 只保存容量和原始存储所有权；`span<T>` 只保存借用指针和长度，不能从 `raw_buffer<T>` 自动得知哪些元素已经构造；`vector<T>`、`string` 等上层类型再维护长度和已构造元素。裸指针不承担这些职责。第一版标准库当前没有提供 `unique<T>` 或 `box<T>` 这类拥有单对象指针封装；需要单对象堆分配时只能直接使用 `new` / `delete` 或自行封装所有权。
