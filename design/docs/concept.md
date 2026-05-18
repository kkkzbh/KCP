# Concept

本文档记录 `concept` 的设计。`concept` 表示编译期能力约束，不引入运行时多态、虚表或 `dyn` 机制。

目标是给结构体和普通类型提供静态协议能力：关联类型、函数要求、默认关联类型、默认函数实现，以及父 concept 约束。

## 语法总览

```text
ConceptDecl        -> export? concept identifier { ConceptItem* }
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
RequiresConstraint -> identifier
                    | Type : ConceptBoundList
                    | Type == Type
ConceptBoundList   -> identifier ( and identifier )*

AssociatedTypeRequirement
                   -> type identifier ;
AssociatedTypeDefault
                   -> type identifier = Type ;

FunctionRequirement
                   -> identifier ( ParameterList? ) ReturnType? ;
FunctionDefault    -> identifier ( ParameterList? ) ReturnType? Block

ConceptImpl        -> impl ConceptName for TypePattern RequiresClause? { ConceptImplItem* }
ConceptImplItem    -> AssociatedTypeBinding
                    | Function
AssociatedTypeBinding
                   -> type identifier = Type ;

TypeAlias          -> type identifier = Type ;
```

`TypePattern` 和 `RequiresClause` 的完整规则见 [generic.md](generic.md)。

`type name = Type;` 是通用类型别名语句，可出现在全局、局部、固有 `impl`、`concept` 和 `impl concept for T` 中。

裸 `type name;` 只允许出现在 `concept` 体内，用来声明关联类型要求。

## Concept 声明

```cp
export concept iterator {
    type iter_item;

    next(self: Self&) -> optional<iter_item>;
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
- `Self` 表示当前实现类型，只能在 `concept` 和对应 `impl concept for T` 上下文中使用。
- `concept` 体内可以直接使用当前 concept 的关联类型名，例如 `iter_item`，不要求写 `Self::iter_item`。

## 父 Concept

父 concept 使用 `requires` 声明，不带括号。多个约束使用 `and` 连接：

```cp
concept sized_iterator {
    requires iterator;

    remaining(self: Self const&) -> i32;
}

concept random_access_iterator {
    requires sized_iterator and bidirectional_iterator;

    offset(self: Self&, n: i32);
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

`concept` 体内的 `requires` 也可以约束关联类型。裸 concept 名是父 concept 简写，等价于 `Self: concept_name`；显式类型约束可以指向 `Self`、当前 concept 的关联类型，或关联类型路径：

```cp
concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self: Self&) -> iter_type;
}
```

规则：

- `requires iterator;` 等价于 `requires Self: iterator;`，表示当前实现类型必须同时实现 `iterator`。
- `requires iter_type: iterator;` 表示关联类型 `iter_type` 必须实现 `iterator`。
- `requires iter_type::iter_item == iter_item;` 表示两个类型表达式必须相同。
- 只支持 `and` 连接，允许用括号组合子表达式，不支持 `or` 约束。

```cp
impl iterator for cursor {
    type iter_item = i32;

    next(self: cursor&) -> optional<i32>
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
    remaining(self: cursor const&) -> i32
    {
        return self.end - self.index;
    }
}
```

## Concept 实现

`impl concept_name for type_name` 为某个具体类型实现 concept。泛型 concept `impl` 可以写作 `impl concept_name for vector<T> requires ...`，目标类型模式和条件实现规则见 [generic.md](generic.md)。`impl` 本身不是可导出的顶层名字，不写 `export impl`，也不通过 `import` 单独引入。

```cp
struct range_iter {
    current_value: i32;
    end_value: i32;
}

impl iterator for range_iter {
    type iter_item = i32;

    next(self: range_iter&) -> optional<i32>
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
- 无默认的关联类型必须由 `type name = Type;` 实现。
- 有默认的关联类型可以省略；省略时使用 concept 中的默认类型。
- 无默认的函数必须由函数定义实现。
- 有默认实现的函数可以省略；省略时使用 concept 中的默认函数体。
- impl 提供的函数签名必须与 concept 要求匹配。`Self` 和关联类型名按当前实现类型与关联类型绑定替换后比较。
- impl 中可以直接使用当前 concept 的关联类型名，例如 `iter_item`。

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

    sum(self: vec2 const&) -> scalar
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
- `impl concept for T` 中的关联类型。
- `impl concept for T` 中的函数。
- concept 默认关联类型和默认函数。

因此 concept 项和固有 impl 项共享同一个命名空间：

```cp
range_iter::iter_item
```

如果两个来源给同一类型引入同名项，则报冲突：

```cp
concept readable {
    read(self: Self&) -> i32
    {
        return 0;
    }
}

concept cached {
    read(self: Self&) -> i32
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
当前 Self 类型的扁平 T:: 命名空间
模块级类型别名 / struct / import 类型 / builtin 类型
```

因此可以直接写：

```cp
concept iterator {
    type iter_item;

    next(self: Self&) -> optional<iter_item>;
}
```

在实现中也可以直接写：

```cp
impl iterator for range_iter {
    type iter_item = i32;

    next(self: range_iter&) -> optional<iter_item>
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

`Self::name` 可以作为可选显式写法保留，用于局部别名遮蔽时消歧：

```cp
impl iterator for range_iter {
    type iter_item = i32;

    make(self: range_iter const&) -> Self::iter_item
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

    parse(self: Self&) -> optional<output_type>;

    has_error(self: Self const&) -> bool
    {
        return false;
    }
}
```

如果实现者省略默认项：

```cp
impl parser for int_parser {
    type output_type = i32;

    parse(self: int_parser&) -> optional<i32>
    {
        return optional<i32>::some(0);
    }
}
```

则 `int_parser::error_type` 使用 `str`，`int_parser::has_error` 使用 concept 默认函数体。

默认函数体中的 `Self`、字段访问、成员调用和关联类型名都按当前实现类型重新检查。默认实现不引入运行时分派。

## 支持内容

`concept` 支持：

- concept 声明和 concept 依赖。
- concept `requires` 使用 `and` 连接父 concept、类型 concept 约束和类型相等约束。
- `type name = Type;` 类型别名语句。
- concept 关联类型要求与默认关联类型。
- concept 函数要求与默认函数。
- `impl concept for Type`。
- 扁平 `T::` 命名空间和冲突检查。

`concept` 不支持：

- 运行时多态、虚表、`dyn concept`。
- 以裸类型参数为目标的 blanket impl，例如 `impl debug for T`。
- concept 参数化。
- 函数重载。

UFCS 的普通调用、成员调用和泛型依赖调用规则分别见 [struct.md](struct.md) 和 [generic.md](generic.md)。concept 默认函数体中的成员调用也遵循同一套 UFCS 规则；如果默认函数体依赖 `Self` 的具体实现，则在对应实现类型上重新检查。
