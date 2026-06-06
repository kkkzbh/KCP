# Concept 协议

本文档记录 `concept` 的设计。`concept` 表示编译期能力约束，不引入运行时多态、虚表或 `dyn` 机制。

目标是给结构体和普通类型提供静态协议能力：关联类型、函数要求、默认关联类型、默认函数实现、父 concept 约束，以及带默认参数的泛型 concept。

## 语法总览

```text
ConceptDecl        -> export? concept identifier GenericParameterList? { ConceptItem* }
ConceptItem        -> RequiresDecl
                    | AssociatedTypeRequirement
                    | AssociatedTypeDefault
                    | FunctionRequirement
                    | FunctionDefault

RequiresDecl       -> requires RequiresConstraintExpr ;

RequiresConstraintExpr
                   -> RequiresAnd
RequiresAnd        -> RequiresPrimary ( and RequiresPrimary )*
RequiresPrimary    -> RequiresConstraint
                    | ( RequiresConstraintExpr )
RequiresConstraint -> ConceptId
                    | Type : ConceptBoundList
                    | Type == Type
ConceptId          -> identifier TypeArgumentList?
ConceptBoundList   -> ConceptId ( and ConceptId )*

AssociatedTypeRequirement
                   -> type identifier ;
AssociatedTypeDefault
                   -> type identifier = Type ;

FunctionRequirement
                   -> ConceptFunctionName ( ParameterList? ) ReturnType? ;
FunctionDefault    -> ConceptFunctionName ( ParameterList? ) ReturnType? Block
ConceptFunctionName
                   -> identifier
                    | operator OverloadOperator
SelfReceiver       -> self
                    | self &
                    | self like &
                    | self const &
                    | self move &

ConceptImpl        -> impl GenericParameterList? ConceptId for TypePattern RequiresClause? { ConceptImplItem* }
ConceptImplItem    -> AssociatedTypeBinding
                    | Function
AssociatedTypeBinding
                   -> type identifier = Type ;

TypeAlias          -> type identifier = Type ;
```

`TypePattern` 和 `RequiresClause` 的完整规则见 [generic.md](generic.md)。

`type name = Type;` 是通用类型别名语句，可出现在全局、局部、固有 `impl`、`concept` 和 `impl Concept for T` 中。

裸 `type name;` 只允许出现在 `concept` 体内，用来声明关联类型要求。

## Concept 声明

```cp
export concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}
```

规则：

- `concept` 是顶层声明，可以在具名模块中写作 `export concept name { ... }`。
- 导出的 concept 可被其他模块 `import` 后作为公共能力约束使用。
- 未导出的 concept 只在当前模块内部可见，适合内部约束和实现细节拆分。
- 匿名模块中的 `concept` 不能使用 `export`。
- `type iter_item;` 表示实现者必须提供关联类型。
- 同一个 concept 体内关联类型名不能重复；`type item; type item;` 报 `duplicate_symbol`。
- 函数声明表示实现者必须提供函数；函数定义表示默认函数实现，实现者可以省略或覆盖。
- 同一个 concept 体内不能重复声明同名、同形态的函数要求或默认函数；重复项报 `duplicate_symbol`。当前 concept 要求不是普通重载集合，不要依赖用同名不同参数列表表达多个 requirement。
- 函数要求可以是普通名字，也可以是 operator 要求，例如 `operator ==(...) -> bool;`、`operator <=>(...) -> weak_ordering;`、`operator ()(...) -> R;` 或 `operator prefix ++(self&) -> this&;`。
- concept 函数要求和默认函数不做普通函数那种返回类型推导；省略 `-> R` 表示返回内部 `unit`。默认函数体即使 `return value;`，也不会把 requirement 推导成 `value` 的类型。
- 纯函数要求只描述完整签名。不要把 `name(value: T = expr) -> R;` 这类 requirement-only 默认值当作公开能力；当前签名匹配按完整参数数量和类型比较，不会把默认值传播给实现函数，也不会让调用者因为 requirement 声明而省略实参。
- 带函数体的 concept 默认函数可以使用普通函数参数默认值。默认值遵循 [generic.md](generic.md) 中的函数参数默认值规则，并在默认函数被某个 `impl Concept for T` 物化成具体函数后属于该具体函数符号。
- 第一个参数可以写作特殊接收者参数 `self`、`self&`、`self like&`、`self const&` 或 `self move&`，分别表示当前类型、当前类型可写引用、receiver-const 传播引用、当前类型 const 引用和当前类型移动引用。`self like&` 会驱动签名中的 `T like*`、`T like&` 等类型转发 constness；`self move&` 的规则见 [ownership.md](ownership.md)。本轮不支持 `self forward&`。
- `self` 只能出现在第一个参数位置，并且必须使用 receiver 语法；不能把名为 `self` 的普通参数写成 `self: T`。
- `next(self: i32) -> R` 这类把 `self` 写成普通命名参数的形状报 `invalid_self_parameter`。
- 非 `self` 参数必须显式声明类型。concept 函数要求不支持普通函数那种“省略参数类型自动引入隐藏泛型参数”的写法；`next(self&, value) -> R` 这类无类型参数报 `invalid_type_argument`。
- `this` 表示当前实现类型，只能出现在类型位置，并且只在 `concept`、固有 `impl` 和对应 `impl Concept for T` 上下文中有效。
- `concept` 体内可以直接使用当前 concept 的关联类型名，例如 `iter_item`，不要求写 `this::iter_item`。

## 泛型 Concept

`concept` 可以声明泛型参数，用来描述当前实现类型与其它类型之间的静态关系：

```cp
concept partial_eq<Rhs = this> {
    equals(self const&, rhs: Rhs const&) -> bool;
}
```

泛型参数的作用域覆盖整个 concept 体，包括 `requires`、关联类型、函数要求和默认函数实现。默认泛型参数按声明顺序求值，可以引用前面已经声明的参数。

`this` 可以作为 concept 泛型参数默认值，表示当前被检查或被实现的类型：

```cp
T: partial_eq
impl partial_eq for i32
impl partial_eq for box<T>
```

上面三处分别等价于：

```text
T: partial_eq<T>
impl partial_eq<i32> for i32
impl partial_eq<box<T>> for box<T>
```

需要表达异类型关系时，显式写出 concept 实参：

```cp
impl partial_eq<str> for string_like {
    equals(self const&, rhs: str const&) -> bool
    {
        return true;
    }
}
```

### Concept 参数包

concept 可以把最后一个类型参数声明为参数包，用来表达“这个能力还依赖一组调用参数类型”这类关系。标准库 `std.meta` 中的 `callable<Args...>` 就是这种用法：

```cp
concept callable<Args...> {
}

apply<F>(callback: F, value: i32) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}
```

泛型约束可以把当前类型参数包展开给 concept 参数包：

```cp
make<F, Args...>() -> call_result<F, Args...>
requires
    F: callable<Args...>
{
    return 42;
}
```

这里 `callable` 的最后一个参数是 concept 类型参数包，因此 `Args...` 会按当前函数实例展开为零个或多个类型实参。

规则：

- concept 参数包只支持类型参数包，不支持整数 const 参数包。
- concept 参数包应位于泛型参数列表末尾；公开语义只保证最后一个参数包按剩余 concept 实参吸收。
- concept 参数包可以为空；例如 `T: callable` 表示零个 concept 实参。当前 parser 不接受空显式类型实参列表，所以不能写 `T: callable<>`。
- concept 参数包没有对应的值参数包，不能写 `values: Args...`，也不能在 concept 体内用 `template for` 展开。
- concept 参数包不能带默认实参。
- concept 引用处按普通类型实参规则检查每个 pack 元素；`F: callable<i32, bool>` 会把 `Args...` 绑定为 `[i32, bool]`。
- `F: callable<Args...>` 只在 `Args` 是当前可见类型参数包时合法；展开语法必须是裸 pack 名加 `...`。
- 对当前公开可依赖能力，函数类型参数包转发只稳定用于编译器识别的 `std.meta.callable`。这一路可以在展开后继续追加固定类型实参，例如 `F: callable<Args..., bool>` 表示先插入当前 `Args...` 的每个元素，再追加一个 `bool` 调用参数。
- 如果目标 concept 的当前位置不是参数包，类型实参 pack expansion 不合法。例如 `T: same_as<Args...>` 只有在 `same_as` 对应位置声明为 concept 参数包时才可能成立。
- concept 参数包不能把运行时值、值参数包或复合 pack pattern 当作类型实参；`callable<box<Args>...>` 不是当前能力。
- `impl<T...> callable<T...> for item` 这类实现级参数包不是当前公开能力；可以写具体实参的 impl，例如 `impl callable<i32> for item`。

## 父 Concept

父 concept 使用 `requires` 声明，不带括号。多个约束使用 `and` 连接：

```cp
concept sized_iterator {
    requires iterator;

    remaining(self const&) -> i32;
}

concept random_access_iterator {
    requires sized_iterator and bidirectional_iterator;

    offset(self&, n: i32);
}
```

规则：

- `requires a;` 表示当前 concept 依赖 `a`。
- `requires a and b and c;` 表示同时依赖多个父 concept。
- 父 concept 必须在当前模块可见，并且可解析为 concept。
- concept 依赖不能形成循环。
- 子 concept 在能力关系上包含父 concept：使用 `T: random_access_iterator` 的位置也会要求 `T` 满足 `sized_iterator` 等父约束。
- 一个类型实现子 concept 时，必须已经满足所有父 concept。
- 不允许在子 concept 的 impl 中顺便补父 concept 的要求；父 concept 需要单独实现。
- 父 concept 的关联类型、默认关联类型和默认函数由父 concept 的 `impl` 负责物化，不会因为写了子 concept 的 `impl` 而复制进子 impl。也就是说，`impl child for T { ... }` 只检查 `T` 已经实现了 `parent`，不会替你生成 `parent` 的默认项。

`concept` 体内的 `requires` 也可以约束关联类型。裸 concept 名是父 concept 简写，等价于 `this: concept_name`；显式类型约束可以指向 `this`、当前 concept 的关联类型，或关联类型路径：

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

规则：

- `requires iterator;` 等价于 `requires this: iterator;`，表示当前实现类型必须同时实现 `iterator`。
- `requires iter_type: iterator;` 表示关联类型 `iter_type` 必须实现 `iterator`。
- `requires iter_type::iter_item == iter_item;` 表示两个类型表达式必须相同。
- `requires` 只接受三类 primary：裸父 concept、`Type: Concept` / `Type: A and B` 类型约束、`Type == Type` 类型相等约束。它不是运行时布尔表达式位置，不能写 `flag`、`value == 1`、函数调用、成员访问或用户 operator 条件。
- `ConceptId` 本身是当前作用域可见的未限定 concept 名加可选类型实参。不能写 `std.compare::equality_comparable`、`module.concept`、`Type::concept` 或其它限定路径；需要先通过 `import` / `export import` 让 concept 以裸名进入当前可见表。
- `and` 是唯一连接符；括号只用于把一组 `and` 约束分组，语义上会被扁平化成同一串约束。没有 `or`、`not`、逗号列表、短路求值或优先级表达式。
- 类型参数包约束必须写 `T...: concept`，不能写 `T... == U` 或裸 `T...`。`T...: A and B` 表示包里的每个类型都必须同时满足 `A` 和 `B`；被引用的名字必须是当前实例中可见的类型参数包。

```cp
impl iterator for cursor {
    type iter_item = i32;

    next(self&) -> optional<i32>
    {
        if(self.index >= self.end) {
            return optional<i32>::none;
        }

        let value = self.index;
        self.index += 1;
        return optional<i32>::some(value);
    }
}

impl sized_iterator for cursor {
    remaining(self const&) -> i32
    {
        return self.end - self.index;
    }
}
```

如果父 concept 带默认函数，正确写法仍然是先实现父 concept，再实现子 concept：

```cp
concept base_reader {
    read(self const&) -> i32
    {
        return 1;
    }
}

concept named_reader {
    requires base_reader;

    name(self const&) -> str;
}

struct item {
}

impl base_reader for item {
} // 这里物化 item::read

impl named_reader for item {
    name(self const&) -> str
    {
        return "item";
    }
}
```

不能只写子 concept impl 并在里面补父 concept 的要求：

```cp
impl named_reader for item {
    read(self const&) -> i32
    {
        return 1;
    }

    name(self const&) -> str
    {
        return "item";
    }
} // 错误：item 仍然没有 base_reader 证明
```

## Concept 实现

`impl concept_name for type_name` 为某个具体类型实现 concept。泛型 concept `impl` 使用 `impl<...>` 声明泛型参数，例如 `impl<T> iterator for vector_iter<T> requires ...`；目标类型模式和条件实现规则见 [generic.md](generic.md)。`impl` 本身不是可导出的顶层名字，不写 `export impl`，也不通过 `import` 单独引入。

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

检查规则：

- 定义 `impl concept_name for type_name` 的位置必须能看见 `concept_name` 和 `type_name`。
- 当前 concept impl 目标可以是 `struct`、`variant`、数组类型或编译器识别的 builtin 类型；opaque alias 可以作为普通固有 `impl handle { ... }` 的目标，但不是当前 concept impl 目标。`impl some_concept for handle { ... }` 会按非法目标报错，`handle` 也不会因为底层 `i32`、`u8*` 等类型已有 concept impl 就自动满足同一个 concept。
- `enum`、`storage`、tuple、指针、引用、函数类型和函数指针也不是当前 concept impl 目标。`enum` 可以通过内建 `==` / `!=` / `<=>` 满足编译器识别的比较类 concept，但不能显式写 `impl some_concept for some_enum { ... }` 来增加普通 concept 证明或成员。其它形状可以作为普通值类型或参数类型参与其它规则，但不能直接写 `impl some_concept for storage T`、`impl some_concept for (A, B)` 或 `impl some_concept for i32*`；这类目标形状报 `unknown_type`。
- 使用 concept 能力的位置必须能看见目标类型和对应 concept，不要求看见 `impl` 所在模块。这里的“concept 能力”指约束证明本身；如果某个 concept impl 函数同时作为成员或关联函数被普通调用，普通调用仍按该函数被注册到的成员、关联函数或扩展方法可见性规则查找。
- `impl` 不需要被使用点所在文件直接导入；只要该文件参与同一次编译输入，合法的 concept impl 就会参与约束证明。
- 同一次编译输入中，不允许存在两个相同的 `impl concept_name<args...> for type_name`；重复实现报 `duplicate_concept_impl`。泛型 concept 的实参也是重复实现 key 的一部分；`impl partial_eq<str> for T` 和 `impl partial_eq<i32> for T` 是不同实现，两个 `impl partial_eq<str> for T` 才冲突。
- 泛型 concept `impl` 的 `requires` 约束不满足时，该 `impl` 不为对应具体类型提供 concept 证明。
- 泛型 concept impl 可以显式写 `impl<T> Concept for box<T>`，也可以在目标类型模式中省略泛型参数列表并由未绑定名字推断；当前不支持实现级类型参数包。
- concept 名后可以带显式实参，例如 `impl partial_eq<str> for string_like`。
- concept 实参省略时，按 concept 声明中的默认泛型参数补齐。
- 无默认的关联类型必须由 `type name = Type;` 实现；缺失时报 `missing_concept_item`。
- 有默认的关联类型可以省略；省略时使用 concept 中的默认类型。
- concept impl 中的 `type name = Type;` 关联类型绑定不要求目标类型是 `struct`。`variant`、builtin 和数组目标都可以显式绑定关联类型，默认关联类型也会为这些目标物化；它们进入同一个目标类型的 `T::` 关联类型命名空间。目标种类限制主要作用在函数项上，例如 builtin/数组函数必须带 `self` receiver。
- concept impl 里的关联类型实现必须写 `= Type`；裸 `type name;` 只表示 concept 体内的关联类型要求，放在 `impl Concept for T` 中会报 `expected_type`。同一个 concept impl 中重复提供同名关联类型会报 `duplicate_symbol`。
- concept impl 块中允许出现当前 concept 没有要求的额外 `type` 绑定或函数。它们不会被当作“未知 requirement”拒绝，也不会参与满足其它 requirement；语义上等价于在同一个 `T::` 关联命名空间里额外声明目标类型的关联类型、成员函数或关联函数，并按普通重复符号规则检查冲突。
- 无默认的函数必须由函数定义实现。
- 有默认实现的函数可以省略；省略时使用 concept 中的默认函数体。
- concept 默认关联类型会在收集实现函数签名前进入当前目标类型的关联类型命名空间，因此实现函数可以直接在返回类型或参数类型中使用默认关联类型名。
- concept 默认函数会按目标类型物化为普通成员函数或关联函数，并进入同一个 `T::` 命名空间；默认函数体中的 `this`、`self`、关联类型名和 concept 泛型参数都会按当前目标类型与 concept 实参替换后检查。
- 默认函数只在当前 requirement 没有实际候选时物化。`impl` 块中显式提供同名函数时，该函数就是候选；对 `struct` 目标，目标类型已有的固有成员函数或关联函数也是候选。只要存在候选，就先做签名匹配；候选签名不匹配时报类型错误，不退回默认函数。
- `variant`、builtin 和数组目标没有“已有固有成员补 requirement”的规则。它们要么在 concept impl 中显式提供 requirement 函数，要么依赖 concept 默认函数。若默认函数物化时与目标类型已经存在的同名成员、关联函数或关联类型冲突，会报重复符号，而不是把已有项当作 requirement 实现。
- impl 提供的函数签名必须与 concept 要求匹配。接收者参数、`this` 和关联类型名按当前实现类型与关联类型绑定替换后比较。
- concept impl 中的函数不做普通函数或固有 impl 函数那种省略返回类型推导；省略 `-> R` 会形成 `unit` 返回签名。除非 concept 要求本身返回 `unit`，否则实现函数必须显式写出返回类型。
- concept impl 中显式提供的函数可以使用普通函数参数默认值，但默认值不参与 requirement 签名匹配。也就是说，实现函数仍必须拥有与 concept 要求完全相同的完整参数列表；默认值只影响这个实现函数后续作为成员函数或关联函数被调用时是否可以省略尾部实参。
- impl 中可以直接使用当前 concept 的关联类型名，例如 `iter_item`。
- 如果 concept 要求是 operator，concept impl 中应显式提供同一个 operator kind。普通函数名不能满足 operator 要求，operator 也不能满足普通函数名要求。
- 对 `struct` 目标的既有成员补位只查找普通成员函数和关联函数，不查找固有 operator overload。也就是说，`impl T { operator ==(...) -> bool { ... } }` 不会自动满足用户自定义 concept 里的 `operator ==(...) -> bool;` requirement；除非该 concept 是编译器识别的 `equality_comparable` / `three_way_comparable` 等内建 concept，或在对应 `impl Concept for T` 中显式提供 operator requirement。
- concept impl 中显式提供的 operator requirement 当前只作为 concept requirement 的实现项参与匹配，不会注册成普通表达式可用的 operator overload。泛型函数体里要写 `left == right`、`++value` 或 `order(left, right)`，仍需要目标类型通过固有 impl、顶层 operator 或编译器内建规则拥有真实可见的调用/运算符。
- 对 `struct` 目标类型，concept 要求可以由 concept impl 块中提供的函数满足，也可以由目标 `struct` 已有的固有成员函数或关联函数满足。当前实现只对 `struct` 做这种既有成员补位；`variant`、builtin 和数组目标需要在对应 concept impl 中提供要求项，或依赖默认函数。
- builtin 和数组目标的 concept impl 函数必须使用 `self` receiver 形式；当前不支持给 builtin 或数组目标在 concept impl 中声明无 receiver 的关联函数。它们被注册为扩展方法，能否在其它模块用普通方法调用语法看到，取决于定义模块是否导出以及使用处是否导入了对应扩展方法；这不影响该 impl 作为 concept 证明参与约束检查。
- 函数签名比较使用替换后的精确类型相等，不做隐式转换、返回类型协变或重载排序。参数数量、receiver 形态、返回类型和关联类型替换结果都必须一致。

省略返回类型的差异尤其容易误用。固有 `impl` 中的源码函数体会推导返回类型：

```cp
variant token {
    empty;
    ready(i32);
}

impl token {
    score(self const&)
    {
        return match self {
            .ready(value) => value,
            .empty => 0,
        };
    }
}
```

但同样的写法放进 concept impl 时不表示 `score -> i32`，而是签名返回 `unit`，因此不能满足下面的 requirement：

```cp
concept measurable {
    score(self const&) -> i32;
}

impl measurable for token {
    score(self const&)
    {
        return match self {
            .ready(value) => value,
            .empty => 0,
        };
    }
} // 错误：实现函数省略返回类型，签名是 unit，不是 i32
```

正确写法必须显式声明返回类型：

```cp
impl measurable for token {
    score(self const&) -> i32
    {
        return match self {
            .ready(value) => value,
            .empty => 0,
        };
    }
}
```

concept 默认函数也遵循同一条规则。默认函数省略返回类型时，requirement 本身就是 `unit` requirement；函数体里的 `return value;` 不会反向推导 requirement 返回类型。

### 条件实现

泛型 concept impl 可以带 `requires`，当前实现支持三类条件：

- 裸 concept 名，表示当前目标类型必须满足该 concept。
- `Type: Concept`，表示某个类型表达式必须满足 concept。
- `Type == Type`，表示两个类型表达式必须相同。

```cp
concept mark {
}

concept parent {
}

concept child {
}

concept int_box {
}

struct box<T> {
    value: T;
}

impl mark for i32 {
}

impl parent for box<i32> {
}

impl<T> child for box<T>
requires parent
{
}

impl<T> int_box for box<T>
requires
    T: mark and T == i32
{
}
```

规则：

- 在 `impl<T> child for box<T> requires parent` 中，裸 `parent` 等价于 `box<T>: parent`。
- 在 `requires T: mark and T == i32` 中，两个条件都必须满足；`box<bool>` 不获得 `int_box` 证明。
- 条件不满足时，该 impl 对具体类型不可用，不是在 impl 定义处报错。使用点如果需要该 concept 且没有其它可用 impl，会报告缺少 concept item/实现证明。
- `requires` 中只支持 `and` 和括号组合，不支持 `or`、`not` 或任意运行时表达式。
- 条件实现不会自动补齐父 concept 的 impl；需要父 concept 时仍要有对应证明。

## 编译器识别的 Concept

当前实现有少数 concept 会被编译器按语言规则直接判定，不需要用户写 `impl`。这些 concept 仍必须在当前模块可见；它们通常来自 `std.compare` 或 `std.meta`，不是未导入也可用的全局关键字。

`std.compare` 中这些名字按当前实现是名字级 special case。也就是说，只要当前可见的 concept 名字匹配下面这些标准名字，语义层就按对应语言规则判定；当前没有额外检查这个 concept 符号一定来自 `std.compare` 模块。相关规则里需要的 `weak_ordering` 也按当前语义结果中名为 `weak_ordering` 的 `variant` 解析。它们应视为标准库保留名字，不要在其它模块中用同名 concept 或比较分类表达不同语义。

名字级 special case 仍然先通过普通 concept 名字解析和 concept 实参检查。也就是说，当前作用域里必须真的有一个可见 concept 叫 `equality_comparable`、`incrementable` 等，泛型默认实参也按这个声明求值；随后内建分支按实参数量和语言规则检查能力。若用户自定义同名 concept 但泛型形状不兼容，例如没有 `Rhs = this` 却写 `T: equality_comparable<T>`，不会得到普通 requirement 匹配，而是先在这个同名 concept 的实参形状上失败或被内建分支判为不满足。concept 体里的函数 requirement 列表不会参与这些名字的能力证明；证明只看下面列出的语言规则。

- `mutable_object`：目标类型必须是可作为可写对象存放的值类型。引用、函数类型、`unit`、`!`、推导类型、泛型参数和 pack expansion 不满足；builtin、数组、storage、tuple、指针、`struct`、`enum`、opaque alias 和 `variant` 满足。
- `ordering<T>`：目标类型必须能作为 order object 用调用表达式 `order(left, right)` 调用，其中 `order` 是该目标类型的值，`left` / `right` 是 `T const&`，返回值必须能隐式转换到当前语义输入中的 `weak_ordering`。可用形式包括函数类型、函数指针、闭包/lambda，或提供 `operator ()(self const&, left: T const&, right: T const&)` 的 `struct` / `variant`。这不是查找名为 `order` 的自由函数，也不是 `order.order(left, right)` 成员调用协议。
- `equality_comparable<Rhs = this>`：`self == rhs` 必须可用，并且返回值必须能隐式转换到 `bool`。内建 `==` 和用户定义 `operator ==` 都会参与判断。
- `three_way_comparable<Rhs = this, Category = weak_ordering>`：`self <=> rhs` 必须可用，并且返回值必须能隐式转换到 `Category`。
- `incrementable`：内建整数满足；非整数类型需要前置 `operator prefix ++(self&)`，并且返回值必须能隐式转换到 `this&`。

`std.meta` 中这些名字和上面的 `std.compare` 名字不同：只有当 concept 声明来自 `std.meta` 模块时才触发内建判定。用户自己声明名为 `callable`、`is_lvalue_reference`、`is_const_lvalue_reference` 或 `is_move_reference` 的 concept，不会解锁这些内建判定或 `std.meta` 类型查询。

- `callable<Args...>`：目标类型的值必须能用 `Args...` 作为调用参数调用；规则与 [meta.md](meta.md) 的 `call_result` 相同。
- `is_lvalue_reference`：目标类型本身是普通左值引用。
- `is_const_lvalue_reference`：目标类型本身是普通 const 左值引用。
- `is_move_reference`：目标类型本身是 move 引用。

对真正来自 `std.meta` 的这些 concept，内建判定结果同样先于显式 `impl` 使用。也就是说，`impl callable<i32> for box {}` 不能让本身不可用 `i32` 调用的 `box` 通过 `box: callable<i32>`；`impl is_move_reference for T {}` 也不能把非 `move&` 类型伪装成 move 引用。显式 impl 块仍按普通规则收集和检查，但这些标准 concept 的能力证明由编译器内建查询决定。

这些内建判定只服务于约束、`template if` 和泛型实例选择，不改变类型本身行为。用户仍然可以为普通 concept 写显式 `impl`，但不要在其它模块中复用这些特殊名字表达不同语义；当前实现对 `std.compare` 这组名字是按名字识别的。

对 `mutable_object`、`ordering`、`equality_comparable`、`three_way_comparable` 和 `incrementable` 这组按名字识别的 concept，使用点会先返回编译器内建判定结果，再考虑显式 `impl`。因此显式写 `impl equality_comparable for T {}` 不能让没有可用 `==` 且返回值不能隐式转换到 `bool` 的 `T` 通过 `T: equality_comparable`，也不能覆盖 `incrementable` 对前置 `++` 返回值可隐式转换到 `this&` 的要求。显式 impl 块本身仍按普通 concept impl 收集和检查，但这些保留名字的能力证明由语言规则决定。

当前没有公开实现 `copyable`、`movable` 或 `move_only` 这三个 concept。copy/move 构造和赋值能力目前不是可导入、可约束的公开 concept 能力，因此不能在泛型约束中写 `T: movable` 来要求它。后续若要暴露这些能力，需要把 concept 名、可见性、特殊成员判定和 `not` 约束一起补成稳定设计。

## Type 语句

`type` 语句用于声明类型别名：

```cp
type index = i32;

main() -> i32
{
    type local_index = i32;
    let value: local_index = 1;
    return value;
}
```

固有 `impl` 中的 `type` 声明进入类型的 `T::` 命名空间：

```cp
impl vec2 {
    type scalar = i32;

    sum(self const&) -> scalar
    {
        return x + y;
    }
}
```

concept impl 中的 `type` 声明是关联类型实现，也进入同一个 `T::` 命名空间：

```cp
impl iterator for range_iter {
    type iter_item = i32;
}

type item = range_iter::iter_item;
```

当前实现中，模块级普通 `type name = Type;` 的右侧在顶层声明收集阶段立即解析，不按源码后续声明做延迟解析。因此：

- 可以引用内建类型、已经进入当前模块类型表的 `struct`、`enum`、前序普通类型别名，以及当前已经进入导出表的导入类型；导入的普通 type alias 仍可能受本轮收集顺序影响。
- 不能引用同一模块中稍后才收集的普通类型别名。
- 不能引用同一模块的 `variant` 声明，即使 `variant` 在源码文本中写在 `type` 之前；当前收集顺序仍是先解析普通 type alias，再收集 variant 声明。
- 需要给数组、元组或函数类型起名后再做类型初始化时，应把 alias 指向当前实现能解析到的类型，例如 `type i32x4 = [i32; 4]; let values = i32x4{1, 2};`。

这个边界只描述当前实现。局部 `type` 语句和 impl/concept 关联类型在所在语义上下文中解析，按该上下文已经可见的类型名工作。

## 扁平命名空间

每个类型 `T` 有一个扁平的 `T::` 关联命名空间。该命名空间包含：

- 固有 `impl T` 中的类型别名。
- 固有 `impl T` 中的成员函数和关联函数。
- `impl Concept for T` 中的关联类型。
- `impl Concept for T` 中的函数。
- concept 默认关联类型和默认函数。

因此 concept 项和固有 impl 项共享同一个命名空间：

```cp
range_iter::iter_item
```

如果两个来源给同一类型引入同名项，则报冲突：

```cp
concept readable {
    read(self&) -> i32
    {
        return 0;
    }
}

concept cached {
    read(self&) -> i32
    {
        return 1;
    }
}

impl readable for file {}
impl cached for file {} // 错误：file::read 冲突
```

设计上要求 concept 作者为关联项选择更具体的名字，例如 `iter_item`、`error_type`、`output_type`，避免不同 concept 在同一个类型上冲突。

如果重复项来自同一个父 concept 要求，则只算一次。例如多个父路径都继承到同一个 `iterator::iter_item` 时，不构成冲突。

## 名字查找

解析类型名时，在 concept/impl 相关上下文中使用上下文查找。

推荐查找顺序：

```text
局部 type 别名
当前 concept / impl 的 type 项
当前 this 类型的扁平 T:: 命名空间
模块级类型别名 / struct / import 类型 / builtin 类型
```

因此可以直接写：

```cp
concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}
```

在实现中也可以直接写：

```cp
impl iterator for range_iter {
    type iter_item = i32;

    next(self&) -> optional<iter_item>
    {
        if(current_value >= end_value) {
            return optional<iter_item>::none;
        }

        let value = current_value;
        current_value += 1;
        return optional<iter_item>::some(value);
    }
}
```

`this::name` 可以作为可选显式写法保留，用于局部别名遮蔽时消歧：

```cp
impl iterator for range_iter {
    type iter_item = i32;

    make(self const&) -> this::iter_item
    {
        type iter_item = i64;
        return current_value;
    }
}
```

## 默认实现

默认关联类型和默认函数进入类型的扁平 `T::` 命名空间。

```cp
concept parser {
    type output_type;
    type error_type = str;

    parse(self&) -> optional<output_type>;

    has_error(self const&) -> bool
    {
        return false;
    }
}
```

如果实现者省略默认项：

```cp
impl parser for int_parser {
    type output_type = i32;

    parse(self&) -> optional<i32>
    {
        return optional<i32>::some(0);
    }
}
```

则 `int_parser::error_type` 使用 `str`，`int_parser::has_error` 使用 concept 默认函数体。

默认函数体中的 `this`、字段访问、成员调用和关联类型名都按当前实现类型重新检查。默认实现不引入运行时分派。

### 默认项物化规则

默认关联类型在收集 concept impl 时先物化，再检查 impl 中的函数签名。因此 impl 函数可以直接在参数或返回类型中使用默认关联类型名：

```cp
concept reader<T> {
    type item = T;

    read(self const&) -> item;

    echo(self const&, value: item) -> item
    {
        return value;
    }
}

struct int_reader {
    value: i32;
}

impl reader<i32> for int_reader {
    read(self const&) -> item
    {
        return value;
    }
}

main() -> i32
{
    type output = int_reader::item;
    let reader = int_reader{ 20 };
    let value: output = reader.read();
    return reader.echo(value) + 22;
}
```

这里 `reader<i32>` 会把 `item` 默认成 `i32`，并把 `echo` 物化为 `int_reader` 的成员函数。concept 泛型参数、`this` 和关联类型名都会按当前 impl 的目标类型与 concept 实参替换。

默认关联类型的物化只补“实现者没有提供”的项。实现者显式写了 `type item = ...;` 时，以显式关联类型为准；没有显式写时，使用 concept 默认类型。默认关联类型名和目标类型已有的关联类型共享同一个扁平命名空间，不能重复。默认类型右侧按 concept 定义所在模块解析，再把 concept 泛型参数和 `this` 替换为当前 impl 的实参和目标类型。

默认函数是否成为成员函数取决于第一个参数：

- 第一个参数是 `self` receiver 时，默认函数物化为成员函数或扩展成员方法。
- 没有 `self` receiver 时，默认函数物化为关联函数。`struct` 和 `variant` 目标可以拥有这种默认关联函数。
- builtin 类型和数组类型的 concept impl 只能引入带 `self` receiver 的成员函数；无 receiver 的默认关联函数或 impl 函数会报 `invalid_self_parameter`。

默认函数物化出的扩展成员方法使用 impl 所在模块的扩展方法可见性。也就是说，`impl Concept for i32` 自身作为 concept 证明可以被同次编译的约束检查使用；但如果调用者要写普通方法调用 `value.measure()`，仍需要该扩展方法在调用点可见。具名模块中的 impl 会把物化出的扩展成员方法登记到该模块的导出扩展方法表，后续模块通过导入该模块获得普通调用可见性；匿名模块里的物化扩展方法只在当前编译单元内部可见。

因此下面这种默认成员函数可以用于 builtin、数组和 variant 目标：

```cp
concept measured {
    measure(self const&) -> i32
    {
        return 1;
    }
}

variant token {
    ready;
}

impl measured for i32 {
}

impl measured for [i32; 2] {
}

impl measured for token {
}

main() -> i32
{
    let values = [1, 2];
    let value = token::ready;
    return (10 as i32).measure() + values.measure() + value.measure();
}
```

但下面这种无 receiver 默认函数不能用于 builtin 或数组目标：

```cp
concept maker {
    make() -> i32
    {
        return 1;
    }
}

impl maker for i32 {
} // 错误：builtin 目标不能由 concept 默认项引入无 receiver 关联函数

impl maker for [i32; 2] {
} // 错误：数组目标同样要求 self receiver
```

默认项不是“兜底重载”。如果实现者显式提供了同名函数，或者 `struct` 目标已有同名固有成员/关联函数，编译器会尝试用这些函数满足 requirement；只要存在同名候选，就不会再物化默认函数。候选签名不匹配时报告类型不匹配，而不是退回到默认实现。

默认函数物化后是一个具体函数实例。实例 key 包含目标类型和 concept 实参；同一个默认函数体可以为不同目标类型或不同 concept 实参产生不同的成员函数。物化后的函数体在目标类型上下文中按普通函数体检查，但它仍遵守 concept 函数规则：没有显式 `-> R` 时返回类型就是 `unit`，不会从默认函数体推导。

默认函数的参数默认值也在物化后按具体目标类型检查和记录，具体表达式规则沿用普通函数默认参数规则。默认参数不改变 requirement 签名：实现匹配仍按完整参数列表比较，省略尾部实参只发生在后续调用这个物化函数的调用点。

`struct` 目标有一条当前实现特例：concept requirement 可以由目标 `struct` 已有的固有成员函数或关联函数满足。这个补位只适用于 `struct`；`variant`、builtin 和数组目标不会用已有固有方法补 requirement：

```cp
concept readable {
    read(self const&) -> i32;
}

struct file {
    value: i32;
}

impl file {
    read(self const&) -> i32
    {
        return value;
    }
}

impl readable for file {
} // 可以：struct 固有成员满足 requirement

variant handle {
    open;
}

impl handle {
    read(self const&) -> i32
    {
        return 1;
    }
}

impl readable for handle {
} // 错误：variant 固有成员不补 concept requirement
```

默认关联类型和默认函数都会进入同一个 `T::` 扁平命名空间。默认关联类型名不能与目标类型已有的关联类型冲突；当默认函数确实需要物化时，它的名字不能与目标类型已有的关联类型、成员函数或关联函数冲突。显式 impl 函数覆盖默认 requirement 时仍使用同一名字，并按替换后的精确签名比较。

## 支持内容

`concept` 支持：

- concept 声明和 concept 依赖。
- 泛型 concept：`concept partial_eq<Rhs = this>`。
- concept 默认泛型参数中的 `this` 绑定。
- concept `requires` 使用 `and` 连接父 concept、类型 concept 约束和类型相等约束。
- `type name = Type;` 类型别名语句。
- concept 关联类型要求与默认关联类型。
- concept 函数要求与默认函数。
- concept 默认函数和 concept impl 函数的尾部参数默认值；纯 requirement 默认值不是公开能力。
- `impl Concept for Type`。
- `impl<...> Concept for TypePattern` 条件实现。
- 显式 concept 实参：`impl partial_eq<str> for string_like`。
- 扁平 `T::` 命名空间和冲突检查。

UFCS 的普通调用、成员调用和泛型依赖调用规则分别见 [struct.md](struct.md) 和 [generic.md](generic.md)。concept 默认函数体中的成员调用也遵循同一套 UFCS 规则；如果默认函数体依赖 `this` 的具体实现，则在对应实现类型上重新检查。
