# 底层内存分配

本文档记录 cp 的底层内存分配与释放设计。语言提供四个最小原语，并在其上提供 `new` / `delete` 语法糖；不引入 `new[]` 或 `delete[]`。

`unique<T>`、`raw_buffer<T>`、`vector<T>`、`string` 等拥有资源的类型作为核心库类型或标准库类型建立在这些原语之上。

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

这些原语语法上表现为全局函数，不使用 `mem::` 前缀。它们不是普通用户库函数，而是编译器识别并特殊 lowering 的 builtin。

类型实参规则：

- `alloc<T>(count)` 必须且只能写一个显式类型实参。
- `alloc` 的类型实参必须是类型，不能是值、名字占位或参数包展开。
- `free`、`construct_at`、`destroy_at` 不接受显式类型实参；目标类型都从指针参数推导。
- 四个原语只按这些 builtin 规则解析，不参与普通函数重载、UFCS 或用户自定义替换。

### alloc

`alloc<T>(count)` 分配能容纳 `count` 个 `T` 的原始未初始化内存，返回 `T*`。

```cp
let p = alloc<i32>(1);
```

规则：

- `T` 必须是完整类型。
- `count` 必须是整数类型。
- `count` 只表示元素数量，不是字节数。
- 编译器根据 `T` 自动计算元素大小和对齐。
- 返回的内存未初始化，不能在对象生命周期开始前读取。
- 返回类型总是 `T*`，不携带长度、所有权或初始化状态。

### free

`free(ptr)` 释放由 `alloc` 返回的整块原始内存。

```cp
free(p);
```

规则：

- `ptr` 必须是指针类型。
- `free` 不调用析构函数。
- `free` 不区分单对象和数组。
- 同一块内存只能释放一次。
- `ptr` 必须来自 `alloc` 返回的分配块起始地址。
- `nullptr` 作为指针值时只通过底层 runtime 契约处理；语言层只检查类型，不把 `free(nullptr)` 特判成语义 no-op。

`free` 的类型参数由 `ptr` 的类型推导，用户不需要写 `free<T>(ptr)`。

### construct_at

`construct_at(ptr, value)` 在 `ptr` 指向的位置构造一个对象。

```cp
construct_at(p, 123);
```

规则：

- `ptr` 必须是 `T*`。
- `value` 必须能转换到 `T`。
- 调用成功后，该地址上的 `T` 对象生命周期开始。
- 对标量、指针等 trivial 类型，lowering 为一次 store。
- 对结构体和拥有资源的类型，IR 先按 `T` 构造右侧值，再把构造结果写入 `ptr` 指向的位置。
- `construct_at` 只构造一个对象；连续存储需要调用者逐个 slot 调用。

### destroy_at

`destroy_at(ptr)` 析构 `ptr` 指向的一个对象。

```cp
destroy_at(p);
```

规则：

- `ptr` 必须是 `T*`。
- 该地址上必须已经存在一个生命周期中的 `T` 对象。
- 如果 `T` 有析构函数，编译器生成析构函数调用。
- 如果 `T` 是标量、指针等 trivial 类型，则 `destroy_at` 是 no-op。
- 如果 `T` 是 `[E; N]`，IR 按从后到前的顺序递归析构各元素。
- 调用后，该地址上的对象生命周期结束。

`destroy_at` 的类型参数由 `ptr` 的类型推导，用户不需要写 `destroy_at<T>(ptr)`。

## new / delete

`new` 是 `alloc + construct_at` 的便捷语法，只使用构造初始化的 `{}` 形式：

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
- `delete` 的操作数必须是非 const 的对象指针。
- `delete nullptr;` 是允许的 no-op。
- `delete` 不区分数组和非数组；`new [T; N]{...}` 返回 `[T; N]*`，`delete` 析构并释放这个 `[T; N]` 对象。
- 编译器只检查指针类型和析构调用，不证明指针一定来自 `new` 或 `alloc`。
- `delete ref p`、`delete const ref p` 等显式借用结果无效；`delete` 要消费的是指针值本身。

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
let p = new i32{123};
let value = *p;
delete p;
```

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

数组对象：

```cp
let p = new [i32; 4]{1, 2, 3, 4};
delete p;
```

## 检查边界

语言规则要求：

- `alloc` 后得到的是未初始化内存。
- `construct_at` 后对象生命周期开始。
- `destroy_at` 后对象生命周期结束。
- `free` 只释放原始内存。

编译器只做类型检查：

- `alloc<T>(count)` 检查 `T` 和 `count`。
- `free(ptr)` 检查 `ptr` 是指针。
- `construct_at(ptr, value)` 检查指针目标类型和值类型匹配。
- `destroy_at(ptr)` 检查 `ptr` 是指针。
- `new T{...}` 检查初始化表达式能构造 `T`。
- `delete ptr` 检查 `ptr` 是非 const 对象指针。

编译器不完整证明：

- 某个地址是否已经 `construct_at`。
- 某个对象是否重复 `destroy_at`。
- `free` 前是否所有已构造对象都已 `destroy_at`。
- `free` 的指针是否一定来自 `alloc`。
- `free` 的指针是否是分配块起始地址。
- `delete` 的指针是否一定来自 `new`。

这些属于底层 unsafe 契约。安全使用通过 `unique<T>`、`raw_buffer<T>`、`vector<T>` 等库类型封装。

## Runtime ABI

真正的堆分配不由编译器直接实现，也不直接生成操作系统 syscall。

编译器把：

```cp
alloc<T>(count)
free(ptr)
```

lower 成 runtime ABI 调用：

```text
declare ptr @cp_alloc(i64 elem_size, i64 align, i64 count)
declare void @cp_free(ptr)
```

runtime 提供 C ABI 符号：

```cpp
extern "C" void* cp_alloc(std::uint64_t elem_size, std::uint64_t align, std::uint64_t count);
extern "C" void cp_free(void* ptr);
```

runtime 可以用普通 C++ `.cpp` 实现，内部调用 `malloc/free`，必要时用 `posix_memalign` 处理大对齐。`free(ptr)` 不需要用户传 `count` 或 layout，释放所需的分配块信息由底层 allocator 像 C `free` 一样内部保存。

推荐项目位置：

```text
runtime/
  cp_runtime.cpp
  CMakeLists.txt
  abi.md
```

职责划分：

- `runtime/cp_runtime.cpp` 实现 `cp_alloc` 和 `cp_free`。
- `codegen/llvm/llvm.cppm` 只声明并调用 `@cp_alloc` 和 `@cp_free`。
- `compiler/tool/main.cpp` 在链接最终程序时自动带上 runtime 对象或静态库。

`construct_at` 和 `destroy_at` 通常不需要 runtime：

- `construct_at` lowering 为写入、构造或 move 初始化逻辑。
- `destroy_at` lowering 为析构函数调用或 no-op。

## 库封装

在四个原语之上提供库封装：

```text
unique<T>  = alloc + construct_at + destroy_at + free
box<T>     = new + delete
vector<T>  = alloc + 多次 construct_at + 多次 destroy_at + free
raw_buffer<T> = 保存 ptr / capacity 的底层原始存储拥有类型，元素构造数量由上层容器维护
```

这些封装按各自层级保存容量、长度、已构造数量和所有权状态。`raw_buffer<T>` 只保存容量和原始存储所有权；`vector<T>`、`string` 等上层类型再维护长度和已构造元素。裸指针不承担这些职责。
