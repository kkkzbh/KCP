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

聚合模块 `std.ranges` 重导出 `std.compare`、sources、iota、adapters 和 terminals。`std` 重导出 `std.ranges`，因此 `import std;` 后可以直接使用这些入口。

直接导入子模块时只得到该子模块和它显式 `export import` 的内容：

- `import std.ranges.sources;` 暴露 `view`、`to_view`、三种 view 桥接、`empty` / `single` / `repeat`，以及数组 iterable impl；同时因为 `sources` 重导出 `std.core.iter`、`std.core.option` 和 `std.meta`，也能看到 `iterator` / `iterable`、`optional`、`callable` 和类型查询名。它看不到 `iota`、adapter 或 terminal。
- `import std.ranges.iota;` 暴露 `iota`，并经由它的 `export import std.compare` / `std.ranges.sources` 暴露比较 concept、`weak_ordering`、sources/view 基础和上述 core/meta 依赖；但仍看不到 `filter`、`take`、`count` 等 adapter/terminal。
- `import std.ranges.adapters;` 暴露 adapters，并经由 `export import std.compare` / `std.ranges.sources` 暴露比较 concept、`weak_ordering`、sources/view 基础和上述 core/meta 依赖；不自动暴露 `iota` 或 terminals。
- `import std.ranges.terminals;` 暴露 terminals，并经由 `std.ranges.sources` 暴露 sources/view 基础和上述 core/meta 依赖；不自动暴露 `iota` 或 adapters。

UFCS 点调用本质仍是普通函数查找。`values.filter(...)` 要求当前文件能看见自由函数 `filter`，`values.count()` 要求能看见 `count`；只有可见性满足后，才会按首参 UFCS 把点调用改写成普通调用。

## 公开类型边界

第一版 ranges 的 source、adapter 和 iterator 类型都是普通 `export struct`。当前语言没有字段私有性，因此这些类型的字段在源码层可见：

```text
sources:  ref_view, const_ref_view, owning_view,
          empty_view, empty_iter, single_view, single_iter,
          repeat_view, repeat_iter
iota:     iota_view, iota_iter
adapters: take_view, take_iter, drop_view, drop_iter,
          filter_view, filter_iter, transform_view, transform_iter,
          enumerate_view, enumerate_iter, zip_view, zip_iter,
          concat_view, concat_iter
```

这些结构体是公开的组合结果类型，不是额外语法。用户代码可以写出这些名字，也可以在低层实验中手动构造字段；但稳定用法仍然是调用 `empty` / `single` / `repeat` / `iota`、`to_view` 和各个 adapter 函数来得到它们。手动构造时，调用者需要自行保证字段不变量，例如 `ref_view.source` / `const_ref_view.source` 指向仍存活的 range，`take_iter.remaining`、`enumerate_iter.index` 和 `concat_iter.left_done` 与内部 iterator 状态一致。

所有 `*_iter` 都是单次前进游标：`next(self&)` 需要可写 iterator 引用，并可能移动、复制或丢弃内部字段中的值。iterator 不是 range/view 输入，不能直接 range-for，也不能直接接 adapter/terminal；需要可组合对象时保存 source/view 或 adapter 返回值，而不是保存 `.iter()` 的结果。

`*_view` 才是可组合的 `view`。view 保存 source view、callable 或另一侧 range，每次 `.iter()` 重新创建对应 iterator。由于 view 字段是公开实现细节，直接改写这些字段会绕过 adapter 构造时建立的借用/拥有关系和 callable 复制假设；普通代码不应依赖这种改写保持长期兼容。

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

`ref_view<R>` 的 const receiver 只表示 view 对象本身不能改写，不表示底层左值 range 被重新借成 const。也就是说，从可写 `values` 得到的 `let view = values.to_view();` 即使后续通过 const view receiver 迭代，item 仍然是 `R::iter_item`，例如 `T&`。需要只读元素时，应让原始 range 表达式本身是 const 左值，从而选择 `const_ref_view<R>`；不要把 `const ref_view` 当成 `const_ref_view` 使用。

## Sources

`iota(begin, end)` 产生半开 view `[begin, end)`。`T` 需要满足 `equality_comparable<T>` 和 `incrementable`；推进只使用前置 `++current`。只有后置 `operator postfix ++` 的类型不满足当前 `iota` 要求，缺少 `operator ==` 的类型也不能作为 `iota` 元素。不提供 `iota(end)`。

```cp
let indices = iota(0, 4);
```

当前 `iota_view<T>` 按值保存 `begin` 和 `end`，每次 `.iter()` 都复制这两个值创建新的 `iota_iter<T>`。iterator 的 `iter_item` 是值类型 `T`，不是引用；`next()` 先检查 `current == end`，未到 end 时复制当前值作为返回项，再执行 `++current`。它不使用 `<` / `<=` 或三路比较，也不会检测“已经越过 end”。如果 `++current` 永远不能让 `current == end` 成立，`iota` 就是无限 range。

其它 source：

- `empty<T>()`：空 view。由于没有 CTAD，调用点必须显式给出元素类型。
- `single(value)`：单元素 view。
- `repeat(value)`：无限 view，有限重复写作 `repeat(value).take(count)`；当前没有 `repeat(value, count)` 二参重载。

`single` 和 `repeat` 的 iterator item 都是值类型 `T`，不是引用。`single(value)` 会把实参 move 进 view，每次 `.iter()` 会把保存的
`value` 复制进新的 iterator；这个 iterator 的第一次 `next()` 再把自己的 `value` 字段 move 出来，并把 `remaining` 置为 `false`。
`repeat(value)` 每次 `.iter()` 和每次 `next()` 都要复制保存的 `value`。对只能 move、不能 copy 的类型，不要依赖 `single` 或 `repeat`
作为可重复 view。

普通容器、`span<T>`、`string` 和数组不需要额外包装。左值直接作为 adapter receiver，`to_view` 会选择借用 view：

```cp
let values: [i32; 3] = [1, 2, 3];
let positives = values.filter(f(value: i32&) -> bool { return value > 0; });
```

数组在 ranges 层通过 `std.ranges.sources` 中的普通 `impl<T, N> iterable for [T; N]` / `const_iterable` 接入 adapter 和 terminal。这和 range-for 的编译器内建数组路径不同：`for(let x : array)` 不依赖导入 ranges，但 `array.count()`、`array.filter(...)` 这类 UFCS 组合需要能看见标准库的数组 iterable impl。

`[T; 0]` 是合法数组类型，但当前 ranges 数组 `iter()` 实现用 `&self[0]` 构造 begin/end 指针；因此不要把零长数组直接作为 `array.count()`、`array.filter(...)`、`array.iter()` 等标准库 ranges receiver。需要空 ranges 输入时，使用 `empty<T>()`、空 `span<T>` 或空 `vector<T>`。

`str` 可以用于 range-for，因为 `std.text.str` 提供普通 `iterable` / `const_iterable` 实现；但当前 ranges adapter / terminal 的泛型实例在 `std.ranges.sources` 内部检查 `.iter()`，不能把 `str` receiver 作为稳定可用的 ranges 输入。也就是说，`for(let ch : text)` 可用，`string{"abc"}.count()` 可用，但 `text: str` 上的 `text.count()` / `text.filter(...)` 不是当前公开能力。需要对只读字符串视图做 ranges 组合时，第一版应先转成拥有的 `string`、显式构造其它 view，或直接写 range-for / 手动循环。

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

- `filter(predicate)`：保留谓词返回 `true` 的元素，谓词结果必须是 `bool`。
- `transform(mapper)`：把每个元素映射成 `mapper(item)` 的结果类型。
- `take(count)`：最多保留前 `count` 个元素。
- `drop(count)`：跳过前 `count` 个元素。
- `enumerate()`：产生 `(usize, item)`。
- `zip(other)`：产生 `(left_item, right_item)`，任一侧结束即结束。
- `concat(other)`：要求两侧 item 类型相同，先遍历左侧再遍历右侧。

规则：

- adapter 入口会把 predicate / mapper / 另一个 range 参数 move 进返回的 view；view 的每次 `iter()` 会从保存的 source 重新创建 iterator，并把 view 中保存的 callable 复制进 iterator。
- `zip` / `concat` 会分别对左右两侧调用 `to_view(forward ...)`。左值侧保存为 `ref_view` / `const_ref_view` 借用，右值侧保存为 `owning_view`；右侧 range 不会在构造 adapter 时立即消费，而是在结果 view 的 `.iter()` / `next()` 中按对应 adapter 规则消费。
- 因此 `filter` / `transform` 的 predicate / mapper 需要能按当前语言规则从 view 字段复制到 iterator 字段。带有 move-only 捕获状态、删除 copy 构造的闭包或只允许一次消费的 callable 不是当前 adapter 的稳定输入；需要这类状态时，应把状态放进可复制句柄，或写手动循环/terminal 直接消费。
- adapter view 的 const 迭代按 `const_iterable` 单独实例化：`take`、`drop`、`enumerate` 要求保存的 source view 也能 const 迭代；`filter` / `transform` 还要求 predicate / mapper 能接收 `V::const_iter_item`；`zip` 要求两侧都能 const 迭代；`concat` 要求两侧都能 const 迭代且 `const_iter_item` 完全相同。因此某个 view 可能能在可写接收者上迭代，但不能在 const 接收者上迭代。
- `take(0)` 立即结束；如果 source 提前结束，结果也提前结束。
- `drop(count)` 在第一次取值前按需消费并丢弃最多 `count` 个元素；source 提前结束时结果为空。
- `filter(predicate)` 保留原始 item 类型。source item 是引用时，通过 `filter` 后仍是引用。
- `transform(mapper)` 产生 mapper 返回类型的值；如果 mapper 返回引用，则 item 类型也可以是引用。
- `enumerate()` 的 index 从 `0 as usize` 开始，每产生一个元素递增一次。
- `zip(left, right)` 要求两侧都是 range/view 输入，不接受裸 iterator。
- `zip` 的 `next()` 先拉取左侧，再拉取右侧；如果右侧已经结束，本次已经拉出的左侧 item 会被丢弃，调用返回 `.none`。不要依赖右侧较短时左侧的下一项还能被后续组合继续消费。
- `concat(left, right)` 要求两侧 `iter_item` 完全相同；`T&` 和 `T const&`、`i32` 和 `i64` 都不是同一个 item 类型。
- `concat` 每次 `.iter()` 会立即分别创建左、右两个 iterator，但 `next()` 会一直消费左侧，直到左侧第一次返回 `.none` 后才把内部 `left_done` 置为 true 并开始消费右侧。左侧若是无限 range，右侧永远不会被拉取；左侧结束后不会再回头检查左侧是否后来又有元素。
- 当前没有 `map` / `flat_map` adapter；映射元素使用 `transform(mapper)`，`map<K,V>` 这个名字留给关联容器。
- 当前没有 `cycle`、`reverse`、`chunk`、`windows`、`flatten` 或 `take_while`。

## Terminals

Terminal 消费 iterable/view，内部调用 `.iter()`：

- `count()`：返回元素数量。对无限 range 调用不会终止。
- `fold(init, op)`：从左到右累积。
- `any(pred)`：存在满足谓词的元素时返回 `true`。
- `all_of(pred)`：所有元素满足谓词时返回 `true`。
- `find(pred)`：返回第一个满足谓词的 `optional<item>`。

规则：

- terminal 入口和 adapter 一样先调用 `to_view(forward source)`；对左值 source 是借用消费，对右值 source 会先拥有到 `owning_view` 再消费。
- 每次 terminal 调用都会对这个 view 调用一次 `.iter()` 并消费该 iterator，terminal 本身不保存可复用状态。若 source/view 的 `.iter()` 是可重复的，之后再次调用 terminal 会重新创建 iterator；若底层 range 自己是单次消费语义，则重复调用是否可用由该 range 的 `iter()` 实现负责。
- `count()` 使用 `usize` 从 0 开始逐项 `+= 1`。当前标准库不做溢出检查，也不尝试证明 range 有限；元素数量超过 `usize` 可表示范围或无限 range 都属于调用者选择的运行时边界。
- `fold(init, op)` 先把 `init` move 到内部 `state`，空 range 直接返回这个 `state`。非空时每个元素按 `state = op(move state, item)` 更新；因此 `op` 必须能以当前 accumulator 的 move 值和 item 调用，返回值必须能赋回 `Acc`。每轮旧 `state` 都已经被 move 给 callback，不要依赖它在 callback 外继续保持可观察身份。
- `any` 找到第一个满足谓词的元素后立即返回 `true`；空 range 返回 `false`。
- `all_of` 找到第一个不满足谓词的元素后立即返回 `false`；空 range 返回 `true`。
- `find` 找到元素后返回 iterator 产生的同一个 item 类型；source item 是引用时，返回 `optional<T&>` 或 `optional<T const&>`。
- 对无限 range，`count()` 和没有外部截断的 `fold()` 不会自行终止；`any`、`all_of` 和 `find` 只有在短路条件实际出现时才会返回，否则同样会持续拉取 source。需要固定上界时，先组合 `take(count)`。
- `any` / `all_of` / `find` 的谓词必须能以当前 item 调用，并且调用结果必须是 `bool`；不会使用整数、指针或用户对象 truthiness。
- 谓词和 fold/transform 回调都是普通 callable，必须满足 `std.meta` 的 `callable` / `call_result` 检查。
- `any`、`all_of`、`find` 和 `fold` 的公开函数签名没有单独写 `requires` 子句；当前约束来自函数体内对 `predicate(*item)` 或 `op(move state, *item)` 的实际调用，以及 `if(predicate(*item))` 对 `bool` 条件的要求。因此不满足 callable 或返回 `bool` 的错误会在函数实例化和表达式检查时暴露，而不是表现成一个独立的 ranges concept 不满足。
- `find` 没有显式返回类型，返回类型由 `iter.next()` 推导出来，等价于当前 iterator 的 `optional<iter_item>`。找不到元素时返回最后一次 `next()` 得到的 `.none`；找到元素时返回包含当前 item 的 `.some`。它不会额外包一层结果类型，也不会把错误状态和“未找到”区分开。
- `fold` 的 `op`、`any` / `all_of` / `find` 的 predicate 都是 terminal 的按值参数。调用 terminal 时它们先按普通参数规则构造一次，
  terminal 内部随后反复调用同一个局部回调对象；不会像 adapter view 那样把回调保存到可复用 view，也不会为每个元素重新复制回调。
  因此带可变内部状态的回调会在一次 terminal 调用内连续观察到自己的状态变化；但 terminal 返回后这个局部回调对象也随调用结束销毁，
  不能把它作为 pipeline 的持久状态保存。
- `find` 不把引用 item 复制成拥有值。对左值 range，`optional<T&>` 指向原 range；对值 item range，返回的是值。不要对临时容器或函数返回的容器直接调用
  `find` 后长期保存引用结果，例如 `make_values().find(pred)` 在元素类型为 `T&` 时会通过 terminal 内部的 `owning_view` 迭代，返回的引用会指向即将离开的临时 view 内部存储。需要引用结果时，先把容器绑定到局部变量；需要跨表达式保存结果时，先转换成值。

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
