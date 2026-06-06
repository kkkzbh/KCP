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
                | TypeAlias

Constructor     -> TypeName ( ParameterList? ) FunctionBody
                | TypeName ( ) = default ;
                | TypeName ( ParameterList? ) = delete ;
Destructor      -> ~ TypeName ( ) FunctionBody
Function        -> identifier ( ParameterList? ) ReturnType? FunctionBody
                | identifier ( ParameterList? ) = delete ;
Operator        -> operator OverloadableOperator ( ParameterList? ) ReturnType? FunctionBody
                | operator <=> ( self const& , rhs : this const& ) -> weak_ordering = default ;
                | operator OverloadableOperator ( ParameterList? ) ReturnType? = delete ;
TypeAlias       -> type identifier = Type ;

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
- 字段缺省表达式按字段声明类型检查。检查上下文是声明所在源码单元的模块级上下文；泛型结构体的类型参数可见，当前可见的顶层函数、类型和导入名字可见。
- 字段缺省表达式在结构体声明语义检查阶段就必须合法；不会等到某个 `type_name{}` / `type_name{ .field = ... }` 调用点实际省略该字段时才检查。因此即使所有初始化表达式都显式给出这个字段，非法默认表达式仍会让结构体声明报错。
- 字段缺省表达式没有 `self`，也没有构造函数或普通函数局部作用域；不能直接引用同一个结构体的其它字段名、`self.field`、函数参数或局部变量。
- 字段缺省表达式在每次实际填充省略字段时求值；它不是编译期常量，也不是所有实例共享的缓存值。
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

`impl` 把构造函数、析构函数、成员函数、关联函数、operator 和关联类型别名挂到某个类型下。当前实现支持的固有 `impl` 目标包括 `struct`、`variant`、opaque alias、内建类型和数组类型；不同目标可声明的项不同：

- `struct` 可以声明构造函数、析构函数、成员函数、关联函数、关联类型别名和 operator。
- `variant` 可以声明成员函数、关联函数、关联类型别名和 operator，不能声明构造函数或析构函数。
- opaque alias 可以声明成员函数、关联函数和关联类型别名；当前实现不把 opaque alias 的 `impl operator` 注册为可用 operator。
- 内建类型和数组类型的 `impl` 函数必须带 `self` receiver，因此只能作为扩展成员函数使用，不能声明关联函数、构造函数或析构函数；关联类型别名仍按目标类型收集。
- 内建类型可以声明扩展 operator，按模块导入/导出规则可见；数组类型当前不提供可用的扩展 operator 注册路径。

因此下面这些是当前可用能力：

```cp
impl i32 {
    double(self const&) -> i32
    {
        return self + self;
    }
}

impl [T; N] {
    first(self const&) -> T
    {
        return self[0];
    }
}
```

而这些不是当前可用能力：

```cp
impl [i32; 2] {
    size() -> i32 { return 2; }               // 错误：数组 impl 函数必须带 self receiver
    operator +(self const&, rhs: this const&) -> this = delete; // 不注册为可用 operator
}

impl i32 {
    make() -> i32 { return 0; }               // 错误：内建类型 impl 函数必须带 self receiver
}
```

其它类型不能作为固有 `impl` 目标。特别是 `storage T` / `storage [T; N]` 虽然是可作为值存放的 raw storage 类型，但不能声明 `impl storage T { ... }`；tuple、指针、引用、函数类型和函数指针同样不能被扩展。`impl i32* { ... }` 这类非法目标形状报 `unknown_type`。需要给这些形状提供方法时，应包一层 `struct`，或对内建/数组这类已允许目标使用模块 extension method。

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

`impl` 的物理文件位置不重要。只要 `impl` 所在文件参与同一次编译输入，且该位置能解析到目标类型，`impl` 中可用的成员函数、关联函数和已注册 operator 就挂到对应类型下。`struct` 和 `variant` 的固有成员及已注册 operator 跟随类型本身；opaque alias 的成员函数和关联函数也跟随类型本身，但它的 `impl operator` 当前不会注册为可用 operator。导入方只要能看见该导出类型，就能使用挂在该类型上的这些固有项；语言里没有 `export impl` 或“只导出部分方法”的机制。

内建类型和数组类型的扩展成员函数、内建类型的扩展 operator 是另一条路径：它们没有用户声明的导出类型可依附，必须由定义模块导出并被使用处 `import` 或经 `export import` 重导出后才参与查找。未导出的扩展项只在定义模块内部可见。构造函数和析构函数只适用于 `struct` 目标。

固有 `impl` 中的 `type name = Type;` 声明进入目标类型的扁平关联类型命名空间，可在后续类型位置写作 `Target::name` 或在同一 impl 函数签名/函数体类型位置中直接使用。它不是泛型类型构造器，不能写 `Target::name<T>` 或 `name<T>`。同一目标类型上重复关联类型名会报 `duplicate_symbol`；关联类型名也会阻止同名成员函数或关联函数声明。这个冲突检查跨多个 `impl` 块生效，后写的同名函数不会覆盖或隐藏已有的关联类型。裸 `type name;` 只用于 concept 关联类型要求，在固有 `impl` 中会报 `expected_type`。

`Target::name` 的 `Target` 必须是类型语法中的命名类型路径，而不是任意类型表达式后缀。因此内建类型可以直接写 `i32::item`；数组目标如果需要从外部引用关联类型，应先给数组类型取别名，例如 `type pair = [i32; 2]; impl pair { type item = i32; }` 后写 `pair::item`。当前语法不支持直接写 `[i32; 2]::item`、`(i32, bool)::item`、`i32*::item` 这类 raw type owner。

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
- 如果构造函数写了 `-> R`，当前语义层报 `invalid_constructor`；构造函数返回类型始终由当前 `impl` 目标决定。
- 构造函数返回类型固定为当前 `impl` 的结构体类型，不参与普通函数返回类型推导。
- 构造函数不能声明 `self` 参数。
- 构造函数体是普通函数体，使用普通 `return value;` 返回构造出的值。
- 构造函数里的所有带值 `return` 都必须能转换到当前结构体类型。
- `return;` 不允许用于构造函数。
- 构造函数允许重载，多个构造函数可以有不同参数列表。

`Type{...}` 对 `struct` 的位置初始化先尝试匹配构造函数，再考虑字段 aggregate 初始化：

- 只有全位置项初始化器会参与构造函数匹配，例如 `point{1, 2}`。命名字段初始化器 `point{ .x = 1, .y = 2 }` 不查构造函数，只按字段名检查。
- 构造函数匹配要求实参数量完全相同，并按普通参数转换规则评分。没有可行构造函数时，位置项才退回到字段声明顺序的 aggregate 初始化，省略字段按字段缺省表达式或默认初始化补齐。
- 一旦有可行构造函数，`Type{...}` 就按构造函数调用处理，不再同时作为字段 aggregate 初始化。选中的构造函数如果是 `= delete;`，调用点报 `not_callable`，也不会因为字段数量和类型刚好匹配而退回 aggregate 初始化。
- 如果多个构造函数最高评分相同，会报构造函数二义性；当前错误恢复仍选取其中一个候选继续记录表达式类型，但合法源码必须消除二义性。

这个规则在构造函数体内也不特殊处理。也就是说，`impl box { box(value: i32) { return box{ value }; } }` 里的 `box{ value }` 仍会先匹配当前这个单参数构造函数，而不是自动变成字段聚合；运行时会递归调用构造函数。构造函数要返回按字段写好的对象时，应使用命名字段初始化 `box{ .value = value }`，或显式调用另一个不会递归回来的构造 / 工厂函数。

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
- `type_name(value: T) = default;` 这类带参数 default 构造声明报 `invalid_constructor`。
- 稳定公开写法只能是当前类型名对应的 `type_name() = default;`。不要把 `= default` 写在普通成员函数或关联函数上；当前语义层把 struct impl 中的零参数 `= default;` 项当作默认构造标记处理，不把它作为普通函数的默认函数体能力。
- 当前语义层按 `= default;` 标记识别默认构造项，而不是重新要求源码名字等于当前类型名。因此 `impl vec2 { make() = default; }` 这类非推荐写法会被归类成一个 defaulted constructor 符号，并进入构造候选，而不是创建名为 `make` 的可调用关联函数。公开代码不应依赖这个实现宽松度；后续收紧为只接受 `vec2() = default;` 不应破坏语言设计。
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
- 返回类型必须显式写出，并且必须能隐式转换到当前语义输入中解析到的 `weak_ordering`；这不表示可以公开返回 `strong_ordering`、`partial_ordering` 或用户自定义比较分类。
- 行为是按字段声明顺序逐字段调用 `<=>`，把每个字段比较结果归一成 `weak_ordering`；第一个非 `equivalent` 结果立即返回，全部等价则返回 `weak_ordering::equivalent`。
- 每个字段类型都必须支持可用 `<=>`。字段比较结果如果已经能隐式转换为 `weak_ordering`，直接使用；否则必须提供可用的 `to_weak()` 成员方法，且该方法返回 `weak_ordering`。需要跳过字段、改变排序顺序或处理资源句柄时，应手写 `operator <=>`。

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
- 析构函数没有 `= default;` / `= delete;` 的 bodyless 形式；需要 no-op 析构时写空函数体，或不声明析构函数。
- 析构函数名必须匹配当前 `impl` 目标；`impl item { ~other() { } }` 报 `invalid_destructor`。
- 析构函数带参数或显式返回类型也报 `invalid_destructor`。
- 析构函数不能使用 `return value;`。
- 析构函数体是普通语句块。
- 析构函数中存在一个隐式 `self&`。
- 析构函数中可以直接访问字段，也可以写 `self.field`。
- 一个结构体最多声明一个析构函数；重复声明报 `duplicate_symbol`。

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
- 当前没有为无用户析构函数的 `struct` 生成可调用析构函数符号；这类类型在清理路径上等价于 no-op。用户写了 `~T()` 后使用用户版本。
- 当前不会为 copy / move 构造合成可枚举的默认构造函数符号。按值初始化、传参、返回和 range-for 按值绑定中，若存在匹配的非 deleted 用户 copy / move 构造函数，当前会使用它；没有可用用户构造函数时走内建同类型值写入。deleted copy / move 构造函数只稳定禁止显式构造候选，不能当作完整 move-only 类型系统。
- copy / move 赋值先查找用户 `operator =`；没有可用用户赋值 operator 时，当前退回到内建赋值。它不是可枚举的默认 assignment 符号，也不会逐字段调用用户自定义 assignment。
- `= delete` 的 copy / move 构造函数或赋值 operator 会保留一个不可调用候选。调用点选中它时报错；没有匹配 deleted 候选时，编译器不会因为某个字段“不可 copy / move”而递归隐式删除整个 `struct` 的内建同类型写入路径。

## Deleted impl 函数

除了特殊成员，`impl` 中的普通成员函数和关联函数也可以声明为 `= delete;`：

```cp
struct box {
    value: i32;
}

impl box {
    reset(self&) = delete;
    make_disabled() = delete;
}
```

当前语法只支持无函数体、无返回类型标注的 bodyless 形式 `name(parameters...) = delete;`。`=` 必须紧跟参数列表之后；`reset(self&) -> void = delete;` 不是普通 deleted impl 函数语法。不要把普通 deleted impl 函数上的泛型参数或 `requires` 当作公开能力；`reset<T>(self&) = delete;`、`reset(self&) requires T: concept = delete;` 这类形状不应在公共代码中使用。deleted operator 是另一条语法，允许显式写 `-> R = delete;`，见 [operator.md](operator.md#deleted-operator)。普通 deleted impl 函数会创建一个普通函数符号并占用对应成员名或关联函数名，但没有函数体，调用点选中它时只能报不可调用。因为普通成员函数和关联函数当前不支持重载，deleted 普通函数不是“从重载集中移除某个候选”，而是把这个名字声明为存在但不可调用。

调用解析如果选中了 deleted 成员函数或关联函数，语义检查报 `not_callable`。deleted 普通函数仍保留自己的 receiver 和参数列表：调用点仍会按普通方法 / 关联函数规则检查接收者、实参数量、默认参数表达式和实参类型，用于报告签名相关错误和后续错误恢复；它不会因为 deleted 就从名字查找结果中移除，也不会继续尝试同名 variant case、自由函数或其它 fallback。构造函数的 deleted 规则见上面的特殊成员说明；如果构造表达式匹配到 deleted 构造函数，不会继续回退到字段聚合初始化。析构函数当前不能写成 `= delete;`，concept requirement/default function 也不使用这套 deleted impl 函数语法。

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

同名成员函数当前会保存在同一个方法候选集中，而不是在声明阶段按名字直接报重复。调用时只用 receiver 形态做第一轮选择：`self&`、`self const&`、`self like&`、`self move&` 和泛型 receiver pattern 都可以形成同名候选。可写左值同时能绑定到 `self&` 和 `self const&` 时，可写 receiver 优先；const receiver 只能选 const / like 等可绑定候选；临时值或 `move value` 按移动 receiver 规则匹配。若最高优先级仍有多个同分候选，按方法选择二义性报错。这个同名候选能力只用于成员函数 receiver 选择，不表示普通函数或关联函数支持按参数列表重载。

### 成员和关联函数默认参数

成员函数和关联函数可以给尾部普通参数提供默认值：

```cp
impl counter {
    advance(self&, count: usize = 1 as usize) -> void {
        // ...
    }

    make(value: i32 = 0) -> counter {
        return counter{ .value = value };
    }
}

value.advance();
let zero = counter::make();
```

默认参数规则沿用普通函数：

- 只有尾部参数可以带默认值；带默认值的参数后面不能再跟无默认值参数。
- 默认表达式按对应参数类型检查，并在调用点 materialize 成真实实参。
- `self` receiver 不能带默认值；`advance(self& = ..., count: usize)` 报 `invalid_self_parameter`。
- 省略类型的推断参数不能带默认值；`make(value = 0)` 不是公开能力。
- 值参数包不能带默认值；参数包为空来自调用点没有剩余实参，不来自默认参数机制。
- lambda 参数默认值不受支持，即使 parser 能读入，也会在语义阶段报错；闭包值调用也必须按函数类型提供完整实参数量。

成员函数调用时，receiver 不属于调用点可省略的默认参数范围。`value.advance()` 省略的是 `count`，不是 `self`；UFCS fallback `value.free_method()` 也是把 `value` 当作自由函数第一个实参后，再从后续尾部参数补默认值。关联函数没有 receiver，按普通函数符号从第一个参数开始计算可省略尾部默认值。

构造函数不使用这套默认参数补齐。`box{}`、`box{1}` 等花括号初始化在选择构造函数时要求实参数量与构造函数参数数量完全相同；构造函数参数写了默认值也不会让较短的构造表达式匹配到它。需要可省略的构造逻辑时，应显式提供零参构造函数、字段默认值，或使用带默认参数的关联工厂函数。

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

点号 UFCS 中的 `object` 是自由函数调用的第一个实参，而不是必须与第一个形参完全同型的特殊 receiver。它按普通实参转换规则检查，因此可以使用当前允许的隐式转换，例如 `i32` receiver 调用 `wide(value: i64)` 时插入整数转换。自由函数的默认参数也按普通函数规则处理，但 `object` 已经占用了第一个实参位置；`object.name()` 可以省略的是自由函数签名中 receiver 之后的尾部默认实参。

UFCS 只作用于调用表达式。单独的 `object.name` 不会改写成 `name(object)`，也不会把成员函数或自由函数暴露成可传递的函数值；它按普通字段访问检查，只能命中 tuple 字段、`str` 的内建字段或 `struct` 字段，失败时报告字段 / 成员访问错误。

普通调用 `name(args...)` 的解析顺序：

1. 先在当前可见自由函数中查找 `name`。
2. 如果存在同名自由函数，直接按普通函数调用检查参数和返回类型。检查失败时报错，不继续回退。
3. 如果不存在同名自由函数，且当前位于成员函数或析构函数体内，再在 `self` 的类型上查找成员函数 `name`。
4. 如果存在 `self` 成员函数 `name`，按 `self.name(args...)` 检查。检查失败时报错，不继续回退。
5. 如果 `self` 上也不存在同名成员函数，且调用至少有一个参数，再在第一个参数 `first` 的类型上查找成员函数 `name`。
6. 如果存在成员函数 `name`，按 `first.name(rest...)` 检查。

这里的“当前可见自由函数”遵循 [module.md](module.md) 的模块可见性规则，包括当前模块内声明和 `import` 引入的导出函数。没有函数重载；因此一旦首选路径中存在同名函数，就视为用户明确选择了该名字，参数不匹配时应报告错误，而不是换到另一条 UFCS 路径。点号调用进入自由函数 fallback 后也是同样规则：`object.name(args...)` 若按 `name(object, args...)` 找到可见自由函数，参数数量、receiver 转换、默认参数或泛型实参检查失败时停在这条 UFCS 函数路径，不再报告未知成员或尝试其它候选。

普通调用的 UFCS 回退只在裸 `name` 没有解析成普通值或函数符号时触发。局部变量、参数、捕获变量或其它值名会先遮蔽同名自由函数和隐式 `self` 成员调用；遮蔽后 `name(args...)` 按“这个值是否可调用”检查，失败时报 `not_callable`，不会再退回到 `self.name(args...)` 或首参 UFCS：

```cp
impl item {
    reset(self&) -> void
    {
    }

    test(self&) -> void
    {
        let reset = 1;
        reset(); // 错误：局部值 reset 不可调用，不会改写成 self.reset()
    }
}
```

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
直接报告该成员调用错误，不继续尝试首参 UFCS。普通调用走到首参 UFCS 后也不再做其它回退：
只要第一个实参类型上存在可用成员名，就按 `first.name(rest...)` 的成员调用规则检查；receiver、
参数、默认参数或显式泛型实参失败时直接报告成员调用错误。需要调用其它对象的同名成员时应显式写
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

`type_name::name` 这种不带调用括号的 associated name 不是关联函数值。对 `struct` 和
opaque alias 来说，关联函数只能通过 `type_name::name(...)` 调用；单独写 `type_name::name`
会按 associated-name 表达式检查并报错，不会把同名关联函数当作可传递的函数对象。`variant`
有额外的 case 构造器规则：不带括号的 `Type::case` 只表示 unit case，带括号的
`Type::case(...)` 先尝试 case 构造器再查找关联函数；详见 [variant.md](variant.md)。

关联函数调用的当前检查边界：

- `Type::name(...)` 的 `Type` 必须解析为 `struct`、`variant` 或 opaque alias。内建类型、数组、tuple、指针、引用、函数类型、函数指针和 `enum` 都没有普通 associated-call surface；这些类型即使能出现在某些 `impl` 目标位置，也只能通过允许的成员调用、自由函数或其它专门语法使用。
- `::` 左侧必须是类型名语法，不是任意表达式。`vec2::zero()` 可用；`make_vec2()::zero()`、`value::zero()`、`(vec2)::zero()` 或 `alias.value::zero()` 都不会被解析成关联函数访问。需要基于值调用时使用成员函数 `value.method(...)` 或自由函数。
- 只有泛型关联函数接受显式类型实参。`Type::make<i32>(...)` 会实例化泛型关联函数；对非泛型关联函数写类型实参会报 `invalid_type_argument`，不会改走其它同名查找路径。
- 关联函数按普通函数符号处理尾部默认参数；调用点可以省略尾部默认实参，默认表达式在调用点 materialize。默认参数不把关联函数变成函数值，也不改变函数类型签名。
- 如果查找到的关联函数是 `= delete`，调用会报 `not_callable`，不会继续尝试同名 variant case、自由函数或其它 fallback。

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
- 不能混合命名字段初始化和顺序初始化；`.field = value` 不能跟在顺序实参之后，顺序实参也不能跟在命名字段之后。这个限制在 parser 阶段执行：一旦在同一个 `Type{...}` 中遇到混合形式，会报 `unexpected_token` 并且不产生可供语义层继续做构造函数匹配或字段聚合 fallback 的完整初始化表达式。
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

当前实现为了先选择构造函数，会在进入字段聚合 fallback 前先用“无 expected type”的方式检查每个位置实参。因此，顺序初始化里的位置实参不能依赖字段类型才成立。典型例子是空数组字面量：

```cp
struct box {
    xs: [i32; 0];
}

let a = box{ [] };       // 错误：[] 在构造函数候选检查阶段没有上下文类型
let b = box{ .xs = [] }; // 合法：命名字段初始化直接用 xs 的字段类型检查
```

非空数组字面量、普通数值字面量、已经写出类型的构造表达式等能自行得到类型的表达式仍可用于顺序初始化。需要字段上下文才能判断的表达式，应使用命名字段初始化，或先把值放到有显式类型的局部变量中。

本语言没有 C++ `initializer_list` 特权。`vector{ n, 0 }` 不会被隐式解释为元素列表；它只表示构造函数调用或顺序聚合。元素列表应使用数组字面量和显式关联函数，例如 `vector::from_array([10, 0])` 或 `vector::filled(n, 0)`。

### 默认初始化

`type_name{}` 是结构体默认初始化表达式：

```cp
let a = vec2{};
```

解析顺序：

1. 优先匹配零参数构造函数，包括显式 `type_name() = default;`。
2. 没有零参数构造函数匹配时，按字段默认初始化。

这里的“零参数构造函数”按构造函数签名的实际参数数量判断，不使用参数默认值补齐。因此 `impl point { point(x: i32 = 0) { ... } }` 不会让 `point{}` 走这个构造函数；`point{}` 会继续按字段默认初始化 fallback 检查。需要让空初始化调用构造逻辑时，应显式声明 `point()` 或 `point() = default;`。

按字段默认初始化时，字段声明上的 `= Expression` 优先于字段类型默认初始化：

```cp
struct production {
    lhs: nonterminal_kind = nonterminal_kind::augmented;
    rhs: vector<grammar_symbol> = vector<grammar_symbol>{};
}
```

字段默认值不只用于空的 `type_name{}`。顺序聚合初始化和命名聚合初始化缺少某个字段时，也按同一规则补齐缺失字段：先看字段声明上的默认表达式，再看字段类型能否默认初始化。因此 `point{ .y = 5 }` 可以省略带默认值或可默认初始化的 `x`，`point{ 3 }` 可以省略尾部的 `y`；如果某个缺省字段既没有字段缺省表达式，字段类型又不可默认初始化，初始化失败并报告错误。

字段默认表达式在结构体声明语义检查阶段按字段类型检查。对于泛型结构体，这个检查发生在声明的泛型参数作用域中；实际聚合初始化缺字段时会按当前结构体实例的类型实参替换字段类型，再决定缺失字段是否可由默认表达式或字段类型默认初始化补齐。默认初始化结果由 [type_system.md](type_system.md) 定义。

### 构造函数候选选择

如果顺序初始化同时存在多个可用构造函数，语义分析使用小型重载规则选择候选：

1. 参数数量必须一致。
2. 完全类型匹配优先于需要隐式转换的匹配。
3. 如果最高优先级仍有多个候选，报二义性错误。

这个规则只用于结构体初始化选择构造函数。当前构造函数选择要求调用点实参数量与构造函数参数数量完全相同，不使用函数参数默认值补齐构造调用；成员函数和关联函数默认参数规则见上文。不支持 C++ 完整重载体系，例如初始化列表优先级、用户自定义转换和模板偏序。

除构造函数和 operator 特殊项外，不支持重载：

- 普通函数同名即冲突。
- 成员函数可以有同名候选，但当前选择只围绕 receiver 形态展开；不要把它理解成完整参数列表重载。
- 关联函数同名即冲突，不能靠参数数量、参数类型、默认参数或泛型实参重载。
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

显式 `object.field` 字段访问只对结构体字段成立；`str` 的 `ptr` / `len` 和 tuple 的 `.0` / `.1` 是类型系统里的专门内建路径，见 [type_system.md](type_system.md)。如果 `object` 不是结构体、`str` 或 tuple 的对应专门路径，点号字段访问会报错；它不会把任意值自动当成有动态成员的对象。

字段结果的左值性按当前字段类型和对象表达式决定：

- 非引用字段只有在 `object` 本身是左值时才是左值；`make_point().x = 1` 这类对临时结构体普通字段赋值不是当前稳定能力。
- 若字段类型本身是引用类型，例如 `value: i32&`，字段访问保留这个引用字段的左值性。即使外层结构体值来自函数返回，只要引用目标可写，`make_holder().value = 1` 也按引用目标写入检查。
- 若字段类型是 `T const&` 或引用目标带 const，字段访问结果只读；若字段不是引用，则只读性来自 `object` 本身是否是 const。

## 块表达式

块表达式、`return` 和正常完成判断的主规则见 [控制流](flow.md#函数返回和正常完成)。本节保留与结构体字段 / 成员访问同页的历史说明，方便从字段访问规则继续阅读块表达式的基础形态。

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
- 如果尾表达式之前的语句保证不能正常完成，块表达式整体类型为 `!`。典型例子是前序语句已经 `return`、`break`、`continue`，或是表达式语句 / 声明初始化器本身类型为 `!`。

lambda 的 `{ ... }` body 也复用尾表达式规则，但 lambda 自身形成新的函数边界；其中的 `return` 返回当前 lambda。详见 [lambda.md](lambda.md)。

```cp
from_block(flag: bool)
{
    let ignored: i32 = {
        if(flag) {
            return 1;
        } else {
            return 2;
        }

        0 // 仍会被解析和类型检查；但前面的 if 已经让块不能正常到达这里，块整体类型是 !
    };
}
```

当前实现的正常完成判断是语义级、保守的控制流形状判断：

- 普通表达式语句和声明初始化器如果类型为 `!`，则该语句不能正常完成，例如 `panic("bad");` 或 `let x = panic("bad");`。
- `return` 不能正常完成当前函数；在块表达式中写 `return value;` 会把 `value` 贡献给所在函数的返回类型推导。
- `break` 和 `continue` 不能正常完成当前块；它们只允许出现在循环内部。
- `if` 有 `else` 且两个分支都不能正常完成时，整个 `if` 不能正常完成；没有 `else` 的 `if` 仍可能跳过分支后继续。
- `match` 作为表达式时，如果所有 arm 值都不能正常完成，整个 `match` 表达式不能正常完成；只要存在一个能正常完成的 arm，`match` 表达式仍可能正常完成。
- `while`、`do while`、范围 `for` 和 `template for` 当前按“可能正常完成”处理；编译器不把 `while(true)` 当成证明无后续路径的无限循环。
- `template if` 只按当前实例选中的分支判断；未选中的分支不参与语义检查和正常完成判断。

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
