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
                   -> identifier ( ParameterList? ) ReturnType? ;
FunctionDefault    -> identifier ( ParameterList? ) ReturnType? Block
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
- 函数声明表示实现者必须提供函数。
- 函数定义表示默认函数实现，实现者可以省略或覆盖。
- 第一个参数可以写作特殊接收者参数 `self`、`self&`、`self like&`、`self const&` 或 `self move&`，分别表示当前类型、当前类型可写引用、receiver-const 传播引用、当前类型 const 引用和当前类型移动引用。`self like&` 会驱动签名中的 `T like*`、`T like&` 等类型转发 constness；`self move&` 的规则见 [ownership.md](ownership.md)。本轮不支持 `self forward&`。
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
- concept 参数包可以为空；例如 `T: callable` 等价于 `T: callable<>`。
- concept 参数包没有对应的值参数包，不能写 `values: Args...`，也不能在 concept 体内用 `template for` 展开。
- concept 参数包不能带默认实参。
- concept 引用处按普通类型实参规则检查每个 pack 元素；`F: callable<i32, bool>` 会把 `Args...` 绑定为 `[i32, bool]`。
- `F: callable<Args...>` 只在 `Args` 是当前可见类型参数包时合法；展开语法必须是裸 pack 名加 `...`。
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
- 子 concept 继承父 concept 的要求、默认关联类型和默认函数。
- 一个类型实现子 concept 时，必须已经满足所有父 concept。
- 不允许在子 concept 的 impl 中顺便补父 concept 的要求；父 concept 需要单独实现。

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
- 只支持 `and` 连接，允许用括号组合子表达式，不支持 `or` 约束。

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
- 使用 concept 能力的位置必须能看见目标类型和对应 concept，不要求看见 `impl` 所在模块。
- `impl` 的物理文件位置不重要；只要该文件参与同一次编译输入，合法的 concept impl 就作为全局实现事实参与检查。
- 同一次编译输入中，不允许存在两个相同的 `impl concept_name for type_name`。
- 泛型 concept `impl` 的 `requires` 约束不满足时，该 `impl` 不为对应具体类型提供 concept 证明。
- concept 名后可以带显式实参，例如 `impl partial_eq<str> for string_like`。
- concept 实参省略时，按 concept 声明中的默认泛型参数补齐。
- 无默认的关联类型必须由 `type name = Type;` 实现。
- 有默认的关联类型可以省略；省略时使用 concept 中的默认类型。
- 无默认的函数必须由函数定义实现。
- 有默认实现的函数可以省略；省略时使用 concept 中的默认函数体。
- impl 提供的函数签名必须与 concept 要求匹配。接收者参数、`this` 和关联类型名按当前实现类型与关联类型绑定替换后比较。
- impl 中可以直接使用当前 concept 的关联类型名，例如 `iter_item`。

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

## 语言级推导 Concept

少数 concept 由编译器根据语言规则直接推导，不需要用户写 `impl`：

- `copyable`：copy constructor 和 copy assignment 都可用。
- `movable`：move constructor 和 move assignment 都可用。
- `move_only`：满足 `movable`，但不满足 `copyable`。

这些 concept 只用于约束和查询，不改变类型行为。类型是否可 copy 或可 move 仍由特殊成员函数决定，完整规则见 [ownership.md](ownership.md)。

如果第一版还不支持 `not` 约束表达式，`move_only` 可以作为编译器内建 concept 暴露。

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

## 支持内容

`concept` 支持：

- concept 声明和 concept 依赖。
- 泛型 concept：`concept partial_eq<Rhs = this>`。
- concept 默认泛型参数中的 `this` 绑定。
- concept `requires` 使用 `and` 连接父 concept、类型 concept 约束和类型相等约束。
- `type name = Type;` 类型别名语句。
- concept 关联类型要求与默认关联类型。
- concept 函数要求与默认函数。
- `impl Concept for Type`。
- `impl<...> Concept for TypePattern` 条件实现。
- 显式 concept 实参：`impl partial_eq<str> for string_like`。
- 扁平 `T::` 命名空间和冲突检查。

UFCS 的普通调用、成员调用和泛型依赖调用规则分别见 [struct.md](struct.md) 和 [generic.md](generic.md)。concept 默认函数体中的成员调用也遵循同一套 UFCS 规则；如果默认函数体依赖 `this` 的具体实现，则在对应实现类型上重新检查。
