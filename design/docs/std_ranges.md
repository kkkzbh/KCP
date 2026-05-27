# 标准库 ranges

`std.ranges` 是建立在 [iteration.md](iteration.md) 的 `iterator` / `iterable` 协议上的标准库范围层。`iterator` 本身就是可组合的一次性 range；`iterable` 表示容器、数组或借用对象能产出 iterator。ranges 不新增循环语法；组合使用普通泛型自由函数，并通过 UFCS 写成点调用：

```cp
let total = iota(0, n)
    .filter(f(value: i32) -> bool { return value != 3; })
    .transform(f(value: i32) -> i32 { return value + 1; })
    .take(10 as usize)
    .fold(0, f(sum: i32, value: i32) -> i32 { return sum + value; });
```

所有 source、adapter 和 terminal 都是普通函数。cp 当前不支持普通函数重载，因此 ranges API 不用参数数量区分含义。

## 模块边界

```text
std/ranges.cp              -> export module std.ranges;
std/ranges/iota.cp         -> export module std.ranges.iota;
std/ranges/sources.cp      -> export module std.ranges.sources;
std/ranges/adapters.cp     -> export module std.ranges.adapters;
std/ranges/terminals.cp    -> export module std.ranges.terminals;
```

聚合模块：

```cp
export module std.ranges;

export import std.ranges.iota;
export import std.ranges.sources;
export import std.ranges.adapters;
export import std.ranges.terminals;
```

`std` 重导出 `std.ranges`，因此 `import std;` 后可以直接使用这些入口。

## Sources

`iota(begin, end)` 产生半开区间 `[begin, end)`。`T` 需要满足 `equality_comparable<T>` 和 `incrementable`；推进只使用前置 `++current`。不提供 `iota(end)`。

```cp
let indices = iota(0, 4);
```

`empty<T>()` 产生空 iterator。由于没有 CTAD，调用点必须显式给出元素类型。

```cp
let none = empty<i32>();
```

`single(value)` 产生单元素 iterator。

```cp
let one = single(42);
```

`repeat(value)` 是无限 iterator。有限重复使用组合表达：

```cp
let five = repeat(1).take(5 as usize);
```

`all(ref source)` 把 lvalue 容器、数组、`str` 或其它 `iterable` 显式转成可组合 iterator。

```cp
let values: [i32; 3] = [1, 2, 3];
let positives = all(ref values).filter(f(value: i32&) -> bool { return value > 0; });
```

## Lazy Adapters

Adapter 不立即拉取全部元素，只包装并返回新的 iterator。

```cp
filter<I: iterator, P>(source: I, predicate: P) -> filter_iter<I, P>
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
```

`filter` 的谓词结果必须是 `bool`。

```cp
transform<I: iterator, F>(source: I, mapper: F) -> transform_iter<I, F>
requires
    F: callable<I::iter_item>
```

`transform` 的元素类型是 `call_result<F, I::iter_item>`。这依赖 [meta.md](meta.md) 的 callable result 类型查询，而不是把参数限制成 `f(...) -> R` 函数类型。

其它 adapter：

- `take(count)`：最多保留前 `count` 个元素。
- `drop(count)`：跳过前 `count` 个元素。
- `enumerate()`：产生 `(usize, item)`。
- `zip(other)`：产生 `(left_item, right_item)`，任一侧结束即结束。
- `concat(other)`：要求两侧 `iter_item` 相同，先遍历左侧再遍历右侧。

## Terminals

Terminal 会消费 iterator：

- `count()`：返回元素数量。对无限 range 调用不会终止。
- `fold(init, op)`：从左到右累积；`op` 必须可用 `(Acc, item)` 调用并返回 `Acc`。
- `any(pred)`：存在满足谓词的元素时返回 `true`。
- `all_of(pred)`：所有元素满足谓词时返回 `true`。
- `find(pred)`：返回第一个满足谓词的 `optional<item>`。

这些 terminal 对无限 iterator 的终止性由调用者负责。例如 `repeat(1).any(f(value: i32) -> bool { return value == 1; })` 会终止，`repeat(1).count()` 不会终止。

## 组合规则

`I` 可作为 adapter/terminal 输入，当且仅当 `I: iterator`。这意味着 ranges 组合默认是一次性消费语义；组合过程中保存的是 iterator 状态，而不是可重复遍历的 range 对象。

Source 和 adapter 的输出都是 `iterator`，因此可以继续组合：

```cp
iota(0, n).drop(2 as usize).take(5 as usize).enumerate()
```

约束在具体类型确定后检查：

- `filter`、`any`、`all_of`、`find` 要求谓词返回 `bool`。
- `transform` 的输出元素类型由 `call_result` 决定。
- `fold` 要求累积函数返回同一个 accumulator 类型。
- `zip` 不要求两侧元素类型相同。
- `concat` 要求两侧元素类型相同。

无限 iterator 是合法表达能力。`iterator.next()` 不承诺最终返回 `.none`，终止性由被调用的 terminal 或调用者控制。

## to 的预留

当前不提供 `to()`，也不提供 `to_vector()`。

长期目标是统一的泛型 terminal，例如：

```cp
range.to<Container>()
```

具体设计必须等 CTAD 或等价的“类型构造器实参推导”规则明确后再落地。目标容器类型和 range 元素类型需要共同决定最终容器实例；在这条规则缺失前，先做 `to_vector()` 会把标准库推向一次性接口。
