# Iteration

本文档记录 cp 的迭代协议。迭代协议建立在 `concept`、泛型 `variant` 和 `requires` 类型约束之上，不引入运行时多态。

目标是定义统一的 range-for 语义：范围表达式先被解析成一个 `iterator`，之后循环只依赖 `iterator.next()`。

## 基础类型

迭代结束使用标准库 `std.option` 中的泛型 `variant` 表示：

```cp
export module std.option;

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

## Iterator

`iterator` 表示有状态游标。调用 `next` 会推进游标，并返回本次产生的元素或结束标记。

```cp
export module std.iter;

export import std.option;

export concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}
```

规则：

- `iter_item` 是每次迭代产生的元素类型。
- `next(self&)` 需要可写 `self`，因为 iterator 本身保存遍历状态。
- 返回 `.some(value)` 表示本轮产生一个元素，并且 iterator 已经前进到下一状态。
- 返回 `.none` 表示迭代结束。结束后再次调用 `next` 应继续返回 `.none`。
- 按值产生元素。引用迭代、可变引用迭代和借用生命周期不属于当前迭代协议。

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

## Iterable

`iterable` 表示一个值可以产生 iterator。它通常由容器、范围对象或视图实现。

```cp
export module std.iter;

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
- 只定义可写 `self` 的入口。只读迭代需要通过 `iter(self const&)`、独立 concept 或函数重载规则单独定义。

例如：

```cp
struct range {
    begin: i32;
    end: i32;
}

impl range {
    iter(self&) -> range_iter
    {
        return range_iter{ .current_value = begin, .end_value = end };
    }
}

impl iterable for range {
    type iter_type = range_iter;
    type iter_item = i32;
}
```

## Range For

范围 `for` 写作：

```cp
for(let value : values) {
    use(value);
}
```

语义检查先把范围表达式转换成 iterator：

1. 令 `R = type(values)`。
2. 如果 `R implements iterable`，则检查 `R::iter_type implements iterator`，并检查 `R::iter_type::iter_item == R::iter_item`。循环使用 `values.iter()` 产生的 iterator。
3. 否则，如果 `R implements iterator`，则直接把 `values` 作为一次性 iterator 消费。
4. 否则报错：范围表达式必须实现 `iterable` 或 `iterator`。

如果选中的入口需要 `self&`，但范围表达式只能作为 const 值或 const 引用使用，则报错。实现 `iterator` 不代表任意当前值都能推进；`next(self&)` 需要可写 iterator 状态。

取得 iterator 后，range-for 只有一套展开规则：

```cp
let __iter = /* values.iter() or values */;

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

`for(const value : values)` 只表示循环变量 binding 不可重新赋值，不改变 iterator 的推进方式。

## Break And Continue

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

也就是说，一个容器或范围对象实现 `iterable`，它产生的游标实现 `iterator`。一个 iterator 本身也可以直接被 range-for 消费，但这表示一次性消费当前游标状态，不表示从头重新遍历某个集合。

```cp
for(let value : values) {
    // values implements iterable
}

for(let value : values.iter()) {
    // values.iter() returns an iterator
}
```

不把“所有 iterator 自动也是 iterable”写成 blanket impl，因为泛型设计不支持以裸类型参数为目标的 blanket impl。range-for 对 iterator 的直接入口是语言层面的解析规则。

## 内建类型

内建类型通过等价的 `iterable` 能力接入 range-for：

```text
array<T,N> implements iterable
str        implements iterable
```

`str` 是否按 Unicode 标量值、字节或 `char` 遍历，需要在字符串设计中单独确定；在确定前不因为 `str` 有 `size()` 和 `[]` 自动支持 range-for。
