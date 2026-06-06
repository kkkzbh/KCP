# 标准库 core

本文档记录 `std.core` 第一版公共接口。它提供可恢复结果类型和迭代协议基础，是 `std.ranges`、容器、文件 IO 和格式化等模块共同依赖的底层标准库层。

`std.core` 聚合模块当前重导出：

```text
std.core.option
std.core.expected
std.core.iter
```

`std.core` 不是隐式 prelude。需要直接写 `optional<T>`、`expected<T,E>`、`iterator` 或 `iterable` 时，必须导入 `std.core`、`std`，或导入其它重导出它们的模块。

直接导入子模块时只得到该子模块自己的导出项。`std.core.iter` 对 `std.core.option` 是普通 `import`，不会把 `optional` 继续重导出；手写 `next(self&) -> optional<T>` 这类 iterator 签名时，应导入 `std.core` 或同时导入 `std.core.iter` 和 `std.core.option`。

## optional

```cp
variant optional<T> {
    none;
    some(T);
}
```

`optional<T>` 表示“可能没有值”。公开成员：

```cp
has_value(self const&) -> bool;
value_or(self const&, fallback: T) -> T;
operator *(self like&) -> T like&;
```

规则：

- `optional<T>::none` 没有 payload。
- `optional<T>::some(value)` 保存一个 `T` payload。
- `optional<T>` 是普通 `variant`，不支持默认初始化；空值必须显式写 `optional<T>::none`，不能写 `optional<T>{}` 或指望局部默认构造成 `.none`。
- `has_value()` 在 `.some` 时返回 `true`，在 `.none` 时返回 `false`。
- `optional<T>` 本身不能作为条件表达式；`if(item)`、`while(item)` 这类写法会按“条件必须是 `bool`”被拒绝。需要判断是否有值时写 `item.has_value()`，需要同时取 payload 时使用 `match` 或先判断后解引用。
- `value_or(fallback)` 在 `.some(value)` 时返回 `value`，在 `.none` 时返回 `fallback`。
- `value_or` 的 receiver 是 `self const&`，因此成功分支从 `T const&` 构造返回 `T`；它不会 move 内部 payload。
- `fallback` 是普通按值实参，调用点必须能构造它。即使当前 optional 运行时有值，`fallback` 也会先按普通调用规则检查和求值；`value_or` 不是 lazy fallback。
- `operator *` 在 `.some` 时返回 payload 引用。receiver 是可写左值时结果为 `T&`，receiver 是 const 左值时结果为 `T const&`。
- 对 `.none` 解引用会 `panic("optional dereference on none")`。
- `T` 可以是引用类型。标准库的 `ptr_iter<T>::next()`、`map.find(...)` 和 `set.find(...)` 都会返回 `optional<T&>` 或 `optional<T const&>`。这种 `optional` 保存的是引用 payload，不会把元素复制成拥有值；`value_or(fallback)` 的 `fallback` 也必须是同类引用，`operator *` 继续借出被引用对象。

第一版没有 `unwrap`、`expect`、`map`、`and_then`、`or_else`、`take`、`reset`、`emplace` 或 `error` 风格成员。需要更复杂的分支处理时，直接使用 `match`：

```cp
let value = match item {
    .some(payload) => payload,
    .none => 0,
};
```

## expected

```cp
variant expected<T,E> {
    value(T);
    unexpected(E);
}
```

`expected<T,E>` 表示成功值或错误 payload。公开成员：

```cp
has_value(self const&) -> bool;
value_or(self const&, fallback: T) -> T;
operator *(self like&) -> T like&;
```

规则：

- `expected<T,E>::value(value)` 保存成功 payload。
- `expected<T,E>::unexpected(error)` 保存错误 payload。
- `expected<T,E>` 是普通 `variant`，不支持默认初始化；必须显式构造 `.value(...)` 或 `.unexpected(...)`，不能写 `expected<T,E>{}` 作为默认失败或默认成功值。
- `has_value()` 在 `.value` 时返回 `true`，在 `.unexpected` 时返回 `false`。
- `expected<T,E>` 本身也不是 bool-like；`if(result)` 不会隐式检查成功分支。需要判断成功与否时写 `result.has_value()`，需要读取错误 payload 时直接 `match` `.value(...)` / `.unexpected(...)`。
- `value_or(fallback)` 在 `.value(value)` 时返回成功值，在 `.unexpected(error)` 时返回 `fallback`。
- `value_or` 不暴露错误 payload，也不会 lazy 构造 fallback。需要读取错误值、转换错误类型或根据错误生成 fallback 时，直接 `match`。
- `operator *` 在 `.value` 时返回成功 payload 引用；receiver constness 按 `like&` 转发。
- 对 `.unexpected` 解引用会 `panic("expected dereference on unexpected")`。
- 成功 payload `T` 可以是引用类型，规则和 `optional<T&>` 相同：`expected<T&, E>` 表示成功时借到一个外部对象，而不是拥有一个 `T` 值。`value_or` 的 fallback 也按 `T` 检查；如果 `T` 是引用，fallback 必须能提供同类引用。

第一版没有 `error()`、`unexpected()`、`map`、`map_error`、`and_then`、`or_else`、`unwrap` 或 `expect` 成员。错误处理文档见 [error_handling.md](error_handling.md)，variant 匹配规则见 [variant.md](variant.md)。

## iterator

`iterator` 是有状态游标 concept：

```cp
concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}
```

规则：

- `next(self&)` 会推进游标，因此要求可写 receiver。
- 返回 `.some(item)` 表示产生一个元素，返回 `.none` 表示结束。
- `iter_item` 可以是值类型，也可以是引用类型，例如 `T&` 或 `T const&`。
- 裸 iterator 本身不是 range-for 的范围表达式，也不是 ranges adapter 的普通 source。它只是 `iterable` 产生出来的游标。需要遍历时通常写 `for(let item : values)`，不要写 `for(let item : values.iter())`。

## iterable 和 const_iterable

可写范围入口：

```cp
concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self&) -> iter_type;
}
```

只读范围入口：

```cp
concept const_iterable {
    type const_iter_type;
    type const_iter_item;

    requires (
        const_iter_type: iterator
        and const_iter_type::iter_item == const_iter_item
    );

    iter(self const&) -> const_iter_type;
}
```

规则：

- `iterable` 表示可写 receiver 的 range 入口，通常产生可写元素引用或值。
- `const_iterable` 表示 const receiver 的 range 入口，通常产生只读元素引用或值。
- 两个 concept 都要求返回的 iterator 类型显式满足 `iterator`，并且 iterator 的 `iter_item` 与 range 声明的 item 类型完全相同。
- range-for 会根据范围表达式的 constness、引用类别和数组内建路径选择具体入口；完整规则见 [iteration.md](iteration.md)。
- `std.ranges` adapter 和 terminal 也基于这些协议，但它们要求的是 range/view，不是裸 iterator。

## ptr_iter

`std.core.iter` 提供两个连续指针游标：

```cp
struct ptr_iter<T> {
    current: T*;
    end: T*;
}

struct const_ptr_iter<T> {
    current: T const*;
    end: T const*;
}
```

实现：

```cp
ptr_iter<T> implements iterator
    type iter_item = T&;
    next(self&) -> optional<T&>;

const_ptr_iter<T> implements iterator
    type iter_item = T const&;
    next(self&) -> optional<T const&>;
```

规则：

- 两个 iterator 都按 `[current, end)` 半开区间工作。
- `current >= end` 时返回 `.none`。
- 否则保存当前指针，`current = current + 1`，再返回当前元素引用。
- `ptr_iter<T>` 返回 `T&`，`const_ptr_iter<T>` 返回 `T const&`。
- 它们不拥有底层元素，也不检查指针是否来自同一合法连续区间。调用者或产生 iterator 的 range 必须保证生命周期、边界和指针比较前置条件。

这些 iterator 主要由 `span<T>`、`vector<T>`、`string`、数组 range adapter 等连续存储类型的 `iter()` 返回。`str` 使用自己的 `str_iter`，按值产生 `char`，不借出 `char const&`。用户代码也可以直接构造 `ptr_iter` / `const_ptr_iter`，但直接构造时要自行维护 `current` / `end` 的有效性。

## 不支持内容

第一版 `std.core` 不提供：

- `result` / `either` 等额外结果类型或别名。
- `optional` / `expected` 的 monadic helper、移动取值 helper 或错误访问器。
- iterator category、bidirectional/random-access iterator 分类、sentinel 类型或 iterator traits。
- 裸 iterator 到 range 的自动适配。
- 标准库级 prelude；所有名字仍需要通过模块导入可见。
