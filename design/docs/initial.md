# 初始化

本文档记录变量、常量和初始化表达式规则。类型推导、默认初始化结果和聚合字面量规则见 [type_system.md](type_system.md)，结构体初始化见 [struct.md](struct.md)。

## 变量声明

变量声明必须显式写初始化表达式：

```cp
let name: type = initializer;
let name = initializer;
let ref name = lvalue_initializer;
let (a, b) = tuple_initializer;
let ref (a, b) = tuple_lvalue_initializer;
let (a, b): TupleType = tuple_initializer;
type local_name = SomeType;
```

规则：

- 显式类型存在时，初始化表达式必须能转换到声明类型。
- 省略类型时，变量类型从初始化表达式推导。
- `ref` 声明要求初始化表达式是左值，并推导为引用类型。
- `let ref name = expr;` 会按 `expr` 的左值 constness 推导 `T&` 或 `T const&`。`const ref name = expr;` 总是得到只读引用。
- `ref` 声明也可以带显式类型；显式类型先作为 initializer 的上下文类型检查，随后 binding 仍按引用声明规则建立。也就是说，`let ref alias: i32 = value;` 的显式 `i32` 约束 initializer 必须能作为 `i32` 使用，最终 `alias` 仍是 `i32&` 或 `i32 const&`。常见写法也可以给引用类型取别名后使用：`type value_ref = decltype(ref value); let ref alias: value_ref = value;`。即使显式类型已经是引用类型，initializer 仍必须是左值；`let ref alias: i32& = 1 + 2;` 不合法。
- 省略类型时，`nullptr` 不能单独决定局部变量类型；必须写上下文指针类型，例如 `let p: i32* = nullptr;`。
- 省略类型时，内部 `unit` 表达式不能初始化局部变量；例如没有尾表达式的块、`return;` 风格的 `unit` 值都不能变成一个可命名局部。
- 元组解构声明必须面对 tuple 初始化器，binding 数量必须与 tuple 元素数量一致。可以在整个解构后写显式 tuple 类型，例如 `let (a, b): (i32, bool) = value;`；显式类型先作为 initializer 的上下文类型，再按该 tuple 的元素类型创建各个 binding。
- 如果解构 binding 数量和 tuple 元素数量不一致，当前语义层会报告 `aggregate_length_mismatch`，但错误恢复仍会按共同前缀创建已经能对应上的 binding，用于后续表达式继续检查。例如 `let (a, b, c) = (1, 2);` 中 `a` / `b` 可能在恢复路径中可见，`c` 不会绑定；这只是诊断恢复，不表示数量不匹配的解构合法，正确源码必须让两边数量完全一致。
- `let ref (a, b) = tuple_lvalue;` 要求初始化器是左值，并按每个元素的 constness 产生引用 binding。`const ref (a, b) = tuple_lvalue;` 产生只读引用 binding。
- 解构绑定列表当前只接受一层简单 identifier：`let (a, b) = value;`、`let ref (a, b) = value;` 和 `const (a, b) = value;` 可用；`let ((a, b), c) = value;`、`let (a: i32, b) = value;`、`let (ref a, b) = value;`、`let (_, b) = value;`、`let (a, ..) = value;` 以及给单个元素写默认值都不是当前语法。`ref`、`const` 和显式类型标注只能作用在整个解构声明上，不能逐元素拆开。
- 解构声明不会创建一个“整体 tuple 局部变量”。按值解构会先求出 initializer，再把每个元素分别存入对应 binding；按引用解构会取 initializer 的地址，再让每个 binding 绑定到对应 tuple 元素。解构 binding 自己进入局部名字表，但整个解构语句没有 `statement_bindings` 中的单一符号。
- 按值解构出的非引用元素按普通局部值处理；需要析构的元素会分别按局部对象规则清理。按引用解构出的 binding 是引用，不拥有被引用元素。
- 普通局部值、引用 binding、解构 binding 和 static local 都进入当前词法作用域的值名字表。同一作用域内不能重复声明同名值；内层 block 可以重新声明同名值并 shadow 外层名字，离开内层 block 后外层名字恢复可见。
- initializer 在新 binding 进入作用域前检查。它可以读取当前作用域中已经可见的函数参数、外层名字和之前声明的局部名字，但不能引用正在声明的这个名字本身；`let value = value;` 或 `let value = { return value; };` 中右侧的 `value` 只会查找外层已有名字，没有外层名字时按未知名字报错。解构声明同理，initializer 不能引用将要解构出的 `a` / `b`。
- 局部值名字和局部类型别名使用不同名字表。`let item = 1; type item = i32;` 在当前实现中不会因为同名直接冲突；表达式位置的 `item` 查值名字，类型位置的 `item` 查类型别名或其它可见类型。公开代码仍应避免这种同名写法，以免阅读时混淆。
- 不支持未初始化局部变量声明。
- `static` 局部声明只支持普通值 binding，不支持 `static ref` 或 `static` 解构。

```cp
let count: i32 = 1;
let total = count + 2;
```

如果需要默认值，必须写默认初始化表达式：

```cp
let count = i32{};
let point = vec2{};
```

裸 `{}` 不表示默认初始化。它保留给块表达式。

## static 局部变量

局部声明可以在 `let` / `const` 后写 `static`：

```cp
next() -> i32
{
    let static counter = 0;
    counter += 1;
    return counter;
}
```

规则：

- `static` 是声明修饰符，只在 `let` / `const` 之后、`ref` / binding 名之前的局部声明位置生效，例如 `let static name = expr`、`const static name = expr`、`let static name: Type = expr`。普通名字仍然可以叫 `static`，例如 `let static = 1;` 或 `let static: i32 = 1;`。
- `let static name = expr` 创建函数内可见、跨调用保留状态的 binding；它不是模块全局变量，不能导出，也不能被其他函数按名字访问。
- 存储归属于具体声明符号，不归属于文本名字。不同函数、不同 block 或同一函数中互相 shadow 的同名 static local 是不同对象。
- 初始化表达式在运行时第一次执行到该声明时求值，之后再次执行到该声明时跳过初始化并复用已有值。
- 初始化表达式是普通运行时表达式，可以读取当前调用的参数和该声明之前已经在作用域中的局部 binding；这些值只在首次正常初始化时被读取，后续调用不会重新求值。
- 初始化 guard 只在初始化表达式正常完成后置为 true；如果初始化表达式通过 `return`、`panic(...)` 等路径离开当前控制流，这次执行不会把 static 标记为已初始化。
- 当前 guard 只有“是否已初始化”状态，没有“正在初始化”状态；如果初始化表达式在 guard 置 true 前递归重新进入同一个 static 声明，初始化表达式可能再次执行。不要依赖 C++ 风格递归初始化检测或 once 语义。
- `const static name = expr` 也保留跨调用状态，但 binding const 仍然禁止重新赋值。
- `const static` 不是编译期常量，也不参与类型级常量求值；它只是一个运行时 static local binding 加上不可赋值的 binding const。
- static local 的类型、显式类型转换、直接初始化、默认初始化等检查与普通局部值声明一致；仍然必须写初始化表达式，不支持未初始化 static local。
- 泛型函数实例中的 static local 按具体函数实例拥有独立存储。例如 `next<i32>` 和 `next<bool>` 中同名 `counter` 不是同一个对象。
- 当前实现没有线程安全初始化协议；不要依赖多线程并发首次进入同一个 static local 时的互斥或 once 语义。
- static local 不按普通局部变量作用域退出生成析构清理，也没有程序退出时的析构顺序保证；需要拥有资源的 static 对象时，应由后续 runtime/global-lifetime 设计统一处理。
- 不支持 `let static ref name = expr`、`const static ref name = expr`、`let static (a, b) = tuple` 或 `const static (a, b) = tuple`；这些形状按 `invalid_assignment_target` 诊断处理。static local 只创建单个普通值 binding，不创建跨调用引用别名，也不创建多个解构出来的 static binding。当前错误恢复中，`static ref` 报错后仍会按引用 binding 建立符号以继续检查后续语句；`static` 解构报错后不建立各个解构名字。合法程序不能依赖这些恢复符号。

## 常量声明

常量声明把 `let` 换成 `const`：

```cp
const name: type = initializer;
const name = initializer;
const ref name = lvalue_initializer;
```

这里的 `const` 是 binding const，表示这个名字绑定不可重新赋值：

```cp
const limit: i32 = 10;
```

binding const 和类型里的 target const 相互独立：

```cp
const p: i32 const* = value;
```

上例中：

- `const p` 表示 `p` 这个名字不能重新绑定。
- `i32 const*` 表示 `p` 最终指向的 `i32` 不可写。

裸 `type const` 不是合法类型。类型中的 `const` 必须跟随 `*` 或 `&` 使用，详见 [type_system.md](type_system.md)。

## 局部类型别名

函数体和块语句中可以写普通类型别名语句：

```cp
main() -> i32
{
    type count = i32;
    type pair = (count, bool);

    let item: pair = (1, true);
    return item.0;
}
```

规则：

- 局部别名语法固定为 `type name = Type;`，右侧必须是类型表达式，不是运行时表达式。
- 局部别名只在声明之后、当前词法作用域及其子作用域中可见；离开 block 后不可见。
- 同一作用域内不能重复声明同名局部类型别名。内层 block 可以 shadow 外层同名别名。
- 局部类型别名不导出、不创建名义类型，也不接受泛型参数。`type name<T> = T;` 不是当前语法。
- 局部别名解析右侧时只能使用当时已经可见的类型名、泛型参数、关联类型和外层别名；不能引用同一 block 中稍后才声明的局部别名。
- `type name = opaque T;` 只作为顶层 opaque alias 声明使用，不是局部别名语句能力。
- 局部类型别名只在类型位置可用，不创建运行时值，也不会初始化存储或触发析构。`type item = i32; let value = item;` 中的 `item`
  仍按表达式名字查找；除非同一作用域或外层另有值 binding，否则会报未知名字。需要使用别名时
  写在类型位置，例如 `let value: item = 1;`、`item{}` 或 `box<item>{}`。

## 初始化表达式

初始化表达式可以是：

- 字面量。
- 名字。
- 函数调用。
- 类型转换。
- 数组或元组字面量。
- 结构体初始化。
- 默认初始化 `Type{}`。
- 块表达式。
- 返回上述值的其他表达式。

部分局部值声明使用目标位置初始化：初始化表达式直接构造到该局部变量的存储中，不先形成一个用户可观察的临时值再整体复制。这个规则不改变源码层的类型检查或求值顺序，只影响是否需要 copy / move 一个中间结果。

当前走目标位置初始化的局部值声明 initializer 是：

- 函数调用表达式。
- 数组字面量。
- 元组字面量。
- `struct` / `str` / 数组类型 / storage 类型等 `Type{...}` 结构化初始化表达式。

外层括号会被剥掉，因此 `let value = (item{ .x = 1 });` 仍按目标位置初始化处理。引用声明初始化的是引用 binding；解构声明会展开为多个 binding，因此不按一个整体目标位置初始化处理。

不属于目标位置初始化的例子包括普通名字、字段访问、索引、cast、二元表达式、`move value`、match、block 表达式和 enum case。它们仍然可以作为合法初始化表达式，但会按普通表达式结果再流向局部目标；如果类型需要 copy / move / 转换，这些规则仍会参与检查。

默认初始化表达式写作：

```cp
let count = i32{};
let ok = bool{};
let title = str{};
let point = vec2{};

type i32x4 = [i32; 4];
let values = i32x4{};
let prefix = i32x4{1, 2};
```

`Type{...}` 不是任意类型的通用“花括号转换”。当前语义层先识别 `str`、数组类型、`storage` 和 `struct` 的专用初始化规则；除此之外，只有空的 `Type{}` 可作为默认初始化表达式：

- `i32{}`、`bool{}`、`char{}`、opaque alias 的 `handle{}` 这类空初始化按 [type_system.md](type_system.md) 的默认初始化规则检查。
- 函数指针类型语义上可默认初始化为空函数指针，但表达式 parser 当前不会把裸 `f*(i32) -> i32{}` 识别成 `Type{}` 初始化器。需要这种默认值时，使用显式类型上下文的 `nullptr`，或先给函数指针类型取别名再写 `callback{}`：`type callback = f*(i32) -> i32; let cb = callback{};`。
- `i32{1}`、`bool{true}`、`SomeEnum{0}` 这类非专用类型的带元素初始化不是当前能力；需要转换时使用 `as`，需要选择枚举值时使用具体枚举 case。
- `enum`、`variant`、引用、函数类型和 `!` 等不可默认初始化的类型即使写空 `Type{}` 也会报默认初始化失败；`variant` 必须显式选择 case，引用必须绑定到已有左值。

`struct` 的 `Type{...}` 是专用初始化规则中最复杂的一类。全位置项形式会先用无上下文实参匹配构造函数；只有没有可行构造函数时，才按字段声明顺序退回聚合初始化。命名字段形式不查构造函数，只按字段名检查，并允许省略有默认值或可默认初始化的字段。选中 deleted 构造函数、构造函数二义性、构造函数默认参数不参与匹配、以及顺序聚合实参不能依赖字段类型上下文等细节见 [struct.md](struct.md#构造函数)。

同一个 `Type{...}` 初始化器内部不能混用位置项和命名项。`point{ 1, .y = 2 }` 和 `str{ text.ptr, .len = text.len }` 都会在 parser 阶段报错；应全部使用位置形式，或全部使用 `.field = value` 命名形式。这个限制在语法层统一生效，不因目标类型是 `struct`、`str`、数组或其它专用初始化器而放宽。

`Type{...}` 的右侧写明了要初始化的类型；如果它出现在有显式上下文类型的位置，当前语义层要求上下文类型和 `Type` 的读出类型一致，不能先构造 `Type{}` 再靠普通隐式转换流向另一个声明类型。因此 `let wide: i64 = i32{};` 会按 initializer 类型不匹配报错，即使普通 `i32` 值可以隐式流向 `i64`。需要这种值时，应把 initializer 写成目标类型 `i64{}`，或者先生成普通值再显式转换，例如 `let wide: i64 = (i32{}) as i64;`。

`str` 有编译器识别的专用初始化器：

```cp
let empty = str{};
let view = str{ .ptr = text.ptr, .len = text.len };
let prefix = str{ text.ptr, 3 as usize };
```

`str{}` 产生默认空视图；命名字段只接受 `ptr` 和 `len`，位置字段最多两个并按 `ptr`、`len` 顺序检查。可以只给出部分字段，省略字段沿用默认空视图中的对应值；例如 `str{ .ptr = p }` 的 `len` 仍是 `0`，不会扫描 `p` 指向的 C 字符串。`ptr` 必须能转换到 `char const*`，`len` 必须能转换到 `usize`。`str` 初始化器不是普通 `struct` 聚合初始化，也不会根据 `ptr` 自动计算长度。完整字段、下标和混合初始化边界见 [type_system.md](type_system.md#str-字符串视图)。

数组类型初始化器可以为空，也可以给出不超过 `N` 个位置元素；省略的尾部元素按 `T` 默认初始化。零长数组初始化器不需要产生任何元素，因此 `type empty_refs = [holder; 0]; let values = empty_refs{};` 这类写法可以成立，即使 `holder` 本身因为引用字段而不可默认初始化；但只要 `N > 0` 且存在省略元素，尾部元素仍必须能默认初始化。普通表达式位置当前需要通过类型名或类型别名触发数组类型初始化器，例如 `i32x4{1, 2}`；裸 `[i32; 4]{1, 2}` 不作为普通表达式解析。它不同于数组字面量 `[1, 2, 3]`：数组字面量可以在无上下文时推导长度和元素类型，而数组类型初始化器显式给出目标 `[T; N]`。数组类型初始化器不支持 named field，`storage [T; N]{}` 也不能带元素列表。

底层代码可以显式创建内联原始存储：

```cp
let slot = storage T{};
let slots = storage [T; 16]{};
```

这仍然是一个完整的初始化表达式：被初始化的是 storage 对象，不是 `T` 对象。它不等价于“未初始化的 `T` 变量”，也不放宽局部变量必须显式初始化的规则。

块表达式可以作为初始化表达式：

```cp
let value = {
    let base = 1;
    base + 2
};
```

如果块没有尾表达式，或者尾表达式后带分号，块表达式类型为内部 `unit`。如果块中前序语句已经保证不能正常完成，例如 `return`、`break`、`continue` 或 `panic(...)`，块表达式类型为 `!`，可以出现在任意需要值类型的位置，因为 `!` 可以隐式转换到任意类型：

```cp
from_initializer()
{
    let ignored: i32 = {
        return 1;
    };
}
```

这里 `return 1;` 返回的是 `from_initializer`，不是给 `ignored` 赋值；`ignored` 的初始化表达式本身类型为 `!`，因此类型检查通过但运行时不会初始化并继续执行到后续语句。块表达式、函数返回推导和正常完成判断见 [控制流](flow.md#函数返回和正常完成)。

## 标识符

标识符当前使用 ASCII 规则，整体与 C++ 接近。

词法规则：

- 起始字符只能是 `_`、`a..z` 或 `A..Z`。
- 后续字符可以继续使用 `_`、ASCII 字母或十进制数字 `0..9`。
- 不支持 Unicode 标识符，也不支持以数字开头的标识符。`value1` 可用，`1value` 会先按数字字面量和后续 token 处理。
- 大小写敏感，`value` 和 `Value` 是不同名字。

下面这些词是词法关键字，不能作为普通标识符、局部 binding 名、类型名、字段名或函数名使用：

```text
and
as
break
concept
const
continue
delete
do
else
enum
export
false
for
forward
if
impl
import
let
like
module
move
new
not
nullptr
operator
or
ref
return
struct
true
while
```

另一些词按上下文识别，不是词法关键字。它们只在特定语法位置有特殊含义，其它位置仍可以作为普通标识符使用：

- `static` 只在 `let static name = ...` / `const static name = ...` 这类局部声明修饰位置表示 static local；`let static = 1;` 中的 `static` 是普通 binding 名。
- `type` 在顶层、函数体、`impl`、concept 或 concept impl 的类型别名位置表示别名声明；在其它表达式位置只是普通名字。
- `extern` 只在顶层函数前且后面紧跟字符串字面量时表示 `extern "C"` 函数；否则按普通名字参与语法。
- `match`、`template`、`requires`、`variant`、`storage`、`decltype`、`opaque`、`this`、`self`、`f`、`prefix`、`postfix` 和 `default` 都是按上下文解析的词。只有对应语法形状成立时才有特殊含义；例如 `f(` 或 `f*(` 在类型位置表示函数类型，`f(...)` 在表达式位置可能是 lambda marker，`operator prefix ++` / `operator postfix ++` 中的 `prefix` / `postfix` 只修饰 update operator，`= default;` 中的 `default` 只作为 bodyless 特殊标记。不要把这些词当作未导入也全局可用的普通值或类型。
- `delete` 不属于上下文词；它是上表中的词法关键字，不能作为普通名字使用。它也可以出现在 bodyless 声明的 `= delete;` 标记位置，但这不改变它的关键字身份。

上下文词不表示“任何位置都能随便出现”。例如裸 `match`、裸 `static` 或裸 `requires` 作为顶层 token 不构成合法顶层声明；它们只是不会在词法阶段被固定成不可当名字的关键字。具体位置能否使用，仍由对应 parser 规则和语义名字查找决定。
