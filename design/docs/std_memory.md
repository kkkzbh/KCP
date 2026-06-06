# 标准库 memory

本文档记录 `std.memory` 第一版公共接口。它提供两类能力：拥有原始连续存储的 `raw_buffer<T>`，以及借用已经构造元素区间的 `span<T>` / `contiguous_mutable_range` 协议。

`std.memory` 聚合模块当前只重导出：

```text
std.memory.raw_buffer
std.memory.span
```

`raw_buffer<T>` 管理分配块，`span<T>` 管理借用视图。二者都不是自动生命周期容器；需要自动构造、析构和扩容元素时使用 [std_collections.md](std_collections.md) 中的 `vector<T>`。

当前语言没有字段私有性，因此 `raw_buffer<T>` 的 `ptr` / `cap` 和 `span<T>` 的 `ptr` / `len` 在源码层是可访问字段。稳定用法仍然是通过构造函数和成员函数建立值；直接改写字段会绕过所有权、容量、借用区间和元素生命周期不变量。

本页提到的 `alloc`、`free`、`construct_at` 和 `destroy_at` 不是 `std.memory` 导出的普通函数，而是编译器按裸名字识别的内建入口。它们不需要 `import std.memory;` 才能调用，也不能作为一等函数值保存或被同名函数替换。完整调用规则、恢复诊断和运行时边界见 [memory_allocation.md](memory_allocation.md)。

## raw_buffer

```cp
raw_buffer<T>
```

`raw_buffer<T>` 只拥有一块能容纳若干个 `T` 的原始存储。它记录 `ptr: T*` 和 `cap: usize`，不记录已经构造了多少个元素。

公开接口：

```cp
raw_buffer() = default;
raw_buffer(capacity: usize);
raw_buffer(other: this const&) = delete;
operator =(self&, rhs: this const&) = delete;
operator =(self&, rhs: this move&) = delete;
raw_buffer(other: this move&);
~raw_buffer();
data(self like&) -> T like*;
capacity(self const&) -> usize;
empty(self const&) -> bool;
take(self&) -> raw_buffer<T>;
reset(self&) -> void;
replace(self&, next: this move&) -> void;
swap(self&, other: raw_buffer<T>&) -> void;
```

规则：

- `raw_buffer<T>{}` 构造空 buffer，保存 `nullptr` 和容量 `0`。
- `raw_buffer<T>{capacity}` 在 `capacity != 0` 时调用 `alloc<T>(capacity)`；`capacity == 0` 时仍是空 buffer，不申请占位块。
- `capacity()` 返回可容纳的 `T` 槽位数量；`empty()` 等价于 `capacity() == 0`。它不表示“已构造元素数量为 0”，因为 `raw_buffer` 不跟踪元素生命周期。
- `data()` 返回底层指针。普通 receiver 返回 `T*`，const receiver 返回 `T const*`。
- 容量构造只让底层存储可用，不会默认构造任何 `T`。在某个槽位上执行 `construct_at` 之前，不能把 `data()[index]` 当作 `T` 对象读取、赋值、比较、排序或析构。已经构造的槽位需要由调用者维护数量和生命周期。
- 析构函数只调用 `free(ptr)`，不会逐个 `destroy_at` 槽位中的对象。调用者如果曾经 `construct_at` 元素，必须在 buffer 析构、`reset()`、`replace()` 或覆盖所有权前自行按相反顺序或合适顺序销毁。
- 源码中声明了删除的 copy 构造和 copy 赋值；稳定用法必须把 `raw_buffer<T>` 当作唯一所有权对象，不能复制。当前编译器对删除特殊成员的检查还不覆盖所有 copy 形状，例如 `let copy = storage;` 这类局部初始化可能仍被接受，但它只会复制裸指针和容量，两个 buffer 析构时会重复 `free` 同一块内存。公共代码不要依赖这种行为；需要转移所有权时使用 move 构造、`take()` 或 `replace(move next)`。
- move 赋值也被删除；需要替换当前 buffer 时使用 `replace(move next)`。`replace` 会先 `reset()` 当前块，再接管 `next`，并把 `next` 置空。
- `take()` 返回当前 `(ptr, cap)` 形成的新 `raw_buffer<T>`，并把当前对象置空。它不会移动或销毁槽位中的对象，只移动底层块所有权。
- `reset()` 释放当前块并置空。它同样不销毁已经构造的元素。
- `swap(other)` 只交换两个 buffer 的指针和容量，不移动元素，也不改变各槽位生命周期。
- `raw_buffer<T>` 没有 `size()`、`operator []`、`begin()` / `end()`、range-for 入口或 `contiguous_mutable_range` 实现。需要把已构造前缀交给算法时，显式构造 `span<T>{buffer.data(), len}`，并保证这 `len` 个对象已经开始生命周期。

`raw_buffer<T>` 是底层 building block，不是数组值。它不维护初始化位图、长度、异常/失败清理或元素移动不变量；这些职责属于调用者或更高层容器。

## span

```cp
span<T>
```

`span<T>` 是连续区间借用视图。它保存 `ptr: T*` 和 `len: usize`，不拥有内存，也不延长元素生命周期。

公开接口：

```cp
span(ptr: T*, len: usize);
data(self like&) -> T like*;
size(self const&) -> usize;
empty(self const&) -> bool;
operator [](self like&, index: usize) -> T like&;
```

迭代接口：

```cp
span<T> implements iterable
    type iter_type = ptr_iter<T>;
    type iter_item = T&;
    iter(self&) -> ptr_iter<T>;

span<T> implements const_iterable
    type const_iter_type = const_ptr_iter<T>;
    type const_iter_item = T const&;
    iter(self const&) -> const_ptr_iter<T>;
```

规则：

- `span<T>{ptr, len}` 只记录传入指针和长度，不检查 `[ptr, ptr + len)` 是否有效，也不检查这些槽位是否已经构造了 `T`。
- `span<T>` 是普通轻量值，复制 span 只复制 `ptr` 和 `len`，不复制底层元素。通过任意副本写入都会修改同一段外部区间。
- `data()` 返回当前指针；接口签名写作 `self like&`，目标是让 receiver constness 影响返回指针的 constness。但当前不要把 `span` 对象本身的 `const` 当作可靠只读权限边界：`const view: span<i32>` 仍可能通过下标写到底层 `i32`。需要只读借用时，应让元素来源本身是 const，或只暴露 const 迭代入口。
- `size()` 返回元素数量，`empty()` 等价于 `size() == 0`。
- `operator [](index)` 使用 `assert(index < len, "span index out of bounds")` 表达前置条件。checked 模式下越界会 panic；`--release` 会移除 assert，调用者必须保证下标有效。
- 下标结果使用 `self like&`，语义目标是可写 span 返回 `T&`、const span 返回 `T const&`；但如上所述，当前 wrapper constness 不是安全边界。
- range-for 可直接遍历 `span<T>`。可写 span 产生 `T&`；const 迭代入口声明为产生 `T const&`。
- `span<T>` 没有 `subspan`、`slice`、`first`、`last`、`front`、`back`、`as_bytes`、拥有式复制或扩容接口。
- `span<T>` 不拥有底层对象；原容器扩容、移动、析构，或调用者结束元素生命周期后，旧 span 和从它取出的指针/引用都可能失效。

空 span 可以写成 `span<T>{nullptr as T*, 0 as usize}`，也可以使用任何不会被解引用的具体 `T*` 和长度 0。当前接口不会在 `size() == 0` 时读取 `data()` 指向内容。

## contiguous_mutable_range

`contiguous_mutable_range` 是排序等算法使用的连续可写 range 协议：

```cp
concept contiguous_mutable_range {
    type item;

    data(self like&) -> item like*;
    size(self const&) -> usize;
}
```

当前标准库实现：

```cp
impl<T> contiguous_mutable_range for span<T>;
impl<T, N: usize> contiguous_mutable_range for [T; N];
```

`vector<T>` 和 `string` 也通过各自模块实现该 concept。`raw_buffer<T>` 不实现它，因为 raw buffer 没有已构造元素长度。

规则：

- 满足该 concept 只表示可以借出一段连续可写元素区间；它不表示对象拥有内存、能扩容、能安全切片，也不证明所有槽位生命周期已经开始。
- `data(self like&)` 的 `like` constness 决定借出的指针是否只读；排序这类原地算法需要可写 receiver。
- 数组桥接的 `data()` 当前通过 `&self[0]` 取得首元素地址。因此 `[T; 0]` 虽然是合法数组类型，也可以被 range-for 零次遍历，但不要把零长数组直接传给会先调用 `.data()` 的 `contiguous_mutable_range` 算法。需要空连续区间时使用长度为 0 的 `span<T>` 或空 `vector<T>`。

## 不支持内容

`std.memory` 第一版不提供：

- `unique_ptr`、`shared_ptr`、allocator object、arena 或 reference-counted buffer。
- `span` 切片、字节视图、const-only `span<const T>` 专门语法或生命周期检查。
- `raw_buffer` 的元素数量、自动析构、初始化位图、迭代器、下标或 range 协议。
- `memcpy`、`memmove`、`fill`、`uninitialized_copy` 这类批量内存算法；需要时显式循环并配合 `construct_at` / `destroy_at`。
