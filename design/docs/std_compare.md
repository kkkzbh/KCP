# 标准库 compare

本文档记录 `std.compare` 第一版公共接口。它提供三路比较结果类型、排序 / 有序容器使用的 order object，以及一组编译器按名字识别的保留 concept。

`std.compare` 不是隐式 prelude。需要使用 `weak_ordering`、`asc<T>`、`ordering<T>` 或这些扩展 `<=>` 时，必须导入 `std.compare`、`std`，或导入其它重导出它的模块。

## 比较分类

公开三种比较结果：

```cp
variant partial_ordering {
    less;
    equivalent;
    greater;
    unordered;
}

variant weak_ordering {
    less;
    equivalent;
    greater;
}

variant strong_ordering {
    less;
    equivalent;
    greater;
}
```

`partial_ordering` 可以表达 `unordered`，但当前默认排序和有序容器协议不接受它作为最终 order 结果。`weak_ordering` 是 `sort`、`map`、`set` 当前使用的标准结果类型。`strong_ordering` 可以通过 `to_weak()` 降成 `weak_ordering`。

公开 helper：

```cp
is_less(value: weak_ordering) -> bool;
is_equivalent(value: weak_ordering) -> bool;
is_greater(value: weak_ordering) -> bool;
reverse_order(value: weak_ordering) -> weak_ordering;
```

这些 helper 只接受 `weak_ordering`，不接受 `partial_ordering` 或 `strong_ordering`。`is_less` 只在 `.less` 时为 true，`is_equivalent` 只在 `.equivalent` 时为 true，`is_greater` 只在 `.greater` 时为 true。`reverse_order` 交换 `.less` 和 `.greater`，保留 `.equivalent`。

转换 helper：

```cp
weak_ordering::to_weak(self) -> weak_ordering;
strong_ordering::to_weak(self) -> weak_ordering;
```

当前 `partial_ordering` 没有 `to_weak()`。因此返回 `partial_ordering` 的 `<=>` 不能直接用于默认 `asc<T>` / `desc<T>`，也不能直接满足 `ordering<T>`。

## 保留 concept

`std.compare` 声明这些 concept：

```cp
concept mutable_object;

concept ordering<T> {
    operator ()(self const&, left: T const&, right: T const&) -> weak_ordering;
}

concept equality_comparable<Rhs = this> {
    operator ==(self const&, rhs: Rhs const&) -> bool;
}

concept three_way_comparable<Rhs = this, Category = weak_ordering> {
    operator <=>(self const&, rhs: Rhs const&) -> Category;
}

concept incrementable {
    operator prefix ++(self&) -> this&;
}
```

这组名字在当前编译器里有保留判定逻辑：使用点会先按内建规则检查能力，再考虑普通显式 `impl`。显式写 `impl equality_comparable for T {}` 不能让没有可用 `==` 的类型通过 `T: equality_comparable`；显式写 `impl incrementable for T {}` 也不能绕过前置 `++` 返回值必须能隐式转换到 `this&` 的要求。

这些判定按当前可见的 concept 符号名触发，公共代码应把这组名字当作标准库保留名字使用，不要在其它模块复用同名 concept 或比较分类表达不同语义。

保留 concept 的实际边界：

- `mutable_object`：类型必须能作为可写对象存放。引用、函数类型、内部 `unit`、`!`、推导类型、泛型参数和 pack expansion 不满足；builtin、数组、storage、tuple、指针、`struct`、`enum`、opaque alias 和 `variant` 满足。
- `ordering<T>`：目标类型必须能作为 order object 调用 `order(left, right)`，其中 `left` / `right` 是 `T const&`，返回值必须能隐式转换到当前可见的 `weak_ordering`。可用形式包括函数类型、函数指针、lambda / 闭包，或提供 `operator ()(self const&, left: T const&, right: T const&) -> weak_ordering` 的 `struct` / `variant`。
- `equality_comparable<Rhs>`：`left == right` 必须可用，结果必须能隐式转换到 `bool`。
- `three_way_comparable<Rhs, Category>`：`left <=> right` 必须可用，结果必须能隐式转换到 `Category`。
- `incrementable`：内建整数类型直接满足；非整数类型需要前置 `++value` 可用，且结果必须能隐式转换到 `this&`。这里的“整数”按编译器内建整数分类判断，`bool`、`f32` / `f64`、指针、数组和用户容器不会因为有其它更新语法或算术能力而自动满足。只有后置 `value++` 的类型也不满足 `incrementable`；需要用于 `iota` 时必须提供 `operator prefix ++(self&) -> this&`。

这些 concept 仍然要通过导入可见后才能在源码中写出来；它们不是全局关键字。不要在其它模块复用同名 concept 表达不同语义，也不要依赖显式 `impl` 覆盖这些名字的内建判定。

## 默认 order object

`asc<T>` 和 `desc<T>` 是零字段 order object：

```cp
struct asc<T> {
}

struct desc<T> {
}
```

`asc<T>{}(left, right)` 调用：

```cp
(left <=> right).to_weak()
```

`desc<T>{}(left, right)` 调用 `asc<T>{}` 后再 `reverse_order(...)`。

规则：

- `asc<T>` 要求 `left <=> right` 可用，并且比较结果有 `to_weak()`。
- 默认 order object 不会从 `<`、`<=`、`>`、`>=` 或 `==` 组合出三路比较。只有这些关系运算但没有 `<=>` 的类型，不能直接用默认 `sort`、`map` 或 `set`。
- `partial_ordering` 没有 `to_weak()`，所以返回 `partial_ordering` 的 `<=>` 不能直接用于 `asc<T>`。
- `desc<T>` 只是反转 `asc<T>`，不重新定义比较语义。
- `asc<T>` / `desc<T>` 没有运行时状态；需要有状态比较器时，定义自己的 order object 并显式传给算法或容器类型。

## 内建类型比较扩展

`std.compare` 为以下类型提供同类型 `<=> -> weak_ordering` 扩展：

```text
bool
i8 i16 i32 i64
u8 u16 u32 u64
isize usize
char
```

这些扩展都只覆盖左右类型完全相同的比较。例如 `1 as i32 <=> 2 as i32` 可用，`1 as i32 <=> 2 as i64` 不可直接使用；需要先显式转换到同一类型。

当前没有为 `f32` / `f64` 提供 `<=>`。浮点值不能直接使用默认 `asc<f64>` / `desc<f64>`，也不能直接作为默认排序或有序容器 key。需要浮点排序时，应提供自己的 order object，并明确处理 NaN、等价类和排序前置条件。

`str` 的字典序比较扩展不在 `std.compare` 中，而在 `std.text.str` 中。导入 `std` 时两者都可见；只导入 `std.compare` 不会给 `str` 带来字符串比较。

## 与排序和有序容器的关系

`std.algorithm.sort` 的默认形式使用：

```cp
sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void;
```

`map<K, V, Order = asc<K>>` 和 `set<K, Order = asc<K>>` 也依赖 `Order: ordering<K>`。

因此：

- 对内建整数、`bool`、`char`，导入 `std.compare` / `std` 后默认排序和默认 map/set key 比较可用。
- 对 `struct`，可以在固有 `impl` 中显式写 `operator <=> = default;`，或实现自己的 `<=>` 返回带 `to_weak()` 的比较分类。
- 对只提供 `operator ==` 或关系运算但没有 `<=>` 的类型，默认 `asc<T>` 不可用。
- 对需要自定义顺序的类型，传入或指定一个满足 `ordering<T>` 的 order object。

默认三路比较的边界也会影响 `asc<T>`：`operator <=> = default` 当前只支持非泛型 `struct` 目标，receiver 必须是 `self const&`，右操作数必须是 `this const&`，返回类型应写 `weak_ordering`。默认比较按字段声明顺序逐字段比较；字段是 enum 时用 enum 内建 `<=>`，其它字段按普通 `<=>` 查找。字段比较结果如果不是直接可转成 `weak_ordering`，必须提供无额外实参的 `to_weak()` 成员并返回 `weak_ordering`。`variant`、泛型 `struct`、缺少可比较字段、字段 `<=> = delete`、返回 `partial_ordering` 且没有 `to_weak()` 的字段，都会让默认比较不可用。需要跳过字段、改变排序 key 或处理资源句柄时，应手写 `<=>` 或手写 order object。

示例：

```cp
struct point {
    x: i32;
    y: i32;
}

impl point {
    operator <=>(self const&, rhs: point const&) -> weak_ordering = default;
}

main()
{
    let values = vector<point>{};
    sort(values); // 使用 asc<point>
}
```

有状态或非默认顺序：

```cp
struct by_y_desc {
}

impl by_y_desc {
    operator ()(self const&, left: point const&, right: point const&) -> weak_ordering
    {
        return desc<i32>{}(left.y, right.y);
    }
}

main()
{
    let values = vector<point>{};
    sort(values, by_y_desc{});
}
```

## 不支持内容

第一版 `std.compare` 不提供：

- 浮点默认三路比较。
- 跨整数类型 `<=>`。
- 从 `<` / `==` 自动合成 `asc<T>` 所需三路比较。
- `partial_ordering::to_weak()` 或 unordered 的默认降级策略。
- `std::less` / `std::greater` 风格二元 bool comparator 协议；排序和 map/set 使用 `weak_ordering` order object。
- 自动派生所有用户类型比较；除默认三路比较支持的 `struct` 字段展开外，用户类型需要显式提供比较能力。
