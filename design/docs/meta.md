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

`call_result<F, Args...>` 表示用 `Args...` 调用 `F` 的返回类型。`F` 可以是函数类型、函数指针、无捕获 lambda、捕获闭包，或实现 `operator ()` 的类型。

```cp
apply<F>(value: i32, callback: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}
```

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

`callable` 只验证调用是否成立；返回类型用 `call_result` 提取，再通过类型相等约束表达更精确的要求：

```cp
requires
    F: callable<i32> and call_result<F, i32> == bool
```

## 设计边界

这些查询都是类型层工具，不能作为值表达式使用。它们服务于长期的泛型标准库能力，例如 `std.ranges.transform` 需要从任意 callable 提取输出元素类型。

当前没有 CTAD，也没有类型构造器实参推导。因此容器构造类 terminal，例如 `to<Container>()`，暂不落地。
