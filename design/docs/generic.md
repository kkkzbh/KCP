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

这里的 `ConceptId` 是当前作用域可见的未限定 concept 名。模块系统不提供 concept 的限定访问语法；不能写 `std.compare::equality_comparable`、`module.concept` 或 `Type::concept`。需要跨模块使用 concept 时，先通过 `import` / `export import` 让它以裸名进入当前文件可见表。

泛型参数列表不能为空。声明没有泛型参数时，应省略整组尖括号，例如写 `id(value: i32)` 而不是 `id<>(value: i32)`；`struct box<> { ... }`、`concept marker<> { ... }`、`impl<> item { ... }` 和泛型 lambda 的 `f<>() -> R { ... }` 都会在 parser 阶段被拒绝。调用点同样没有空显式类型实参列表语法；需要让默认泛型实参或空参数包生效时，直接省略 `<...>`，例如 `make()` 或 `count()`。

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

每个省略类型参数都会生成一个独立的隐藏类型参数，不会因为参数名、使用方式或相邻位置自动合并。`same(left, right)` 表示 `left` 和 `right` 可以分别推导成不同类型；如果 API 要求两个实参类型相同，必须显式写共享的 `T`：

```cp
same<T>(left: T, right: T) -> bool
{
    return left == right;
}
```

实现内部会给这些隐藏参数分配类似 `$parameter.0` 的名字，但这不是源码可引用的名字，也不是稳定文档接口。源码中不能写 `$parameter.0`，也不能在类型位置用值参数名 `left` 代表“left 的类型”；需要复用类型时就显式声明泛型参数。

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

`forward&` 是泛型形参的实例化规则，不是一个可以用于具体非依赖类型的普通引用种类。调用泛型函数时，`forward&` 形参会先按普通类型参数推导出被引用类型，再根据实参值类别物化：

- 可写左值实参物化为普通可写引用 `T&`。
- 只读左值实参物化为只读引用 `T const&`。
- 右值或 `move` 实参物化为移动引用 `T move&`。

因此含 `forward&` 形参的泛型函数实例不仅由类型实参决定，还由每个 forward 形参的绑定类别决定。同一个 `relay<i32>` 对可写左值、只读左值和右值调用时，可以形成不同的单态化实例。

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

泛型参数默认是编译期类型参数。函数泛型还支持第一版整数 const 参数，写作 `N: usize` 或 `I: isize`，用于 `[T; N]` 这样的类型模式。当前类型实参语法中的整数 const 实参只接受一个非负整数字面量，或在类型位置引用一个已经存在的整数 const 泛型参数；不支持 `N + 1`、`2 * N`、`-1`、枚举 case、普通变量或其它常量表达式作为整数 const 实参。`usize` / `isize` 只决定该泛型参数在类型系统中的整数种类；当前语义检查只区分“类型实参”与“整数常量实参”，不做范围证明，也不把 `isize` 参数开放成可写负数字面量的语法。

泛型参数也可以声明为类型参数包：

```cp
print<T...>(values: T...)
{
    template for(let value : values...) {
        write(value);
    }
}
```

`T...` 表示零个或多个类型参数，`values: T...` 表示与 `T...` 等长的值参数包。函数和泛型 lambda 的类型参数包必须写在泛型参数列表末尾；对应的值参数包也必须写在普通参数列表末尾。`bad<T..., U>()`、`bad<T...>(values: T..., last: i32)` 和 `bad(values: i32...)` 都不是当前能力：值参数包不是“重复某个固定类型”的语法，必须由同一个函数级类型参数包驱动。参数包规则见“参数包与 template for”。

当前实现的参数包能力分两类：

- 函数和泛型 lambda 可以建立类型参数包以及对应的值参数包，例如 `print<T...>(values: T...)`。具体函数实例或具体 lambda 实例会得到 `T...` 的类型实参序列，`values...` 与它等长。
- `concept` 声明可以把最后一个类型参数声明为 concept 参数包，例如 `concept callable<Args...>`。它只用于 concept 实参匹配和约束检查，不会创建值参数包，也不能在 concept 体内用 `template for` 展开。

泛型 lambda 的参数包规则和泛型函数相同，但当前泛型 lambda 必须显式写返回类型；`f<T>(value: T) { return value; }` 会在 parser 阶段报错。非泛型 lambda 仍可以按自身 body 推导返回类型。lambda 的其它捕获和调用规则见 [lambda.md](lambda.md)。

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

函数调用的显式泛型实参有两条独立规则：

- 没有类型参数包时，显式实参数量必须覆盖第一个无默认值前缀，并且不能超过源码中显式声明的泛型参数数量；缺少的尾部参数只能由默认泛型实参补齐。
- 有类型参数包时，包必须是最后一个源码泛型参数；调用点一旦写显式泛型实参，就必须写到包位置，包吸收从该位置开始的所有剩余显式实参。不能跳过包前面的默认参数直接给包传值。

省略参数类型自动引入的隐藏泛型参数排在源码泛型参数之前，不占用调用点显式泛型实参位置。它们只能由普通实参或 receiver 类型推导，不能通过 `<...>` 显式传入。

### 函数参数默认值

函数参数也可以带默认值。默认值只能用于尾部参数，调用时只能省略尾部实参：

```cp
sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void
{
    // ...
}

sort(values);             // order 使用 Order{}
sort(values, desc<i32>{}); // 显式降序 order
```

函数默认表达式在泛型实参确定后检查，因此 `Order{}` 会在 `Order` 被推导或由默认泛型实参补齐后再按参数类型检查。默认表达式可以引用前面已经声明的泛型参数，但当前公开能力不包括引用同一函数的值参数；`default: i32 = value + 1` 这类依赖前序形参名字的默认值会按普通名字查找规则检查，通常得到未知名字或匹配到外层同名符号，不会绑定到本次调用的形参值。

尾部默认值按从左到右的参数顺序补齐；后一个默认值会在前一个默认值已经补出后再 materialize，但当前公开语义不允许通过参数名读取前一个值：

```cp
range(base: i32, low: i32 = 0, high: i32 = 100) -> i32
{
    return high - low;
}

range(10);        // 等价于 range(10, 0, 100)
range(10, 8);     // 等价于 range(10, 8, 100)
range(10, 8, 12); // 不使用默认值
```

默认值不是命名实参或占位实参机制；调用只能截断尾部实参，不能跳过中间参数：

```cp
range(10, 8, 12); // 可以：完整传参
range(10);        // 可以：省略 low 和 high
range(10, 8);     // 可以：只省略 high

// 错误：当前调用语法没有“省略 low 但传 high”的形状
// range(10, /* low */ , 12);
```

函数参数默认值的当前实现边界：

- 只能用于尾部参数；一旦某个参数有默认值，后续普通参数也必须有默认值。
- 默认值参数必须有显式参数类型，不能用于省略类型参数，例如 `bad(value = 1)`。
- `self` receiver 不能有默认值。
- 值参数包不能有默认值。
- lambda 参数不能有默认值。
- 调用点只允许省略尾部实参；实参数量必须落在“必填参数数量”和“完整参数数量”之间，不能省略中间实参。
- 默认表达式按对应参数类型做普通表达式检查。当前实现只在调用点确实省略该尾部实参、需要 materialize 默认值时检查对应默认表达式；如果所有实参都由调用点显式提供，该调用不会顺便检查未使用的默认表达式。泛型函数、泛型成员函数和泛型关联函数的默认表达式会在具体实例中检查，使用该实例的类型参数替换和参数包替换。
- 默认表达式在调用点 materialize，并按对应参数类型生成真实实参。当前语义检查默认表达式时只建立泛型替换和参数包替换，不建立同一函数的值参数名字作用域；因此不要写依赖前一个值参数或前一个默认参数名字的默认表达式，也不要把“默认值可引用前序值参数”当作公开语言能力。
- 默认参数属于函数声明/函数符号，不进入 `f(...) -> R` 函数类型或 `f*(...) -> R` 函数指针类型。通过普通函数名、成员调用、关联函数调用、UFCS 解析到函数符号时可以省略尾部默认实参；通过函数类型值、函数指针值或闭包值调用时，调用参数数量必须与函数类型完全一致。
- 默认参数不改变函数的实际参数列表。`extern "C" add(value: i32, delta: i32 = 1) -> i32;` 仍是两个参数的 C 函数；省略的 `delta` 由 KCP 调用点补出后再调用它。

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

标准库和编译器可以提供内建 concept。`mutable_object` 由类型系统判断是否为可写对象类型；`ordering<T>` 按 order object 是否能以 `order(left, right)` 返回 `weak_ordering` 判断；`equality_comparable<Rhs = this>` 按 `==` 是否可用判断；`three_way_comparable<Rhs = this, Category = weak_ordering>` 按 `<=>` 是否可用且结果能转换到 `Category` 判断；`incrementable` 按前置 `++` 是否可用判断。它们用于 `sort`、`map`、`set`、`iota` 这类需要提前表达类型能力的泛型算法、容器和范围。

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

## 泛型返回类型推导

普通函数可以省略 `-> R`，返回类型推导的通用规则见 [type_system.md](type_system.md)。泛型函数、泛型关联函数和泛型成员函数的特殊点是：推导不发生在泛型定义本身，而发生在每个具体函数实例上。

```cp
id<T>(value: T)
{
    return value;
}

let a: i32 = id(1);    // id<i32> 的返回类型推导为 i32
let b: bool = id(true); // id<bool> 的返回类型推导为 bool
```

实例化顺序是：

1. 由显式类型实参、receiver 类型和普通调用实参确定泛型参数。
2. 补齐默认泛型实参，并检查 inline concept bound、参数包 bound、函数 `requires` 和所在 `impl` 的 `requires`。
3. 替换函数签名、默认参数表达式和函数体中的泛型参数。
4. 选择该实例的 `template if` 分支，展开该实例的 `template for`。
5. 在替换后的普通函数体中收集 `return`，并按普通返回类型推导规则统一。

因此，同一个泛型定义可以产生不同返回类型的实例；同一个具体实例内部的多个返回仍必须能统一。整数族和浮点族按普通返回统一规则合并；不同名义类型、`unit` 与普通值、或同一参数包展开出来的 `i32` 与 `bool` 这类不能统一的组合会让该实例返回类型推导失败。

`return` 的贡献边界：

- `return value;` 贡献替换和检查后的 `value` 表达式类型；`return;` 贡献内部 `unit`。
- `panic(...)`、`unreachable()` 和所有 arm 都为 never 的 `match` 贡献 `!`。只要同一实例中还有普通返回类型，`!` 不决定最终类型；如果所有返回都是 `!`，则实例返回 `!`。
- 块表达式中的 `return` 返回当前函数或 lambda 边界，并贡献给这个边界的返回类型；块表达式尾表达式只作为普通表达式类型参与它所在的 `return { ... };` 或其它上下文。
- lambda 体是独立函数边界。外层泛型函数不会收集 lambda 体内的 `return`；普通 lambda 可以按自己的 body 推导返回类型，泛型 lambda 当前必须显式写 `-> R`。
- `if`、`while`、`do while`、运行时 `for`、`match` arm、被选中的 `template if` 分支和 `template for` 展开体中的 `return` 都按替换后的普通语句贡献返回类型。

`template if` 只让当前实例选中的分支参与返回推导。未选中分支只保留语法要求，不解析名字、不检查类型，也不贡献返回类型：

```cp
select<T>(value: T)
{
    template if(T == i32) {
        return value;
    } else {
        return true;
    }
}

let number: i32 = select(1);   // 只看第一支
let flag: bool = select(false); // 只看 else
```

如果没有分支命中且没有 `else`，该 `template if` 等价于空语句，不贡献返回类型。语义错误恢复可以保守检查多个分支，但这不是公开能力；可被程序依赖的语义仍是“具体实例只选择一个分支”。

`template for` 的返回推导按展开结果逐次处理：

```cp
first<T...>(values: T...)
{
    template for(let value : values...) {
        return value;
    }
    return 0;
}
```

非空参数包中，每次展开里的 `return value;` 都贡献当前元素的读出类型；空参数包展开零次，因此展开体内的 `return` 不贡献任何类型。没有其它返回时，空包实例按“没有观察到返回”推导为 `unit`；有 fallback `return 0;` 时按 fallback 推导。类型参数包展开体中的 `return` 也按当前展开绑定的类型别名替换后处理。

泛型 `variant` 的 `match` 在返回推导中先实例化被匹配值的具体 variant 类型，再绑定 payload：

```cp
variant optional<T> {
    none;
    some(T);
}

value_or<T>(value: optional<T>, fallback: T)
{
    return match value {
        .some(item) => item,
        .none => fallback,
    };
}
```

`value_or(optional<i32>::some(1), 0)` 中，`.some(item)` 的 `item` 是 `i32`；`value_or(optional<bool>::none, false)` 中，`item` 是 `bool`。`match` arm 中的 `panic(...)` / `unreachable()` arm 按 never 处理；所有普通 arm 的结果类型仍必须按当前实例统一。`match` 出现在 `template for` 内时，每次 pack 展开先替换当前元素类型，再按普通 variant `match` 检查 case 名、payload 绑定数量、重复 case、穷尽性和 arm 类型。

返回表达式调用另一个省略返回类型的泛型函数时，编译器先实例化被调函数的具体实例，拿到该实例签名中的返回类型，再用于当前返回推导：

```cp
make<T>(value: T)
{
    return value;
}

forward()
{
    return make(41); // forward 推导为 i32
}
```

这个规则适用于普通泛型函数调用、泛型关联函数调用、泛型成员函数调用、隐式 `self` 成员调用和依赖 UFCS 调用。调用本身仍必须能正常实例化；显式类型实参数量错误、参数包冲突、约束失败、未知关联函数或泛型返回递归不能靠外层返回推导吞掉。

返回推导不反向决定泛型实参：

```cp
make<T>() -> T
{
    return T{};
}

let value: i32 = make(); // 错误：不会从目标返回类型反推 T
```

需要由返回类型决定的泛型参数必须显式写出，或通过普通参数、receiver、关联类型约束等前向信息推出。返回推导只在类型实参已经确定的具体实例内部工作。

## 内联 concept 约束

简单约束可以写在泛型参数中：

```cp
advance_once<T: incrementable>(value: T&) -> void
{
    ++value;
}
```

多个 concept 使用 `and`：

```cp
step_if_equal<T: equality_comparable<T> and incrementable>(left: T const&, right: T const&, value: T&) -> void
{
    if(left == right) {
        ++value;
    }
}
```

`and` 后可以只写下一个 concept 名，也可以重复写当前参数名加冒号：

```cp
step<T: equality_comparable<T> and T: incrementable>(value: T&) -> void
{
    ++value;
}
```

重复目标名时必须仍然是当前泛型参数名。`T: equality_comparable<T> and U: incrementable` 不是“同时约束 `T` 和 `U`”的写法，而会在解析内联约束时报错；需要约束另一个参数时，应在 `U` 自己的泛型参数声明上写 `U: incrementable`，或使用函数头后的 `requires` 子句。

`T: equality_comparable<T> and incrementable` 的语义是：

```text
T implements equality_comparable<T>
T implements incrementable
```

这里采用 `T: concept` 作为能力约束写法。约束中的 concept 可以带泛型实参；如果省略实参，则按 concept 声明中的默认泛型参数补齐。

下面的 `ordered_less` 是用户自定义 concept 示例；标准库当前提供的比较相关 concept 名称见 [concept.md](concept.md) 中的
`equality_comparable`、`three_way_comparable`、`ordering` 和 `incrementable`。

内联约束适合短约束。它等价于把同样的 concept 要求写进 `requires`：

```cp
max<T>(left: T, right: T) -> T
requires
    T: ordered_less
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
concept sink {
    type sink_item;
}

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
concept sink {
    type sink_item;
}

merge<T, I, O>(input: I, output: O)
requires
    T: ordered_less
    and I: iterator
    and O: sink
    and I::iter_item == T
    and O::sink_item == T
{
}
```

这里的 `sink` 和上面的 `ordered_less` 一样，是用户自定义 concept 示例，不是当前标准库内建名字。需要真实标准库能力时，应使用
`std.core.iter`、`std.compare`、`std.meta` 或 `std.io.format` 中已经导出的 concept。

推荐规则：

- 简单的一两个 concept 约束写在 `<T: concept>` 中。
- 类型相等、多个类型参数之间的关系、较长约束写进 `requires`。
- `requires` 使用 `and` 连接约束项，允许用括号组合子表达式，不支持 `or` 和 C++ 风格的任意布尔表达式模板。

参数包约束的左侧只支持裸类型参数包名加 `...`：

```cp
ok<T...>() -> i32
requires
    T...: display
{
    return 0;
}
```

它表示当前实例里 `T...` 的每个元素都必须满足 `display`。这不是一般 pack pattern，不能把 `...` 接在任意类型模式后：

```cp
bad<T...>() -> i32
requires
    box<T>...: display // 错误：requires 包约束左侧必须是裸类型参数包
{
    return 0;
}
```

类型参数包也不能用于类型相等约束左侧或右侧来表达“逐元素相等”；`T... == U...`、`T... == i32` 和 `box<T>... == box<i32>` 都不是当前 grammar / 语义能力。需要逐个检查时，应把约束放在元素类型参数上，或在后续设计中引入真正的 pack pattern。

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

concept 的类型参数包只参与 concept 实参匹配。例如 `concept any<Args...> { }` 可以被写成 `impl any<i32, bool> for item { }`，也可以被约束引用为 `T: any<i32, bool>`。这种具体类型实参列表是当前支持的能力。

当前不要把函数自己的类型参数包转发进任意用户自定义 concept 作为公共契约。`requires item: any<Args...>` 这类写法会解析成 pack expansion，但实例化时不能把 `Args... = [i32, bool]` 稳定匹配到已有的 `impl any<i32, bool> for item`，使用点会表现为缺少 concept 证明。已经可依赖的转发场景是编译器认识的 `std.meta.callable<Args...>` / `call_result<F, Args...>` 这类内建查询和内建 concept。

concept 参数包不会像函数参数包那样为 concept 默认函数建立 `active_type_pack_substitutions`；在 concept 默认函数体里写 `template for(type U : Args...)` 不是当前可依赖能力，实际 materialize 默认函数时会报未知类型参数包。需要遍历类型包的逻辑应放在泛型函数、泛型成员函数或泛型 lambda 中。

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
- 具体实例中，值参数包会展开成一个个普通形参；每个 pack 元素产生一个形参，空包产生零个形参。包前面的固定形参仍然必须由调用点提供或由普通默认参数补齐。
- concept 的类型参数包只吸收 concept 引用处的剩余类型实参，例如 `F: callable<i32, bool>` 会把 `Args...` 绑定为 `[i32, bool]`。
- 参数包可以为空；为空时对应的 `template for` body 展开零次。
- 函数、泛型 lambda 和 concept 的类型参数包都应写在泛型参数列表最后一个参数位置；`bad<T..., U>` 不作为公开能力。
- 值参数包必须位于普通参数列表末尾，并且函数或泛型 lambda 必须同时声明类型参数包。`bad(values: i32...)` 不合法；值参数包不是“任意重复某个固定类型参数”的语法。
- 整数 const 泛型参数不能声明为参数包；当前只支持类型参数包。
- 参数包不能带默认实参，值参数包参数也不能带默认值。空包来自调用点没有剩余实参，或显式传入零个包元素，不来自默认值机制。
- 参数包不是数组、元组或 slice，不能整体存储、返回、赋值或作为普通表达式使用。
- 类型参数包不能在需要单个类型的位置直接使用；`bad<T...>(value: T...) -> T` 不合法，返回类型应通过 `template for`、`decltype`、关联类型或其它单个类型表达式得到。
- 值参数包只能在明确的值展开上下文中使用；当前值展开上下文只有 `template for`。
- 类型参数包可以在 `template for(type U : T...)` 中逐个展开，也可以在有限的类型实参列表中写成 `T...` 展开。
- concept 参数包不能 `template for` 展开；它只能出现在 concept 自身的具体类型实参匹配中。把函数类型参数包转发进用户自定义 concept 实参列表不是当前公开能力；具体 `T: any<i32, bool>` 可以匹配 `impl any<i32, bool> for T`，但 `T: any<Args...>` 这类函数包转发不能作为稳定证明。`std.meta.callable<Args...>` 是编译器认识的内建 concept 场景。

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
- `F: callable<Args...>` 这类编译器认识的 `std.meta.callable` concept 引用。
- 展开后继续跟固定类型实参，例如 `call_result<F, Args..., bool>` 和 `F: callable<Args..., bool>`。这表示先插入当前 `Args...` 的每个元素，再追加 `bool`。
- 显式泛型函数或泛型 lambda 调用实参，例如 `make<f(i32, bool) -> i32, i32, bool>()`，以及在泛型内部把当前 `Args...` 展开给另一个可接受这些类型实参的泛型调用。

具体函数实例中，`Args...` 展开为当前实例的实际类型序列；泛型定义体内尚未具体化时，展开保留为依赖的 pack expansion，只能继续流向上述允许的可变实参位置。普通类型构造器、非可变 meta query 和单个类型参数槽不会因为看见 `Args...` 就展开。

不支持的形式：

```cp
type A = read_type<Args...>;      // 错误：read_type 只接收一个类型实参
type B = tuple<Args...>;          // 不表示把 Args 展开为多个 tuple element，当前不可依赖
type C = box<Args...>;            // struct/variant/opaque alias 实参列表不提供公开 pack expansion 语义
type D = call_result<F, box<Args>...>; // 错误：不是裸 pack 名
requires T: same_as<Args...>;     // 错误：用户 concept 不能稳定接收函数类型包转发
bad<T... = i32>();                // 错误：类型参数包不能有默认泛型实参
bad<T...>(values: T... = 1);      // 错误：值参数包参数不能有默认值
```

`T... = i32` 这种泛型参数包默认实参在解析泛型参数列表时就会被拒绝；`values: T... = 1` 这种值参数包默认值能形成函数参数语法，但会在语义检查阶段被拒绝。

因此，当前 `Args...` 在公开支持的类型实参位置的意义是“把当前类型参数包的元素插入这个实参列表”，不是“对任意类型模式做映射”。普通命名类型构造器不会因为写了 `...` 就获得 pack field 或 pack element 展开能力；在具体实例中把类型参数包当作单个类型使用仍会失败。如果需要把每个 `Args` 包进 `box<Args>`，应使用 `template for(type U : Args...)` 分别处理，或等待后续 pack pattern 设计。

值参数包的元素类型可以是简单 `T...`，也可以把 `T` 嵌在当前实现支持推导的复合类型里：

```cp
sum_arrays<T...>(values: [T; 2]...) -> i32
sum_sized_arrays<N: usize, T...>(values: [T; N]...) -> i32
sum_storage<N: usize, T...>(values: storage [T; N]...) -> i32
sum_tuples<T...>(values: (T, i32)...) -> i32
sum_callbacks<T...>(callbacks: (f(T) -> T)...) -> i32
sum_boxes<T...>(values: box<T>...) -> i32
sum_options<T...>(values: optional<T>...) -> i32
count_ptrs<T...>(values: T*...) -> i32
count_const_ptrs<T...>(values: T const*...) -> i32
sum_ref<T...>(values: T const&...) -> i32
sum_forward<T...>(values: T forward&...) -> i32
```

调用这些函数时，编译器按每个实参位置推导当前 pack 元素类型。每个进入值参数包的实参都会产生一个 pack 元素；pack 长度等于调用点剩余实参数量。一个 pack 元素在同一个参数模式中出现多次时，所有位置必须推导出同一个类型；不匹配则该实例化失败。

当前实现会递归穿过这些值参数包元素模式：

- 直接类型参数：`T...`。
- 引用：`T&...`、`T const&...`、`T move&...` 和 `T forward&...` 先按被引用值类型推导 `T`，再按普通引用绑定规则检查可写性、只读性和移动/forward 绑定类别。`T forward&...` 会为每个展开后的形参记录独立 forward 绑定类别，因此同一个函数实例 key 同时包含类型实参序列和每个 forward pack 元素的左值/const 左值/右值类别。
- 指针：`T*...` 要求实参是非 const 指针；`T const*...` 可以接收可写或只读指针并推出 `T`。`T like*...` 的 const / like 规则在具体实例化后按普通指针规则物化。
- 数组和 storage：`[T; 2]...`、`[T; N]...`、`storage [T; 2]...`、`storage [T; N]...`。长度可以是固定整数，也可以是位于类型参数包之前的整数 const 泛型参数，例如 `N: usize, T...`；长度不属于 `T...`，所有实参必须推出同一个 `N`。
- 元组：`(T, i32)...` 或 `(T, T)...`。元组元素数量必须一致；`(T, T)...` 这类重复出现的 `T` 要求同一个实参内两个位置类型相同。
- 函数类型：`(f(T) -> T)...` 或 `(f(T, i32) -> T)...`。函数参数个数必须一致，并递归匹配每个参数类型和返回类型；`f(i32) -> i32` 和 `f(bool, bool) -> bool` 不能进入同一个 `(f(T) -> T)...` pack 模式。
- 名义泛型类型：`box<T>...`、`optional<T>...`。声明必须是同一个 `struct` 或同一个 `variant`，类型实参数量必须一致；结构相同但声明不同的 `box<T>` 和 `bag<T>` 不会互相推导。

因此，下列调用会失败：

```cp
same_pair<T...>(values: (T, T)...) -> i32
same_pair((1, true)); // 错误：同一个实参内 T 被推出 i32 和 bool

same_size<N: usize, T...>(values: [T; N]...) -> i32
same_size([1, 2], [true, false, true]); // 错误：N 同时推出 2 和 3

same_callback<T...>(callbacks: (f(T) -> T)...) -> i32
same_callback(f(x: i32) -> i32 { return x; }, f(x: bool, y: bool) -> bool { return x; }); // 错误：函数形参个数不同
```

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
- 对值参数包来说，每个 pack 元素在当前函数实例中已经是一个具体参数 binding。`template for(let value : values...)` 给该参数元素建立当前展开作用域里的局部名字，不额外 copy 构造一个新对象；如果给 `value` 赋值，修改的是该函数实例中的对应参数元素。对于按值参数包，这个参数元素本身已经是 callee 的局部副本，不会改写调用者对象。
- `template for(const value : values...)` 使用同一套绑定方式，只是把展开变量设为 binding const。`let ref` / `const ref` 展开则绑定为引用，不拥有 payload 对象。
- 如果值参数包来源于 `T forward&...`，`template for` 展开出的局部绑定会继承对应 pack 元素的 forward 资格。也就是说，`template for(let value : values...) { use(forward value); }` 会按当前元素的调用点类别继续转发：可写左值继续表现为可写借用，const 左值继续表现为只读借用，右值或 `move` 实参继续表现为移动借用。
- `forward` 不因为名字来自任意值参数包就可用。只有原始函数形参是 `forward&`，或 `template for` 局部绑定来自 `forward&` 值参数包元素时，`forward name` 才合法。普通 `T...`、`T&...`、`T const&...`、`T move&...` 展开出的 `value` 都不是 forward-capable binding；在这些绑定上写 `forward value` 会报错。
- 展开变量本身不单独注册析构清理；按值参数包元素的清理由函数入口参数 binding 负责，展开体中声明的其它局部对象按普通作用域清理。
- 每次展开体的局部清理边界彼此独立。一次展开正常落到体末尾时，会清理该次展开体中新建的局部对象，然后继续下一次展开；这些清理不会覆盖函数参数包元素本身。
- 类型参数包展开只建立类型别名；`template for(type U : T...)` 不产生运行时值、地址或清理。
- `return` 仍然返回外层函数、lambda 或当前函数边界。
- `template for` 展开后按普通顺序语句处理，不生成运行时循环头、循环计数器或统一的 `break` / `continue` 目标。某次展开体中的 `return`、`panic` 或内层运行时循环控制流只按普通语句生效，不会变成可被 `template for` 捕获的控制流。
- `return` 会参与外层函数返回类型推导；类型包和值包展开体中的 `return value;` 都按被展开后的普通语句处理。
- 对具体实例来说，非空参数包的 `return` 按展开次数逐次贡献返回类型；空参数包展开零次，因此展开体内的 `return` 不贡献任何返回类型。
- 如果展开体里对依赖于当前 pack 元素的泛型 `variant` 做 `match`，编译器先把该元素替换成具体 `variant` 实例，再检查 case 名、payload 绑定、穷尽性、重复 case 和 arm 结果类型。
- `match` 的错误恢复不会因为出现在 `template for` 内而提前短路。重复 case、未知 case 或 payload 绑定数量错误的 arm 右侧表达式仍会继续检查；嵌套 `match` 也继续检查。错误 pattern 只能建立当前实现明确能建立的前缀 payload 绑定，不能让多余绑定名变成可用局部变量。
- `template for(let value : values...)` 只能遍历当前函数实例的值参数包，不能遍历数组、元组、range 或普通局部变量。
- `template for(type U : T...)` 只能遍历当前函数实例的类型参数包；`U` 是类型别名，不是运行时值。
- `break` 和 `continue` 不允许直接作用于 `template for`，因为它不是运行时循环。如果 `template for` 展开体内又声明了普通运行时循环，`break` / `continue` 可以控制这个内层循环；但不能穿透 `template for` 去控制外层运行时循环。
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

条件支持类型相等、concept 条件、当前实现能折叠为 `bool` 的小型常量表达式，以及 `not`、`and`、`or` 和括号组合。`T == U` 两侧能作为类型解析时按类型相等处理；当前比较的是两侧规范化后的读出类型，因此普通别名会先展开，引用会被剥掉，`T& == T` 和 `T const& == T` 都按同一读出类型成立。`T: concept<Args...>` 按当前实例的 concept 证明检查；普通表达式相等仍按表达式常量折叠处理。

表达式条件必须先能按普通表达式规则检查为 `bool`，再能在实例化期折叠出 `bool` 常量。当前公开可依赖的表达式常量子集是：

- `true` / `false`、整数字面量、字符字面量，以及已经解析为常量值的 enum case。
- 括号表达式。
- `not` 作用于布尔常量，和一元 `-` 作用于整数常量。
- `==` / `!=` 作用于两侧都能折叠出的字面量值；`<` / `<=` / `>` / `>=` 只对整数常量折叠。
- `and` / `or` 按编译期短路规则处理，因此 `template if(false and missing_name)` 不会检查右侧名字，`template if(true or missing_name)` 也不会检查右侧名字。

这不是完整的 C++ `constexpr`。运行时局部变量、参数、函数调用、成员访问、字段访问、数组/元组/variant 构造、`match`、块表达式、分配表达式、用户 operator 调用，以及字符串、浮点和 `nullptr` 上的条件折叠都不是当前可依赖能力。表达式不能检查为 `bool` 时报条件类型错误；能检查为 `bool` 但不能折叠为常量时报非 constexpr 条件错误。类型相等或 concept 条件在当前非泛型上下文仍然依赖未替换类型时，也会报错，而不是延迟到运行时。

选中分支按普通语句做语义检查；未选中分支只要求语法正确，不解析名字、不检查类型，也不参与返回、循环或借用状态。没有分支命中且没有 `else` 时等价于空语句。被选中的分支会参与返回类型推导，未选中的分支不会贡献返回类型。

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
    T...: display and incrementable
```

不支持对参数包写随机访问、长度查询或条件拆分：

```cp
sizeof...(T)       // 错误：当前没有参数包长度查询语法
at...(values, 0)   // 错误：当前没有按下标读取参数包元素的语法
values...[0]       // 错误：当前没有“先展开再索引”的参数包表达式
```

参数包只能通过 `template for` 顺序展开。需要根据元素数量或位置选择不同逻辑时，第一版应把逻辑写成显式重载/不同函数，或在展开体中用普通运行时变量累计；不能在类型层直接取 pack 长度、取第 N 个元素或按 pattern 拆分。

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

泛型 `variant` 定义的是一个名义类型构造器。`optional<i32>` 和 `optional<f64>` 是不同的具体类型实例，但共享同一个泛型定义。

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

`_` 不是类型实参占位符，也不会从 case payload、调用实参或返回上下文反推成某个具体类型。需要泛型 `variant` 的具体实例时，要么在类型名上写完整实参，要么让声明中的默认泛型实参补齐缺省位置。

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

泛型固有 `impl` 中的关联类型别名按目标类型模式收集。查找 `box<i32>::item` 时，如果存在 `impl<T> box<T> { type item = T; }`，编译器会先用 `box<T>` 匹配具体 owner `box<i32>`，再把别名右侧的 `T` 替换为 `i32`。找不到可匹配 owner pattern 或别名名时，非依赖 owner 立即报未知关联类型；依赖 owner 仍可延迟到实例化阶段。

`impl X<T> for Y<T>` 这种写法中的两个 `T` 只有在 `impl<T>` 中声明后才表示同一个泛型参数；完整写法是 `impl<T> X<T> for Y<T>`。`impl` 头部是泛型作用域的入口，后续的目标类型模式、concept 实参和 `requires` 都引用这个作用域。

如果省略 `impl<...>`，当前实现会从目标类型模式收集未绑定名字作为 `impl` 级泛型参数。这个收集不是简单地把所有大写名字都当作类型参数，而是按类型结构和已知类型构造器的泛型参数种类判断：

```cp
struct bucket<T, N: usize> {
    values: [T; N];
}

impl bucket<T, N> {
    at(self const&, index: usize) -> T const&
    {
        return const ref values[index];
    }
}
```

这里 `impl bucket<T, N>` 等价于 `impl<T, N: usize> bucket<T, N>`。因为 `bucket` 已经是可见的 `struct` 类型构造器，编译器会查它第一个泛型参数是类型、第二个泛型参数是 `usize` 整数 const 参数，于是把 `T` 收集为类型参数，把 `N` 收集为整数 const 参数。同样规则适用于泛型 `variant`：

```cp
variant packet<T, N: usize> {
    item([T; N]);
}

impl packet<T, N> {
    fallback(self const&, value: [T; N]) -> [T; N]
    {
        return value;
    }
}
```

数组和 storage 类型模式中的长度名字也会收集为 `usize` 整数 const 参数：

```cp
impl [T; N] {
    view_self(self const&) -> [T; N] const&
    {
        return const ref self;
    }
}
```

省略收集规则的边界：

- 只在 `impl` 或 concept `impl` 头部省略显式泛型参数列表时生效；写了 `impl<T> bucket<T, N>` 后，显式列表就是完整泛型作用域，`N` 不会被额外自动声明。
- 只收集目标类型模式里未绑定、未解析成已有类型/内建/`this`/`array`/`tuple` 的裸名字。
- 对 `struct` / `variant` 类型实参位置，会按目标类型声明中的参数种类收集；对应位置是 `N: usize` 时，裸名字收集为整数 const 参数，不会误收集成类型参数。
- 对 `[T; N]` 和 `storage [T; N]` 的长度位置，裸名字收集为 `usize` 整数 const 参数。
- 省略收集只负责让目标类型模式里的裸名字成为可用泛型参数，不会把不支持的 impl 目标变成合法目标。`impl (T, U) { ... }` 会先收集 `T` / `U`，再因为 tuple 不是固有 `impl` 目标而报错；`impl storage [T; N] { ... }` 同样会先收集 `T` / `N`，再因为 storage 类型不能挂普通 `impl` 而报错。当前允许的固有 `impl` 目标仍以 [module.md](module.md#impl-目标和模块边界) 为准。
- 类型参数包不是当前 `impl` 泛型参数能力；省略收集也不会从目标模式里创建 `T...`。
- 需要避免歧义或希望写 `I: isize` 这类非默认整数种类时，应显式写 `impl<T, I: isize> ...`。

`impl` 级 `requires` 是条件实现。只有当具体类型实参满足约束时，这个 `impl` 块里的项才参与成员查找和关联项查找：

```cp
impl<T> vector<T>
requires
    T: incrementable
{
    push_back(self&, value: T)
    {
    }
}
```

含义是：

```text
for every T:
    if T implements incrementable:
        vector<T> has this impl block
```

因此 `vector<i32>` 是否拥有 `push_back`，取决于 `i32` 是否满足 `incrementable`。约束不满足时，不是在 `impl` 定义处报错，而是在使用点把该 `impl` 排除出候选；如果没有其它可用候选，则报告“没有满足约束的成员/关联项”，并指出失败的约束。

函数级 `requires` 只控制单个成员函数是否参与候选：

```cp
impl<T> vector<T> {
    contains(self const&, value: T const&) -> bool
    requires
        T: equality_comparable<T>
    {
        return false;
    }
}
```

`vector<file>` 如果不满足 `file: equality_comparable<file>`，则 `contains` 不参与 `vector<file>` 的成员查找。诊断应指向调用点，并说明候选存在但约束不满足。

定义阶段仍然检查非依赖错误。完全不依赖 `T` 或成员函数自身泛型参数的未知名字、语法错误、重复定义等，不允许延迟到实例化阶段。

`impl` 内部可以定义自己的泛型函数。成员函数泛型参数的作用域只覆盖该函数，不覆盖整个 `impl`：

```cp
impl<T> vector<T> {
    contains_as<U>(self const&, value: U const&) -> bool
    requires
        T: equality_comparable<U>
    {
        return false;
    }
}
```

这里 `T` 来自 `impl<T>`，`U` 来自 `contains_as<U>`。当前标准库没有公开的 `convertible_to` concept；需要表达转换能力时，应先定义自己的
concept 和对应 `impl`，或依赖具体调用点的表达式检查。

调用泛型 `impl` 中的成员函数或关联函数时，`impl` 级泛型参数属于隐式前缀，由 receiver 类型或关联调用的 owner 类型推导；调用点显式 `<...>` 只绑定成员函数自己声明的源码泛型参数：

```cp
struct box<T> {
    value: T;
}

impl<T> box<T> {
    convert<U>(self const&, fallback: U) -> U
    {
        return fallback;
    }
}

let item = box<i32>{ 1 };
let a = item.convert<bool>(true); // T 从 item 推导为 i32，<bool> 只给 U

struct maker<T> {
}

impl<T> maker<T> {
    make<U>(value: U) -> U
    {
        return value;
    }
}

let b = maker<i32>::make<bool>(false); // T 从 owner maker<i32> 推导，<bool> 只给 U
```

因此不能写 `item.convert<i32, bool>(true)` 试图同时传入 `impl` 级 `T` 和函数级 `U`；`T` 已经由 `item` 决定。若 receiver / owner 类型无法匹配 `impl` 目标模式，或者显式函数级实参与普通实参推导冲突，调用失败。

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
impl<T> equality_comparable<vector<T>> for vector<T>
requires
    T: equality_comparable<T>
{
    operator ==(self const&, rhs: this const&) -> bool
    {
        return self.len() == rhs.len();
    }
}
```

含义是：

```text
if T implements equality_comparable<T>:
    vector<T> implements equality_comparable<vector<T>>
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
concept ordered_less {
    less(self const&, rhs: this const&) -> bool;
}
```

类型通过 `impl Concept for Type` 证明自己满足能力：

```cp
impl ordered_less for i32 {
    less(self const&, rhs: this const&) -> bool
    {
        return self < rhs;
    }
}
```

泛型函数通过约束要求类型参数具备能力：

```cp
min<T: ordered_less>(left: T, right: T) -> T
{
    if(right.less(left)) {
        return right;
    }

    return left;
}
```

三者关系为：

```text
concept ordered_less          描述用户自定义能力
impl ordered_less for i32     证明 i32 有这个能力
T: ordered_less               要求泛型参数 T 有这个能力
impl<T> ordered_less for vector<T> requires T: ordered_less
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

表达式层面的显式类型实参必须紧跟一次调用参数列表。当前 parser 只有在 `<...>` 后面继续看到 `(` 时，才把它解析为泛型调用后缀；否则 `<` / `>` 会按比较或位移相关 token 参与普通表达式解析，或者在已解析出类型实参但没有调用括号时报告语法错误。因此：

```cp
let value = id<i32>(1);          // 合法：显式实例化并立即调用
let size = xs.len<usize>();      // 合法：成员函数显式实例化并立即调用
let made = box::make<i32>(1);    // 合法：关联函数显式实例化并立即调用
let less = value < id<i32>(4);   // 合法：左侧 `<` 是比较，右侧是泛型调用
let f = id<i32>;                 // 错误：不能把特化后的函数名作为一等函数值保存
let m = xs.len<usize>;           // 错误：成员函数同样不能保存为“已特化函数值”
run(id<i32>);                    // 错误：显式类型实参后必须接调用参数列表
```

需要函数值时，应先声明具体函数类型的变量并用函数名初始化，或传递一个 lambda / 闭包；显式泛型实参当前只用于调用表达式本身，不产生可单独命名的“函数模板特化值”。这条规则对普通函数、成员函数、关联函数和泛型 lambda 调用一致；`operator ()` 不接收调用点显式类型实参，相关边界见 [operator.md](operator.md#函数调用)。

调用点显式类型实参只绑定源码泛型参数列表中能由调用者提供的那一段。由省略参数类型自动引入的隐藏类型参数排在源码泛型参数之前，只能从普通实参或 receiver 类型推导，不能在调用点显式写出：

```cp
head<T>(value, fallback: T) -> T
{
    return fallback;
}

let a = head<i32>("name", 1); // 合法：显式 T = i32，隐藏的 value 类型从 "name" 推导
let b = head<i32>(1);         // 错误：缺少普通实参，隐藏参数不能靠 <i32> 补齐
```

显式类型实参按源码泛型参数的前缀绑定。遇到带默认值的泛型参数后，调用点可以省略后续实参，由默认泛型实参按声明顺序补齐；不能跳过中间参数，也不能让返回上下文补齐前面的参数。显式实参与普通实参推导结果冲突时，调用失败：

```cp
select<T, U = T>(left: T, right: U) -> U
{
    return right;
}

let a = select<i32>(1, 2);       // U 默认成 i32
let b = select<i32, bool>(1, true);
let c = select<bool>(1, false);  // 错误：T 显式为 bool，但 left 推导为 i32
```

如果函数有类型参数包，包参数吸收显式实参列表中从包位置开始的剩余类型实参；普通实参推导出的包元素也必须与显式包元素一致。当前语法不支持空的显式类型实参列表，因此空包只能通过普通调用 `func()` 推导得到，或在有前置非包参数时写成 `func<Prefix>()`。

当包前存在带默认值的泛型参数时，不写显式泛型实参可以让默认值补齐前缀并得到空包；一旦写显式泛型实参，仍按前缀绑定，不能写出“只给包、不写前缀”的形式：

```cp
count<T = i32, Args...>() -> i32
{
    return 0;
}

count();            // T 使用默认 i32，Args... 为空
count<bool>();      // T = bool，Args... 为空
count<bool, i32>(); // T = bool，Args... = [i32]
```

`concept` 和 `requires` 约束不参与反向推导，只在类型实参确定后检查：

```cp
concept sink {
    type sink_item;
}

copy_all<I: iterator, O: sink>(input: I, output: O)
requires
    I::iter_item == O::sink_item
{
}
```

这里 `I` 和 `O` 只能从 `input`、`output` 的实参类型推导；`I::iter_item == O::sink_item` 用于验证推导结果，而不是产生新的类型参数绑定。

## 实例化语义

泛型不是运行时多态。当前按具体类型实参生成对应实例：

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
3. 用前面已经确定的类型实参求默认泛型实参，补齐缺省的尾部参数。
4. 检查类型参数种类：普通类型参数必须收到类型实参，`N: usize` / `I: isize` 这类整数 const 参数必须收到整数常量实参；当前不进一步检查整数常量是否落在目标 kind 的范围内。
5. 检查内联 concept bound、参数包 concept bound、函数自身 `requires` 和所在 `impl` 的 `requires`。
6. 用具体类型替换类型参数、替换函数签名和默认值表达式中的类型。
7. 对替换后的函数体执行完整语义检查，包括依赖 UFCS 调用、默认参数表达式和 `template if` / `template for` 展开。
8. 缓存已成功实例化的版本，避免重复生成；含 `forward&` 参数的函数还要把每个 forward 参数的可写左值、const 左值、右值绑定类别放入实例化 key。

如果实例化失败，诊断应同时指出调用点和泛型函数体中失败的依赖操作。

泛型 `struct`、泛型 `variant` 和 `impl` 的实例化规则与函数一致：先由具体使用点确定类型实参，再检查 `requires` 约束，最后对依赖函数体执行完整语义检查。条件 `impl` 的约束不满足时，该 `impl` 不生成对应具体实例，也不向该具体类型贡献成员或 concept 证明。

## 支持内容

泛型支持：

- 类型参数：`func<T, U>(...)`
- 省略参数类型引入的隐藏类型参数：`func(x, y)`、`func(x&)`、`func(x const&)`、`func(x move&)`、`func(x forward&)`
- 强无约束泛型：允许依赖操作，实例化时检查
- 内联 concept 约束：`T: equality_comparable<T>`
- 多 concept 约束：`T: equality_comparable<T> and incrementable`
- `requires` 子句
- 类型相等约束：`I::iter_item == T`
- 依赖 `decltype(expr)`：实例化后确定表达式类型
- 参数包：`func<T...>(values: T...)`
- 参数包约束：`T...: display`
- 复合值参数包推导：`[T; N]...`、`storage [T; N]...`、`(f(T) -> T)...`、`box<T>...`、`optional<T>...`、`T*...`、`T const&...`
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
- `impl` 级条件约束：`impl<T> vector<T> requires T: incrementable`
- 成员泛型函数：`contains_as<U>(...)`
- 泛型 concept `impl`：`impl<T> equality_comparable<vector<T>> for vector<T> requires T: equality_comparable<T>`
- 显式 concept 实参：`impl partial_eq<str> for string_like`
- 单态化代码生成

## 当前不支持和实现边界

本节把第一版实现边界集中列出，避免把设计目标误读成已经可用能力。

类型参数包当前只作为函数、泛型 lambda 和泛型 concept 的最后一个类型参数使用：

```cp
sum<T...>(values: T...) -> i32      // 支持：函数类型参数包和值参数包
concept callable<Args...> { }       // 支持：concept 类型参数包
```

下面这些不是当前公开能力：

```cp
struct tuple<T...> { }              // 错误：struct 不能声明类型参数包
variant choice<T...> { none; }      // 错误：variant 不能声明类型参数包
impl<T...> box<i32> { }             // 错误：impl 不能声明类型参数包
impl<T...> mark for item { }        // 错误：concept impl 不能声明类型参数包
```

值参数包必须是函数或泛型 lambda 的最后一个普通参数，并且必须由同一个函数级类型参数包驱动。不能写非尾部值包、多个值包，也不能把固定类型写成重复参数：

```cp
bad<T...>(values: T..., last: i32)  // 错误：值参数包必须在最后
bad(values: i32...)                 // 错误：值参数包需要类型参数包
bad<T..., U>()                      // 错误：类型参数包必须在最后
bad<T...>(values: T... = 1)         // 错误：参数包不能有默认值
```

类型参数包不能当作单个类型使用，也不能用空类型实参列表显式调用：

```cp
bad<T...>(values: T...) -> T        // 错误：T... 不是一个单独类型 T
touch_types<>()                     // 错误：当前语法没有空类型实参列表
touch_types()                       // 支持：没有普通实参时推导为空包
touch_types<i32>()                  // 支持：显式给出非空类型包
```

类型实参列表中的 pack expansion 只支持裸类型包名：

```cp
make<F, Args...>()                  // 支持：Args... 插入实参列表
make<F, Args..., bool>()            // 支持：展开后追加固定类型
read_type<Args...>                  // 错误：read_type 不是可变类型实参查询
box<Args...>                        // 错误：普通类型构造器不提供 pack expansion 语义
call_result<F, box<Args>...>        // 错误：不是裸包名
```

`template for` 只遍历当前实例中的参数包，不遍历运行时集合：

```cp
template for(let value : values...) // 支持：值参数包
template for(type U : T...)         // 支持：类型参数包
template for(let value : array...)  // 错误：数组不是值参数包
template for(type U : concept_pack...) // 错误：concept 参数包不能展开
```

空包展开零次，因此展开体内的 `return` 不会贡献返回类型。非空包按展开后每个普通语句检查，`return;` 作为 `unit` 参与推导，`return value;` 按当前元素的具体类型参与推导；同一函数实例中混合不兼容返回类型会报错。
