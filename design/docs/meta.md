# 元编程与反射基础

`std.meta` 提供第一批编译期类型查询能力。它的目标不是运行时反射，而是让标准库和用户代码能在泛型约束中提取类型、判断可调用性，并把结果继续作为普通类型使用。

## 模块

```cp
export module std.meta;
```

`std` 重导出 `std.meta`。这些查询是 `std.meta` 的真实模块接口：用户代码必须 `import std.meta;` 或导入重导出它的模块后才能使用。编译器可以为这些名字提供内建求值语义，但不把它们作为未导入也可见的全局裸名。

## 类型查询

`read_type<T>` 去掉引用壳，得到表达式读出后的值类型。

```cp
type value = read_type<i32&>; // i32
```

`remove_reference<T>` 只移除最外层引用。

```cp
type value = remove_reference<i32&>; // i32
```

`pointee<T>` 提取指针或引用指向的类型。

```cp
type item = pointee<i32*>; // i32
```

`tuple_element<Tuple, Index>` 提取 tuple 类型的第 `Index` 个元素。

```cp
type first = tuple_element<(i32, bool), 0>; // i32
```

`call_result<F, Args...>` 表示用 `Args...` 调用 `F` 的返回类型。`F` 可以是函数类型、函数指针、无捕获 lambda、捕获闭包，或实现 `operator ()` 的类型。`Args...` 可以为空，表示零参数调用。

```cp
apply<F>(value: i32, callback: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}
```

在泛型函数中，`call_result` 可以直接展开当前类型参数包：

```cp
make<F, Args...>() -> call_result<F, Args...>
requires
    F: callable<Args...>
{
    return 42;
}

make_with_flag<F, Args...>() -> call_result<F, Args..., bool>
requires
    F: callable<Args..., bool>
{
    return 42;
}

main() -> i32
{
    type zero = call_result<f() -> i32>;
    type two = call_result<f(i32, bool) -> i32, i32, bool>;
    type with_flag = call_result<f(i32, bool) -> i32, i32, bool>;
    return make<f() -> i32>() + make<f(i32, bool) -> i32, i32, bool>() + make_with_flag<f(i32, bool) -> i32, i32>() - 42;
}
```

这里 `Args...` 是类型实参列表中的 pack expansion：在 `make<f(i32, bool) -> i32, i32, bool>()` 这个实例里，`call_result<F, Args...>` 等价于 `call_result<f(i32, bool) -> i32, i32, bool>`，`callable<Args...>` 等价于 `callable<i32, bool>`。展开后可以继续跟固定类型实参；在 `make_with_flag<f(i32, bool) -> i32, i32>()` 中，`Args...` 先展开为 `i32`，再追加固定的 `bool`。

`call_result` 的调用参数按类型构造一个假想调用表达式：

- 非引用类型实参，例如 `i32`，按普通值实参检查。它可以传给按值参数或 `i32 const&` 参数，但不能绑定到需要可写 `i32&` 的参数。
- 引用类型实参，例如 `i32&` 或 `i32 const&`，按 lvalue 实参检查，并保留 const 性；`i32&` 可以匹配可写引用参数，`i32 const&` 不能匹配可写引用参数。
- `F` 是泛型闭包类型时，`call_result` 用 `Args...` 推导这个闭包的具体调用实例；泛型 lambda 自身仍必须显式写返回类型，不能通过 `call_result` 绕过泛型 lambda 返回类型限制。

这些查询按类型实参工作，不接收运行时值：

- `read_type<T>`、`remove_reference<T>` 和 `pointee<T>` 都只接受一个类型实参。
- `pointee<T>` 要求 `T` 是指针或引用类型。
- `tuple_element<Tuple, Index>` 要求第一个实参是 tuple 类型，第二个实参是编译期整数索引。
- `call_result<F, Args...>` 至少需要 callable 类型 `F`；之后的调用参数类型列表可以为空，也可以是多个类型实参。参数数量和类型必须能通过普通调用检查。
- `call_result` 的结果可以作为函数返回类型、局部 `type` 别名、`requires` 类型相等约束或泛型实参继续使用。
- 查询名没有导入时就是普通未知类型名，不是全局魔法名。
- 只有 `call_result` 接受可变数量类型实参。`read_type<Args...>`、`remove_reference<Args...>`、`pointee<Args...>` 和 `tuple_element<Tuple, Args...>` 都不是合法展开场景。
- 类型实参包展开必须是裸类型参数包名，例如 `Args...`。`box<Args>...`、`Args const&...` 或运行时值展开都不是当前 `std.meta` 查询能力。

在泛型中，查询可以依赖类型参数、关联类型和参数包：

```cp
concept readable {
    type item;
    get(self const&) -> item;
}

read<T: readable>(value: T) -> T::item
{
    return value.get();
}

apply<F, T>(callback: F, value: T) -> call_result<F, T>
{
    return callback(value);
}
```

实例化时，编译器先把 `T::item`、`F`、`T` 等依赖类型替换为具体类型，再检查 `value.get()` 或 `callback(value)` 是否成立，并把结果类型写入 `call_result`。

## callable

`callable<Args...>` 是编译器识别的内建 concept。`T: callable<Args...>` 表示 `T` 类型的值能以 `Args...` 调用。

```cp
map_one<F>(value: i32, mapper: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return mapper(value);
}
```

`Args...` 是 concept 级类型参数包，不是函数值参数包。它只记录调用参数类型列表：

- `T: callable` 或 `T: callable<>` 检查 `T` 是否能以零个参数调用。
- `T: callable<i32>` 检查 `T(i32)` 是否成立。
- `T: callable<i32, bool>` 检查 `T(i32, bool)` 是否成立。
- 每个 `Args...` 元素都必须是类型实参，不能写运行时值。
- 在泛型中可以写 `F: callable<Args...>`，前提是 `Args` 是当前作用域中可见的类型参数包。
- 可以写 `F: callable<Args..., bool>` 这类“展开包后追加固定参数”的约束；它检查调用参数列表为当前包元素再接 `bool`。
- `callable<box<Args>...>` 这类 pack pattern 不支持；如果要检查每个包元素的派生类型，需要在 `template for(type U : Args...)` 中逐项表达。
- `callable<Args...>` 本身不提供返回类型；需要用 `call_result<F, Args...>` 提取。

`callable` 只验证调用是否成立；返回类型用 `call_result` 提取，再通过类型相等约束表达更精确的要求：

```cp
requires
    F: callable<i32> and call_result<F, i32> == bool
```

## 设计边界

这些查询都是类型层工具，不能作为值表达式使用。它们服务于长期的泛型标准库能力，例如 `std.ranges.transform` 需要从任意 callable 提取输出元素类型。

不支持的内容：

- 运行时反射、字段枚举、函数列表枚举或属性查询。
- 从值表达式直接生成类型查询实参，例如 `read_type<value>`。
- 对类型构造器做实参推导；例如当前没有 `call_result<vector>` 这类 CTAD 风格能力。
- 用 `std.meta` 查询绕过普通可见性、重载、`requires` 或 concept 检查。

`std.meta` 还导出编译器识别的 reference 分类 concept：

- `is_lvalue_reference<T>`
- `is_const_lvalue_reference<T>`
- `is_move_reference<T>`

它们主要服务于 `template if` 分发。例如 `std.ranges.to_view(source: R forward&)` 会检查 `decltype(forward source)`，把可写左值转成 `ref_view`，把 const 左值转成 `const_ref_view`，把右值转成 `owning_view`。

当前没有 CTAD，也没有类型构造器实参推导。因此容器构造类 terminal，例如 `to<Container>()`，暂不落地。
