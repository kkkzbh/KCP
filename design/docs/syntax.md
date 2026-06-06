# 语法导览

这页按学习顺序解释 KCP 程序的主要语法。需要查完整规则时，再进入对应参考页。本页示例只展示当前编译器已经支持的主路径；函数、成员函数和关联函数的尾部默认参数是当前公开能力，构造函数、operator 和 lambda 调用不使用默认参数补齐，具体边界见
[generic.md](generic.md#函数参数默认值) 和 [operator.md](operator.md#函数调用)。namespace、类继承、trait object、宏、异步、用户 `union`、`defer` 和全局变量都不是当前公开语法。

## 空白和注释

空格、tab、换行、`\r`、`\v` 和 `\f` 都按空白处理。换行只影响诊断行列和 token 的行首标记，不会像 Python 那样结束语句；语句结束仍然由语法中的 `;`、`}` 或特定结构决定。

注释支持两种 C/C++ 风格：

```cp
// line comment
/* block comment */
```

注释在预处理阶段被替换为等长空白，换行原样保留，因此注释不会产生 token，也不会改变后续 token 的源码 span。源码开头可以写注释再写 `export module`，因为注释不算第一个 token；但空语句 `;` 会产生 parser 诊断，不等同于注释。

块注释以第一个 `*/` 结束，当前不支持嵌套块注释。未闭合块注释会产生 `unterminated_block_comment` 预处理诊断。字符串和字符字面量中的 `//`、`/*`、`*/` 只是字面量内容，不会开始或结束注释。

预处理阶段不支持 C/C++ 风格反斜杠换行拼接。`//` 行注释只延续到源码中的实际换行；字符串和字符字面量也不能通过行尾 `\` 跨行，换行会让后续 lexer 按未闭合字面量恢复。

## 字面量

整数 literal 只支持十进制数字序列。单个 `0` 合法，但多位整数不能以 `0` 开头；`012` 会产生 `leading_zero_integer` 诊断。整数 literal 后不能紧跟标识符起始字符；`12abc` 会被当作带非法后缀的数字并产生 `invalid_number_suffix`，不是整数 `12` 后接标识符 `abc`。

浮点 literal 使用十进制小数点或指数形式：`0.5`、`1.`、`0e3`、`2.5e+1`、`3E8` 和 `10e-23` 都是当前 lexer 接受的浮点 token。指数标记 `e` / `E` 后可以写 `+` 或 `-`，但之后必须至少有一个数字；否则会产生 `missing_exponent_digits`。小数点只有在点号后面不是标识符起始字符时才并入浮点 token，因此 `1.foo` 会先产生整数 `1`，再按点号成员访问继续解析。

数字 literal 没有源码级类型后缀、分隔符或进制前缀：`42u`、`1_i32`、`1_000`、`0x10`、`0b1010` 和 `0o77` 都不是当前合法数字写法，通常会按数字后接非法后缀恢复。前导 `+` / `-` 也不是 literal 的一部分，而是普通一元运算符；`-1` 在语法上是对整数 literal `1` 取负，`-128 as i8` 这类边界按一元表达式和后续转换规则处理。

字符串 literal 用双引号，字符 literal 用单引号。两者都不能跨越换行；未闭合时分别产生 `unterminated_string_literal` 或 `unterminated_char_literal`。lexer 接受的转义拼写包括 `\\`、`\'`、`\"`、`\?`、`\a`、`\b`、`\f`、`\n`、`\r`、`\t`、`\v`、一到三位八进制转义，以及 `\x` 后跟至少一位十六进制数字的十六进制转义；`\x` 后没有十六进制数字或未知转义会产生 `invalid_escape_sequence`。字符 literal 必须正好包含一个字符或一个转义序列；`''` 和 `'ab'` 都会产生 `invalid_char_literal`。

当前语义层的 literal 值解码比 lexer 接受范围更窄：`'\n'` / `"\n"`、`'\r'` / `"\r"`、`'\t'` / `"\t"`、`'\0'` / `"\0"`、`'\\'` / `"\\"`、`'\''` / `"\'"` 和 `'\"'` / `"\""` 会按对应字符解码；其它 lexer 已接受的转义不会按 C 语义转成控制字符或码点。也就是说，`'\a'` 当前值是字符 `a`，`"\x41"` 当前字符串内容是 `x41`，不是 `A`。需要稳定字符值时，第一版应只依赖前述已解码的简单转义，数值转义和其它简单转义只视为 lexer 已接受但语义值尚未完整实现的边界。

字符串和字符 literal 也没有前缀或拼接语法。`r"..."`、`u8"..."`、`L'x'`、多行 raw string、相邻字符串自动拼接 `"a" "b"` 都不是当前公开能力；需要组合字符串时使用标准库 `string` 的构造和追加接口，或显式传递 `str` 片段。

## 最小程序

KCP 的可执行入口通常写成匿名模块中的顶层 `main() -> i32`。函数体使用花括号，返回类型放在参数列表之后。

```cp
main() -> i32
{
    return 0;
}
```

只有没有 `export module ...;` 头的匿名模块顶层自由函数 `main` 会作为可执行程序入口。具名模块里的 `main` 只是普通模块函数，例如 `export module app; export main() -> i32 { ... }` 不会自动成为 OS 入口；需要写独立可执行示例时，应把入口文件保持为匿名模块并通过 `import` 调用其它具名模块。

变量用 `let`，常量用 `const`。类型可以显式标注，也可以由初始化表达式推导。

```cp
main() -> i32
{
    const answer: i32 = 42;
    let ratio = 0.5;
    let value = (answer as f64 * ratio) as i32;
    return value;
}
```

声明必须有初始化表达式；需要默认值时写 `Type{}`。`nullptr` 需要显式指针上下文，不能单独推出局部变量类型。

```cp
main() -> i32
{
    type pair = (i32, bool);
    let item: pair = (1, true);
    let (count, ok) = item;
    let static calls = 0;

    calls += 1;
    return count;
}
```

`type name = Type;` 在函数体里是局部类型别名，只在后续当前块和子块可见。`let static` 是函数内跨调用保留状态的局部 binding，不是模块全局变量；它仍然必须写初始化表达式。

相关参考：[初始化](initial.md)、[类型系统](type_system.md)、[类型转换](cast.md)。

## 模块

一个文件可以声明命名模块，`export` 决定哪些声明对导入者可见。

```cp
export module sample.math;

export add(left: i32, right: i32) -> i32
{
    return left + right;
}
```

使用模块时通过 `import` 引入：

```cp
import sample.math;

main() -> i32
{
    return add(16, 26);
}
```

`import` 只能写完整模块名，并把目标模块导出的名字直接引入当前文件；当前没有 `import sample.math as m;`、`from sample.math import add;` 或
`sample.math::add(...)` 这类别名、选择性导入或模块限定调用语法。

模块声明只能写在文件第一个 token 位置，导入语句必须紧跟模块声明之后并形成连续 import list。顶层声明开始之后再写 `import`，或者在模块声明和 `import` 之间写空语句 `;`，都会按普通顶层语法错误恢复处理。

`export` 只在具名模块中修饰可导出的顶层声明，例如函数、operator、`struct`、`enum`、`variant`、`type`、`concept` 或 `import`。匿名模块中写 `export` 会报错；`impl` 和 `impl Concept for Type` 也不能写成 `export impl`。impl 成员、concept item、字段、variant case 等内部成员的可见性不由局部 `export` 修饰符控制，具体规则见模块参考页。

相关参考：[模块](module.md)。

## 表达式和控制流

KCP 使用 `if`、`while`、`do while` 和范围 `for`。布尔逻辑使用 `and`、`or`、`not`；C/C++ 风格的 `&&`、`||` 和 `!` 不是当前表达式语法中的逻辑运算符，也不能通过 operator 重载打开。需要短路逻辑时写关键字形式，具体规则见 [operator.md](operator.md#逻辑运算)。

当前没有 C/C++ 风格三目条件表达式。`?` 只是 lexer 能识别的标点 token，不是可用表达式语法；`condition ? a : b` 和裸 `?` 都会被 parser 拒绝。需要条件选择时写语句级 `if` / `else`，或用 `match` 表达式处理 `variant`。

单独的空语句 `;` 会被 parser 当作空语句诊断并丢弃。该诊断是 warning，不会像语法 error 一样让整次解析失败，但空语句不是推荐的占位语句能力，也不会产生 AST 语句节点。需要空块时写 `{}`，需要什么都不做的分支时保留一个空 block。

```cp
import std;

sum_to(end: i32) -> i32
{
    let total = 0;

    for(let value : iota(0, end)) {
        if(value == 3) {
            continue;
        }
        total += value;
    }

    return total;
}
```

范围 `for` 依赖 `iterable` / `const_iterable` 协议，标准库的 `iota` 提供最常见的半开范围。`.iter()` 返回的是内部游标，不能直接作为范围表达式。

相关参考：[流程控制](flow.md)、[迭代](iteration.md)、[标准库 ranges](std_ranges.md)。

## 结构体和 impl

`struct` 定义数据布局，`impl` 给类型挂接构造函数、成员函数、关联函数和运算符。

```cp
struct point {
    x: i32;
    y: i32;
}

impl point {
    point(x: i32, y: i32)
    {
        return point{ .x = x, .y = y };
    }

    len2(self const&) -> i32
    {
        return x * x + y * y;
    }
}

main() -> i32
{
    let p = point{ 3, 4 };
    return p.len2();
}
```

成员函数显式声明 `self` 的借用方式。`self const&` 表示只读借用，`self&` 表示可写借用。

相关参考：[结构](struct.md)、[所有权、借用与移动](ownership.md)。

## Enum、Variant 和 match

`enum` 表示带底层整数的强类型枚举。`variant` 表示名义类型，可以拥有空 case 或带 payload 的 case。

```cp
enum color : u8 {
    red = 1;
    green = 2;
    blue = 3;
}

variant result {
    value(i32);
    unexpected(str);
}

read_value(ok: bool) -> result
{
    if(ok) {
        return result::value(1);
    }
    return result::unexpected("failed");
}

main() -> i32
{
    match read_value(true) {
        .value(value) => {
            return value;
        },
        .unexpected(error) => {
            return 1;
        },
    };
}
```

`enum` case 用 `name = ConstIntegerExpression;`，每个 case 都必须显式写整数常量值，当前没有 C 风格自动编号，也不能根据前一个 case 推导下一个值。`variant` case 用 `name;` 或
`name(Type, ...);`，两者都用分号结束。`match` arm 才用逗号结束；当前 `match` 只匹配 `variant`，不匹配 `enum`。每个 arm 的 pattern 可以写
`.case`、`.case(binding, ...)` 或 `_`，pattern 里的 payload 绑定数量必须和对应 case payload 数量一致。

`match` 是表达式，必须覆盖所有 `variant` case，或者提供 `_` 通配 arm。每个 arm 的右侧也是表达式；需要多条语句时写块表达式。所有能正常完成的 arm
必须能落到同一个结果类型，`panic(...)` / `unreachable()` 这类 `!` arm 不单独决定结果类型。

相关参考：[Enum](enum.md)、[Variant](variant.md)、[错误处理](error_handling.md)。

## Lambda、函数值和可调用对象

函数类型写成 `f(Args...) -> R`，函数指针写成 `f*(Args...) -> R`。`f(...) => expr` 是表达式 lambda，`f(...) { ... }`
是块 lambda；没有捕获的非泛型 lambda 可以落到函数类型或函数指针，有捕获 lambda 是闭包对象，只能通过推导、泛型参数或 `operator ()`
这类可调用协议接收。

```cp
apply(value: i32, op: f(i32) -> i32) -> i32
{
    return op(value);
}

main() -> i32
{
    let inc: f(i32) -> i32 = f(value) => value + 1;
    let bias = 10;
    let add_bias = f(value: i32) -> i32 {
        return value + bias;
    };

    return apply(1, inc) + add_bias(31);
}
```

lambda 参数不能使用默认值。没有上下文函数类型时，非泛型 lambda 参数必须显式写类型；泛型 lambda 必须显式写返回类型，当前不对每个单态实例单独做返回类型推导。泛型 lambda 调用点可以显式写类型实参，也可以像泛型函数一样从普通调用实参推导；它不会从返回类型或变量声明目标类型反推类型实参。

相关参考：[Lambda 与函数值](lambda.md)、[元编程与反射基础](meta.md)、[运算符](operator.md)。

## 泛型和 Concept

泛型使用 `<...>`。Concept 描述静态协议，泛型参数可以用 concept 约束表达所需能力。

```cp
import std.compare;

same<T: equality_comparable<T>>(left: T, right: T) -> bool
{
    return left == right;
}
```

普通函数参数也可以省略类型，编译器会为每个省略类型的参数引入一个由调用实参推导的隐藏类型参数。一个参数列表里可以同时包含显式类型参数和省略类型参数；只有省略类型的参数会引入隐藏类型参数。需要借用形态时，把引用后缀留在参数名后：

```cp
read(value const&) -> i32
{
    return value;
}

add(left, right) -> i32
{
    return left + right;
}
```

同一个参数不能把省略类型的引用后缀和显式类型混写；`value&: i32` 应写成 `value: i32&` 或 `value&`。

参数包配合 `template for` 做编译期展开，`template if` 在具体泛型实例中选择一个分支：

```cp
count_values<T...>(values: T...) -> i32
{
    let count = 0;
    template for(let value : values...) {
        count += 1;
    }
    return count;
}
```

`template for` 和 `template if` 都是语句级编译期结构，不是运行时 range 循环或表达式。`template for(let value : values...)` 只能遍历当前函数、成员函数或 lambda 实例中的值参数包；`template for(type U : T...)` 只能遍历类型参数包。它们不能遍历数组、tuple、range、普通局部变量或 concept 参数包，也不能直接作为 `break` / `continue` 的目标。`template if` 的未选中分支不参与当前实例的语义检查和返回推导，但每个分支体仍必须写成块语句。

`impl Concept for Type` 给具体类型实现协议；标准库也可以提供由编译器识别的内建 concept。当前 parser 还保留一组按上下文识别的 identifier，只在对应语法位置有特殊含义：顶层 `variant` / `type` / `extern`，表达式开头的 `match`，类型位置的 `storage` / `decltype` / `f`，lambda 表达式开头的 `f`，`template for` / `template if`，函数、impl 和 concept impl 后的 `requires`，operator 声明里的 `prefix` / `postfix`，opaque alias 里的 `opaque`，局部声明里的 `static`，以及 `= default;` 函数体标记里的 `default`。这些词离开对应位置仍可作为普通名字；例如 `f(1)` 是调用名为 `f` 的值，`storage` 也可以作为普通变量名。`for`、`enum`、`impl`、`concept`、`import`、`module`、`move`、`forward` 和 `delete` 等是词法关键字，不能作为普通标识符使用。

标准库 concept 必须在当前模块可见才会参与约束检查。通常通过 `import std;`、`import std.compare;` 或 `import std.meta;` 引入
`equality_comparable`、`three_way_comparable`、`incrementable`、`callable` 等名字；这些不是未导入也全局可用的关键字。

相关参考：[泛型](generic.md)、[Concept](concept.md)、[元编程与反射基础](meta.md)。

## 所有权、借用和移动

KCP 默认按普通 copy 语义传值。需要借用时显式写 `ref` / `const ref`，需要转移所有权时显式写 `move`，需要在泛型中保留实参值类别时写 `forward`。

```cp
take(value: string move&) -> string
{
    return move value;
}

relay<T>(value: T forward&) -> T
{
    return forward value;
}

use_text(text: string const&) -> usize
{
    return text.size();
}
```

显式借用和移动让函数签名直接表达调用者需要保留、读取还是交出对象。

相关参考：[所有权、借用与移动](ownership.md)、[底层内存分配](memory_allocation.md)。

## 标准库和底层边界

标准库是普通 KCP 模块。`import std;` 会重导出核心类型、内存工具、collections、text、compare、ranges、meta、algorithm、io 和 fs；
也可以按需导入子模块。数组范围 `for` 是语言直接支持的路径；ranges pipeline、`vector`、`map`、`set`、`string`、`file` 等来自标准库。
这些名字不是未导入也可用的全局内建；未导入 `std` 或对应子模块时，`vector`、`iota`、`string`、`file`、`sort`、`println` 等都按普通未知名字或未知成员处理。

`std` 不是递归导入整个标准库源码树的魔法 prelude，只是一个显式 `export import` 若干公共模块的聚合入口。导入子模块时也只获得该模块和它源码里显式重导出的内容；
例如只写 `import std.ranges.terminals;` 可以看到 terminal 和基础 range source 协议，但看不到 `filter` / `transform` 等 adapters。当前也没有模块限定名字访问：
不能写 `std::vector`、`std.collections::vector` 或 `std.vector<i32>`；导入后直接按未限定名字查找。

```cp
import std;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(1);
    values.push_back(2);
    let count = iota(0, 4)
        .filter(f(value: i32) -> bool { return value != 2; })
        .count();
    return values[0 as usize] + count as i32;
}
```

底层互操作使用显式语法：`extern "C"` 声明 C ABI 函数，`type name = opaque T;` 声明名义封装，`alloc<T>` / `construct_at` /
`destroy_at` / `free` 管理裸存储生命周期。`extern "C"` 只用于顶层自由函数，ABI 边界只接受明确的标量、指针和函数指针形状；`str`、`struct`、`variant`、opaque alias
等按值传递不是当前公开能力，指针参数也只说明传地址，不证明 pointee 拥有 C-compatible layout。这些能力不会自动提供 RAII 容器语义；特别是 `variant` payload 当前还没有按 tag 自动析构分发。

相关参考：[标准库导览](stdlib.md)、[标准库 collections](std_collections.md)、[标准库 ranges](std_ranges.md)、[标准库 fs](std_fs.md)、[`extern "C"`](extern_c.md)、[Opaque alias](opaque_alias.md)、[底层内存分配](memory_allocation.md)。
