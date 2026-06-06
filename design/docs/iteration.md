# 迭代

本文档记录 KCP 的迭代协议。迭代协议建立在 `concept`、泛型 `variant` 和 `requires` 类型约束之上，不引入运行时多态。

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

当前 range-for 语义检查并不按模块符号身份查找 `std.core.option.optional`。它检查 `next()` 的返回读出类型是否是某个 `variant`，并要求存在名为 `some` 和 `none` 的 case；`some` 必须恰好携带一个 payload，且该 payload 的读出类型等于 iterator 的 `iter_item`。公共标准库和用户代码仍应使用 `optional<T>` 作为协议返回类型；不要依赖其它同形 `variant` 作为长期稳定协议。

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
- 非 const 临时 range 可以被当作可写 receiver 调用 `iter(self&)`；因此 `iota(0, 3)`、函数返回的 `vector<T>` 或 adapter 链结果都可以直接用于 range-for 或 terminal。

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

## 标准库指针游标

`std.core.iter` 还提供两个连续内存游标：

```cp
ptr_iter<T>
const_ptr_iter<T>
```

`ptr_iter<T>` 的 `iter_item` 是 `T&`，`const_ptr_iter<T>` 的 `iter_item` 是 `T const&`。它们只保存 `current` 和 `end` 两个指针，`next()` 的结束判断是 `current >= end`；未结束时返回当前元素引用，然后把 `current` 加 1。

这些游标不拥有内存，也不记录长度、容量或初始化状态。调用者必须保证 `[current, end)` 是同一连续对象区间内的有效范围，且元素生命周期已经开始；`end` 应是 one-past 指针。把来源不同的指针、反向区间、未构造 storage slot 或已经失效的容器缓冲区交给 `ptr_iter` / `const_ptr_iter`，都属于调用者违反底层契约。标准库的 `vector<T>`、`span<T>`、`string` 和 ranges 数组桥接会用它们实现连续区间迭代；裸用户代码只有在已经掌握这些不变量时才应直接构造。

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

这里的“声明语法子集”只包含 `let` / `const`、可选 `ref` 和一个普通标识符名。range-for 绑定位置不能写显式类型标注、解构模式或 `let (a, b)` 这类 pattern；如果元素是 tuple，需要先按值或按引用绑定到一个名字，再在循环体内用普通解构声明处理。

当前 range-for 协议的 concept 查找也是按当前文件可见名字进行的。语义层会解析名为 `iterable` 或 `const_iterable` 的 concept，并检查范围表达式的读出类型是否实现对应 concept；它不会把协议 concept 的符号身份硬编码为 `std.core.iter.iterable` / `std.core.iter.const_iterable`。因此，未导入标准库但在当前模块定义了同名 concept、同名关联类型和同名 `iter` / `next` 方法时，range-for 也可能通过。

公共代码仍应导入并实现标准库的 `std.core.iter` 协议。否则不同模块各自定义同名 `iterable` / `iterator` 时，range-for 使用哪套协议完全取决于当前可见 concept 名字，不能作为跨模块稳定契约。

语义检查先把范围表达式转换成 iterable，再取得 iterator。当前实现的选择顺序是：

1. 令 `R = read_type(type(values))`。
2. 如果 `R` 是内建数组 `[T; N]`，则使用数组内建路径，元素类型为 `T&` 或 `T const&`，取决于范围表达式是否只读。数组路径不要求导入或实现 `iterable` / `const_iterable`，也不会调用 `.iter()`；即使当前模块定义了名为 `iterable` 的 concept，也不能覆盖数组的 range-for 行为。
3. 对非数组类型，如果范围表达式是只读访问，则只尝试 `R implements const_iterable`，并要求 `values.iter()` 选择到可用的 `iter(self const&)`，返回类型读出后等于 `R::const_iter_type`，元素类型为 `R::const_iter_item`。
4. 对非数组类型，如果范围表达式不是只读访问，则只尝试 `R implements iterable`，并要求 `values.iter()` 选择到可用的 `iter(self&)`，返回类型读出后等于 `R::iter_type`，元素类型为 `R::iter_item`。非 const 临时 range 会在语义上物化成可调用 `self&` 的 receiver；显式 `move values` 仍保持 move 表达式类别，不能绑定到普通 `iter(self&)`，除非后续语言设计专门开放 move receiver 的迭代入口。
5. 否则报 `invalid_range`：范围表达式必须是 `[T; N]`、`iterable` 或 `const_iterable`。因此 `for(let item : some_iterator)` 和 `for(let item : values.iter())` 都不是合法 range-for；它们拿到的是游标，不是用户层 range。

也就是说，const range 不会退回去调用 `iterable.iter(self&)`；需要只读遍历时，目标类型必须提供 `const_iterable`。实现 `iterator` 本身也不让一个值成为 range，`next(self&)` 只描述游标如何前进，不描述如何从用户层范围表达式取得游标。

裸 iterator 不能通过返回类型推导或错误恢复变成合法 range。普通语义检查仍会对 `for(let item : some_iterator)` 和 `for(let item : values.iter())` 报 `invalid_range`。

选定 iterator 类型后还会继续做结构检查。普通 range-for 语义不会再次要求这个 iterator 类型显式实现当前可见的 `iterator` concept；标准库 `iterable` / `const_iterable` concept 会通过 `requires iter_type: iterator` 表达这层约束，但如果当前模块定义了形状相近却更宽松的同名 range concept，range-for 后续只看下面这些实际能力：

- iterator 类型必须有 `iter_item` 关联类型。
- iterator 类型必须有可用的 `next(self&)`。
- `next()` 的返回读出类型必须是带 `some` / `none` case 的 `variant`。
- `some` case 必须携带一个 payload，且 payload 读出类型必须等于 `iter_item`。
- range-for 只用 tag 区分 `some` 和 `none`；当前语义不会检查 `none` 是否携带 payload，但合法协议必须让 `none` 是 unit case，和 `optional<T>::none` 保持一致。

这意味着下面这种同形但非标准库的结束类型当前可能通过检查，但不应作为公共协议设计：

```cp
variant maybe_i32 {
    none(i32); // 当前 range-for 不检查 none payload；公开协议不要这样写
    some(i32);
}
```

range-for 只判断 `next()` 返回值是否是 `some` case；遇到其它 case，包括带 payload 的 `none`，都会结束循环且不会读取该 payload。需要把结束原因或错误信息带出循环时，应让 range terminal 返回 `expected` / 自定义结果类型，而不是把数据塞进 `none`。

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

范围表达式本身只在进入循环前求值一次，不会在每轮 body 前重新求值。协议路径会先调用一次
`values.iter()` 取得 iterator，之后每轮只调用这个 iterator 的 `next()`；如果 `iter(self&)`
需要引用 receiver，而范围表达式不是左值，编译器会先保存这个 range 值，再把保存后的值作为 `iter`
receiver。数组路径同样先确定数组对象地址；非左值数组会先保存成临时数组，然后逐元素遍历。因此
`for(let item : make_range())` 只调用一次 `make_range()`，循环次数由返回 range 的 iterator 状态决定，
而不是每轮重新生成一个 range。

取得 `iter_item` 后，循环变量按声明规则从隐式 initializer 推导：

- `for(let value : values)`：按值绑定，类型为 `read_type(iter_item)`。
- `for(const value : values)`：按值绑定，类型为 `read_type(iter_item)`，binding 不可写。
- `for(let ref value : values)`：要求 `iter_item` 是可写引用 `T&`，循环变量类型为 `T&`。
- `for(const ref value : values)`：要求 `iter_item` 是引用 `T&` 或 `T const&`，循环变量类型为 `T const&`。

因此 `for(const value : values)` 不表示 const iteration，也不保留引用；它只表示本轮的 value binding 不可重新赋值。const iteration 由范围表达式本身的只读 receiver 决定。

```cp
for(const value : values) {
    // values 仍按自身表达式的 constness 选择 iterable 或 const_iterable；
    // 这里只是 value 这个按值副本不能重新赋值。
}

const ref readonly = values;
for(const value : readonly) {
    // 这里才因为 range 表达式是只读引用而选择 const_iterable。
}
```

循环变量是循环 body 作用域中的普通局部值名，只在 body 内可见。它可以 shadow 外层同名局部，离开循环后外层名字恢复可见；body 的同一词法层级不能再次声明同名值，例如 `for(let item : values) { let item = 1; }` 会报 `duplicate_symbol`。内层嵌套 block 仍可以按普通局部规则重新 shadow。

按值绑定会复制或移动本轮 item 的读出值，不会 alias 原 range 元素：

```cp
for(let value : values) {
    value += 1; // 只修改循环变量副本
}

for(let ref value : values) {
    value += 1; // 修改原 range 元素，前提是 iter_item 是可写引用
}
```

如果 range 表达式是 const binding、const 引用或只能产生 const iterator item，那么 `for(let ref value : range)` 会报错；需要写 `for(const ref value : range)` 或按值绑定。

循环变量每轮都会重新初始化。按引用绑定时，循环变量引用当前 item；按值绑定时，如果 item 是引用，会先读出被引用值，并在需要时调用对应 const/非 const copy constructor，否则按普通读值处理。非引用循环变量在每轮 body 结束、`continue` 或 `break` 时都会按普通局部对象规则清理；引用循环变量本身不拥有元素，也不注册析构。

## break 和 continue

因为 `next` 同时完成“取值”和“推进”，`continue` 不需要额外跳到一个 `advance` 步骤。

规则：

- `break` 离开当前循环，并执行离开作用域所需的清理。
- `continue` 结束当前 body，进入下一轮循环；下一轮会重新调用 `__iter.next()`。
- 带标签的 `break label` / `continue label` 仍按 [flow.md](flow.md) 的标签规则解析。

range-for 是普通运行时循环目标；`while`、`do while` 和 range-for 嵌套时，不带标签的 `break` / `continue` 总是作用于最近的运行时循环。只有 range-for 能声明标签，语法是 `for: label(let item : range) { ... }`，因此带标签跳转只能选中当前循环栈中对应标签的 range-for。`template for` 是编译期展开，不是 range-for；展开体里的 `break` / `continue` 只能控制展开体内部新声明的运行时循环，不能穿透到展开体外层的 range-for，即使用 `break outer` / `continue outer` 也不行。

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

不把“所有 iterator 自动也是 iterable”写成 blanket impl，也不做 `iterable -> iterator` 隐式转换。需要组合时，标准库 ranges 通过 `to_view(source: R forward&)` 把左值借用为 view、把右值保存为 owning view。`values.iter().count()` 和 `for(let value : values.iter())` 都是错误；应写 `values.count()` 或 `for(let value : values)`。

## 内建和标准库范围

内建数组由编译器直接接入 range-for，不需要导入 `std.core.iter`，也不需要真的存在以下 impl：

换句话说，`for(... : array)` 的语言路径并不是在源码层查找“数组实现了
`iterable` / `const_iterable`”。只要范围表达式读出类型是 `[T; N]`，语义层就直接进入数组 range-for
路径：可写数组元素类型是 `T&`，只读数组元素类型是 `T const&`。这条内建路径只服务 range-for，
不会让 `[T; N]` 自动满足标准库的 `iterable` concept，也不会让数组天然拥有 `.iter()`、`.count()`、
`.filter(...)` 这类库接口。

数组 range-for 按编译期长度逐元素遍历；不调用 `.iter()` / `.next()`，也不需要运行时计数器。`[T; 0]` 作为 range-for 输入时循环体执行零次，也不会取 `&array[0]`；这和 `std.ranges.sources` 中数组 adapter 桥接的底层实现不同。数组范围表达式如果不是左值，编译器会先保存整个数组值，再在每轮绑定对应元素。

`str` 是 compiler-recognized 内建类型，但它的 range-for 能力来自标准库 `std.text.str` 中的普通 `iterable` / `const_iterable` impl，而不是编译器特判：

```text
str implements iterable
str implements const_iterable
```

`str` 第一版按 `char` 遍历，`str_iter` 保存 `str` 值和当前下标，`next(self&)` 每次按 `text[index]` 取出一个 `char`。迭代边界是 `str.size()`，不是 trailing `'\0'`；因此包含中间 `'\0'` 的字符串视图仍会完整遍历其 `len` 个字符。
