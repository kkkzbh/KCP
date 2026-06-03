# 标准库 ranges

`std.ranges` 是建立在 [iteration.md](iteration.md) 的 `iterable` / `const_iterable` 协议上的标准库范围层。用户层组合只接收 range/view；`iterator` 是 `.iter()` 产生的内部游标，不能直接用于 range-for 或 adapter。

```cp
let total = iota(0, n)
    .filter(f(value: i32) -> bool { return value != 3; })
    .transform(f(value: i32) -> i32 { return value + 1; })
    .take(10 as usize)
    .fold(0, f(sum: i32, value: i32) -> i32 { return sum + value; });
```

所有 source、adapter 和 terminal 都是普通自由函数，并通过 UFCS 写成点调用。`std.ranges` 不引入特殊语法，也不把某个函数做成编译器特权。

## 模块边界

```text
std/ranges.cp              -> export module std.ranges;
std/ranges/sources.cp      -> export module std.ranges.sources;
std/ranges/iota.cp         -> export module std.ranges.iota;
std/ranges/adapters.cp     -> export module std.ranges.adapters;
std/ranges/terminals.cp    -> export module std.ranges.terminals;
```

聚合模块重导出 `std.compare`、sources、iota、adapters 和 terminals。`std` 重导出 `std.ranges`，因此 `import std;` 后可以直接使用这些入口。

## View 边界

`view` 是可保存、可组合的 `iterable`。它是普通 concept，当前定义只要求目标类型实现 `iterable`，不要求一定实现 `const_iterable`。标准库提供三类桥接 view：

- `ref_view<R>`：保存 `R*`，用于可写左值 range。
- `const_ref_view<R>`：保存 `R const*`，用于 const 左值 range。
- `owning_view<R>`：保存 `R`，用于右值或函数返回的非 view range。

统一入口是：

```cp
to_view<R>(source: R forward&)
```

`to_view` 用 `decltype(forward source)` 搭配 `std.meta` 的 reference concept 分发。adapter 第一参数统一写成 `R forward&`，然后在函数体内把 `forward source` 转成 view：

```cp
first_two<R>(source: R forward&)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return take_view<V>{ .source = move view, .count = 2 as usize };
}
```

这意味着自定义 adapter 仍然是普通库函数，可继续通过 UFCS 调用。

桥接规则：

- `to_view` 自身接收 `source: R forward&`，并只对这个参数执行 `forward source`；它依赖 `decltype(forward source)` 区分可写左值、const 左值和右值。
- 可写左值产生 `ref_view<R>`，它的 `iter(self&)` 和 `iter(self const&)` 都转发到原始 `R::iter(self&)`，因此元素类型仍是可写侧的 `R::iter_item`。
- const 左值产生 `const_ref_view<R>`，要求 `R: const_iterable`，元素类型来自 `R::const_iter_item`。
- 右值或 `move` 实参产生 `owning_view<R>`，把 source 存进 view；只有当 `R` 同时实现 `const_iterable` 时，`owning_view<R>` 的 const receiver 才能迭代。
- adapter 保存的是 view，不是裸 iterator。每次对 adapter 结果调用 `.iter()` 都会重新从保存的 source view 产生 iterator。

## Sources

`iota(begin, end)` 产生半开 view `[begin, end)`。`T` 需要满足 `equality_comparable<T>` 和 `incrementable`；推进只使用前置 `++current`。不提供 `iota(end)`。

```cp
let indices = iota(0, 4);
```

其它 source：

- `empty<T>()`：空 view。由于没有 CTAD，调用点必须显式给出元素类型。
- `single(value)`：单元素 view。
- `repeat(value)`：无限 view，有限重复写作 `repeat(value).take(count)`。

`single` 和 `repeat` 的 iterator item 都是值类型 `T`，不是引用。`repeat(value)` 每次 `next()` 都返回一个 `T` 值；对需要移动才可产生的类型，不应依赖 `repeat` 做无限复制。

普通容器、数组和 `str` 不需要额外包装。左值直接作为 adapter receiver，`to_view` 会选择借用 view：

```cp
let values: [i32; 3] = [1, 2, 3];
let positives = values.filter(f(value: i32&) -> bool { return value > 0; });
```

const 左值 range 会走 `const_ref_view`，谓词或 mapper 必须能接收 const iterator item：

```cp
const fixed = values;
let count = fixed.filter(f(value: i32 const&) -> bool { return value > 0; }).count();
```

函数返回值或临时 range 会走 `owning_view`，因此可以直接组合：

```cp
make_values().enumerate().count();
iota(0, 3).count();
```

## Lazy Adapters

Adapter 不立即拉取全部元素，只保存一个 source view，并在 `.iter()` 时产生新的游标。公开 adapter 入口统一接收 `R forward&`，返回 view：

```cp
filter<R, P>(source: R forward&, predicate: P)
```

`filter` / `any` / `all_of` / `find` 的谓词结果必须是 `bool`。`transform` 的元素类型是 `call_result<F, item>`，依赖 [meta.md](meta.md) 的 callable result 类型查询。

当前公开 adapter：

- `take(count)`：最多保留前 `count` 个元素。
- `drop(count)`：跳过前 `count` 个元素。
- `enumerate()`：产生 `(usize, item)`。
- `zip(other)`：产生 `(left_item, right_item)`，任一侧结束即结束。
- `concat(other)`：要求两侧 item 类型相同，先遍历左侧再遍历右侧。

规则：

- `take(0)` 立即结束；如果 source 提前结束，结果也提前结束。
- `drop(count)` 在第一次取值前按需消费并丢弃最多 `count` 个元素；source 提前结束时结果为空。
- `filter(predicate)` 保留原始 item 类型。source item 是引用时，通过 `filter` 后仍是引用。
- `transform(mapper)` 产生 mapper 返回类型的值；如果 mapper 返回引用，则 item 类型也可以是引用。
- `enumerate()` 的 index 从 `0 as usize` 开始，每产生一个元素递增一次。
- `zip(left, right)` 要求两侧都是 range/view 输入，不接受裸 iterator。
- `concat(left, right)` 要求两侧 `iter_item` 完全相同；`T&` 和 `T const&`、`i32` 和 `i64` 都不是同一个 item 类型。
- 当前没有 `cycle`、`reverse`、`chunk`、`windows`、`flatten` 或 `take_while`。

## Terminals

Terminal 消费 iterable/view，内部调用 `.iter()`：

- `count()`：返回元素数量。对无限 range 调用不会终止。
- `fold(init, op)`：从左到右累积。
- `any(pred)`：存在满足谓词的元素时返回 `true`。
- `all_of(pred)`：所有元素满足谓词时返回 `true`。
- `find(pred)`：返回第一个满足谓词的 `optional<item>`。

规则：

- `fold(init, op)` 的 accumulator 初始值按值保存，循环中使用 `state = op(move state, item)` 更新。
- `any` 找到第一个满足谓词的元素后立即返回 `true`；空 range 返回 `false`。
- `all_of` 找到第一个不满足谓词的元素后立即返回 `false`；空 range 返回 `true`。
- `find` 找到元素后返回 iterator 产生的同一个 item 类型；source item 是引用时，返回 `optional<T&>` 或 `optional<T const&>`。
- 谓词和 fold/transform 回调都是普通 callable，必须满足 `std.meta` 的 `callable` / `call_result` 检查。

无限 range 是合法表达能力。`iterator.next()` 不承诺最终返回 `.none`，终止性由 terminal 或调用者控制。

## 组合规则

Source 和 adapter 的输出都是 view，因此可以保存并重复参与组合：

```cp
let indexed = iota(0, n).drop(2 as usize).take(5 as usize).enumerate();
let total = indexed.count();
```

裸 iterator 不再是 ranges 输入：

```cp
for(let value : values.iter()) {
    // error
}

values.iter().count(); // error
```

## to 的预留

当前不提供 `to()`，也不提供 `to_vector()`。

长期目标是统一的泛型 terminal，例如：

```cp
range.to<Container>()
```

具体设计必须等 CTAD 或等价的“类型构造器实参推导”规则明确后再落地。目标容器类型和 range 元素类型需要共同决定最终容器实例；在这条规则缺失前，先做 `to_vector()` 会把标准库推向一次性接口。

同理，当前也不提供 `first()`、`last()`、`sum()`、`min()`、`max()` 或 `collect()` 这类 terminal。需要这些能力时，第一版使用 `fold`、`find` 或手写循环表达。
