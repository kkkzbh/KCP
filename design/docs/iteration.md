# 迭代

本文档记录 cp 的迭代协议。迭代协议建立在 `concept`、泛型 `variant` 和 `requires` 类型约束之上，不引入运行时多态。

目标是定义统一的 range-for 语义：用户层范围表达式必须是 `iterable` / `const_iterable`，循环内部再调用 `.iter()` 取得有状态 `iterator` 游标。

## 基础类型

迭代结束使用标准库 `std.core.option` 中的泛型 `variant` 表示：

```cp
export module std.core.option;

export variant optional<T> {
    none;
    some(T);
}
```

`optional<T>` 不是语法关键字。它是标准库中约定的普通泛型 `variant`。构造时遵循泛型 `variant` 规则，必须写完整类型实参：

```cp
let end = optional<i32>::none;
let item = optional<i32>::some(1);
```

## Iterator 协议

`iterator` 表示有状态游标。调用 `next` 会推进游标，并返回本次产生的元素或结束标记。

```cp
export module std.core.iter;

import std.core.option;

export concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}
```

规则：

- `iter_item` 是每次迭代产生的元素类型。
- `next(self&)` 需要可写 `self`，因为 iterator 本身保存遍历状态。
- 返回 `.some(value)` 表示本轮产生一个元素，并且 iterator 已经前进到下一状态。
- 返回 `.none` 表示迭代结束。有限 iterator 结束后再次调用 `next` 应继续返回 `.none`。
- `next()` 不承诺最终返回 `.none`。无限 iterator 是合法表达能力，终止性由调用者或消费它的 terminal 控制。
- `iter_item` 可以是值类型，也可以是引用类型。借用 view 应把引用语义明确体现在 `iter_item` 中，例如 `T&`。

例如：

```cp
struct range_iter {
    current_value: i32;
    end_value: i32;
}

impl iterator for range_iter {
    type iter_item = i32;

    next(self&) -> optional<i32>
    {
        if(current_value >= end_value) {
            return optional<i32>::none;
        }

        let value = current_value;
        current_value += 1;
        return optional<i32>::some(value);
    }
}
```

## Iterable 协议

`iterable` 表示一个值可以产生 iterator。它通常由容器、数组或 view 实现；iterator 只表示内部游标，不再作为用户层 range。

```cp
export module std.core.iter;

export concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self&) -> iter_type;
}
```

规则：

- `iter_type` 是 `iter(self)` 产生的 iterator 类型。
- `iter_type` 必须实现 `iterator`。
- `iter_type::iter_item` 必须和 `iterable::iter_item` 相同。
- `iter(self&)` 可以保存必要的当前位置、边界、指针或引用信息。
- `iter(self&)` 是可写 receiver 入口。只读 receiver 使用下面的 `const_iterable`，仍然叫 `iter`，不引入 `iter_const()` 这种公开方法名。

例如：

```cp
struct range {
    begin: i32;
    end: i32;
}

impl iterable for range {
    type iter_type = range_iter;
    type iter_item = i32;

    iter(self&) -> range_iter
    {
        return range_iter{ .current_value = begin, .end_value = end };
    }
}
```

## Const Iterable 协议

`const_iterable` 表示只读 receiver 也可以产生 iterator。它使用不同的 associated type 名，避免和可写 `iterable` 的 `iter_type` / `iter_item` 在同一个目标类型上冲突：

```cp
export concept const_iterable {
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

- `next(self&)` 仍然要求可写 iterator，因为 iterator 自身状态要前进。
- 只读迭代通常需要独立 iterator 类型，让 `next()` 的返回类型静态固定。例如连续内存容器可以复用 `ptr_iter<T>` / `const_ptr_iter<T>`。
- 普通方法和 concept impl 方法允许同名 receiver 重载。`iter(self&)` 与 `iter(self const&)` 由 receiver 类型选择。
- 能用 `self like&` 表达的 accessor 继续用 `like`，例如 `data(self like&) -> T like*`、`operator [](self like&, index) -> T like&`。`iter(self const&)` 可以调用这些 accessor 得到 `T const*`。

## 范围 for

范围 `for` 写作：

```cp
for(let value : values) {
    use(value);
}
```

range-for 绑定语法是声明语法的子集：

```cp
for(let value : values) {
}

for(const value : values) {
}

for(let ref value : values) {
}

for(const ref value : values) {
}
```

语义检查先把范围表达式转换成 iterable，再取得 iterator：

1. 令 `R = read_type(type(values))`。
2. 如果范围表达式是只读访问且 `R implements const_iterable`，则使用 `values.iter()`，要求选择到 `iter(self const&)`，元素类型为 `R::const_iter_item`。
3. 否则，如果 `R` 是内建数组 `[T; N]`，则使用数组 iterator，元素类型为 `T&` 或 `T const&`，取决于范围表达式是否只读。
4. 否则，如果 `R implements iterable`，则检查 `R::iter_type implements iterator`，并检查 `R::iter_type::iter_item == R::iter_item`。循环使用 `values.iter()` 产生的 iterator。
5. 否则报错：范围表达式必须实现 `iterable` 或 `const_iterable`。

如果选中的入口需要 `self&`，但范围表达式只能作为 const 值或 const 引用使用，则报错。实现 `iterator` 不代表任意当前值都能推进；`next(self&)` 需要可写 iterator 状态。

取得 iterator 后，range-for 只有一套展开规则。对无限 iterable 使用 range-for 会无限执行，除非循环体自行 `break`：

```cp
let __iter = values.iter();

while(true) {
    let __next = __iter.next();

    match __next {
        .some(value) => {
            body;
        },
        .none => {
            break;
        },
    };
}
```

取得 `iter_item` 后，循环变量按声明规则从隐式 initializer 推导：

- `for(let value : values)`：按值绑定，类型为 `read_type(iter_item)`。
- `for(const value : values)`：按值绑定，类型为 `read_type(iter_item)`，binding 不可写。
- `for(let ref value : values)`：要求 `iter_item` 是可写引用 `T&`，循环变量类型为 `T&`。
- `for(const ref value : values)`：要求 `iter_item` 是引用 `T&` 或 `T const&`，循环变量类型为 `T const&`。

因此 `for(const value : values)` 不表示 const iteration，也不保留引用；它只表示本轮的 value binding 不可重新赋值。const iteration 由范围表达式本身的只读 receiver 决定。

## break 和 continue

因为 `next` 同时完成“取值”和“推进”，`continue` 不需要额外跳到一个 `advance` 步骤。

规则：

- `break` 离开当前循环，并执行离开作用域所需的清理。
- `continue` 结束当前 body，进入下一轮循环；下一轮会重新调用 `__iter.next()`。
- 带标签的 `break label` / `continue label` 仍按 [flow.md](flow.md) 的标签规则解析。

## Iterator 与 Iterable 的关系

`iterator` 和 `iterable` 是单向关系：

```text
iterable depends on iterator
iterator does not depend on iterable
```

也就是说，一个容器或 view 实现 `iterable`，它产生的游标实现 `iterator`。一个 iterator 本身不能直接被 range-for 消费，也不能直接作为 ranges adapter 的输入。

```cp
for(let value : values) {
    // values implements iterable
}

for(let value : values.iter()) {
    // error: values.iter() returns a cursor, not an iterable
}
```

不把“所有 iterator 自动也是 iterable”写成 blanket impl，也不做 `iterable -> iterator` 隐式转换。需要组合时，标准库 ranges 通过 `to_view(source: R forward&)` 把左值借用为 view、把右值保存为 owning view。

## 内建类型

内建类型通过等价的 `iterable` 能力接入 range-for：

```text
[T; N] implements iterable
[T; N] implements const_iterable
str        implements iterable
str        implements const_iterable
```

`str` 第一版按 `char` 遍历，迭代边界是 `str.size()`，不是 trailing `'\0'`。因此包含中间 `'\0'` 的字符串视图仍会完整遍历其 `len` 个字符。
