# 标准库导览

KCP 标准库位于仓库根目录 `std/`，使用普通 KCP 模块实现。语言本身只提供必要的语法和少量内建能力，集合、字符串、范围、格式化和文件 IO 都通过标准库模块组织。

## 导入方式

学习阶段可以直接导入聚合入口：

```cp
import std;
```

`std` 会重导出第一批公共领域模块。需要更清晰的依赖边界时，也可以导入具体模块，例如 `std.collections`、`std.ranges` 或 `std.fs`。

导入模块后，导出的类型、函数、concept 和扩展项按普通未限定名字参与查找；当前语言没有 `std::vector`、`std.collections::vector` 或 `std.vector` 这类模块限定访问语法。也就是说，`import std;` 后直接写 `vector<i32>`、`println(...)`、`sort(values)`；没有导入时，这些标准库名字不会作为全局 prelude 自动可见。

当前标准库使用分层模块名，不保留旧式扁平兼容入口。集合类型位于
`std.collections.*` 下：应写 `import std.collections.vector;`、
`import std.collections.map;`、`import std.collections.set;` 或聚合入口
`import std.collections;`，不应写 `import std.vector;`、`import std.map;`
或 `import std.set;`。这类扁平名字不是兼容别名，会按未知模块处理。
带 `detail` 的模块是实现细节，公共聚合模块不会重导出它们；用户代码不应依赖这些内部名字。
这类文件即使在源码中写成 `export module std.xxx.detail.yyy;`，也只表示编译器能按模块名处理它们，不表示它们进入稳定公共 API。
直接 `import std.collections.detail.btree;`、`import std.text.detail.string_storage;` 或 `import std.detail.runtime;` 这类写法可能在当前实现中通过解析和语义检查，但调用方会直接承担内部字段布局、辅助函数签名、runtime ABI 名称和不变量变化的风险；公开示例和库接口应只依赖聚合模块或非 `detail` 子模块。

`std` 本身只是普通聚合模块，不是隐式 prelude。它当前重导出 `std.core`、
`std.memory`、`std.collections`、`std.text`、`std.compare`、`std.ranges`、
`std.meta`、`std.algorithm`、`std.io` 和 `std.fs`；不会自动递归导入仓库
`std/` 下所有模块。导入子模块时也只获得该子模块及其源码中显式
`export import` 的内容，例如 `import std.ranges.terminals;` 可以看到 terminal
和 `std.ranges.sources` 暴露的 view/source 基础；由于 `sources` 自己重导出
`std.core.iter`、`std.core.option` 和 `std.meta`，这些迭代协议、`optional`
和类型查询名也会可见，但不会因此看到 `filter` / `transform` 等 adapters。

## 模块分层

| 模块 | 内容 |
| --- | --- |
| `std.core` | `optional<T>`、`expected<T,E>`、`iterator`、`iterable`、`const_iterable`、`ptr_iter<T>`、`const_ptr_iter<T>` |
| `std.memory` | `raw_buffer<T>`、`span<T>`、`contiguous_mutable_range` 和数组连续区间桥接 |
| `std.collections` | `vector<T>`、`map<K,V>`、`set<K>` |
| `std.text` | compiler-recognized `str` 的扩展和拥有字符串 `string` |
| `std.ranges` | 可组合 range sources、lazy adapters 和 terminals |
| `std.meta` | 类型查询、`call_result` 和 `callable` |
| `std.compare` | 三路比较分类、比较器和泛型算法所需 concept |
| `std.algorithm` | 泛型算法，第一版提供 `sort` 和 `stable_sort` |
| `std.io` | `format`、`format_to`、输出流、格式化结果类型、`display` 协议和 stdout/stderr 输出 |
| `std.fs` | 同步文件打开、读取、写入和关闭 |

## optional 和 expected

`optional<T>` 表示“可能没有值”，`expected<T,E>` 表示“值或错误”。它们常用于可恢复失败路径。

```cp
main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let value = expected<i32,str>::value(12);
    let error = expected<i32,str>::unexpected("bad");

    if(some.has_value() and not none.has_value() and value.has_value() and not error.has_value()) {
        return some.value_or(0) + none.value_or(10) + value.value_or(0);
    }

    return 1;
}
```

相关参考：[错误处理](error_handling.md)。

完整接口和迭代协议见 [标准库 core](std_core.md)。

当前 `std.core` 只给这两个结果类型提供一组很小的成员操作：

- `has_value(self const&) -> bool`：`optional::some` / `expected::value` 返回 `true`，`optional::none` / `expected::unexpected` 返回 `false`。
- `value_or(self const&, fallback: T) -> T`：有值时从保存的成功 payload 构造返回值，没有值或为 `unexpected` 时返回 `fallback`。因为 receiver 是 `self const&`，成功分支要求当前 `T const&` 能构造返回 `T`；它不会 move payload。`fallback` 是普通按值实参，调用点必须能构造它；`value_or` 不是 lazy fallback，也不会因为当前对象已有值就跳过实参检查。这个函数不返回引用，也不暴露 `expected` 的错误 payload。需要 move 成功值、按需构造 fallback，或读取 error payload 时，直接 `match` 对应 variant case。
- `operator *(self like&) -> T like&`：有值时返回 payload 引用；receiver 是可写左值时可通过 `*item = new_value` 修改 payload，receiver 是 const 左值时得到 const 引用。对 `optional::none` 解引用会 `panic("optional dereference on none")`，对 `expected::unexpected` 解引用会 `panic("expected dereference on unexpected")`。

没有 `unwrap`、`expect`、`map`、`and_then`、`error()` 或 `unexpected()` 成员访问器。需要区分错误内容、移动 payload，或把失败分支映射成其它类型时，直接用 `match` 解开 `variant` case：

```cp
let status = match result {
    .value(item) => item,
    .unexpected(error) => {
        println("error = {}", error);
        0
    },
};
```

`std.core.iter` 定义 range-for 和 ranges 共享的协议层。`iterator` 是有状态游标，要求 `next(self&) -> optional<iter_item>`；`iterable` / `const_iterable` 是用户层范围入口，分别要求 `iter(self&)` / `iter(self const&)` 产生对应 iterator。二者不是可互换关系：容器、view、`span`、`str` 或 `string` 可以实现 `iterable`，它们产生的 `ptr_iter<T>` / `const_ptr_iter<T>` 等游标实现 `iterator`；但一个裸 iterator 本身不能直接作为 range-for 范围表达式，也不能直接作为 ranges adapter 输入。需要消费范围时写 `for(let item : values)` 或 `values.count()`，不要写 `for(let item : values.iter())` 或 `values.iter().count()`。

相关参考：[迭代](iteration.md)、[标准库 ranges](std_ranges.md)。

## raw_buffer、span、vector

`raw_buffer<T>` 拥有原始连续存储，负责容量和释放；元素构造和析构由上层容器或调用者控制。`span<T>` 借用连续区间，不拥有内存。`vector<T>` 是基于 `vector_storage<T>` 的动态数组，`string` 通过 `string_storage` 封装 trailing `'\0'` 所需的物理容量。

```cp
main() -> i32
{
    let storage = raw_buffer<i32>{2};
    construct_at(storage.data(), 5);
    construct_at(storage.data() + 1, 7);

    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(4);
    values.insert(1 as usize, 5);

    let sum = *storage.data() + *(storage.data() + 1) + values[0 as usize];

    destroy_at(storage.data() + 1);
    destroy_at(storage.data());
    return sum;
}
```

相关参考：[底层内存分配](memory_allocation.md)、[标准库 memory](std_memory.md)、[标准库 collections](std_collections.md)。

这一层的边界：

- `raw_buffer<T>{}` 构造空 buffer；`raw_buffer<T>{capacity}` 只申请能容纳 `capacity` 个 `T` 的原始存储。`capacity == 0` 时和空构造一样保存 `nullptr` 和 `0`。它不记录已经构造了多少元素，也不会在析构时逐个 `destroy_at` 元素；析构只释放底层块。因此用它直接管理非 trivial 元素时，调用者必须自己维护已构造区间，并在 `raw_buffer` 离开作用域前逐个销毁。
- `raw_buffer<T>` 只提供 `data()`、`capacity()` 和 `empty()` 这三个查询；`empty()` 表示 `capacity() == 0`，不是“已构造元素数量为 0”。它没有 `size()`、`operator []`、迭代器或 `contiguous_mutable_range` 实现，即使底层指针非空，也不能直接当作 `span`、`vector` 或可排序 range 使用。需要把其中一段已构造元素交给算法时，调用者必须显式构造 `span<T>{buffer.data(), len}`，并保证这 `len` 个元素生命周期已经开始。
- `raw_buffer<T>` 禁止 copy 构造、copy 赋值和 move 赋值；move 构造通过 `take()` 转移所有权。`take()` 会把当前对象清成空 buffer 并返回原来的 `(ptr, cap)`；`reset()` 释放当前块并置空；`replace(move next)` 先 reset 当前块再接管 `next`，`swap(other)` 只交换指针和容量。
- `span<T>{ptr, len}` 只是借用连续区间，不拥有元素，也不延长 `ptr` 指向对象的生命周期。它提供 `data()`、`size()`、`empty()` 和 `operator []`；没有 `slice`、`subspan`、`front`、`back`、`as_bytes` 或拥有式复制接口。
- `span<T>::operator []` 使用 `assert(index < len, "span index out of bounds")` 做 checked 访问；release 模式会移除 `assert`，因此越界仍是调用者前置条件。
- `span<T>` 同时实现 `iterable` / `const_iterable`。可写 `span<T>` 迭代产生 `T&`，const `span<T>` 迭代产生 `T const&`。`span<T>` 和数组 `[T; N]` 都实现 `contiguous_mutable_range`，这是 `sort` / `stable_sort` 这类原地算法使用的连续可写 range 协议。
- `contiguous_mutable_range` 的公开 shape 只是 `type item`、`data(self like&) -> item like*` 和 `size(self const&) -> usize`。满足这个 concept 不表示对象拥有内存、能扩容、能安全切片，也不证明 `[data(), data() + size())` 内每个槽位的元素生命周期已经开始；这些仍由具体类型或调用者契约保证。
- 数组的 `contiguous_mutable_range` 桥接由 `std.memory.span` 提供，`size()` 返回编译期长度 `N`，`data()` 当前通过 `&self[0]` 取得首元素地址。因此 `[T; 0]` 虽然是合法数组类型，也能参与内建 range-for 的零次循环，但不要把零长数组传给会先调用 `.data()` 的标准库协议入口；需要空连续区间时显式构造长度为 0 的 `span<T>`，或使用空 `vector<T>`。

## map 和 set

`map` / `set` 是有序唯一键容器，支持按序遍历、按序下标访问和 `rank(key)` 查询。重复插入不会覆盖已有 key，结果会标记 `inserted = false`。查询或插入返回的引用在下一次容器修改后可能失效。

```cp
main() -> i32
{
    let ids = map<i32, i32>{};
    let keys = set<i32>{};

    ids.insert(2, 20);
    ids[1] = 10;
    ids.insert(2, 99);

    keys.insert(4);
    keys.insert(2);
    keys.insert(4);

    return ids.at(1) + ids.at(2) + ids.nth(0 as usize).value + keys.nth(0 as usize);
}
```

相关参考：[标准库 collections](std_collections.md)。

查询接口要区分语义：`contains` 只返回是否存在，`find` 返回 `optional`，`at` 是“调用者保证存在”的前置条件访问，缺 key 会 panic。`map::operator [](key)` 不是只读查询；key 不存在时会插入 `V{}` 并返回 value 引用，因此要求 `V` 可默认构造。`insert` / `insert_node` 对 `map` 和 `set` 都是不覆盖插入；重复 key 返回已有节点且 `inserted == false`，但传入的 key / value / node 已经按值接收并在调用内部被 move 消费。

## str 和 string

`str` 是 compiler-recognized 的只读字符串视图，标准库为它提供长度、数据指针、迭代和字典序比较。`string` 拥有字符存储，维护 trailing `'\0'`，可以借出 `str`。

```cp
main() -> i32
{
    let text = string{"hi"};
    text.push_back('!');
    text.append(" cp");
    text[0 as usize] = 'H';
    println("owned string = {}", text);
    return text.size() as i32;
}
```

相关参考：[标准库 text](std_text.md)、[类型系统](type_system.md)、[迭代](iteration.md)。

当前 `std.text` 的公开面很小：

- `str` 由编译器提供 `ptr` / `len` 字段和只读下标；导入 `std.text.str`、`std.text` 或 `std` 后才有 `size()`、`data()`、迭代，以及 `== != < <= > >=` / `<=>` 字典序比较。
- `str` 的所有标准库操作都按 `len` 限定的字符区间工作，不按 C 字符串规则遇到 `'\0'` 截断。比较、迭代、显示和 `size()` 都会把中间 `'\0'` 当作普通字符；`data()` 只借出底层 `char const*`，不表示这个视图拥有存储或一定能作为 nul-terminated 字符串使用。
- `string` 提供拥有式构造、copy / move、`data()`、`begin()`、`end()`、`as_str()`、`size()`、`capacity()`、`empty()`、`operator []`、`front()`、`back()`、`reserve()`、`clear()`、`push_back()`、`pop_back()`、`resize()` 和 `append(str)`。它没有 `c_str()`，需要裸指针时用 `data()`；需要只读视图时用 `as_str()`。
- `string{}` 构造空串；`string{text: str}` 拷贝 `text` 的当前内容。copy 构造和 copy 赋值都会拷贝字符内容；move 构造和 move 赋值接管底层 storage，并把源对象长度置为 0。move 后的源对象可以析构、重新赋值、`clear()` 或再次追加内容，但在重新分配前不要依赖它的 `data()` 非空或带 trailing `'\0'`。copy/move 赋值都显式处理自赋值。
- `string` 的内部 storage 额外保留一个 trailing `'\0'` 槽位。`capacity()` 返回可保存的字符数，不包含这个终止符槽位；`data()` 指向字符缓冲，`end()` 指向 `data() + size()`，`as_str()` 返回 `{ data(), size() }`，不把 trailing `'\0'` 计入 `str.len`。这个终止符是当前实现为运行时互操作保留的存储细节，不表示 `string` 文本内容不能包含中间 `'\0'` 字符。
- `operator []`、`front()`、`back()` 和 `pop_back()` 都使用 `assert` 表达前置条件。debug/check 模式下越界或空串访问会 panic；`--release` 会移除这些 assert，调用者必须保证 `index < size()` 或 `not empty()`。可写 `string` 的下标、`front()` 和 `back()` 返回 `char&`，const receiver 返回 `char const&`。
- `reserve(new_capacity)` 只保证容量至少达到 `new_capacity`；`new_capacity <= capacity()` 时不做事。增长时会申请新的 `string_storage`，按从低到高顺序复制当前字符，替换旧 storage，并重新写 trailing `'\0'`。
- `clear()` 只把长度置 0 并写终止符，不释放容量。`push_back(ch)` 在尾部追加一个字符；`pop_back()` 删除最后一个字符但不返回值；`resize(new_size, ch)` 缩小时截断，扩张时用 `ch` 填充新位置。
- `push_back`、`append` 和扩张式 `resize` 会按需自动增长容量；增长可能替换 storage 并使旧指针、视图、引用和迭代器失效。直接调用 `reserve(new_capacity)` 只保证容量至少达到传入值。
- `append(text: str)` 按当前 `text.size()` 追加字节级字符序列；它没有 Unicode 码点、编码校验或格式化能力。当前实现只特殊处理 `text.data() == data()` 的整串自追加，保证 `s.append(s.as_str())` 在可能扩容后仍从新的自身缓冲读取。对部分重叠的外部 `str` 视图没有额外重叠处理；不要把指向同一 `string` 中间区域的视图传给 `append` 后再依赖具体复制结果。
- `reserve`、`push_back`、`append` 和扩张式 `resize` 可能重新分配 storage；之前取得的 `data()` / `begin()` / `end()` 指针、`as_str()` 视图、下标引用或迭代器在这些修改后可能失效。`clear()`、`pop_back()` 和缩短式 `resize` 不释放 storage，但会改变长度和 trailing 终止符，旧的越界引用或视图长度不能继续使用。
- `string` 本身当前没有内建或标准库比较 operator，也没有 `find`、`contains`、`starts_with`、切片、split、trim、parse、`operator +` 或插值。需要比较两个拥有字符串时，先比较它们的 `as_str()`；需要组合文本时，用 `append` 或格式化输出。
- `std.io` 给 `str` 和 `string` 都实现了 `display`，因此可以直接传给 `{}`；这个能力来自 `std.io.format`，不是 `std.text.string` 自身的成员方法。

## compare

`std.compare` 提供排序和有序容器使用的三路比较分类、order object 和能力 concept。`std` 聚合导入会重导出它。

比较分类是普通 `variant` 值：

```cp
partial_ordering::{ less, equivalent, greater, unordered }
weak_ordering::{ less, equivalent, greater }
strong_ordering::{ less, equivalent, greater }
```

当前标准库 helper 只面向 `weak_ordering`：

```cp
is_less(value: weak_ordering) -> bool;
is_equivalent(value: weak_ordering) -> bool;
is_greater(value: weak_ordering) -> bool;
reverse_order(value: weak_ordering) -> weak_ordering;
```

这些 helper 不接受 `partial_ordering` 或 `strong_ordering` 实参，也不做隐式“unordered”处理。`is_less` 只在 `.less` 时返回 `true`，`is_equivalent` 只在 `.equivalent` 时返回 `true`，`is_greater` 只在 `.greater` 时返回 `true`；其它 case 都返回 `false`。`reverse_order` 把 `.less` 和 `.greater` 互换，`.equivalent` 保持不变。

`weak_ordering` 有 `to_weak()`，`strong_ordering` 也有 `to_weak()`；`partial_ordering` 当前没有 `to_weak()`，因此不能作为默认 `sort`、`map`、`set` 的 ordering 结果，也不能直接满足默认比较需要的弱序结果。

`asc<T>` / `desc<T>` 是零字段 order object。`asc<T>{}(left, right)` 调用 `left <=> right`，再调用结果的 `to_weak()`；`desc<T>{}` 反转 `asc<T>{}` 的结果。默认 order object 不会从 `<`、`<=`、`>`、`>=` 或 `==` 关系运算组合出三路比较；只有这些关系运算而没有可用 `<=>` 的类型，不能直接使用默认 `sort`、`map` 或 `set`。元素类型的 `<=>` 结果如果不是 `weak_ordering`，也必须提供返回 `weak_ordering` 的 `to_weak()`，否则默认升序/降序不能用于排序或有序容器。

`std.compare` 还为 `bool`、所有内建整数类型、`isize` / `usize` 和 `char` 提供同类型 `operator <=> -> weak_ordering` 扩展。它没有给 `f32` / `f64` 提供 `<=>`，也没有提供跨整数类型的三路比较；`1 as i32 <=> 1 as i64` 这类写法需要调用方先显式转换到同一类型，或提供自己的比较 operator。

`mutable_object`、`ordering<T>`、`equality_comparable<Rhs = this>`、`three_way_comparable<Rhs = this, Category = weak_ordering>` 和 `incrementable` 是编译器识别的标准库保留 concept 名。它们仍要在当前模块可见；未导入 `std.compare` 或 `std` 时不能当作全局关键字使用。当前实现对这些名字按名字触发内建判定，用户不应在其它模块复用同名 concept 表达不同语义，也不要依赖显式 `impl` 覆盖这些内建判定。`incrementable` 对内建整数直接成立；非整数类型必须提供前置 `++value`，且结果能隐式转换到 `this&`。`bool`、浮点、指针、数组、容器和只有后置 `value++` 的类型都不会自动满足 `incrementable`。

相关参考：[标准库 compare](std_compare.md)、[Concept](concept.md)、[运算符](operator.md)。

## ranges

`std.ranges` 使用 UFCS 点调用组合。`iota(begin, end)` 产生半开 view；`repeat(value)` 是无限 range，有限重复写作 `repeat(value).take(count)`；lvalue 容器、`span<T>`、`string` 或数组可以直接作为 adapter receiver，标准库会借用成 view。

```cp
main() -> i32
{
    return iota(0, 8)
        .filter(f(value: i32) -> bool { return value != 3; })
        .transform(f(value: i32) -> i32 { return value + 1; })
        .take(4 as usize)
        .fold(0, f(total: i32, value: i32) -> i32 { return total + value; });
}
```

相关参考：[标准库 ranges](std_ranges.md)、[迭代](iteration.md)。

当前 source 入口包括 `iota(begin, end)`、`empty<T>()`、`single(value)` 和 `repeat(value)`。`iota` 只提供二参半开范围，不提供 `iota(end)`；它靠 `==` 判断结束、靠前置 `++` 推进，不使用 `<` / `<=`。`empty<T>()` 需要显式元素类型。`single` / `repeat` 的 item 是值类型，不是引用；它们会把保存的 value 复制进 iterator，`repeat` 还会在每次 `next()` 复制 value。有限重复用 `repeat(value).take(count)`，当前没有 `repeat(value, count)` 二参重载。

当前 lazy adapter 包括 `take(count)`、`drop(count)`、`filter(predicate)`、`transform(mapper)`、`enumerate()`、`zip(other)` 和 `concat(other)`。adapter 只保存 source view 和 callable / 另一个 range，不会立即拉取全部元素；每次 `.iter()` 会重新创建内部 iterator。`filter` 保留原 item 类型，`transform` 才改变 item 类型；当前没有名为 `map` 的 adapter，也没有 `flat_map`、`cycle`、`reverse`、`chunk`、`windows`、`flatten` 或 `take_while`。`zip` 两侧任一结束就结束；`concat` 要求两侧 item 类型完全相同。

需要注意：range-for 的内建数组路径不等同于 ranges UFCS。`for(let x : array)` 是编译器支持；`array.count()`、`array.filter(...)` 则依赖 `std.ranges.sources` 中的数组 `iterable` / `const_iterable` impl，因此需要导入 `std.ranges` 或更高层的 `std`。`str` 也是类似的边界：`for(let ch : text)` 可用，但当前 `text: str` 不能稳定作为 ranges adapter / terminal receiver；需要字符串参与 ranges 时使用 `string` 或手写循环。

当前 terminal 只有 `count()`、`fold(init, op)`、`any(pred)`、`all_of(pred)` 和 `find(pred)`。它们都会先把 receiver 转成 view，再调用一次 `.iter()` 消费 iterator；terminal 本身不保存可复用状态。`any` / `all_of` / `find` 的谓词结果必须是 `bool`，不会使用整数或指针 truthiness。`find` 返回 iterator 产生的同一个 item 类型包装成 `optional`，source item 是引用时结果也是引用；对临时容器直接 `find` 后不要长期保存引用结果。第一版没有 `collect`、`to_vector`、`to<Container>()`、`sum`、`min`、`max` 或 `for_each` terminal。

## meta

`std.meta` 提供类型层工具，例如 `call_result<F, Args...>` 和 `callable<Args...>`。它用于标准库的长期泛型表达能力，而不是运行时反射。

当前入口包括类型查询 `read_type<T>`、`remove_reference<T>`、`pointee<T>`、`tuple_element<Tuple, Index>` 和 `call_result<F, Args...>`，以及编译器识别的 concept `callable<Args...>`、`is_lvalue_reference`、`is_const_lvalue_reference`、`is_move_reference`。这些名字必须通过 `std.meta`、`std` 或其它重导出链可见后才会按内建查询 / 内建 concept 处理；未导入时不是全局关键字。它们只接受类型实参或编译期 tuple 索引，不接受运行时值，也不提供字段枚举、函数列表枚举、属性查询或 CTAD 风格类型构造器推导。

```cp
apply<F>(value: i32, callback: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}
```

相关参考：[元编程与反射基础](meta.md)。

## 泛型算法

`std.algorithm.sort` 第一版提供 `sort` 和 `stable_sort`，`std.algorithm` 聚合模块会重导出该排序子模块，`std` 聚合导入也会继续重导出它们。只需要排序入口时可以导入 `std.algorithm.sort`；需要算法聚合入口时导入 `std.algorithm`；已经导入 `std` 时不需要再单独导入。两个入口都要求 receiver 满足 `contiguous_mutable_range`，并在取得 `data()` / `size()` 后原地重排元素。

```cp
main() -> i32
{
    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(1);
    values.push_back(2);
    sort(values);
    values.sort(desc<i32>{});

    let text = string{"cba"};
    stable_sort(text);
    return values[0 as usize] + text[0 as usize] as i32;
}
```

公开接口：

```cp
sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void;
stable_sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void;
```

相关参考：[标准库 algorithm](std_algorithm.md)、[标准库 memory](std_memory.md)、[Concept](concept.md)。

当前只有整段原地排序入口，返回类型都是 `void`。没有返回排序后 range 的链式 API，没有 sorted copy / collect 入口，也没有 `sort(first, last)`、`sort_by_key`、projection 参数或按字段名排序的专用重载。需要排序子区间时，第一版应先显式构造 `span<T>{ptr, len}` 指向要排序的连续区间；需要按 key 排序时，传入返回 `weak_ordering` 的 order object 或 lambda。

`values.sort(order)`、`fixed.sort(...)` 和 `text.sort(...)` 不是容器成员；它们是点号 UFCS 调用，按普通自由函数 `sort(values, order)` 检查。`vector<T>`、数组和 `string` 本身不拥有 `sort` 成员，因此未导入 `std.algorithm` 或 `std` 时，这种点号写法同样不可见。

可排序 receiver 包括可写 `vector<T>`、可写数组、`span<T>` 和 `string`。`string` 排序只重排当前 `size()` 范围内的字符，尾部 trailing `'\0'` 不属于排序区间。只读 `str` 视图不是可写 range，`raw_buffer<T>` 只拥有原始存储而不维护已构造元素数量，也不满足 `contiguous_mutable_range`。const `vector`、const 数组和 const `string` 会因为无法借出可写元素而被拒绝。元素类型还必须是可搬移、可写入的普通对象；引用、函数类型、`unit`、`!`、推导类型等不是当前排序元素类型。

排序入口会先调用 receiver 的 `data()` 和 `size()` 构造 `span`，再进入具体排序逻辑。自定义 `contiguous_mutable_range` 必须保证这两个函数在当前状态下可调用；即使 `size() == 0`，也不能依赖排序入口先跳过 `data()`。排序不会改变容器长度、容量或元素生命周期，只在现有连续区间内比较、move 和赋值元素。

入口参数写作 `R forward&`，因此临时 `vector<T>`、临时 `string` 或函数返回的可写连续 range 也能绑定并通过检查；排序会在这个临时对象内部完成，然后临时对象在表达式结束时被丢弃。也就是说 `sort(vector<i32>{});` 是合法但结果不可观察的调用，不会把排好序的容器返回出来。需要保留排序结果时，先把容器绑定到 `let` 局部变量，再调用 `sort(values)` 或 `values.sort(...)`。临时 `span<T>{ptr, len}` 则只是一个借用壳；排序会通过 span 写回它指向的外部元素。

`Order` 必须满足 `ordering<R::item>`：order object 以 `order(left, right)` 形式调用，`left` 和 `right` 是元素的 `const&`，返回值必须能转换到 `weak_ordering`。返回 `bool` 的二元谓词不是排序比较器。默认 `asc<T>` 基于 `<=>` 升序，`desc<T>` 反向。排序入口按值接收 `order`，而 `ordering<T>` 要求调用 `operator ()(self const&, left, right)`；因此比较器应被设计成值语义的 order object，不要依赖在排序过程中修改它自身字段后由调用点观察这些状态变化。

省略第二个参数时，调用点会按已经确定的 `Order` 类型 materialize 默认实参 `Order{}`。如果只写 `sort<Range, CustomOrder>(values)`，`CustomOrder` 仍必须能默认初始化；需要携带运行时状态或不可默认初始化的比较器时，应显式传入实例，例如 `sort(values, CustomOrder{...})`。显式传入 order 后，未使用的 `Order{}` 默认表达式不会额外检查。

`[T; 0]` 是合法语言数组类型，但当前标准库给数组实现 `contiguous_mutable_range` 时，`data()` 通过 `&self[0]` 取得首元素地址；`sort` / `stable_sort` 入口又会先调用 `values.data()` 再进入长度分支。因此不要把零长数组直接传给 `sort` 或 `stable_sort`。需要表达空可排序区间时，使用长度为 0 的 `span<T>` 或空 `vector<T>`。

`sort` 是原地排序，但不承诺稳定性；当前实现可能在部分输入上保留等价元素顺序，这不是接口保证。需要保持等价元素相对顺序时使用 `stable_sort`。两种排序都会比较并搬移元素，比较器和元素类型不应依赖排序过程中的访问顺序、比较次数或临时移动次数。

## 格式化输出

`std.io` 第一版只提供输出，不提供标准输入。底层输出流是 `output_stream{ fd: i32 }`，`stdout()` 使用 fd 1，`stderr()` 使用 fd 2；当前 runtime 只有 fd 2 映射到 stderr，其它 fd 都映射到 stdout，因此第一版不提供真实任意文件描述符输出。`write_str` 按 `str` 长度逐字符写出，中间 `'\0'` 不会按 C string 截断，也不会返回已经成功写出的字符数。

格式字符串支持 `{}` 占位，以及双左花括号 / 双右花括号输出字面量花括号。

这些占位只在 `format`、`format_to`、`print`、`println`、`eprint`、`eprintln` 或 `formatter.format` 扫描 `fmt` 参数时生效；普通字符串字面量不会自动插值，`"value = {x}"` 只是包含花括号字符的 `str`。

花括号扫描是单层的，不解析嵌套格式说明符。单独的 `{`、单独的 `}`、`{x` 这类未知左花括号序列都会返回 `invalid_escape`；`{:...}` 形式返回 `unsupported_specifier`，不是占位符，也不会消耗参数。

```cp
main() -> i32
{
    println("std = {}, stored = {}", "ok", 12);
    eprintln("error code = {}", 1);
    return 0;
}
```

公开接口：

```cp
variant formatter {
    stream(output_stream&);
    buffer(string&);
}

format<T...: display>(fmt: str, values: T...) -> format_result;
format_to<T...: display>(out: output_stream&, fmt: str, values: T...) -> display_result;
print<T...: display>(fmt: str, values: T...) -> display_result;
println<T...: display>(fmt: str, values: T...) -> display_result;
eprint<T...: display>(fmt: str, values: T...) -> display_result;
eprintln<T...: display>(fmt: str, values: T...) -> display_result;
```

相关参考：[标准库 IO](std_io.md)。

`format` 返回 `format_result::ok(string)` 或 `format_result::error(format_error)`；其它输出函数返回 `display_result::ok` 或 `display_result::error(format_error)`。格式化错误不会 panic，调用者可以通过 `match` 检查。`std.io.format` 会重导出 `std.text.string`，因此只导入格式化模块时，`format_result::ok(string)` 的 `string` 类型也可见；`std.io.raw` 本身只提供 `output_stream`、`stdout`、`stderr`、`write_char` 和 `write_str`。

`format_to` 的第一个参数是 `output_stream&`，必须传入可写的 stream lvalue，例如先写 `let out = stdout(); format_to(ref out, "{}", value);`。`format_to(stdout(), ...)` 这种把临时 stream 直接传给可写引用的写法不是当前能力。它只在调用期间使用这个 stream 引用，不保存 stream 或格式化参数。`print` / `println` / `eprint` / `eprintln` 则在函数内部各自构造 `stdout()` 或 `stderr()` 局部 stream，不需要调用点提供 stream。

`formatter` 本身也是公开类型，两个 case 都持有可写引用：`formatter::stream(ref out)` 包装已有 `output_stream`，`formatter::buffer(ref text)` 包装已有 `string`。调用者一般不需要手动构造它，只有自定义 `display` 组合输出或测试显示实现时才会直接接触。

格式字符串在运行时按 `str` 扫描，不做编译期字面量解析或占位符数量检查。泛型约束 `T...: display` 只证明每个值参数都能显示；它不会证明 `{}` 的数量和参数数量相等，也不会提前拒绝 `"{:"`、`"{x"` 或单独的 `"}"`。因此格式串可以来自变量或函数返回值，所有转义、占位符和参数数量错误都会通过 `format_result::error(...)` / `display_result::error(...)` 在调用时报告。

格式化是边扫描边写出：普通文本和已经完成的参数会先写入目标，之后如果遇到格式串错误或参数显示错误，函数返回 `error(...)`，但不会回滚已经写出的内容。`format(...)` 失败时不返回内部部分 buffer；stream 输出失败前的内容已经发送到底层 `output_stream`。`println` / `eprintln` 只有在格式串和所有参数都成功写出后，才追加最后的 `'\n'`。

`format_error` 当前包括：

- `placeholder_too_few`：格式串中还剩 `{}`，但值参数已经用完。
- `argument_too_many`：值参数还没用完，格式串已经结束。
- `invalid_escape`：出现单独的 `{`、`}` 或未知花括号转义。
- `unsupported_specifier`：出现 `{:...}` 形式；第一版不支持格式说明符。
- `output_failed`：底层输出目标写入失败。自定义 `display` 实现可以返回任意 `format_error`，格式化入口会原样传播。

不支持宽度、精度、对齐、命名参数、位置参数或自定义格式说明符。底层 stream 写入失败时，格式化函数返回 `output_failed`。

内置 `display` 实现覆盖 `bool`、所有内建整数、`f32` / `f64`、`char`、`str` 和标准库 `string`。`bool` 输出小写 `true` / `false`；整数按十进制输出；`char` 直接写出该字符本身，不加引号、不转义；`str` 按 `str.len` 写出完整视图区间，`string` 先借出 `as_str()` 再写出。浮点当前固定输出整数部分、`.` 和 6 位小数，不做四舍五入，也不处理宽度、科学计数或 NaN/Inf 专门拼写。

`display` 是普通标准库 concept，不是自动派生能力。数组、tuple、指针、`optional<T>`、`expected<T,E>`、`vector<T>`、`map<K,V>`、`set<K>` 和用户自定义 `struct` / `variant` 当前都没有内建 display；把这些值传给 `{}` 会因为缺少 `display` 实现而在语义检查时报错。

用户类型需要格式化时实现：

```cp
impl display for point {
    display(self const&, out: formatter&) -> display_result
    {
        return out.format("point({}, {})", x, y);
    }
}
```

`display` 实现接收的是 `self const&`，因此格式化协议本身不提供可写 receiver。实现体应通过 `formatter` 写出内容并传播错误：

```cp
out.write_char(ch) -> display_result;
out.write_str(text) -> display_result;
out.format(fmt, values...) -> display_result;
```

`formatter` 是内部输出目标的变体，可能指向 stream，也可能指向 `format(...)` 使用的临时 `string` buffer。自定义 `display` 不应假设目标一定是 stdout/stderr，也不应直接调用 `print` / `println` 作为子格式化；直接打印会绕过传入的 formatter，导致 `format(...)` 无法把内容收进返回的 string。

## 文件 IO

`std.fs` 提供同步文件 IO。`std.fs` 是聚合入口，会重导出 `std.fs.file`；只导入 `std.fs.file` 时可以看到文件 API，但不会顺带导出 `expected`、`span` 或 `str` 扩展方法。调用点需要显式写这些类型或方法时，应额外导入 `std.core`、`std.memory`、`std.text`，或直接导入聚合入口 `std`。

`file::open` 返回 `expected<file, io_error>` 风格结果，常用 `match` 处理成功和失败分支。

```cp
main() -> i32
{
    let path: str = "/tmp/cp-example-fs.txt";

    match file::open(path, open_options{}.write().create().truncate()) {
        .value(out) => {
            out.write_str("cp");
            out.close();
        },
        .unexpected(error) => {
            return 1;
        },
    };

    return 0;
}
```

相关参考：[标准库 fs](std_fs.md)。

第一版 `std.fs` 只有文件句柄上的同步顺序 `open` / `read` / `write` / `write_str` / `close`。它不提供目录遍历、创建或删除目录、删除或重命名文件、metadata 查询、seek/tell、flush/fsync、异步 IO、非阻塞 IO、内存映射、权限模型、路径规范化或跨平台路径抽象；这些能力不能通过直接拼 `file_handle` 或 `open_options` 绕出来，需要后续显式扩展 runtime 和标准库 API。

`file` 是 move-only RAII 句柄封装：copy 构造和 copy 赋值被删除，move 构造/赋值会把源句柄置空。析构路径不能返回错误。需要观察 close 失败时，应显式调用 `close()` 并检查 `expected<usize, io_error>`；关闭已经为空的句柄会返回 `value(0)`。非空句柄 close 时会先把对象置空，再调用 runtime，因此即使底层 close 失败，后续析构也不会再次关闭同一个 raw handle。读写一个已经关闭的 file 会经 runtime 返回失败，不是成功 no-op。`open_options` 是 opaque bitset，KCP 层不验证 flag 组合，实际能否打开取决于 runtime 的 mode 映射。

`read`、`write` 和 `write_str` 只在调用期间使用传入的 `span` / `str` 指针和长度，不保存这些视图。零长度读写没有单独成功快路径：如果传入的 data 指针是 null，当前 runtime 仍会返回失败。需要把零长度视为成功时，应在调用前自行判断长度，或传入非 null 的有效缓冲区指针。完整规则见 [标准库 fs](std_fs.md)。
