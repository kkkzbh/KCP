# 结构

本文档记录非泛型 `struct / impl` 的基础语言设计。目标是保留 C++ 风格的数据类型与花括号初始化，同时借鉴 Rust 的 `impl` 分离模型。

泛型 `struct`、泛型固有 `impl` 和条件 `impl` 见 [generic.md](generic.md)。所有权、移动引用、特殊成员和 `= delete` 见 [ownership.md](ownership.md)。`concept` 见 [concept.md](concept.md)。

## 语法总览

```text
StructDecl      -> export? struct identifier { FieldDecl* }
FieldDecl       -> identifier : Type ( = Expression )? ;

ImplBlock       -> impl TypeName { ImplItem* }
ImplItem        -> Constructor
                | Destructor
                | Function
                | Operator

Constructor     -> TypeName ( ParameterList? ) FunctionBody
                | TypeName ( ) = default ;
                | TypeName ( ParameterList? ) = delete ;
Destructor      -> ~ TypeName ( ) FunctionBody
Function        -> identifier ( ParameterList? ) ReturnType? FunctionBody
Operator        -> operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody
                | operator <=> ( self const& , rhs : this const& ) -> weak_ordering = default ;
                | operator OverloadableOperator ( ParameterList? ) ReturnType? = delete ;

StructInit      -> TypeName { InitArgList? }
InitArgList     -> NamedFieldInit ( , NamedFieldInit )*
                | Expression ( , Expression )*
NamedFieldInit  -> . identifier = Expression
```

## 结构体定义

`struct` 定义一个名义类型和它的字段布局：

```cp
struct vec2 {
    x: f64 = 0.0;
    y: f64 = 0.0;
}
```

结构体类型名使用小写命名，和当前语言/项目里的 C++ 风格一致，例如 `vec2`、`file_buffer`，不使用 Rust 风格的 `Vec2` / `FileBuffer`。

字段规则：

- 字段名在同一个 `struct` 内不能重复。
- 字段类型必须能被解析为合法类型。
- 字段可以写 `= Expression` 指定缺省初始化表达式。
- 字段缺省表达式按字段声明类型检查，不能访问 `self` 或函数局部变量。
- 不设计 `public/private`，字段都视为 public。
- `struct` 是名义类型。两个字段完全相同的结构体仍然是不同类型。

`export struct` 导出结构体类型名，使它可以被其他模块 `import` 后使用：

```cp
export struct vec2 {
    x: f64;
    y: f64;
}
```

因为没有访问限定，导出类型后字段也按 public 规则处理。

## impl 块

`impl` 把构造函数、析构函数、成员函数、关联函数和 operator 挂到某个类型下。当前实现支持的固有 `impl` 目标包括 `struct`、`variant`、opaque alias、内建类型和数组类型；不同目标可声明的项不同：

- `struct` 可以声明构造函数、析构函数、成员函数、关联函数和 operator。
- `variant` 可以声明成员函数、关联函数和 operator，不能声明构造函数或析构函数。
- opaque alias 可以声明成员函数和关联函数；当前实现不把 opaque alias 的 `impl operator` 注册为可用 operator。
- 内建类型和数组类型的 `impl` 函数必须带 `self` receiver，因此只能作为扩展成员函数使用，不能声明关联函数、构造函数或析构函数。
- 内建类型可以声明扩展 operator，按模块导入/导出规则可见；数组类型当前不提供可用的扩展 operator 注册路径。

```cp
impl vec2 {
    vec2(x: f64, y: f64) {
        return vec2{ .x = x, .y = y };
    }

    ~vec2() {
    }

    length(self const&) -> f64 {
        return sqrt(x * x + y * y);
    }

    zero() -> vec2 {
        return vec2{};
    }
}
```

一个类型可以有多个 `impl` 块。`impl` 只能出现在顶层，不能嵌套在函数或其他块中。

`impl` 的物理文件位置不重要。只要 `impl` 所在文件参与同一次编译输入，且该位置能解析到目标类型，`impl` 中可用的成员函数、关联函数和 operator 就挂到对应类型下。使用者能看见该类型并满足模块可见性时，就可以使用这些挂到类型上的项，不需要单独导入 `impl` 所在文件。构造函数和析构函数只适用于 `struct` 目标。

泛型类型的 `impl` 使用 `impl<...>` 声明泛型参数，例如 `impl<T> vector<T>`；`impl` 级 `requires` 约束和成员泛型函数规则见 [generic.md](generic.md)。

## 构造函数

构造函数写在 `impl type_name` 中，名字必须与当前结构体名一致：

```cp
impl vec2 {
    vec2(x: f64, y: f64) {
        return vec2{ .x = x, .y = y };
    }
}
```

构造函数规则：

- 构造函数禁止显式写返回类型。
- 构造函数返回类型固定为当前 `impl` 的结构体类型，不参与普通函数返回类型推导。
- 构造函数不能声明 `self` 参数。
- 构造函数体是普通函数体，使用普通 `return value;` 返回构造出的值。
- 构造函数里的所有带值 `return` 都必须能转换到当前结构体类型。
- `return;` 不允许用于构造函数。
- 构造函数允许重载，多个构造函数可以有不同参数列表。

构造函数建议只放不太可能失败的初始化逻辑，例如字段初始化、参数归一化和建立对象不变量。文件打开、网络连接、解析外部输入、权限检查等很可能失败的创建逻辑，应使用关联函数表达，并通过返回 `optional<type_name>`、`expected<type_name,error>` 或标准库中的等价结果类型表示失败。

### 默认构造

没有任何用户声明构造函数时，结构体隐式拥有默认构造行为：逐字段默认初始化。

```cp
struct vec2 {
    x: f64;
    y: f64;
}

let v = vec2{};
```

如果用户声明了任意构造函数，编译器不再额外生成隐式默认构造函数。因为所有字段都是 public，`type_name{}` 仍可在没有零参构造函数匹配时退化为字段默认初始化。

用户可以显式请求默认构造函数：

```cp
impl vec2 {
    vec2() = default;
}
```

`= default` 规则：

- 只允许用于零参数构造函数。
- 不能带函数体。
- 行为是逐字段默认初始化。
- 如果某个字段类型不可默认初始化，则该 default 构造函数非法。
- 它参与构造函数候选选择，因此 `vec2{}` 会优先匹配它。

### 默认三路比较

结构体不会隐式拥有排序语义。需要作为 `set` / `map` key，且字段声明顺序正好就是排序顺序时，可以显式请求默认三路比较：

```cp
impl vec2 {
    operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
}
```

默认 `<=>` 规则：

- 只允许用于非泛型 `struct` 的固有 `impl`。
- operator 必须是 `<=>`，参数必须是 `self const&` 和同类型 `rhs: this const&`。
- 返回类型固定为 `weak_ordering`。
- 行为是按字段声明顺序逐字段调用 `<=>`，把结果通过 `to_weak()` 转成 `weak_ordering`；第一个非 `equivalent` 结果立即返回，全部等价则返回 `weak_ordering::equivalent`。
- 每个字段类型都必须支持可用 `<=>`，且比较结果必须提供返回 `weak_ordering` 的 `to_weak()`。需要跳过字段、改变排序顺序或处理资源句柄时，应手写 `operator <=>`。

## 析构函数

析构函数写作 `~type_name()`，名字必须匹配当前 `impl` 类型：

```cp
impl handle {
    ~handle() {
        release(handle);
    }
}
```

析构函数规则：

- 析构函数没有参数。
- 析构函数没有返回类型，返回内部 `unit`。
- 析构函数不能使用 `return value;`。
- 析构函数体是普通语句块。
- 析构函数中存在一个隐式 `self&`。
- 析构函数中可以直接访问字段，也可以写 `self.field`。
- 一个结构体最多声明一个析构函数。

析构调用由编译器在对象生命周期结束时插入，不允许用户直接调用 `value.~type_name()`。显式释放通过标准库函数或所有权规则表达。

## 特殊成员函数

结构体的默认构造、析构、copy 构造、copy 赋值、move 构造和 move 赋值统称为特殊成员函数。copy/move 的所有权规则见 [ownership.md](ownership.md)。

copy 构造和 move 构造写作普通构造函数：

```text
impl handle {
    handle(other: this const&) = delete;
    handle(other: this move&);
}
```

copy 赋值和 move 赋值写作 `operator =`：

```text
impl handle {
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) -> this&;
}
```

`= delete` 表示该特殊成员存在但不可调用。它用于局部禁止 copy、move 或赋值，不引入属性、继承或访问限定。构造表达式如果选中了 deleted 构造函数，必须报错；不能再回退到字段 aggregate 初始化。

隐式生成规则：

- 没有用户声明构造函数时，默认构造按逐字段默认初始化生成。
- 析构函数默认生成；用户写了 `~T()` 后使用用户版本。
- copy 构造和 copy 赋值默认生成；任一字段不可 copy 时隐式 delete。
- move 构造和 move 赋值默认生成；任一字段不可 move 时隐式 delete。
- 用户声明或删除某个特殊成员后，编译器不再生成对应版本。

## 成员函数

普通 `impl` 函数的第一个参数名为 `self` 时，它是成员函数：

```cp
impl vec2 {
    length(self const&) -> f64 {
        return sqrt(x * x + y * y);
    }

    move(self&, dx: f64, dy: f64) {
        x = x + dx;
        self.y = self.y + dy;
    }
}
```

固有 `impl` 可以作用于结构体、variant、opaque alias、内建类型和数组类型。内建类型和数组类型没有用户可导入的类型声明模块，因此它们的扩展方法跟随所在模块的导入导出关系可见；未导入该模块时，不参与成员查找。内建类型和数组类型的扩展函数必须带 `self` receiver，不能作为 `type_name::name(...)` 关联函数调用。

```cp
export module int_ext;

impl i32 {
    plus10(self&) -> void
    {
        self += 10;
    }
}
```

`self` 参数规则：

- 接收者参数必须是第一个参数。
- 接收者参数写作 `self`、`self&`、`self like&`、`self const&` 或 `self move&`，分别表示当前类型、当前类型可写引用、receiver-const 传播引用、当前类型 const 引用和当前类型移动引用。本轮不支持 `self forward&`。
- `self const&` 只能读字段，不能写字段。
- `self&` 可以读写字段。
- `self like&` 可由可写 receiver 或 const receiver 调用；函数体按两种 receiver 视图检查，并把 `T like*`、`T like&` 等返回类型展开为对应的可写或 const 类型。完整规则见 [ownership.md](ownership.md)。
- `self move&` 只能由 `move value`、临时值或函数返回值绑定；函数体内需要继续转移 `self` 或字段时仍要显式写 `move`。
- 不设计 `mut self`，直接使用现有引用和 `const` 类型语法表达可变性。
- 其他类型位置需要当前类型时写 `this`，例如 `clone(self const&) -> this`。

成员函数调用使用点号：

```cp
let v = vec2{ 3.0, 4.0 };
let n = v.length();
```

`type_name::name(...)` 用于关联函数调用，不作为显式成员函数调用入口。

构造函数不通过普通调用语法调用。`point(1, 2)` 会按普通函数名 `point` 检查，不会转成 `point{1, 2}`；构造必须写花括号初始化。

## UFCS 调用

UFCS 允许成员调用形式和自由函数调用形式共享同一套能力，但它只是名字查找的备用路径，不是重载候选集。

```cp
length(v: vec2 const&) -> f64
{
    return sqrt(v.x * v.x + v.y * v.y);
}

let v = vec2{ 3.0, 4.0 };
let n = v.length(); // 找不到 vec2.length 时，按 length(v) 检查
```

点号调用 `object.name(args...)` 的解析顺序：

1. 先在 `object` 的类型上查找成员函数 `name`。
2. 如果存在同名成员函数，直接按成员函数调用检查参数、`self` 可变性和返回类型。检查失败时报错，不继续回退。
3. 如果不存在同名成员函数，再在当前可见自由函数中查找 `name`。
4. 如果存在自由函数 `name`，按 `name(object, args...)` 检查。

普通调用 `name(args...)` 的解析顺序：

1. 先在当前可见自由函数中查找 `name`。
2. 如果存在同名自由函数，直接按普通函数调用检查参数和返回类型。检查失败时报错，不继续回退。
3. 如果不存在同名自由函数，且当前位于成员函数或析构函数体内，再在 `self` 的类型上查找成员函数 `name`。
4. 如果存在 `self` 成员函数 `name`，按 `self.name(args...)` 检查。检查失败时报错，不继续回退。
5. 如果 `self` 上也不存在同名成员函数，且调用至少有一个参数，再在第一个参数 `first` 的类型上查找成员函数 `name`。
6. 如果存在成员函数 `name`，按 `first.name(rest...)` 检查。

这里的“当前可见自由函数”遵循 [module.md](module.md) 的模块可见性规则，包括当前模块内声明和 `import` 引入的导出函数。没有函数重载；因此一旦首选路径中存在同名函数，就视为用户明确选择了该名字，参数不匹配时应报告错误，而不是换到另一条 UFCS 路径。

UFCS 不递归展开。实现时应直接查询自由函数表和成员函数表，而不是让 `object.name(args...)` 重新调用 `name(object, args...)`，再让普通调用反向回到点号调用。

关联函数不参与 UFCS。`type_name::make(...)` 这类没有 `self` 的函数只能通过类型限定调用，不能通过 `value.make(...)` 或 `make(value, ...)` 隐式匹配。

成员函数体内的隐式 `self` 成员调用只在自由函数未命中时触发：

```cp
impl<T> vector<T> {
    push_back(self&, value: T) -> void
    {
        ensure_capacity(size() + 1);
    }
}
```

上例中，如果当前可见自由函数中没有 `ensure_capacity` 和 `size`，则分别按
`self.ensure_capacity(...)` 和 `self.size()` 检查。如果 `self` 上存在同名成员但参数不匹配，
直接报告该成员调用错误，不继续尝试首参 UFCS。需要调用其它对象的同名成员时应显式写
`other.name(...)`。

## 关联函数

不带 `self`、又不是构造函数或析构函数的 `impl` 函数是关联函数：

```cp
impl vec2 {
    zero() -> vec2 {
        return vec2{};
    }

    from_polar(r: f64, theta: f64) -> vec2 {
        return vec2{ r * cos(theta), r * sin(theta) };
    }
}
```

关联函数调用写作：

```cp
let origin = vec2::zero();
let p = vec2::from_polar(r, theta);
```

关联函数的主要用途是类型命名空间和工厂函数。可能失败的创建逻辑应优先设计为关联函数，而不是放进构造函数。

## 初始化表达式

结构体初始化统一使用花括号，支持三种形式：

```cp
vec2{ .x = x, .y = y }
vec2{ x, y }
vec2{}
```

### 命名字段聚合

`type_name{ .field = expr, ... }` 是命名字段聚合初始化，不走构造函数匹配：

```cp
let a = vec2{ .x = 1.0, .y = 2.0 };
let b = vec2{ .x = 1.0 };
```

规则：

- 显式指定的字段不能重复。
- 未指定字段优先使用字段声明上的缺省表达式；没有缺省表达式时按字段类型默认初始化。
- 字段顺序不影响语义。
- 字段初始化表达式按字段声明类型做上下文检查。
- 因为所有字段都是 public，命名字段聚合始终允许，即使结构体声明了构造函数。
- 不能混合命名字段初始化和顺序初始化；`.field = value` 不能跟在顺序实参之后，顺序实参也不能跟在命名字段之后。
- 允许列表末尾多写一个逗号。

### 顺序初始化

`type_name{ expr, ... }` 是顺序初始化：

```cp
let a = vec2{ 1.0, 2.0 };
let b = vec2{ 1.0 };
```

解析顺序：

1. 先按参数列表匹配构造函数。
2. 没有匹配构造函数时，再按字段声明顺序做聚合初始化。

如果匹配到的构造函数是 `= delete`，初始化失败，不会继续回退到字段聚合。只有完全没有匹配构造函数时，才会走顺序聚合初始化。

顺序聚合初始化规则：

- 实参数量不能超过字段数量。
- 已提供实参按字段声明顺序填充字段。
- 剩余字段优先使用字段声明上的缺省表达式；没有缺省表达式时按字段类型默认初始化。
- 字段初始化表达式按字段声明类型做上下文检查。

本语言没有 C++ `initializer_list` 特权。`vector{ n, 0 }` 不会被隐式解释为元素列表；它只表示构造函数调用或顺序聚合。元素列表应使用数组字面量和显式关联函数，例如 `vector::from_array([10, 0])` 或 `vector::filled(n, 0)`。

### 默认初始化

`type_name{}` 是结构体默认初始化表达式：

```cp
let a = vec2{};
```

解析顺序：

1. 优先匹配零参数构造函数，包括显式 `type_name() = default;`。
2. 没有零参数构造函数匹配时，按字段默认初始化。

按字段默认初始化时，字段声明上的 `= Expression` 优先于字段类型默认初始化：

```cp
struct production {
    lhs: nonterminal_kind = nonterminal_kind::augmented;
    rhs: vector<grammar_symbol> = vector<grammar_symbol>{};
}
```

如果某个缺省字段既没有字段缺省表达式，字段类型又不可默认初始化，初始化失败并报告错误。默认初始化结果由 [type_system.md](type_system.md) 定义。

### 构造函数候选选择

如果顺序初始化同时存在多个可用构造函数，语义分析使用小型重载规则选择候选：

1. 参数数量必须一致。
2. 完全类型匹配优先于需要隐式转换的匹配。
3. 如果最高优先级仍有多个候选，报二义性错误。

这个规则只用于结构体初始化选择构造函数。当前构造函数选择要求调用点实参数量与构造函数参数数量完全相同，不使用函数参数默认值补齐构造调用。不支持 C++ 完整重载体系，例如初始化列表优先级、用户自定义转换和模板偏序。

除构造函数和 operator 特殊项外，不支持重载：

- 普通函数同名即冲突。
- 成员函数同名即冲突。
- 关联函数同名即冲突。
- 析构函数不能重载，一个结构体最多一个析构函数。
- operator 的重载规则见 [operator.md](operator.md)，它不表示普通函数系统支持重载。

## 字段访问和隐式成员查找

显式字段访问写作：

```cp
self.x
point.x
```

在成员函数和析构函数中，普通名字也可以查找 `self` 的字段：

```cp
length(self const&) -> f64 {
    return sqrt(x * x + y * y);
}
```

名字解析顺序：

1. 从内到外查找普通词法作用域中的名字，包括局部变量和函数参数。
2. 如果当前函数有 `self`，查找 `self` 的字段。
3. 当前模块和导入模块中可见的顶层名字。

如果局部名字和字段同名，局部名字优先。此时必须写 `self.x` 才能访问字段：

```cp
set_x(self&, x: f64) {
    self.x = x;
}
```

字段赋值必须满足现有赋值规则。`self const&` 下字段视为 const，不能赋值。

## 块表达式

普通 `{ ... }` 可以在表达式位置作为块表达式：

```cp
let value = {
    let a = 1;
    let b = 2;
    a + b
};
```

块表达式规则：

- 块表达式创建新作用域。
- 最后一项如果是没有分号的表达式，块表达式类型就是该表达式类型。
- 没有尾表达式，或尾表达式后有分号，块表达式类型为内部 `unit`。
- 块表达式内部可以包含普通语句。
- `return` 仍然从所在函数返回，不是从块表达式返回。

lambda 的 `{ ... }` body 也复用尾表达式规则，但 lambda 自身形成新的函数边界；其中的 `return` 返回当前 lambda。详见 [lambda.md](lambda.md)。

普通块表达式和语句块按上下文区分：

```cp
{
    let x = 1;
}

let x = {
    1
};
```

第一段是语句块，第二段是块表达式。裸 `{}` 不表示默认初始化；默认初始化必须写作 `Type{}`，详见 [initial.md](initial.md)。
