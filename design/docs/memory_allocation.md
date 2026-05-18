# 底层内存分配

本文档记录 cp 的底层内存分配与释放设计。语言提供最小原语，不引入 `new`、`delete` 或 `delete[]`。

`unique<T>`、`vector<T>`、`buffer<T>`、`string` 等拥有资源的类型作为核心库类型或标准库类型建立在这些原语之上。

## 设计目标

底层内存能力分为两件事：

```text
原始内存生命周期: alloc / free
对象生命周期: construct_at / destroy_at
```

`alloc` 和 `free` 只处理字节存储，不负责对象构造和析构。

`construct_at` 和 `destroy_at` 只处理对象生命周期，不负责申请和释放底层存储。

这样释放内存时不需要区分单个对象和数组，也不会出现 C++ 风格的 `delete` / `delete[]` 二选一问题。

## 原语

提供四个编译器内建原语：

```cp
alloc<T>(count) -> T*
free(ptr)

construct_at(ptr, value)
destroy_at(ptr)
```

这些原语语法上表现为全局函数，不使用 `mem::` 前缀。它们不是普通用户库函数，而是编译器识别并特殊 lowering 的 builtin。

### alloc

`alloc<T>(count)` 分配能容纳 `count` 个 `T` 的原始未初始化内存，返回 `T*`。

```cp
let p = alloc<i32>(1);
```

规则：

- `T` 必须是完整类型。
- `count` 必须是整数类型。
- 编译器根据 `T` 自动计算元素大小和对齐。
- 返回的内存未初始化，不能在对象生命周期开始前读取。

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
- 对结构体和拥有资源的类型，lowering 为构造、移动或初始化逻辑。

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
- 调用后，该地址上的对象生命周期结束。

`destroy_at` 的类型参数由 `ptr` 的类型推导，用户不需要写 `destroy_at<T>(ptr)`。

## 使用示例

单个对象：

```cp
let p = alloc<i32>(1);
construct_at(p, 123);

let value = *p;

destroy_at(p);
free(p);
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

编译器不完整证明：

- 某个地址是否已经 `construct_at`。
- 某个对象是否重复 `destroy_at`。
- `free` 前是否所有已构造对象都已 `destroy_at`。
- `free` 的指针是否一定来自 `alloc`。
- `free` 的指针是否是分配块起始地址。

这些属于底层 unsafe 契约。安全使用通过 `unique<T>`、`vector<T>`、`buffer<T>` 等库类型封装。

## Runtime ABI

真正的堆分配不由编译器直接实现，也不直接生成操作系统 syscall。

编译器把：

```cp
alloc<T>(count)
free(ptr)
```

lower 成 runtime ABI 调用：

```llvm
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
vector<T>  = alloc + 多次 construct_at + 多次 destroy_at + free
buffer<T>  = 保存 ptr / capacity / initialized_count 的底层拥有容器
```

这些封装负责保存长度、容量、已构造数量和所有权状态。裸指针不承担这些职责。
