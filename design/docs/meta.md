# 元编程与反射基础

`std.meta` 提供第一批编译期类型查询能力。它的目标不是运行时反射，而是让标准库和用户代码能在泛型约束中提取类型、判断可调用性，并把结果继续作为普通类型使用。

## 模块

```cp
export module std.meta;
```

`std` 重导出 `std.meta`。这些查询是 `std.meta` 的真实模块接口：用户代码必须 `import std.meta;` 或导入重导出它的模块后才能使用。编译器可以为这些名字提供内建求值语义，但不把它们作为未导入也可见的全局裸名。当前 `std/meta.cp` 只显式声明 `callable` 和 reference 分类 concept；`read_type` 等类型查询名由语义分析器在 `std.meta` 可见时识别，不是普通 `type` alias、函数或 concept 声明。

识别发生在未限定类型名位置：导入 `std.meta` 后可以写 `read_type<T>`，但没有值层的 `read_type` 符号，也没有可通过模块路径、成员访问或关联类型访问取得的 `std.meta::read_type` / `meta.read_type` / `T::read_type`。

当前实现只在以下情况下把 `read_type`、`remove_reference`、`pointee`、`tuple_element` 和 `call_result` 识别为内建类型查询：

- 当前文件属于 `std.meta` 模块。
- 当前文件直接 `import std.meta`。
- 当前文件导入了某个通过 `export import` 链重导出 `std.meta` 的模块，例如 `std`。
- 当前可见的 `callable` concept 符号来自 `std.meta`。

用户自己声明名为 `callable` 的 concept 不会解锁这些查询名；如果可见的 `callable` 不是来自 `std.meta` 模块的符号，查询名仍按普通类型名解析。

## 类型查询

`read_type<T>` 去掉所有引用壳，得到表达式读出后的值类型。引用上的 target constness 不会作为单独的 `const` 类型保留下来；当前类型系统把 `T const&` 表示为“引用到 `T`，target const = true”，而不是另一个可返回的 `const T` 类型。

```cp
type value = read_type<i32&>; // i32
type text = read_type<i32 const&>; // i32
```

如果 `T` 不是引用，`read_type<T>` 原样返回 `T`。如果 `T` 是通过别名、关联类型或替换形成的多层引用，`read_type<T>` 会一直剥到非引用值类型。

`remove_reference<T>` 只移除最外层引用。如果 `T` 不是引用，结果仍是 `T`。和 `read_type` 一样，它返回引用保存的 pointee 类型，不把 `T const&` 的 target constness 编码进结果类型。

```cp
type value = remove_reference<i32&>; // i32
type text = remove_reference<i32 const&>; // i32
```

`pointee<T>` 提取指针或引用指向的类型。指针和引用的 target constness 也是边上的限定，不是 pointee 类型 id 的一部分；因此 `pointee<i32 const*>` 和 `pointee<i32 const&>` 当前都得到 `i32`。需要区分引用是否只读时，应使用 `is_const_lvalue_reference` 这类分类 concept 或在具体表达式检查中保留 const 视图，不能只看 `pointee<T>` 的结果。

```cp
type item = pointee<i32*>; // i32
type readonly_pointer_item = pointee<i32 const*>; // i32
type readonly_ref_item = pointee<i32 const&>; // i32
```

`tuple_element<Tuple, Index>` 提取 tuple 类型的第 `Index` 个元素。

```cp
type first = tuple_element<(i32, bool), 0>; // i32
```

`Tuple` 会先按 `read_type` 规则读出值类型，因此 tuple 引用也可以查询元素。读出后不是 tuple 的类型不合法；当前实现把“目标不是 tuple”和“索引越界”合并到同一条 `invalid_type_argument` 失败路径，诊断文本都是 `tuple_element index is out of bounds`。`Index` 是类型实参位置上的 const `usize` 泛型实参，不是普通类型，也不是运行时值。当前可写整数常量，例如 `0`、`1`；不能写 `bool`、变量名或运行时表达式，非整数索引会先按整数泛型实参报错。索引必须在 tuple 元素范围内；越界索引会报 `invalid_type_argument`。`-1` 这类负数字面量当前在类型实参位置直接解析失败，不会进入 `tuple_element` 的语义越界检查。

`tuple_element` 只是类型层查询，不会引入值层访问 API。它不表示存在 `get<N>(tuple)`、`tuple.get<N>()`、`tuple[N]` 或 range-style tuple iteration；读取 tuple 值仍使用 `.0` / `.1` 等编译期字段访问，或使用解构声明。

`call_result<F, Args...>` 表示用 `Args...` 调用 `F` 的返回类型。`F` 可以是函数类型、函数指针、无捕获 lambda、捕获闭包，或任何能通过当前可见 `operator ()` 规则被调用的类型。常见路径是 `struct` / `variant` 上的 `operator ()`，但实现实际会走普通 operator lookup，因此可见的 extension / 顶层 call operator 也会参与。`Args...` 可以为空，表示零参数调用。

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

只有 `call_result` 会把后续类型实参列表当作调用实参列表处理。`call_result<f(i32) -> i32, 1>` 不合法，因为 `1` 不是类型实参；`call_result<F, box<Args>...>` 也不合法，因为当前只支持裸类型参数包展开，不支持 pack pattern。

`F` 是 lambda 或闭包类型时，`call_result` 使用闭包记录的 callable 签名。这个闭包路径先于普通 `operator ()` 查找；不能通过给闭包类型额外提供 call operator 来覆盖它自己的 lambda 调用语义。普通非泛型 lambda 直接检查参数数量和转换；泛型 lambda 会先用 `Args...` 构造假想调用实参，按普通泛型 lambda 调用规则推导类型实参、实例化签名，再返回该实例的返回类型：

```cp
import std.meta;

main() -> i32
{
    let count = f<T...>(values: T...) -> i32 {
        let total = 0;
        template for(let value : values...) {
            total = total + 1;
        }
        return total;
    };

    type result = call_result<decltype(count), i32, bool>;
    let value: result = count(1, true);
    return value;
}
```

`decltype(count)` 是匿名闭包类型。`call_result<decltype(count), i32, bool>` 不读取运行时变量 `count`，也不构造闭包对象；它只使用该闭包类型对应的 lambda 语义信息来检查“如果以 `i32, bool` 调用会返回什么类型”。

对于 `T forward&...` 泛型 lambda，`call_result` 的参数类型会决定每个 forward 参数的假想调用类别：

```cp
import std.meta;

main()
{
    let callback = f<T>(value: T forward&) -> i32 {
        return 0;
    };

    type moved = call_result<decltype(callback), i32>;
    type borrowed = call_result<decltype(callback), i32&>;
    type readonly = call_result<decltype(callback), i32 const&>;
}
```

普通非引用类型实参表示非左值实参，会把 `T forward&` 物化为 move 引用；`T&` 表示可写左值，`T const&` 表示只读左值。这个分类与真实调用保持一致，也会进入泛型 lambda 实例 key。

`call_result` 的调用参数按类型构造一个假想调用表达式：

- 非引用类型实参，例如 `i32`，按普通值实参检查。它可以传给按值参数或 `i32 const&` 参数，但不能绑定到需要可写 `i32&` 的参数。
- 引用类型实参，例如 `i32&` 或 `i32 const&`，按 lvalue 实参检查，并保留 const 性；`i32&` 可以匹配可写引用参数，`i32 const&` 不能匹配可写引用参数。
- `F` 会先按 `read_type` 规则读出 callable 值类型；因此 `F&` 这类 callable 引用按被引用 callable 类型检查。
- `F` 自身也按假想表达式参与 `operator ()` 匹配。`call_result<Functor, Args...>` 中的被调对象不是左值，只能匹配按值 receiver、`self const&` 或其它能接收临时值的 call operator；如果要查询需要可写 receiver 的 `operator ()(self&, ...)`，应写 `call_result<Functor&, Args...>` 或在 `callable` 约束左侧使用引用类型。`call_result<Functor const&, Args...>` 表示只读左值 receiver，可以匹配 `self const&`，但不能匹配需要可写 `self&` 的 call operator。
- 函数类型和函数指针都按完整函数签名检查，参数数量必须一致，每个假想实参都必须能隐式转换到对应参数类型。
- 普通函数声明上的尾部默认参数不进入 `f(...) -> R` / `f*(...) -> R` 类型，也不进入 `decltype(function_name)` 得到的函数类型。即使 `add(1)` 这类真实调用能通过函数符号补齐默认实参，`call_result<decltype(add), i32>` 仍按完整函数类型检查并报参数数量不匹配；需要查询这种函数时应写出完整实参类型列表，或在外层包一层真实可用的一元 lambda / wrapper。
- `F` 是泛型闭包类型时，`call_result` 用 `Args...` 推导这个闭包的具体调用实例；泛型 lambda 自身仍必须显式写返回类型，不能通过 `call_result` 绕过泛型 lambda 返回类型限制。
- `call_result` 不能在查询内部给泛型 lambda 单独写显式类型实参；它只能通过查询参数类型列表推导实例。
- 泛型 lambda 的值参数包在 `call_result` 中按查询参数数量展开，可以为空包；空包不执行 `template for` 展开体，也不会让展开体内的 `return` 贡献返回类型。
- 如果 `F` 不是函数、函数指针或 lambda/closure 类型，编译器会把 `F` 的值类型作为 owner，按普通 operator 查找规则尝试选择 `operator ()`。这包括目标类型自身的 operator、当前文件可见的 extension operator 和当前文件可见的顶层 operator；找不到可用调用运算符时，`call_result` 报 `not_callable`。

这些查询按类型实参工作，不接收运行时值：

- `read_type<T>`、`remove_reference<T>` 和 `pointee<T>` 都只接受一个类型实参。
- `pointee<T>` 要求 `T` 是指针或引用类型。
- `tuple_element<Tuple, Index>` 要求第一个实参是 tuple 类型，第二个实参是编译期整数索引。
- `call_result<F, Args...>` 至少需要 callable 类型 `F`；之后的调用参数类型列表可以为空，也可以是多个类型实参。参数数量和类型必须能通过普通调用检查。
- `call_result` 的结果可以作为函数返回类型、局部 `type` 别名、`requires` 类型相等约束或泛型实参继续使用。
- 查询名没有导入时就是普通未知类型名，不是全局魔法名；导入后也只在类型语法中作为内建查询识别，不能作为运行时表达式取值、传参或赋给变量。
- 只有 `call_result` 接受可变数量类型实参。`read_type<Args...>`、`remove_reference<Args...>`、`pointee<Args...>` 和 `tuple_element<Tuple, Args...>` 都不是合法展开场景。
- 类型实参包展开必须是裸类型参数包名，例如 `Args...`。`box<Args>...`、`Args const&...` 或运行时值展开都不是当前 `std.meta` 查询能力。
- `read_type`、`remove_reference`、`pointee` 和 `tuple_element` 的每个参数位置都必须是单个类型实参；写整数、运行时值或多余类型实参会报 `invalid_type_argument`。空泛型实参列表，例如 `read_type<>`，当前由 parser 直接拒绝。

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

依赖查询不会在泛型声明阶段强行求值。只要查询参数里还有类型参数、关联类型或未展开的类型参数包，编译器会保留一个待实例化的查询类型；到具体函数实例或具体类型实例里再执行上述 arity、类型实参、可调用性和越界检查。

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

- `T: callable` 检查 `T` 是否能以零个参数调用。当前 parser 不接受空显式类型实参列表，所以不能写 `T: callable<>`。
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

`callable` 和 `call_result` 使用同一套可调用性判断。函数类型、函数指针、lambda/closure 和可见 `operator ()` 都参与；如果某个类型只有在导入某个模块后才获得 extension call operator，那么 `T: callable<...>` 和 `call_result<T, ...>` 也只有在该 extension operator 可见的文件中成立。

真正来自 `std.meta` 的 `callable` 是内建 concept：能力证明由编译器按可调用性规则计算，先于普通显式 `impl` 使用。显式写 `impl callable<i32> for box {}` 不能让没有可用 `operator ()`、函数签名或闭包签名的 `box` 通过 `box: callable<i32>`；显式 impl 块本身仍按普通 concept impl 语法收集和检查，但不会覆盖这个标准 concept 的内建结果。

在泛型声明阶段，如果 `callable` 左侧目标类型或任一调用参数类型仍是依赖类型，当前实现会把内建 `callable` 判定视为暂时成立，保留到具体实例化后再用替换出的类型执行真正的可调用性检查。也就是说，`F: callable<Args...>` 本身不会在泛型声明处枚举所有未来可能的 `F` / `Args...`，失败会出现在使用某组具体类型实例化该约束时。

因为 `callable` 的左侧是类型而不是函数符号，函数声明上的默认参数同样不会参与 concept 判定。`T: callable<i32>` 只表示 `T` 类型的值能用一个 `i32` 参数调用；如果 `T` 是 `f(i32, i32) -> i32`，第二个参数是否来自某个原始函数声明的默认值已经不可见，所以该约束不成立。

## 设计边界

这些查询都是类型层工具，不能作为值表达式使用。它们服务于长期的泛型标准库能力，例如 `std.ranges.transform` 需要从任意 callable 提取输出元素类型。

不支持的内容：

- 运行时反射、字段枚举、函数列表枚举或属性查询。
- 从值表达式直接生成类型查询实参，例如 `read_type<value>`。
- 对类型构造器做实参推导；例如当前没有 `call_result<vector>` 这类 CTAD 风格能力。
- 用 `std.meta` 查询绕过普通可见性、重载、`requires` 或 concept 检查。

`std.meta` 还导出编译器识别的 reference 分类 concept：

- `is_lvalue_reference`
- `is_const_lvalue_reference`
- `is_move_reference`

它们主要服务于 `template if` 分发。例如 `std.ranges.to_view(source: R forward&)` 会检查 `decltype(forward source)`，把可写左值转成 `ref_view`，把 const 左值转成 `const_ref_view`，把右值转成 `owning_view`。

这三个 concept 和 `callable` 一样，只有当当前可见的 concept 符号来自 `std.meta` 模块时才触发内建判定；用户自己声明同名 concept 不会得到这些分类语义。它们不带 concept 实参，目标类型写在 `T: concept` 的左侧：

- `T: is_lvalue_reference` 对普通 lvalue 引用成立，包括可写 `U&` 和只读 `U const&`。
- `T: is_const_lvalue_reference` 只对只读普通 lvalue 引用成立。
- `T: is_move_reference` 只对 `U move&` 成立。
- 非引用类型、函数类型、指针、tuple、数组和 `forward&` 本身都不满足这些分类。`forward&` 形参在具体调用中先按值类别物化为普通 lvalue 引用、const lvalue 引用或 move 引用，再进入这些 concept 检查。

这些 reference 分类同样不能靠显式 `impl` 伪造。对真正来自 `std.meta` 的名字，`impl is_move_reference for i32 {}` 不会让 `i32: is_move_reference` 成立；分类只由目标类型本身的引用种类决定。

这些名字不是 `trait<T>` 风格的一元查询，也不产生可继续使用的类型结果。`T: is_lvalue_reference<i32>`、`T: is_move_reference<U>` 这类写法会因为 concept 实参数量不匹配而不满足约束；需要取得被引用的元素类型时，使用 `pointee<T>` 或 `remove_reference<T>`，需要同时判断引用类别和元素类型时，把 reference 分类 concept 与类型相等约束组合起来。

当前没有 CTAD，也没有类型构造器实参推导。因此容器构造类 terminal，例如 `to<Container>()`，暂不落地。
