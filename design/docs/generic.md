# 泛型

本文档记录泛型设计，覆盖泛型函数、函数级整数 const 参数、泛型 `struct`、泛型 `variant`、泛型 `concept`、泛型固有 `impl`、泛型 concept `impl`、默认泛型参数、参数包、`template for` 展开以及 `requires` 约束。

函数泛型采用偏现代 C++ 的强模板模型：无约束泛型也可以在函数体内使用依赖于类型参数的操作；`concept` 和 `requires` 是可选约束机制，用于提前表达能力要求、改善诊断、约束公共 API，并约束构造函数和 operator 候选。

## 目标

函数泛型同时满足两类写法：

```cp
add<T>(left: T, right: T) -> T
{
    return left + right;
}
```

```cp
add_by<T: addable>(left: T, right: T) -> T
{
    return left.add(right);
}
```

第一种写法保留模板的表达力。`left + right` 依赖类型参数 `T`，在泛型函数定义阶段不要求立即证明所有 `T` 都支持加法；当某处实际使用 `add<i32>`、`add<f64>` 或 `add<some_type>` 时，编译器再用具体类型检查实例化后的函数体。

第二种写法显式声明能力要求。它不负责恢复模板能力，而是让调用者和编译器更早知道该泛型需要 `T` 满足 `addable`。

## 语法总览

```text
GenericFunction
    -> identifier GenericParameterList? ( ParameterList? ) ReturnType? RequiresClause? Block

GenericStruct
    -> export? struct identifier GenericParameterList? { FieldDecl* }

GenericVariant
    -> export? variant identifier GenericParameterList? { VariantCase* }

GenericConcept
    -> export? concept identifier GenericParameterList? { ConceptItem* }

InherentImpl
    -> impl GenericParameterList? TypePattern RequiresClause? { ImplItem* }

ConceptImpl
    -> impl GenericParameterList? ConceptId for TypePattern RequiresClause? { ConceptImplItem* }

GenericParameterList
    -> < GenericParameter ( , GenericParameter )* >

GenericParameter
    -> identifier GenericDefault?
     | identifier : IntegerGenericKind GenericDefault?
     | identifier : ConceptBoundList GenericDefault?
     | identifier ...
     | identifier ... : ConceptBoundList

IntegerGenericKind
    -> usize
     | isize

GenericDefault
    -> = TypeArgument

TypeArgumentList
    -> < TypeArgument ( , TypeArgument )* >

TypeArgument
    -> Type
     | Type ...
     | IntegerLiteral

ConceptId
    -> identifier TypeArgumentList?

ConceptBoundList
    -> ConceptId ( and ConceptId )*

RequiresClause
    -> requires RequiresConstraintExpr

RequiresConstraintExpr
    -> RequiresAnd

RequiresAnd
    -> RequiresPrimary ( and RequiresPrimary )*

RequiresPrimary
    -> RequiresConstraint
     | ( RequiresConstraintExpr )

RequiresConstraint
    -> Type : ConceptBoundList
     | TypePack : ConceptBoundList
     | Type == Type
     | identifier

TypePattern
    -> Type

TypePack
    -> identifier ...

TemplateFor
    -> template for ( TemplateForBinding : PackExpansion ) Block

TemplateIf
    -> template if ( TemplateIfCondition ) Block TemplateIfElse*

TemplateIfElse
    -> else template if ( TemplateIfCondition ) Block
     | else Block

TemplateIfCondition
    -> Type == Type
     | Type : ConceptId
     | Expression
     | not TemplateIfCondition
     | TemplateIfCondition and TemplateIfCondition
     | TemplateIfCondition or TemplateIfCondition
     | ( TemplateIfCondition )

TemplateForBinding
    -> let identifier
     | let ref identifier
     | const identifier
     | const ref identifier
     | type identifier

PackExpansion
    -> identifier ...
```

函数名之后、参数列表之前可以出现泛型参数列表：

```cp
id<T>(value: T) -> T
{
    return value;
}
```

普通函数参数也可以省略类型。每个省略类型的非 `self` 参数都会引入一个只能由调用实参推导的隐藏类型参数：

```cp
add(left, right) -> i32
{
    return left + right;
}
```

上面等价于声明两个新的函数级类型参数，并把参数类型写成对应参数：

```text
add<$parameter.0, $parameter.1>(left: $parameter.0, right: $parameter.1) -> i32
```

这些隐藏参数不出现在源码泛型参数列表中，也不能在调用点显式传入；需要在返回类型、`requires` 或其它参数类型中引用同一个类型时，应写完整泛型：

```cp
id<T>(value: T) -> T
{
    return value;
}
```

省略类型参数可以保留引用形态：

```cp
read(value const&) -> i32   // 等价于 read<T>(value: T const&) -> i32
edit(value&) -> void        // 等价于 edit<T>(value: T&) -> void
take(value move&) -> i32    // 等价于 take<T>(value: T move&) -> i32
relay(value forward&)       // 等价于 relay<T>(value: T forward&)
```

这种写法只省略基础类型，`const&`、`&`、`move&`、`forward&` 仍然是参数类型的一部分。显式类型时必须把引用写在类型位置：

```cp
read(value: i32 const&) -> i32
edit(value: i32&) -> void
take(value: i32 move&) -> i32
relay<T>(value: T forward&) -> void
```

不允许混合两种写法：

```cp
bad(value&: i32) -> i32   // 错误：应写 value: i32& 或 value&
bad(value const&: i32)    // 错误：应写 value: i32 const& 或 value const&
```

省略类型参数不能带默认值，因为调用省略该实参时没有普通实参可用于推导隐藏类型参数。需要默认值时应写显式类型或显式泛型参数：

```cp
fill(value: i32 = 0) -> i32
{
    return value;
}
```

泛型参数默认是编译期类型参数。函数泛型还支持第一版整数 const 参数，写作 `N: usize` 或 `I: isize`，用于 `[T; N]` 这样的类型模式。

泛型参数也可以声明为类型参数包：

```cp
print<T...>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}
```

`T...` 表示零个或多个类型参数，`values: T...` 表示与 `T...` 等长的值参数包。参数包规则见“参数包与 template for”。

当前实现的参数包能力分两类：

- 函数和泛型 lambda 可以建立类型参数包以及对应的值参数包，例如 `print<T...>(values: T...)`。具体函数实例或具体 lambda 实例会得到 `T...` 的类型实参序列，`values...` 与它等长。
- `concept` 声明可以把最后一个类型参数声明为 concept 参数包，例如 `concept callable<Args...>`。它只用于 concept 实参匹配和约束检查，不会创建值参数包，也不能在 concept 体内用 `template for` 展开。

`struct tuple<T...>`、`variant choice<T...>`、`impl<T...> ...` 或 `impl<T...> concept for type` 这类类型构造器/实现级参数包不是当前可用的公开能力；第一版也没有 pack field expansion。concept 参数包的公开用法同样要求写在泛型参数列表末尾；非末尾 concept pack 不作为可依赖语义。

泛型参数可以带默认实参：

```cp
struct pair<T, U = T> {
    first: T;
    second: U;
}

concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}
```

默认实参按声明顺序求值，可以引用前面已经声明的泛型参数。调用或使用类型构造器时，显式实参按前缀匹配，缺省位置由默认实参补齐：

```cp
let same: pair<i32> = pair<i32>{ .first = 1, .second = 2 };
let mixed: pair<i32, bool> = pair<i32, bool>{ .first = 1, .second = true };
```

默认泛型实参只用来补齐当前声明中仍未确定的尾部参数。它不能跳过中间参数，也不能反向推出前面的参数。函数调用点显式写出的泛型实参同样按前缀绑定；如果某个前缀实参与普通实参推导出的类型冲突，调用失败。

函数参数也可以带默认值。默认值只能用于尾部参数，调用时只能省略尾部实参：

```cp
sort<T: mutable_object, Order: ordering<T> = asc<T>>(values: span<T>, order: Order = Order{}) -> void
{
    // ...
}

sort(values);             // order 使用 Order{}
sort(values, desc<i32>{}); // 显式降序 order
```

函数默认表达式在泛型实参确定后检查，因此 `Order{}` 会在 `Order` 被推导或由默认泛型实参补齐后再按参数类型检查。默认表达式可以引用前面已经声明的泛型参数和值参数。

函数参数默认值的当前实现边界：

- 只能用于尾部参数；一旦某个参数有默认值，后续普通参数也必须有默认值。
- 默认值参数必须有显式参数类型，不能用于省略类型参数，例如 `bad(value = 1)`。
- `self` receiver 不能有默认值。
- 值参数包不能有默认值。
- lambda 参数不能有默认值。
- 调用点只允许省略尾部实参；默认表达式按对应参数类型做普通表达式检查。

`this` 可以出现在 concept 泛型参数默认值中，表示当前被检查或被实现的类型。因此：

```cp
T: partial_eq
impl partial_eq for i32
impl partial_eq for box<T>
```

分别按上下文展开为：

```text
T: partial_eq<T>
impl partial_eq<i32> for i32
impl partial_eq<box<T>> for box<T>
```

标准库和编译器可以提供内建 concept。`mutable_object` 由类型系统判断是否为可写对象类型；`equality_comparable<Rhs = this>` 按 `==` 是否可用判断；`incrementable` 按前置 `++` 是否可用判断。它们用于 `sort`、`swap`、`iota` 这类需要提前表达类型能力的泛型算法和范围。

## 无约束泛型

无约束类型参数不声明任何 concept：

```cp
id<T>(value: T) -> T
{
    return value;
}

make_pair<T, U>(first: T, second: U) -> (T, U)
{
    return (first, second);
}
```

无约束泛型允许使用依赖于类型参数的操作：

```cp
max<T>(left: T, right: T) -> T
{
    if(left < right) {
        return right;
    }

    return left;
}

len<T>(value: T) -> i32
{
    return value.size();
}
```

这些操作不是对所有 `T` 都合法，而是形成依赖语义：

- `left < right` 依赖 `T`。
- `value.size()` 依赖 `T`。
- `T::item` 这类关联类型查找依赖 `T`。

编译器在泛型函数定义阶段只检查语法、泛型参数绑定、非依赖名字和明显不依赖 `T` 的错误。实际类型实参确定后，编译器把 `T` 替换为具体类型，并对实例化后的函数体执行完整语义检查。

例如：

```cp
let a = max(1, 2);      // 实例化 max<i32>
let b = max(1.0, 2.0);  // 实例化 max<f64>
```

如果某个类型不支持 `<`，则对应的 `max<that_type>` 实例化失败。

## UFCS 与依赖调用

UFCS 规则见 [struct.md](struct.md)。泛型函数中，接收者或首个实参依赖类型参数时，UFCS 查找可以延迟到实例化阶段：

```cp
call_size<T>(value: T) -> i32
{
    return value.size();
}

call_free<T>(value: T) -> i32
{
    return size(value);
}
```

在泛型函数定义阶段，`value.size()` 和 `size(value)` 都可以形成依赖调用。实例化时替换 `T` 后，再按普通 UFCS 规则解析：

- `value.size()` 先查具体类型的成员 `size`，名字不存在时再查自由函数 `size(value)`。
- `size(value)` 先查当前可见自由函数 `size`；如果当前在成员函数体内，名字不存在时先查 `self.size(value)`；仍不存在且调用至少有一个参数时，再查具体类型的成员 `value.size()`。

回退只在首选路径中不存在同名函数时触发。若同名函数存在，但实例化后的参数类型不匹配、`self` 可变性不满足、返回类型不符合上下文，或 concept / `requires` 约束不满足，都应报告该路径上的错误，不继续尝试另一种 UFCS 形式。

如果调用不依赖类型参数，则在泛型函数定义阶段立即按同一规则解析。非依赖名字已经存在但签名不匹配时，不能把它延迟到实例化阶段。

## 内联 concept 约束

简单约束可以写在泛型参数中：

```cp
max<T: comparable>(left: T, right: T) -> T
{
    if(left.less(right)) {
        return right;
    }

    return left;
}
```

多个 concept 使用 `and`：

```cp
sort_value<T: comparable and movable>(value: T) -> T
{
    return value;
}
```

`T: comparable and movable` 的语义是：

```text
T implements comparable
T implements movable
```

这里采用 `T: concept` 作为能力约束写法。约束中的 concept 可以带泛型实参；如果省略实参，则按 concept 声明中的默认泛型参数补齐。

内联约束适合短约束。它等价于把同样的 concept 要求写进 `requires`：

```cp
max<T>(left: T, right: T) -> T
requires
    T: comparable
{
    if(left.less(right)) {
        return right;
    }

    return left;
}
```

## requires 子句

复杂约束写在函数头后的 `requires` 子句中：

```cp
copy_all<I: iterator, O: sink>(input: I, output: O)
requires
    I::iter_item == O::sink_item
{
}
```

`requires` 约束表达式可以包含：

- 类型参数的 concept 要求。
- 多个 concept 的 `and` 组合。
- 类型相等约束，常用于关联类型。
- 在 `concept` 体内，裸 concept 名表示 `this` 必须满足该父 concept。

例如：

```cp
merge<T, I, O>(input: I, output: O)
requires
    T: comparable
    and I: iterator
    and O: sink
    and I::iter_item == T
    and O::sink_item == T
{
}
```

推荐规则：

- 简单的一两个 concept 约束写在 `<T: concept>` 中。
- 类型相等、多个类型参数之间的关系、较长约束写进 `requires`。
- `requires` 使用 `and` 连接约束项，允许用括号组合子表达式，不支持 `or` 和 C++ 风格的任意布尔表达式模板。

## 泛型 concept

`concept` 名之后可以出现泛型参数列表。泛型 concept 描述“当前类型和其它类型之间”的静态关系：

```cp
concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}
```

`Rhs = this` 让同类型比较成为默认约束，异类型比较仍可以显式写出右侧类型：

```cp
requires
    T: partial_eq
    and T: partial_eq<str>
```

在 concept 声明体内，泛型参数的作用域覆盖 `requires`、关联类型、函数要求和默认函数实现。`this` 表示当前实现类型；当 concept 被用于 `T: partial_eq`、`impl partial_eq for i32` 或 `impl partial_eq for box<T>` 时，`this` 分别绑定到 `T`、`i32` 和 `box<T>`。

## 参数包与 template for

参数包用于类型安全的变参泛型。只支持顺序展开，不支持 `sizeof...`、编译期索引、fold expression 或 pack pattern matching。

类型参数包写作：

```cp
print<T...>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}
```

规则：

- `T...` 在泛型参数列表中声明一个类型参数包。
- `values: T...` 在函数参数列表中声明一个值参数包。
- 函数和泛型 lambda 的值参数包长度与对应类型参数包长度相同。
- concept 的类型参数包只吸收 concept 引用处的剩余类型实参，例如 `F: callable<i32, bool>` 会把 `Args...` 绑定为 `[i32, bool]`。
- 参数包可以为空；为空时对应的 `template for` body 展开零次。
- 函数、泛型 lambda 和 concept 的类型参数包都应写在泛型参数列表最后一个参数位置；`bad<T..., U>` 不作为公开能力。
- 值参数包必须位于普通参数列表末尾，并且函数或泛型 lambda 必须同时声明类型参数包。`bad(values: i32...)` 不合法；值参数包不是“任意重复某个固定类型参数”的语法。
- 整数 const 泛型参数不能声明为参数包；当前只支持类型参数包。
- 参数包不能带默认实参，值参数包参数也不能带默认值。
- 参数包不是数组、元组或 slice，不能整体存储、返回、赋值或作为普通表达式使用。
- 类型参数包不能在需要单个类型的位置直接使用；`bad<T...>(value: T...) -> T` 不合法，返回类型应通过 `template for`、`decltype`、关联类型或其它单个类型表达式得到。
- 值参数包只能在明确的值展开上下文中使用；当前值展开上下文只有 `template for`。
- 类型参数包可以在 `template for(type U : T...)` 中逐个展开，也可以在有限的类型实参列表中写成 `T...` 展开。
- concept 参数包不能 `template for` 展开；它只能出现在 concept 自身的类型实参匹配中，或作为被约束 concept 的实参列表的一部分。

类型实参列表中的参数包展开只支持“裸类型参数包名加 `...`”。它不是一般 pack pattern：

```cp
make<F, Args...>() -> call_result<F, Args...>
requires
    F: callable<Args...>
{
    return 42;
}
```

允许的公开场景：

- `call_result<F, Args...>` 这类支持可变类型实参的元类型查询。
- `F: callable<Args...>` 这类目标 concept 最后一个参数是类型参数包的 concept 引用。
- 展开后继续跟固定类型实参，例如 `call_result<F, Args..., bool>` 和 `F: callable<Args..., bool>`。这表示先插入当前 `Args...` 的每个元素，再追加 `bool`。
- 显式泛型函数或泛型 lambda 调用实参，例如 `make<f(i32, bool) -> i32, i32, bool>()`，以及在泛型内部把当前 `Args...` 展开给另一个可接受这些类型实参的泛型调用。

具体函数实例中，`Args...` 展开为当前实例的实际类型序列；泛型定义体内尚未具体化时，展开保留为依赖的 pack expansion，只能继续流向上述允许的可变实参位置。普通类型构造器、非可变 meta query 和单个类型参数槽不会因为看见 `Args...` 就展开。

不支持的形式：

```cp
type A = read_type<Args...>;      // 错误：read_type 只接收一个类型实参
type B = tuple<Args...>;          // 不表示把 Args 展开为多个 tuple element，当前不可依赖
type C = box<Args...>;            // struct/variant/opaque alias 实参列表不提供公开 pack expansion 语义
type D = call_result<F, box<Args>...>; // 错误：不是裸 pack 名
requires T: same_as<Args...>;     // 错误：same_as 若不是参数包 concept，不能接收展开
```

因此，当前 `Args...` 在公开支持的类型实参位置的意义是“把当前类型参数包的元素插入这个实参列表”，不是“对任意类型模式做映射”。普通命名类型构造器不会因为写了 `...` 就获得 pack field 或 pack element 展开能力；在具体实例中把类型参数包当作单个类型使用仍会失败。如果需要把每个 `Args` 包进 `box<Args>`，应使用 `template for(type U : Args...)` 分别处理，或等待后续 pack pattern 设计。

值参数包的元素类型可以是简单 `T...`，也可以把 `T` 嵌在当前实现支持推导的复合类型里：

```cp
sum_arrays<T...>(values: [T; 2]...) -> i32
sum_tuples<T...>(values: (T, i32)...) -> i32
sum_boxes<T...>(values: box<T>...) -> i32
sum_options<T...>(values: optional<T>...) -> i32
sum_ref<T...>(values: T const&...) -> i32
```

调用这些函数时，编译器按每个实参位置推导当前 pack 元素类型。一个 pack 元素在同一个参数模式中出现多次时，所有位置必须推导出同一个类型；不匹配则该实例化失败。

值参数包的目的始终是展开某个函数级类型参数包。即使元素类型看起来是固定类型，也必须有类型参数包参与该函数的实例化语义；当前公开写法应把每个元素的类型模式写成和类型参数包相关的形式，例如 `T...`、`[T; 2]...` 或 `box<T>...`。不能只写 `values: i32...` 来表达“任意数量的 i32 参数”。

`template for` 遍历值参数包：

```cp
log<T...>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}
```

语义上，如果调用：

```cp
log(1, "x", 2.0);
```

则 `template for` 在实例化期按位置展开三次，每次展开体内的 `value` 分别绑定到当前实参。它不是运行时循环。

`template for` 也可以遍历类型参数包：

```cp
register_all<T...>()
{
    template for(type U : T...) {
        register_type<U>();
    }
}
```

这里 `U` 是每次展开中的类型别名。

如果遍历值包时需要当前值的类型，使用 `decltype(value)`：

```cp
debug<T...>(values: T...)
{
    template for(let value : values...) {
        type U = decltype(value);
        write_type_name<U>();
        write(value);
    }
}
```

因此不设计“类型包和值包同步遍历”的特殊语法。值包迭代变量天然携带当前静态类型，`decltype(value)` 足以取出它。

`template for` 展开规则：

- 展开顺序从左到右。
- 每次展开 body 都建立独立作用域。
- `let value` 绑定当前值，`const value` 绑定当前只读值。
- `let ref value` 要求当前 pack 元素是可引用左值，并按普通 `let ref` 规则保留可写性。
- `const ref value` 要求当前 pack 元素可引用，并绑定为只读引用。
- `type U` 绑定当前类型。
- `return` 仍然返回外层函数、lambda 或当前函数边界。
- `return` 会参与外层函数返回类型推导；类型包和值包展开体中的 `return value;` 都按被展开后的普通语句处理。
- 对具体实例来说，非空参数包的 `return` 按展开次数逐次贡献返回类型；空参数包展开零次，因此展开体内的 `return` 不贡献任何返回类型。
- `template for(let value : values...)` 只能遍历当前函数实例的值参数包，不能遍历数组、元组、range 或普通局部变量。
- `template for(type U : T...)` 只能遍历当前函数实例的类型参数包；`U` 是类型别名，不是运行时值。
- `break` 和 `continue` 不允许直接作用于 `template for`，因为它不是运行时循环，也不能穿透 `template for` 去控制外层运行时循环。
- 展开后的语句按普通语义检查；依赖类型或依赖表达式可以延迟到实例化后检查。

## template if

`template if` 是语句级编译期分支，用于在泛型实例化后选择一段普通语句体：

```cp
select<T>(value: T) -> i32
{
    template if(T == i32) {
        return value;
    } else template if(T == bool) {
        return 1;
    } else {
        return 0;
    }
}
```

条件支持类型相等、concept 条件、可求值为 `bool` 的编译期常量表达式，以及 `not`、`and`、`or` 和括号组合。`T == U` 两侧能作为类型解析时按类型相等处理；普通表达式相等仍按常量表达式求值。

`template if` 条件必须能在实例化期得到 `bool` 常量。非 `bool` 条件报错；依赖运行时局部变量的条件不是 constexpr，也会报错。`and` / `or` 按编译期短路规则处理，因此 `template if(false and missing_name)` 不会检查右侧名字，`template if(true or missing_name)` 也不会检查右侧名字。

选中分支按普通语句做语义检查并参与 IR/codegen；未选中分支只要求语法正确，不解析名字、不检查类型，也不参与返回、循环或借用状态。没有分支命中且没有 `else` 时等价于空语句。被选中的分支会参与返回类型推导，未选中的分支不会贡献返回类型。

参数包约束可以写在内联泛型参数或 `requires` 中：

```cp
print<T...: display>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}
```

等价于：

```cp
print<T...>(values: T...)
requires
    T...: display
{
    template for(let value : values...) {
        write(value);
    }
}
```

`T...: display` 表示参数包中每个类型都必须满足 `display`。多个 concept 仍使用 `and`：

```cp
requires
    T...: display and movable
```

不支持对参数包写随机访问、长度查询或条件拆分：

```cp
sizeof...(T)       // unsupported in first stage
at...(values, 0)   // unsupported in first stage
values...[0]       // unsupported in first stage
```

## 泛型 struct

`struct` 名之后可以出现泛型参数列表：

```cp
struct vector<T> {
    data: ptr<T>;
    size: usize;
    capacity: usize;
}
```

泛型 `struct` 定义的是一个名义类型构造器。`vector<i32>` 和 `vector<f64>` 是不同的具体类型实例，但共享同一个泛型定义。

字段类型可以直接使用结构体泛型参数。结构体泛型参数的作用域覆盖整个 `struct` 体，不覆盖对应的 `impl`；`impl` 需要从自己的目标类型模式重新绑定参数。

用户自定义泛型类型支持类型参数和第一版整数 const 参数，因此可以声明 `struct buffer<T, N: usize>` 这种由值参数参与类型的名义类型。固定数组 `[T; N]` 是类型系统小内建，规则见 [type_system.md](type_system.md)。

## 泛型 variant

`variant` 名之后可以出现泛型参数列表，规则和泛型 `struct` 一致：

```cp
variant optional<T> {
    none;
    some(T);
}

variant expected<T, E> {
    value(T);
    unexpected(E);
}
```

泛型 `variant` 定义的是一个名义和类型构造器。`optional<i32>` 和 `optional<f64>` 是不同的具体类型实例，但共享同一个泛型定义。

case payload 类型可以直接使用 `variant` 的泛型参数。`variant` 泛型参数的作用域覆盖整个 `variant` 体，不覆盖对应的 `impl`；`impl` 需要从自己的目标类型模式重新绑定参数。

泛型 `variant` 和泛型 `struct` 一样支持类型参数、整数 const 参数和默认泛型实参：

```cp
variant fixed_optional<T, N: usize = 1> {
    none;
    some([T; N]);
}
```

case 构造器挂在具体实例的类型命名空间下。不做 case 构造器类型实参推导，因此构造泛型 `variant` 时，类型名处必须已经能形成具体实例。具体实例可以由显式实参给出，也可以由默认泛型实参补齐：

```cp
let a = optional<i32>::none;
let b = optional<i32>::some(1);
let c = expected<i32, str>::unexpected("bad input");

variant maybe<T = i32> {
    none;
    some(T);
}

let d = maybe::some(1); // T 由默认实参补为 i32，不是从 1 反推出来
```

下面这些形式不支持：

```cp
optional::some(1)
optional<_>::some(1)
expected<i32>::unexpected("bad input")
```

`match` 面对泛型 `variant` 的具体实例时，先把类型参数替换为具体类型，再检查 case payload 绑定和分支类型统一：

```cp
value_or<T>(value: optional<T>, fallback: T) -> T
{
    return match value {
        .some(v) => v,
        .none => fallback,
    };
}
```

泛型 `variant` 的运行时布局按具体实例计算。`optional<i32>`、`optional<str>`、`expected<i32,str>` 分别拥有自己的 `tag + payload storage` 布局；tag 编号仍是编译器内部细节，不进入源语言语义。

## 泛型 impl

`impl` 使用 `impl<...>` 声明自己的泛型参数，随后在目标类型模式中使用这些参数：

```cp
impl<T> vector<T> {
    len(self const&) -> usize
    {
        return size;
    }
}
```

上例中，`T` 来自 `impl<T>`，作用域覆盖：

- `impl` 级 `requires` 子句。
- `impl` 块内的成员函数、关联函数、构造函数和析构函数签名。
- `impl` 块内的函数体。
- `impl` 块内的 `type` 类型别名。

`impl X<T> for Y<T>` 这种写法中的两个 `T` 只有在 `impl<T>` 中声明后才表示同一个泛型参数；完整写法是 `impl<T> X<T> for Y<T>`。`impl` 头部是泛型作用域的入口，后续的目标类型模式、concept 实参和 `requires` 都引用这个作用域。

`impl` 级 `requires` 是条件实现。只有当具体类型实参满足约束时，这个 `impl` 块里的项才参与成员查找和关联项查找：

```cp
impl<T> vector<T>
requires
    T: movable
{
    push_back(self&, value: T)
    {
    }
}
```

含义是：

```text
for every T:
    if T implements movable:
        vector<T> has this impl block
```

因此 `vector<i32>` 是否拥有 `push_back`，取决于 `i32` 是否满足 `movable`。约束不满足时，不是在 `impl` 定义处报错，而是在使用点把该 `impl` 排除出候选；如果没有其它可用候选，则报告“没有满足约束的成员/关联项”，并指出失败的约束。

函数级 `requires` 只控制单个成员函数是否参与候选：

```cp
impl<T> vector<T> {
    contains(self const&, value: T const&) -> bool
    requires
        T: comparable
    {
        return false;
    }
}
```

`vector<file>` 如果不满足 `file: comparable`，则 `contains` 不参与 `vector<file>` 的成员查找。诊断应指向调用点，并说明候选存在但约束不满足。

定义阶段仍然检查非依赖错误。完全不依赖 `T` 或成员函数自身泛型参数的未知名字、语法错误、重复定义等，不允许延迟到实例化阶段。

`impl` 内部可以定义自己的泛型函数。成员函数泛型参数的作用域只覆盖该函数，不覆盖整个 `impl`：

```cp
impl<T> vector<T> {
    convert_all<U>(self const&) -> vector<U>
    requires
        T: convertible_to<U>
    {
        return vector<U>{};
    }
}
```

这里 `T` 来自 `impl<T>`，`U` 来自 `convert_all<U>`。

泛型 `impl` 中的构造函数和析构函数仍然使用类型构造器名，不写类型实参：

```cp
impl<T> vector<T> {
    vector()
    {
        return vector<T>{};
    }

    ~vector()
    {
    }
}
```

## 泛型 concept impl

`impl<...> ConceptId for TypePattern` 为一族类型提供 concept 证明，并可使用 `requires` 表达条件实现：

```cp
impl<T> comparable for vector<T>
requires
    T: comparable
{
    less(self const&, rhs: this const&) -> bool
    {
        return self.len() < rhs.len();
    }
}
```

含义是：

```text
if T implements comparable:
    vector<T> implements comparable
```

泛型 concept 的实参写在 concept 名后。省略时按 concept 声明中的默认泛型参数补齐：

```cp
concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}

impl<T> partial_eq for box<T>
requires
    T: partial_eq
{
    equals(self const&, rhs: box<T> const&) -> bool
    {
        return value.equals(rhs.value);
    }
}
```

这里 `impl<T> partial_eq for box<T>` 等价于 `impl<T> partial_eq<box<T>> for box<T>`，因为 `partial_eq` 的 `Rhs` 默认值是 `this`。

异类型关系可以显式写出 concept 实参：

```cp
impl partial_eq<str> for string_like {
    equals(self const&, rhs: str const&) -> bool
    {
        return true;
    }
}
```

## 与 concept 的关系

`concept` 描述静态能力：

```cp
concept comparable {
    less(self const&, rhs: this const&) -> bool;
}
```

类型通过 `impl Concept for Type` 证明自己满足能力：

```cp
impl comparable for i32 {
    less(self const&, rhs: this const&) -> bool
    {
        return self < rhs;
    }
}
```

泛型函数通过约束要求类型参数具备能力：

```cp
min<T: comparable>(left: T, right: T) -> T
{
    if(right.less(left)) {
        return right;
    }

    return left;
}
```

三者关系为：

```text
concept comparable          描述能力
impl comparable for i32     证明 i32 有这个能力
T: comparable               要求泛型参数 T 有这个能力
impl<T> comparable for vector<T> requires T: comparable
                            条件证明 vector<T> 有这个能力
```

无约束泛型仍然可以使用依赖操作，但 concept 约束让这些操作从“实例化时才发现是否成立”变成“公共接口上已经声明需要什么能力”。

## 函数类型实参推导

函数调用可以省略类型实参，由普通实参反推出泛型参数：

```cp
add<T>(left: T, right: T) -> T
{
    return left + right;
}

let value = add(1, 2);      // 推导 T = i32，等价于 add<i32>(1, 2)
let other = add(1.0, 2.0);  // 推导 T = f64，等价于 add<f64>(1.0, 2.0)
```

没有函数重载，因此推导不参与重载排序。编译器先根据函数名找到唯一函数声明，再决定使用显式类型实参还是执行推导：

- 如果调用点写了显式泛型实参，它们按泛型参数列表前缀匹配；剩余参数由默认实参补齐。
- 如果调用点没有写显式泛型实参，编译器先从普通实参推导泛型参数，再用默认实参补齐仍未确定的参数。
- 不从返回类型、变量声明类型或其他上下文反推类型参数。
- 省略参数类型引入的隐藏类型参数只能由普通实参推导，不接受调用点显式类型实参。

推导过程把形参类型当作模式，把实参表达式的已知类型当作目标类型：

```cp
id<T>(value: T) -> T
{
    return value;
}

let a = id(1); // T = i32
```

当同一个类型参数出现多次时，所有位置必须推导出同一个具体类型：

```cp
same<T>(left: T, right: T) -> T
{
    return left;
}

let ok = same(1, 2);      // T = i32
let bad = same(1, 2.0);   // 错误：T 同时要求为 i32 和 f64
```

推导阶段不做隐式转换。隐式转换只在类型实参已经确定之后，按普通调用检查规则处理。也就是说，`same(1, 2.0)` 不会为了推导 `T` 而先把 `1` 提升为 `f64`。

形参类型可以是包含类型参数的复合类型。推导时递归匹配类型结构：

```cp
first<T, U>(value: (T, U)) -> T
{
    return value.0;
}

let x = first((1, 2.0)); // T = i32, U = f64
```

递归匹配规则：

- 形参模式是类型参数 `T` 时，把 `T` 绑定到当前目标类型。
- 形参模式和目标类型是同一种复合类型时，递归匹配其类型实参，例如 `(T, U)` 对 `(i32, f64)`。
- `[T; 3]` 可以从 `[i32; 3]` 推导出 `T = i32`。
- `[T; N]` 可以从 `[i32; 4]` 推导出 `T = i32, N = 4`，其中 `N` 必须是当前函数可见的 `usize` 或 `isize` 整数 const 参数。
- 引用形参先按被引用的值类型推导，例如 `T const&`、`T&`、`T move&` 和 `T forward&` 都会从目标表达式的读类型推出 `T`；表达式是否可绑定到对应引用、是否只读、是否需要移动，随后按普通调用转换规则检查。
- 指针形参按指针结构递归匹配。`T*` 只能从非 const 指针推导；`T const*` 可以从只读或可写指针推导出 `T`；`like*` 的 const 性由接收者或外层 `like` 规则在实例化后物化。
- 函数类型形参按完整函数类型匹配，参数列表长度必须一致，并递归匹配每个参数类型和返回类型；例如 `apply<T, R>(callback: f(T) -> R, value: T) -> R` 可以从 `f(i32) -> bool` 推导出 `T = i32, R = bool`。
- 泛型 `struct` 和泛型 `variant` 只在名义类型相同、实参数量相同的情况下递归匹配其类型实参；不同声明即使结构相同也不会互相推导。

参数包推导只支持函数参数列表末尾的值参数包：

```cp
print<T...>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}

print(1, "x", 2.0); // T... = i32, str, f64
```

推导规则：

- `values: T...` 匹配调用点剩余的全部普通实参。
- 每个实参按当前位置推导一个类型实参，形成 `T...`。
- 参数包可以推导为空包。
- 如果函数还有普通形参，普通形参先按位置匹配，剩余实参进入末尾参数包。
- 不支持非末尾参数包，也不支持一个函数参数列表中多个值参数包。
- 包内类型推导不做隐式转换，和普通泛型推导一致。
- 复合模式可以递归穿过数组、storage、元组、函数类型、指针、引用、泛型 `struct` 和泛型 `variant`；结构不匹配时不推导。
- 没有值参数包可用时，非空类型参数包只能由调用点显式给出；不写类型实参且没有普通实参可推导时，只能得到空包。
- 显式类型实参和从普通实参推导出的类型实参必须一致；冲突时报错。

如果某个类型参数无法从普通实参推出，调用必须显式写出全部类型实参：

```cp
make<T>() -> T
{
    return T{};
}

let a = make<i32>(); // 合法
let b: i32 = make(); // 错误：不从返回上下文推导 T
```

`concept` 和 `requires` 约束不参与反向推导，只在类型实参确定后检查：

```cp
copy_all<I: iterator, O: sink>(input: I, output: O)
requires
    I::iter_item == O::sink_item
{
}
```

这里 `I` 和 `O` 只能从 `input`、`output` 的实参类型推导；`I::iter_item == O::sink_item` 用于验证推导结果，而不是产生新的类型参数绑定。

## 实例化语义

泛型不是运行时多态。后端采用单态化：

```cp
add<T>(left: T, right: T) -> T
{
    return left + right;
}

main() -> i32
{
    return add(1, 2);
}
```

`add(1, 2)` 推导出 `T = i32`，编译器生成或复用 `add<i32>` 的实例。

函数实例化流程建议为：

1. 解析并保存泛型函数 AST、类型参数表和约束表达式。
2. 调用点根据显式类型实参或普通实参推导类型参数。
3. 检查显式 concept / `requires` 约束。
4. 用具体类型替换类型参数。
5. 对替换后的函数体执行完整语义检查，包括依赖 UFCS 调用。
6. 缓存已成功实例化的版本，避免重复生成；含 `forward&` 参数的函数还要把每个 forward 参数的可写左值、const 左值、右值绑定类别放入实例化 key。

如果实例化失败，诊断应同时指出调用点和泛型函数体中失败的依赖操作。

泛型 `struct`、泛型 `variant` 和 `impl` 的实例化规则与函数一致：先由具体使用点确定类型实参，再检查 `requires` 约束，最后对依赖函数体执行完整语义检查。条件 `impl` 的约束不满足时，该 `impl` 不生成对应具体实例，也不向该具体类型贡献成员或 concept 证明。

## 支持内容

泛型支持：

- 类型参数：`func<T, U>(...)`
- 省略参数类型引入的隐藏类型参数：`func(x, y)`、`func(x&)`、`func(x const&)`、`func(x move&)`、`func(x forward&)`
- 强无约束泛型：允许依赖操作，实例化时检查
- 内联 concept 约束：`T: comparable`
- 多 concept 约束：`T: readable and writable`
- `requires` 子句
- 类型相等约束：`I::iter_item == T`
- 依赖 `decltype(expr)`：实例化后确定表达式类型
- 参数包：`func<T...>(values: T...)`
- 参数包约束：`T...: display`
- concept 参数包：`concept callable<Args...>`
- 整数 const 参数：`N: usize` / `I: isize`
- `template for(let value : values...)` 值包展开
- `template for(type U : T...)` 类型包展开
- 依赖 UFCS 调用实例化时检查
- 函数类型实参推导：从普通实参推导，支持嵌套类型模式
- 泛型 `struct`：`struct vector<T>`
- 默认泛型参数：`struct pair<T, U = T>`
- 带整数 const 参数的泛型 `struct`：`struct buffer<T, N: usize>`
- 泛型 `variant`：`variant optional<T>`
- 泛型 `concept`：`concept partial_eq<Rhs = this>`
- 泛型 concept 默认参数中的 `this` 绑定
- 泛型固有 `impl`：`impl<T> vector<T>`
- `impl` 级条件约束：`impl<T> vector<T> requires T: movable`
- 成员泛型函数：`convert_all<U>(...)`
- 泛型 concept `impl`：`impl<T> comparable for vector<T> requires T: comparable`
- 显式 concept 实参：`impl partial_eq<str> for string_like`
- 单态化代码生成
