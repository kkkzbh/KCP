# 标准库 algorithm

本文档记录 `std.algorithm` 第一版公共算法接口。当前聚合模块只重导出：

```text
std.algorithm.sort
```

也就是说，第一版算法库只有原地排序入口 `sort` 和 `stable_sort`。它们都通过 `contiguous_mutable_range` 协议取得连续可写区间，不提供链式返回、sorted copy 或泛型 collect。

## 导入

只需要排序入口时可以写：

```cp
import std.algorithm.sort;
```

这只把 `sort` / `stable_sort` 入口带入当前文件。默认升序排序可以直接使用这些入口，因为 `asc<R::item>` 和 `ordering<R::item>` 是函数签名内部的默认类型和约束；但调用点如果要显式写 `desc<T>{}`、`weak_ordering`、`span<T>`、`vector<T>` 或 `string`，仍需要导入对应模块，或者直接导入 `std`。例如只导入 `std.algorithm.sort` 时可以排序当前文件中已有的数组：

```cp
import std.algorithm.sort;

main()
{
    let values: [i32; 3] = [3, 1, 2];
    sort(values);
}
```

但 `sort(values, desc<i32>{})` 需要 `desc` 可见，应额外 `import std.compare;` 或使用 `import std;`。同理，构造 `vector<i32>{}` 需要 `std.collections.vector` / `std.collections` / `std`，构造 `span<T>{...}` 需要 `std.memory.span` / `std.memory` / `std`，构造 `string{"..."}` 需要 `std.text.string` / `std.text` / `std`。

需要算法聚合入口时写：

```cp
import std.algorithm;
```

`import std;` 也会重导出算法入口。`std.algorithm` 不是容器模块；`import std.collections;` 只能看到 `vector`、`map` 和 `set`，不会带来 `sort` / `stable_sort`。

## 公开接口

```cp
sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void;
stable_sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void;
```

两个函数都会把 receiver 转成：

```cp
span<R::item>{values.data(), values.size()}
```

然后在这段区间上原地重排元素。返回类型是 `void`；排序后的结果留在传入对象或传入 `span` 指向的外部区间中。`data()` 和 `size()` 都会在排序入口处被调用；自定义 `contiguous_mutable_range` 必须保证这两个函数在当前对象状态下可调用，并且 `[data(), data() + size())` 是已经构造的连续可写元素区间。即使 `size() == 0`，当前入口也不会先跳过 `data()` 调用。

## 可排序 receiver

可作为 `values` 的公开类型包括：

- 可写 `span<T>`。
- 可写数组 `[T; N]`，但见零长数组限制。
- 可写 `vector<T>`。
- 可写 `string`，按当前长度内的字符排序；尾部 trailing `'\0'` 不属于排序区间。
- 用户自定义并满足 `contiguous_mutable_range` 的类型。

不能直接排序：

- `raw_buffer<T>`：它只拥有原始存储，没有已构造元素长度，也不实现 `contiguous_mutable_range`。需要排序其中已构造前缀时，显式构造 `span<T>{buffer.data(), len}`。
- `str`：它是只读视图，不是可写连续 range。
- const `vector`、const 数组、const `string` 或 const `span`：这些 receiver 不能借出可写元素。
- `map` / `set`：它们是有序关联容器，不是连续可写 range。

`R::item` 还必须满足 `mutable_object`。引用、函数类型、内部 `unit`、`!`、推导类型和其它不能作为可写对象存放的类型不能作为排序元素。

满足 `contiguous_mutable_range` 不表示算法会替 receiver 管理生命周期或容量。排序只会在 `data()` / `size()` 给出的现有区间内比较、move 和赋值元素；不会构造新元素、改变容器长度、扩容或释放存储。

## 调用形式

普通函数调用和点号 UFCS 都可用：

```cp
sort(values);
sort(values, desc<i32>{});
values.sort();
values.sort(desc<i32>{});
stable_sort(values);
```

`values.sort(...)` 不是 `vector` / 数组 / `string` 的成员函数；它按普通自由函数 `sort(values, ...)` 查找。未导入 `std.algorithm.sort`、`std.algorithm` 或 `std` 时，点号写法同样不可见。

`values` 参数是 `R forward&`，因此临时可写连续 range 也能绑定：

```cp
sort(vector<i32>{});
stable_sort(string{"cba"});
```

这种调用合法但结果通常不可观察，因为临时对象在表达式结束后被丢弃。需要保留结果时，先绑定到局部变量：

```cp
let values = vector<i32>{};
values.push_back(3);
values.push_back(1);
sort(values);
```

临时 `span<T>{ptr, len}` 只是借用壳；排序会写回它指向的外部元素。

## 比较器

`Order` 必须满足 `ordering<R::item>`。order object 以：

```cp
order(left, right)
```

形式调用，其中 `left` 和 `right` 是元素的 `const&`，返回值必须能转换到 `weak_ordering`。返回 `bool` 的二元谓词不是排序比较器。

默认 `Order` 是 `asc<R::item>`，也就是基于元素的 `<=>` 做升序排序。降序可以传 `desc<T>{}`：

```cp
sort(values, desc<i32>{});
```

自定义比较器可以是函数、函数指针、lambda / 闭包，或自身实现 `operator ()(self const&, left: T const&, right: T const&) -> weak_ordering` 的 struct / variant 对象。这里的“自身实现”按 `std.compare ordering<T>` 概念检查，不把当前作用域可见的顶层 call operator 或 extension call operator 当作普通类型满足排序比较器的依据。

排序入口按值接收 `order`；比较器应设计成值语义对象，不要依赖排序过程中修改比较器字段再由调用点观察状态变化。

省略第二个参数时，调用点会 materialize 默认实参 `Order{}`。如果显式写了自定义 `Order` 类型但没有传实例：

```cp
sort<R, custom_order>(values);
```

那么 `custom_order{}` 必须可默认初始化。需要携带运行时状态或不可默认初始化的比较器时，应显式传入实例：

```cp
sort(values, custom_order{ .pivot = 10 });
```

显式传入 order 后，未使用的 `Order{}` 默认表达式不会额外检查。

## sort

`sort` 是 in-place 混合排序，不承诺稳定性。

当前实现可能在部分输入上保留等价元素顺序；这是实现选择，不是接口承诺。需要保持等价元素相对顺序时使用 `stable_sort`，不要依赖 `sort` 的当前行为。

## stable_sort

`stable_sort` 保持等价元素的相对顺序。

较大输入会使用临时 buffer 做归并，元素会发生 move / assignment。比较器和元素类型不应依赖排序过程中的访问顺序、比较次数或临时移动次数。

## 零长数组

`[T; 0]` 是合法语言数组类型，也能被 range-for 零次遍历。但当前 `std.memory.span` 给数组实现 `contiguous_mutable_range` 时，`data()` 通过 `&self[0]` 取得首元素地址；`sort` / `stable_sort` 又会先调用 `values.data()` 再看长度。

因此不要把零长数组直接传给排序入口：

```cp
type empty = [i32; 0];
let values = empty{};
// sort(values); // 不作为公开可依赖能力
```

需要空可排序区间时，使用长度为 0 的 `span<T>` 或空 `vector<T>`。

## 不支持内容

第一版 `std.algorithm` 不提供：

- `sort(first, last)` 指针/iterator 双端入口。
- `sort_by_key`、projection 参数、按字段排序或返回 `bool` 的比较谓词入口。
- `is_sorted`、`binary_search`、`lower_bound`、`upper_bound`、`partition`、`reverse`、`rotate`、`unique`、`remove_if` 等其它算法。
- sorted copy、链式返回、`collect`、`to_vector` 或 ranges terminal。
- 针对 `map` / `set` 的重新排序能力；关联容器的顺序由 `Order` 和插入/删除维护。
