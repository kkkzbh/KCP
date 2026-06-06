# enum

本文档记录 KCP 第一版强类型 `enum`。`enum` 是单个整数命名值集合；`variant` 是 tagged union，见 [variant.md](variant.md)。

## 语法总览

```text
EnumDecl -> export? enum identifier : Type { EnumCase* }
EnumCase -> identifier = ConstIntegerExpression ;
```

示例：

```cp
export enum open_flag : u8 {
    read = 1 << 0;
    write = 1 << 1;
    create = 1 << 2;
}
```

case 声明之间使用分号结束，不使用逗号。`red = 1,` 这类写法会在 parser 阶段报错。每个 case 都必须显式写 `= 常量表达式`；第一版没有 C 风格的自动编号，也不会根据前一个 case 推导下一个整数值。因此 `first;`、`second = first + 1;` 和只写 `enum color : u8 { red; green; }` 都不是当前语法或语义能力。

enum case 名必须是裸 identifier，不能像 `variant` case 那样带 payload 或参数列表。`read() = 1;`、`write(u8) = 2;` 和 `item<T> = 3;` 都不是当前语法；parser 在 case 名后只接受 `=`。

## 语义

`enum` 接近现代 C++ `enum class`：

- case 必须通过作用域访问，例如 `open_flag::read`。
- `enum` 值不隐式转换为整数。
- 整数不隐式转换为 `enum`。
- `enum` 默认不支持普通 bitwise 组合；如果需要位集合，先显式转换到底层整数。
- 同一个 `enum` 类型支持内建 `==` / `!=`，结果为 `bool`。
- 同一个 `enum` 类型支持内建 `<=>`，按底层整数值比较并返回 `weak_ordering`。当前语义层按语义输入中名为 `weak_ordering` 的 `variant` 查找结果类型，不检查它是否来自标准库模块；公开代码应通过导入 `std` 或 `std.compare` 提供标准库的 `weak_ordering`。
- 同一个 `enum` 类型不内建 `<` / `<=` / `>` / `>=`。当前 `enum` 也不是可写 `impl` / `operator` overload 目标；需要这些关系时，使用 `<=>` 配合标准库比较 helper，或先显式转换到底层整数后比较。
- 因为 `==` / `!=` / `<=>` 是 enum 的语言内建能力，导入 `std.compare` 后，enum 可以满足编译器按名字识别的 `equality_comparable` / `three_way_comparable` 这类比较 concept。不能通过显式写 `impl equality_comparable for some_enum {}` 或 `impl some_concept for some_enum {}` 来补充、覆盖或伪造 enum 的 concept 证明。
- `struct` 的默认 `operator <=> = default` 可以比较 enum 字段，比较结果按 enum 底层整数值产生 `weak_ordering`；默认比较本身仍要求能看见 `weak_ordering`。
- 不同 `enum` 类型不互相比较，即使底层类型相同、case 值相同也不等价。
- 第一版必须显式写底层整数类型。
- `enum` 是顶层类型声明。它的名字进入当前模块的类型表，并且会和同模块中的顶层函数、`struct`、`variant`、type alias、opaque alias、concept 等顶层名字冲突。
- `export enum` 只允许出现在声明了 `export module name;` 的具名模块文件中；在非具名模块里写 `export enum` 会报 `export_requires_module`。
- 导入一个导出的 `enum` 时，导入的是 enum 类型本身；case 表随这个类型一起通过 `Enum::case` 使用。case 名不是单独的顶层导出符号，不会被 `import` 成裸名字，也不会和其它模块的顶层函数、类型或 concept 名做 import 冲突检查。
- 当前 `enum` 不能作为固有 `impl` 或普通 concept `impl` 的目标；不能给 `enum` 增加成员、关联函数或自定义运算符。
- `enum` 类型不接受泛型实参；`marker<i32>` 会报 `invalid_type_argument`。
- case 值必须是整数常量表达式，支持整数字面量、一元 `-` / `~`、整数算术、位运算和 `1 << 0` 这类表达式。
- case 名在同一个 `enum` 内不能重复。
- 空 `enum` 声明当前合法；它只建立一个没有公开值的名义类型，不自动产生默认值、零值或隐藏 case。
- 底层类型必须是整数类型，不能是 `bool`、浮点、指针、结构体或其它名义类型。
- 默认初始化 `open_flag{}` 不合法。

底层类型的检查发生在 enum 名字收集之后、case 收集阶段。`:` 后面的类型按普通类型规则解析，读出类型必须是内建整数。因此普通类型别名可以作为底层类型，而且可以写在同一文件中 enum 之后：

```cp
enum marker : raw {
    first = 1;
}

type raw = i32; // 合法：enum case 收集时 raw 已经进入类型别名表
```

这个宽松顺序只适用于 enum 底层类型、字段类型、函数签名等较晚解析的位置，不适用于顶层 `type` alias 右侧；alias 右侧的顺序边界见 [module.md](module.md)。底层类型检查不会穿透名义类型：`type bits = opaque u8; enum marker : bits { first = 1; }` 和 `enum raw : i32 { first = 1; } enum marker : raw { second = 2; }` 都会报“enum underlying type must be an integer type”。需要以已有 enum 或 opaque 值建模时，应显式转换到它们的底层整数或使用普通整数别名。

当前 case 值常量表达式只支持一小组整数运算：

- 整数字面量，使用当前 lexer 支持的普通十进制无后缀形式。除单独的 `0` 外，多位整数不能带前导零；`01` 会先按非法数字 token 诊断，而不是作为 case 常量值解析。`0x10`、`0b10`、`1u`、`1_i32` 和带分隔符的写法都不是可用的 enum case 整数字面量。
- 括号分组。
- 前缀一元 `-` 和 `~`。
- 二元 `+`、`-`、`*`、`/`、`%`、`&`、`|`、`^`、`<<` 和 `>>`。
- `/` 或 `%` 的右操作数为 0 时不是常量表达式。
- `<<` 或 `>>` 的右操作数为负数时不是常量表达式。
- 当前按有符号 64 位整数计算 case 值；`/` 和 `%` 是整数除法和取余，不产生浮点或有理数结果。移位只检查右操作数不能为负数，不检查移位位数是否小于底层类型宽度。
- case 值求值不是“普通表达式先类型检查，再按底层类型常量折叠”。它只接受上面列出的语法形状，不建立普通名字、类型或目标底层类型上下文；不要依赖超出 64 位范围或 lexer 异常整数 token 的行为来表达稳定 enum 值。

其它一元表达式不属于 enum case 常量表达式。即使普通表达式位置可以解析，`+1`、`not 0`、`&value`、`*ptr`、`move value`、`ref value`、`const value`、`++value`、`value++`、`--value` 和 `value--` 都不能作为 case 值。

不支持在 enum case 值中引用其它 case、变量、函数调用、cast、比较、逻辑运算、浮点字面量、字符字面量、字符串字面量或 `nullptr`。这些表达式会被当作“不是整数常量表达式”。

case 收集规则：

- case 只进入所属 enum 的 case 表，不作为普通顶层名字或局部名字导出；使用时必须写 `Enum::case`。
- case 按源码顺序收集。case 值无效或 case 名重复时，该 case 不加入 enum case 表；后续 case 仍继续检查。
- 如果 enum 没有 case，case 表保持为空；`Enum::anything` 都会按未知 enum case 报错。因为当前也没有整数到 enum 的公共反向转换和默认初始化，空 enum 只能作为类型出现在声明、签名、指针/引用等类型位置，不能在普通源码中直接构造出一个该 enum 值。
- 当前诊断种类复用 variant 的 `duplicate_variant_case` / `unknown_variant_case`，但语言含义分别是“同一个 enum 内 case 名重复”和“当前 enum 中没有这个 case”。
- `Enum::case` 的表达式类型是 `Enum` 本身，不是底层整数类型；需要底层整数时必须显式写 `as`。
- 当前语义层只检查 case 名是否重复，不检查不同 case 的整数值是否重复：

```cp
enum duplicated_value : i32 {
    first = 1;
    second = 1; // 当前允许
}
```

当前语义层也只把 case 值求成内部有符号整数，不按底层类型做范围检查。因此 `enum small : u8 { negative = -1; }`、`enum small : u8 { too_large = 300; }` 和 `enum small : u8 { shifted = 1 << 40; }` 目前不会在语义阶段因为越过 `u8` 范围而报错。这类越界值的运行时表示不应作为稳定语言能力使用；后续实现可以收紧为必须落在底层类型范围内。

`Enum::case` 可以作为编译期字面值参与当前 `template if` 的 constexpr 表达式折叠，因此这些写法可以用于 `template if`：

```cp
enum mode : i32 {
    fast = 1;
    slow = 2;
}

main() -> i32
{
    template if(mode::fast == mode::fast) {
        return 1;
    } else {
        return 0;
    }
}
```

这里的 constexpr 能力只来自具体 case 表达式，不表示 `enum` 类型本身可以作为整数类型使用。`template if(mode::fast)` 仍不合法，因为 template if 条件表达式必须是 `bool`；需要比较结果或显式转换后的整数逻辑。

省略返回类型时，不要依赖 enum case 条件排除未选中的 `template if` 分支。如果某个 `template if` 条件依赖 `Enum::case`，并且不同分支返回类型不同，应显式写出函数返回类型，或把条件改写成类型相等 / concept 约束这类返回推导能稳定处理的形式。

显式转换：

```cp
let raw: u8 = open_flag::read as u8;
```

第一版只允许 `enum_value as UnderlyingInteger`。反向整数到 enum 的构造暂不作为公共能力开放。

```cp
let raw: u8 = open_flag::read as u8; // 合法
let flag: open_flag = 1 as open_flag; // 不合法
```

转换目标必须恰好是该 enum 声明的底层类型。`enum flag : u8` 的值可以写 `flag::read as u8`，不能直接写 `flag::read as i32`；需要其它整数类型时应先转到底层类型，再按普通数值转换继续转换。

`enum` 值也不继承底层整数的位运算：

```cp
let both = open_flag::read | open_flag::write; // 不合法
let bits = (open_flag::read as u8) | (open_flag::write as u8); // 合法
```

case 访问使用 `Enum::case`，不是函数调用：

```cp
let read = open_flag::read;   // 合法
let bad = open_flag::read();  // 不合法：enum case 不是 associated function
```

`Enum::case()` 不会先解析成 case 再检查“空参数调用”。调用形态走 associated call 规则，而当前 associated call 只面向 `struct`、`variant` 和 `opaque` 类型；`enum` 没有 case 构造器调用形态，也没有可查找的 associated function surface，因此这类调用报 `unknown_member`。需要 enum 值时只能写不带括号的 `Enum::case`。

`Enum::missing` 报 `unknown_variant_case`。当前诊断分类复用 variant case 的诊断种类，但语言层含义是“当前 enum 中没有这个 case”。

`Enum::case` 只能在值表达式位置使用；在类型位置，`Enum` 是 enum 类型本身，case 名不会形成嵌套类型。`Enum::case` 也不是 associated function，不能传类型实参、不能调用默认参数、不能通过普通函数调用语法访问。

`enum` 当前没有构造函数语义，也没有 `Enum{...}` 聚合构造：

```cp
let zero = open_flag{};      // 不合法：enum 不支持默认初始化
let flag = open_flag{ 1 };   // 不合法：非 struct 的 Type{} 只能为空，而 enum 空初始化也没有公共语义
```

需要从已有 enum case 得到值时写 `open_flag::read`。需要从整数恢复 enum 值时，当前语言没有公共反向转换；应该保留整数值，或在 API 中显式建模为 `variant` / `struct` 校验结果。

`enum` 不是 `variant`，不能作为 `match` 的被匹配值：

```cp
match open_flag::read {
    .read => 1,
}
```

需要按 enum 值分支时，当前应使用 `if` / `else` 和 `==`，或者先显式转换到底层整数后使用普通整数逻辑；语言暂未提供面向 enum 的 pattern matching。

## 与 variant 的区别

`enum` 和 `variant` 都是名义类型，但表达不同问题：

- `enum`：运行时就是一个整数值，case 不带 payload。
- `variant`：运行时是 tag + payload 的 tagged union，每个 case 可以携带不同数据。

因此：

```cp
enum color : u8 {
    red = 1;
}

variant event {
    click(i32, i32);
    close;
}
```

`color::red` 是整数形态的名义值；`event::click(1, 2)` 是构造一个 tagged union 值。
