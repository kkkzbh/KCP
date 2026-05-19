# 泛型

本文档记录泛型设计，覆盖泛型函数、泛型 `struct`、泛型 `variant`、泛型固有 `impl`、泛型 concept `impl`、参数包、`template for` 展开以及 `requires` 约束。泛型 `concept`、const generic 和模板特化不属于本文档。

函数泛型采用偏现代 C++ 的强模板模型：无约束泛型也可以在函数体内使用依赖于类型参数的操作；`concept` 和 `requires` 是可选约束机制，用于提前表达能力要求、改善诊断、约束公共 API 和辅助重载选择。

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

InherentImpl
    -> impl TypePattern RequiresClause? { ImplItem* }

ConceptImpl
    -> impl ConceptName for TypePattern RequiresClause? { ConceptImplItem* }

GenericParameterList
    -> < GenericParameter ( , GenericParameter )* >

GenericParameter
    -> identifier
     | identifier : ConceptBoundList
     | identifier ...
     | identifier ... : ConceptBoundList

ConceptBoundList
    -> identifier ( and identifier )*

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

TemplateForBinding
    -> let identifier
     | const identifier
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

泛型参数是编译期类型参数，不是运行时值。

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

## 无约束泛型

无约束类型参数不声明任何 concept：

```cp
id<T>(value: T) -> T
{
    return value;
}

make_pair<T, U>(first: T, second: U) -> tuple<T, U>
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
- `size(value)` 先查当前可见自由函数 `size`，名字不存在时再查具体类型的成员 `value.size()`。

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

这里采用 `T: concept`，不采用 `concept<T>`。当前语言中的 `concept` 是无模板设计，它描述某个类型具备的静态能力，而不是一个以类型为参数的模板谓词。

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
- 值参数包长度与对应类型参数包长度相同。
- 参数包可以为空；为空时对应的 `template for` body 展开零次。
- 值参数包必须位于普通参数列表末尾。
- 参数包不是数组、元组或 slice，不能整体存储、返回、赋值或作为普通表达式使用。
- 参数包只能在明确的展开上下文中使用，展开上下文只有 `template for`。

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
- `type U` 绑定当前类型。
- `return` 仍然返回外层函数、lambda 或当前函数边界。
- `break` 和 `continue` 不允许直接作用于 `template for`，因为它不是运行时循环。
- 展开后的语句按普通语义检查；依赖类型或依赖表达式可以延迟到实例化后检查。

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

只支持类型参数，不支持 const generic。因此用户自定义泛型类型不能声明 `struct array<T, N>` 这种由值参数参与类型的形式。内建 `array<T,N>` 是类型系统原语，规则见 [type_system.md](type_system.md)。

## 泛型 variant

`variant` 名之后可以出现泛型参数列表，规则和泛型 `struct` 一致：

```cp
variant optional<T> {
    none;
    some(T);
}

variant result<T, E> {
    ok(T);
    error(E);
}
```

泛型 `variant` 定义的是一个名义和类型构造器。`optional<i32>` 和 `optional<f64>` 是不同的具体类型实例，但共享同一个泛型定义。

case payload 类型可以直接使用 `variant` 的泛型参数。`variant` 泛型参数的作用域覆盖整个 `variant` 体，不覆盖对应的 `impl`；`impl` 需要从自己的目标类型模式重新绑定参数。

case 构造器挂在具体实例的类型命名空间下。不做 case 构造器类型实参推导，因此构造泛型 `variant` 时必须写满具体类型实参：

```cp
let a = optional<i32>::none;
let b = optional<i32>::some(1);
let c = result<i32, str>::error("bad input");
```

下面这些形式不支持：

```cp
optional::some(1)
optional<_>::some(1)
result<i32>::error("bad input")
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

泛型 `variant` 的运行时布局按具体实例计算。`optional<i32>`、`optional<str>`、`result<i32,str>` 分别拥有自己的 `tag + payload storage` 布局；tag 编号仍是编译器内部细节，不进入源语言语义。

## 泛型 impl

`impl` 不单独声明 `impl<T>` 形式的泛型参数。目标类型模式中出现的自由类型变量自动绑定为该 `impl` 的泛型参数：

```cp
impl vector<T> {
    len(self const&) -> usize
    {
        return size;
    }
}
```

上例中，`T` 从 `vector<T>` 中引入，作用域覆盖：

- `impl` 级 `requires` 子句。
- `impl` 块内的成员函数、关联函数、构造函数和析构函数签名。
- `impl` 块内的函数体。
- `impl` 块内的 `type` 类型别名。

也就是说，推荐语法是：

```cp
impl vector<T> {
}
```

而不是：

```cp
impl<T> vector<T> {
}
```

`impl<T> vector<T>` 形式不作为核心语法。这样避免在同一个位置重复声明类型参数，并把 `impl` 的泛型来源固定为“被实现的目标类型模式”。

`impl` 级 `requires` 是条件实现。只有当具体类型实参满足约束时，这个 `impl` 块里的项才参与成员查找和关联项查找：

```cp
impl vector<T>
requires
    T: movable
{
    push(self&, value: T)
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

因此 `vector<i32>` 是否拥有 `push`，取决于 `i32` 是否满足 `movable`。约束不满足时，不是在 `impl` 定义处报错，而是在使用点把该 `impl` 排除出候选；如果没有其它可用候选，则报告“没有满足约束的成员/关联项”，并指出失败的约束。

函数级 `requires` 只控制单个成员函数是否参与候选：

```cp
impl vector<T> {
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
impl vector<T> {
    convert_all<U>(self const&) -> vector<U>
    requires
        T: convertible_to<U>
    {
        return vector<U>{};
    }
}
```

这里 `T` 来自 `impl vector<T>`，`U` 来自 `convert_all<U>`。

泛型 `impl` 中的构造函数和析构函数仍然使用类型构造器名，不写类型实参：

```cp
impl vector<T> {
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

`impl concept for TypePattern` 同样从目标类型模式绑定自由类型变量，并可使用 `requires` 表达条件实现：

```cp
impl comparable for vector<T>
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

这类实现只为匹配目标类型模式的类型族提供 concept 证明。不支持以裸类型参数作为目标的 blanket impl，例如：

```cp
impl debug for T
requires
    T: printable
{
}
```

因为这种写法会对所有类型实例产生全局实现事实，冲突检查和候选选择规则需要更完整的重叠 impl 设计。

## 与 concept 的关系

`concept` 描述静态能力：

```cp
concept comparable {
    less(self const&, rhs: this const&) -> bool;
}
```

类型通过 `impl concept for Type` 证明自己满足能力：

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
impl comparable for vector<T> requires T: comparable
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

- 如果调用点写了类型实参，必须写满全部类型参数，例如 `add<i32>(1, 2)`。
- 如果调用点没有写类型实参，则全部类型参数都必须从普通实参推导出来。
- 不支持部分显式类型实参，例如 `pair<i32>(1, 2.0)`。
- 不从返回类型、变量声明类型或其他上下文反推类型参数。

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
first<T, U>(value: tuple<T, U>) -> T
{
    return value[0];
}

let x = first((1, 2.0)); // T = i32, U = f64
```

递归匹配规则：

- 形参模式是类型参数 `T` 时，把 `T` 绑定到当前目标类型。
- 形参模式和目标类型是同一种复合类型时，递归匹配其类型实参，例如 `tuple<T, U>` 对 `tuple<i32, f64>`。
- `array<T, 3>` 可以从 `array<i32, 3>` 推导出 `T = i32`，但长度必须已经相同；没有 const generic，因此不从数组长度反推类型参数。
- 引用、指针和 `const` 修饰必须按同一类型结构递归匹配，例如 `T const&` 可以从 `i32 const&` 推导出 `T = i32`。

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
6. 缓存已成功实例化的版本，避免重复生成。

如果实例化失败，诊断应同时指出调用点和泛型函数体中失败的依赖操作。

泛型 `struct`、泛型 `variant` 和 `impl` 的实例化规则与函数一致：先由具体使用点确定类型实参，再检查 `requires` 约束，最后对依赖函数体执行完整语义检查。条件 `impl` 的约束不满足时，该 `impl` 不生成对应具体实例，也不向该具体类型贡献成员或 concept 证明。

## 支持内容

泛型支持：

- 类型参数：`func<T, U>(...)`
- 强无约束泛型：允许依赖操作，实例化时检查
- 内联 concept 约束：`T: comparable`
- 多 concept 约束：`T: readable and writable`
- `requires` 子句
- 类型相等约束：`I::iter_item == T`
- 依赖 `decltype(expr)`：实例化后确定表达式类型
- 参数包：`func<T...>(values: T...)`
- 参数包约束：`T...: display`
- `template for(let value : values...)` 值包展开
- `template for(type U : T...)` 类型包展开
- 依赖 UFCS 调用实例化时检查
- 函数类型实参推导：从普通实参推导，支持嵌套类型模式
- 泛型 `struct`：`struct vector<T>`
- 泛型 `variant`：`variant optional<T>`
- 泛型固有 `impl`：`impl vector<T>`
- `impl` 级条件约束：`impl vector<T> requires T: movable`
- 成员泛型函数：`convert_all<U>(...)`
- 泛型 concept `impl`：`impl comparable for vector<T> requires T: comparable`
- 单态化代码生成

泛型不支持：

- 函数重载
- 部分显式类型实参
- 从返回类型或上下文反推类型参数
- `concept<T>` 形式的参数化 concept
- 泛型 `concept`
- 以裸类型参数为目标的 blanket impl，例如 `impl debug for T`
- 模板特化
- SFINAE
- 泛型常量参数
- `sizeof...`、参数包随机索引和 fold expression
- 非末尾值参数包
- 多值参数包同步推导
- 生命周期参数
- 运行时 `dyn concept`
- `or` 约束和依赖 `or` 的复杂布尔约束表达式，例如 `(A or B) and C`
