# 标准库 ranges

本文档记录 `std.ranges` 的第一批标准库范围对象。`std.ranges` 建立在 [iteration.md](iteration.md) 的 `iterator` / `iterable` 协议上，不新增 `for` 语法，也不引入编译器特权。

## 模块边界

```cp
export module std.ranges;

export import std.ranges.iota;
```

`std.ranges` 是聚合模块，只负责重导出具体 ranges 子模块。具体范围对象放在 `std/ranges/` 目录中，每个能力使用独立模块：

```text
std/ranges.cp          -> export module std.ranges;
std/ranges/iota.cp     -> export module std.ranges.iota;
```

具体子模块只提供可以被范围 `for` 消费的普通库类型和函数。范围对象负责保存遍历边界，iterator 负责保存推进状态。使用者通常通过函数创建范围对象，而不是直接写底层 iterator 类型。

聚合模块 `std` 重导出 `std.ranges`，因此导入 `std` 后可以直接使用第一批 ranges 入口。

## iota

`iota` 产生泛型半开序列 `[begin, end)`，每次迭代产生一个 `T`。`T` 必须能用 `==` 判断终点，并能用前置 `++` 推进。

```cp
import std.ranges;

sum_to(end: i32) -> i32
{
    let values = iota(0, end);
    let total = 0;

    for(let value : values) {
        total += value;
    }

    return total;
}
```

公共入口：

```cp
export module std.ranges.iota;

export import std.compare;
export import std.core.iter;
export import std.core.option;

iota<T: equality_comparable<T> and T: incrementable>(begin: T, end: T) -> iota_range<T>
```

规则：

- `iota(begin, end)` 表示 `[begin, end)`。
- 终止条件是 `current == end`。
- 推进方式是 `++current`，因此 `incrementable` 只要求前置 `++`。
- `iota` 不假定 `<`、`>=`、`+= 1` 或整数类型；如果 `begin` 经过有限次 `++` 不能到达 `end`，循环是否终止由该类型自己的语义决定。
- 第一版不支持负步长、无界区间和闭区间。

因为 cp 当前不支持普通函数重载，`iota(end)` 和 `iota(begin, end)` 不能同名共存。第一版只保留两个参数形式；`[0, end)` 直接写作 `iota(0, end)`。

当前 `iterable` 协议要求 `iter(self&)`，因此直接遍历临时范围对象需要 range-for 额外设计临时值物化规则。第一版示例先把 `iota(0, end)` 绑定到局部变量，再交给 `for` 消费；这保持 `iota_range` 是可重复创建 iterator 的范围对象，而不是退化成一次性 iterator。

## 底层类型

`iota_range` 是可重复创建 iterator 的范围对象：

```cp
export struct iota_range<T: equality_comparable<T> and T: incrementable> {
    begin: T;
    end: T;
}
```

`iota_iter` 是有状态 iterator：

```cp
export struct iota_iter<T: equality_comparable<T> and T: incrementable> {
    current: T;
    end: T;
}
```

`iota_range implements iterable`，`iota_iter implements iterator`。范围 `for` 对 `iota(0, n)` 的处理与其它 `iterable` 完全相同：先调用 `iter()` 得到 iterator，再反复调用 `next()`。

## 命名原则

第一批 `std.ranges` 使用以下命名规则：

- 用户常用入口优先使用自由函数，例如 `iota(0, n)`。
- 底层范围对象使用 `_range` 后缀，例如 `iota_range`。
- 底层 iterator 使用 `_iter` 后缀，例如 `iota_iter`。
- 因为普通函数不支持重载，不用参数数量区分 API；需要多种入口时使用清晰的不同名字。
