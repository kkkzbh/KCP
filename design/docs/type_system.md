# 类型系统

本文档记录 KCP 的核心类型规则。变量初始化写法见 [initial.md](initial.md)，结构体、构造函数、析构函数和成员函数见 [struct.md](struct.md)，所有权、借用和移动见 [ownership.md](ownership.md)，`concept` 和 `type` 类型别名语句见 [concept.md](concept.md)。

类型系统包含内建标量类型、内部 `unit` 类型、`!` never type、数组、元组、结构体、强类型 `enum`、opaque alias、variant、引用、指针、函数类型、函数指针、函数返回类型推导、默认初始化、显式转换、控制流条件规则和运算符类型规则。

`concept`、泛型、`enum`、opaque alias、`variant`、lambda 和闭包的专门规则分别见 [concept.md](concept.md)、[generic.md](generic.md)、[enum.md](enum.md)、[opaque_alias.md](opaque_alias.md)、[variant.md](variant.md) 和 [lambda.md](lambda.md)。

## 类型表达式与名字解析

类型位置中的名字按当前模块可见性解析，不存在“未导入也可用”的魔法类型名。顶层类型、导入类型、关联类型、当前泛型参数、当前 `impl` / `concept` 上下文中的 `this`、内建类型、`array` / `tuple` 小内建和 `std.meta` 查询各自有固定边界：

- `this` 只在当前类型上下文有效，例如 `concept`、固有 `impl`、concept `impl`、成员函数签名和对应函数体。它表示当前实现类型或当前 concept 的被实现类型。
- `this` 不接受类型实参，`this<i32>` 不合法。需要当前类型的关联类型时写 `this::item` 或在 concept 体内使用可见的关联类型简写。
- 内建标量类型不接受类型实参，`i32<T>`、`bool<1>` 都不合法。
- 普通 `type` 别名和关联类型别名当前不是类型构造器，不接受类型实参；`type name<T> = ...` 也不是当前语法。
- `enum` 和 opaque alias 是名义类型，但当前都不是泛型类型构造器；使用处不能写 `flags<i32>` 或 `handle<T>`。
- `struct` 和 `variant` 才能作为用户自定义泛型类型构造器。显式类型实参按声明顺序绑定；缺少的尾部参数只能由默认泛型实参补齐；多余实参报错。
- 泛型整数参数 `N: usize` / `I: isize` 是编译期整数参数，不是类型。它们只能出现在数组长度、storage 长度或整数泛型实参位置，不能作为普通类型写成 `let value: N` 或 `make<N: usize>() -> N`；当前按 `expected_type` 诊断。
- 普通类型泛型参数代表一个待替换类型，不是类型构造器；`T<i32>` 这类给类型参数再传实参的写法报 `invalid_type_argument`。需要类型构造器能力时，当前只能通过具体泛型 `struct` / `variant` 名称表达。
- 类型参数包不能在单个类型位置裸用。`T...` 只能出现在允许 pack expansion 的类型实参列表或 `template for(type U : T...)` 中；`let value: T` 对 `T...` 这类包参数报错。
- `void` 只在函数返回类型位置表示内部 `unit`，不能写成局部变量类型、字段类型、泛型实参或被 `const` / `&` / `*` 修饰的类型。
- `!` 是可写类型，但不能加 `const`、`like`、`forward`、`*`、`&`、`move&` 或 `forward&` 后缀。
- 未解析到任何可见类型、别名、泛型参数、内建或小内建时，报未知类型；编译器不会把未知名字延迟成“可能未来存在的类型”。

关联类型路径使用 `Owner::item` 形式。若 `Owner` 是依赖类型，关联类型查找可以延迟到泛型实例化；若 `Owner` 已经是具体非依赖类型但没有该关联类型，则立即报错。关联类型别名自身不接受额外类型实参。

`std.meta` 查询如 `read_type<T>`、`tuple_element<T, I>` 和 `call_result<F, Args...>` 是模块接口，只有 `std.meta` 本身、直接或间接重导出 `std.meta` 的模块，或让 `std.meta.callable` concept 可见的上下文中才按内建查询求值。没有导入时，这些名字按普通类型名解析，通常会得到未知类型诊断。详细规则见 [meta.md](meta.md)。

## 类型分类

### 内建标量类型

```text
bool
i8 i16 i32 i64
u8 u16 u32 u64
isize usize
f32 f64
char
str
```

`bool` 和 `char` 当前按 1 字节基础类型处理。

`usize` 和 `isize` 是编译器内建的目标平台指针宽度整数类型：

- 32 位目标上，`usize` 等价宽度为 `u32`，`isize` 等价宽度为 `i32`。
- 64 位目标上，`usize` 等价宽度为 `u64`，`isize` 等价宽度为 `i64`。
- `usize` 用于对象大小、对齐、索引和地址宽度相关的非负值。
- `isize` 用于指针差值和可能为负的地址宽度相关偏移。
- 它们是不同的内建类型，不是标准库别名；具体宽度由编译目标决定。

字面量默认类型：

| 字面量           | 类型   |
| ---------------- | ------ |
| `true` / `false` | `bool` |
| 整数字面量       | `i32`  |
| 浮点字面量       | `f64`  |
| 字符字面量       | `char` |
| 字符串字面量     | `str`  |
| `nullptr`        | 空指针字面量，需上下文确定为某个 `T*` |

`nullptr` 有独立的内部字面量类型，只能在目标位置转换成具体指针类型：

```cp
set(pointer: i32*) -> void
{
}

main()
{
    let pointer: i32* = nullptr; // 合法：声明类型提供目标指针类型
    set(nullptr);                // 合法：参数类型提供目标指针类型
    delete nullptr;              // 合法：delete 对裸 nullptr 是 no-op
}
```

边界：

- `let pointer = nullptr;` 不合法，因为省略类型的局部声明没有目标指针类型可用。
- `nullptr` 可以隐式转换到 `T*`、`T const*` 或 `f*(...) -> R` 这类具体函数指针类型；它不能转换到非指针类型，也不能初始化 `f(...) -> R` 函数类型值。
- `nullptr as T*` 可用；`as` 右侧必须是具体指针类型。
- 指针比较允许一侧是具体指针、另一侧是 `nullptr`，例如 `ptr == nullptr` 或 `nullptr != ptr`。
- 裸 `nullptr` 和裸 `nullptr` 之间没有目标指针类型，`nullptr == nullptr` 不作为当前内建比较能力。
- `free(nullptr)` 不合法，因为普通 `free` 调用没有参数目标类型；需要先有具体指针类型的空指针值。`delete nullptr;` 是专门的语法例外。

数字字面量当前只支持十进制形状。整数字面量由十进制数字组成，除单独的 `0` 外不能以前导零开始；`012` 会在 lexer 阶段报前导零整数错误。`-1` 不是一个带符号字面量，而是前缀 `-` 作用在整数字面量 `1` 上；因此数组长度、整数泛型实参和其他只接受非负整数 literal 的类型位置不能写 `-1`。浮点字面量可以写小数点和/或十进制指数，例如 `0.5`、`1.`、`0e3`、`2.5e+1`、`3E8` 和 `10e-23`。指数标记 `e` / `E` 后如果写了可选 `+` / `-`，后面仍必须至少有一位十进制数字；`1e`、`1e+` 和 `1e-` 都不是合法浮点字面量。

当前不支持数字字面量后缀，`1u`、`1_i32`、`2A` 这类写法会作为非法后缀报错。也不支持 `0x` 十六进制、`0b` 二进制、`0o` 八进制或分隔符 `_`。需要目标整数或浮点类型时，先写无后缀十进制字面量，再通过上下文类型、普通隐式整数转换、浮点 rank 转换或显式 `as` 表达目标类型。

普通整数字面量的语义值当前按 `i64` 范围解析后再参与类型检查；超出这个范围的文本没有稳定公开语义，当前实现会把解析失败按恢复值 `0` 继续走后续检查，不会把它当作任意精度整数。数组长度和 `storage [T; N]` 的长度位置走单独的 `usize` 长度解析：太大的长度会报 `invalid array length`，而不是产生一个可用的大整数常量。公开代码应把整数 literal 保持在当前目标类型和 `i64` / `usize` 可表示范围内。

当前可依赖的语义转义字符是 `\n`、`\r`、`\t`、`\0`、`\\`、`\'` 和 `\"`。这些转义在字符字面量和字符串字面量中都表示对应的单个 `char`。字符字面量必须包含恰好一个字符或一个转义字符；`''`、`'ab'`、未闭合字符字面量、跨行字符字面量和未知转义都会在 lexer 阶段报错。

lexer 还会接受 C 风格简单转义中的 `\?`、`\a`、`\b`、`\f`、`\v`，以及八进制和 `\x...` 十六进制 escape 形状；但这些形状当前不属于稳定公开语义，不应在源程序中依赖。现有编译器不会把它们按 C 规则转换成对应控制字符或码点，例如 `'\a'` 当前按字符 `a` 处理，`"\x41"` 当前按字符串内容 `"x41"` 处理，而不是字符 `A`。需要稳定控制字符时，只应使用上面列出的可依赖转义。

`str` 表示字符串视图，语义上接近 C++ 的 `std::string_view`。

#### str 字符串视图

`str` 是编译器认识的标准库字符串视图类型，语义字段等价于 `ptr: char const*` 和 `len: usize`。它非拥有、只读、带运行时长度，不负责分配或释放底层存储；字符串字面量产生的 `str` 指向静态存储。

字符串字面量的底层静态存储可以额外带一个 trailing `'\0'` 以兼容 C ABI，但 `'\0'` 不参与 `str` 的长度语义。`"a\0b".len` 的结果是 `3`；导入 `std.text.str`、`std.text` 或 `std` 后，`"a\0b".size()` 也返回 `3`。遍历会访问中间的 `'\0'`，不会按 C 字符串规则提前结束。

`str` 和 `[char; N]` 不等价：

- `[char; N]` 是拥有存储的固定长度数组，长度 `N` 是类型的一部分。
- `str` 是借用视图，长度是运行时值，不在类型中。
- `[char; N]` 的元素在数组可写时可以通过下标写入；`str` 的下标结果只读。

`str` 的字段和下标是编译器认识的基础能力：

```cp
let text: str = "hello";
let count = text.len;
let pointer = text.ptr;
let first = text[0];
```

`size()` 和 `data()` 不是裸语言内建，而是标准库 `impl str` 方法。它们只在导入导出 `std.text.str` 的模块后可见：

```cp
import std;

main() -> usize
{
    let text: str = "hello";
    return text.size();
}
```

规则：

- `s.ptr` 是 `char const*`，指向第一个字符。
- `s.len` 是 `usize`，保存运行时长度。
- `s.ptr` 和 `s.len` 是编译器识别的 `str` 字段，不依赖标准库导入。
- `str` 视图的字符内容只读，但可写 `str` binding 的字段本身仍是普通字段左值。`let text: str = "cp"; text.len = 1 as usize;` 当前合法，只是把视图长度改成另一个值；`const text: str = "cp"; text.len = ...` 会按 const binding 禁止赋值。
- `s.size()` 返回 `usize`，等价于 `s.len`；这是标准库方法，不导入 `std.text.str` / `std.text` / `std` 时按普通未知成员调用处理。
- `s.data()` 返回 `char const*`，等价于 `s.ptr`；它同样是标准库方法，不是裸内建成员。
- `s[i]` 要求 `s` 的类型是 `str`。
- 下标 `i` 必须是整数类型。
- `s[i]` 的结果类型是 `char`。
- `s[i]` 不是左值，不能赋值。
- 越界行为定义为调用者违反前置条件；checked 第一版会在运行时触发 panic。当前语义层不读取 `str.len`，也不对字符串字面量常量下标做越界诊断；`"cp"[3]` 和 `text[3]` 都只按“整数下标返回 `char`”检查。
- 可以显式构造 `str{ .ptr = p, .len = n }` 或按字段顺序构造 `str{ p, n }`。
- 可以写 `str{}`，也可以只给出部分字段；未给字段沿用默认 `str` 值。`str{ .ptr = p }` 的长度仍是默认长度 `0`，不会扫描 `p` 指向的 C 字符串。
- 命名字段只允许 `ptr` 和 `len`。重复命名字段报重复字段初始化，未知字段报未知字段。
- 位置初始化最多两个元素，顺序固定为 `ptr`、`len`；第三个位置元素报聚合长度不匹配。
- `str` 初始化器当前由专用语义路径处理，不完全等同于普通 `struct` 聚合初始化。语义层会按 `ptr` / `len` 的专用字段类型检查命名项或位置项，但 parser 的通用 `Type{...}` 初始化器列表禁止混合命名项和位置项：`str{ .ptr = p, n }` 与 `str{ p, .len = n }` 都会在 parser 阶段报 `unexpected_token`，不会进入 `str` 专用语义检查。公开代码应使用全命名形式 `str{ .ptr = p, .len = n }` 或全位置形式 `str{ p, n }`。
- `ptr` 字段必须能按 `char const*` 检查，`len` 字段必须能按 `usize` 检查。
- 调用者负责保证 `[ptr, ptr + len)` 指向有效只读字符序列；是否 trailing nul 不是 `str` 的不变量。

`str` 不内建 `size()`、`data()`、`empty`、`starts_with`、`contains`、切片或解析等高层能力。当前标准库 `std.text.str` 为 `str` 提供 `size()`、`data()`、`operator <=>`、`== != < <= > >=`，以及 [iteration.md](iteration.md) 中的 `iterable` / `const_iterable` 实现；这些能力都要求对应模块可见。`empty`、`starts_with`、`contains`、切片和解析当前不是已实现的标准库方法，需要调用方按 `len`、下标和指针自行组合，或等待后续库扩展。比较规则是按 `char` 序列做字典序比较；公共前缀相同后，短串为 `less`，长串为 `greater`，长度相同为 `equivalent`。比较始终使用 `str.len`，不会按 C 字符串在中间 `'\0'` 截断。标准库直接声明这些比较运算，关系运算不是编译器从 `<=>` 自动派生的内建规则：`==` 先比较长度再逐字符比较，`<` 使用同一字典序小于规则，`!=`、`<=`、`>`、`>=` 分别由 `==` / `<` 组合实现。字符串范围遍历依赖 `str` 的 `iterable` 实现，而不是因为 `str` 拥有 `size()` 和 `[]` 自动成立。

#### string 拥有字符串

`string` 是标准库提供的拥有型字符串，底层通过 `string_storage` 管理字符存储和 trailing `'\0'` 物理容量。它不是内建标量类型；编译器只需要理解 `str` 视图和字符串字面量，拥有、扩容和可变操作由标准库 `std.text.string` 实现。

`string` 的第一版语义：

- `string{}` 构造空字符串。
- `string{text}` 从 `str` 构造拥有副本；字符串字面量类型仍是 `str`，因此常用写法是 `string{"text"}`。
- copy 构造和 copy 赋值做深拷贝；move 构造和 move 赋值转移底层存储，并把源对象长度置为 0。move 后的源对象可以继续析构、重新赋值、`clear()` 或再次 `push_back` / `append`，但它的底层 `string_storage` 可能已经是空 raw buffer；在重新分配前不要依赖源对象的 `data()` 非空或带 trailing `'\0'`。
- `str` 不会隐式转换成 `string`，`as` 也不是构造钩子；`"text" as string` 不合法。需要拥有副本时显式写 `string{"text"}` 或 `string{some_str}`。
- `size()` 返回有效字符数，不包含 trailing `'\0'`。
- `capacity()` 返回可存放的有效字符数，不包含 trailing `'\0'` 预留位。
- `data()` 返回底层连续存储指针；普通对象返回 `char*`，const 对象返回 `char const*`。
- `begin()` / `end()` 返回当前有效字符区间的起止指针；普通对象返回 `char*`，const 对象返回 `char const*`。`end()` 指向有效字符之后的位置，不指向 trailing `'\0'`。
- 只要底层 storage 非空，标准库会在 `data() + size()` 位置维护一个 trailing `'\0'`，因此普通构造出来且未被 move 取走 storage 的 `string` 可以把 `data()` 作为 C++11 风格字符串数据入口使用。moved-from 源对象只保证 `size() == 0`，不保证 `data()` 是可解引用的空字符串指针。
- 不提供 `c_str()`；需要裸指针时直接使用 `data()`。
- `as_str()` 返回借用视图 `str`，长度等于 `size()`，不拥有底层存储。
- `data()` 指针和 `as_str()` 视图只借用当前缓冲区。`reserve`、扩容触发的 `push_back` / `append` / `resize`、copy/move 赋值等可能替换底层 storage，旧指针和旧 `str` 视图随即失效；不扩容的内容修改会让旧视图继续指向同一缓冲区，但看到的是修改后的字节。需要长期保存文本时应复制成新的 `string`，不要保存旧 `str` 当作拥有值。
- `empty()` 返回 `size() == 0`。
- `operator [](index)`、`front()` 和 `back()` 返回 `char like&`，可写 `string` 上可以赋值修改字符，const `string` 上得到 `char const&`。这些访问使用 `assert` 表达当前长度前置条件；checked 第一版失败时 panic，release 模式下仍是调用者契约。
- `reserve(new_capacity)` 只保证容量至少达到 `new_capacity`；`new_capacity <= capacity()` 时不做事。它不改变 `size()`，但可能替换底层 storage 并使旧指针和旧 `str` 视图失效。
- `push_back(ch)`、`pop_back()`、`resize(new_size, ch)`、`append(str)` 和 `clear()` 都保持 trailing `'\0'` 不变量。`pop_back()` 要求非空；`resize` 变小时截断，变大时用 `ch` 填充新增字符。
- `append(text)` 按 `text.size()` 复制字符，不按 `'\0'` 截断。`text` 是 `self.as_str()` 这种从同一 buffer 开头借出的视图时，当前实现会在扩容后重新取得源指针，从而支持自追加；不要把指向同一 buffer 中间位置的旧 `str` 视图传给可能扩容的 `append`，因为它不是拥有值，扩容后会失效。
- `string` 实现 `iterable` 和 `const_iterable`，可写迭代元素为 `char&`，只读迭代元素为 `char const&`。
- `string` 实现 `contiguous_mutable_range`，可作为标准库需要连续可写字符区间的输入。
- 第一版没有 `insert`、`erase`、`replace`、`find`、`contains`、`starts_with`、`substring`、`split`、`trim`、`parse`、`operator +` 或字符串插值。需要这些操作时，当前只能通过 `size()`、下标、`append` 和显式循环组合。

`string` 不是 `str` 的替代品：`str` 表达借用和字面量视图，`string` 表达拥有和可变缓冲区。需要传递只读文本时优先使用 `str` 参数；需要保存或修改文本时使用 `string`。

### unit 和 `!`

内部存在 `unit` 类型，用于表示没有值的结果。它不是可命名的一等源语言类型。

`unit` 不是普通用户可写类型，主要由以下场景产生：

- 没有带值 `return` 的函数。
- `return;`。
- 没有尾表达式的块表达式。
- 尾表达式后带分号的块表达式。

返回类型位置可以写 `void`，它在语义上等同于内部 `unit`：

```cp
touch<T>(value: T) -> void
{
    return;
}
```

`void` 只允许表达“函数没有返回值”，不能作为局部变量类型、字段类型、容器元素类型或泛型实参使用。当前实现只在函数返回类型这类返回类型位置把 `void` 识别为内部 `unit`；在普通类型位置写 `let x: void = ...`、字段 `item: void` 或 `box<void>` 时通常会按未知类型报错，而不是创建一个可命名的 `void` 类型后再拒绝。返回类型位置的 `void` 也不能被修饰或带实参；`-> void const`、`-> void<i32>` 和 `-> void*` 都报 `invalid_type_argument`。需要 C 风格无类型指针时，当前没有 `void*`，应使用具体 `T*`、`u8*` 或自行封装的 opaque / struct 句柄。

`!` 是 never type，表示表达式不会正常产生值。`panic(...)`、`unreachable()` 和只产生发散控制流的表达式使用 `!`。`!` 可以隐式转换到任意类型；任意普通类型不能转换到 `!`。详细错误处理规则见 [error_handling.md](error_handling.md)。

### 结构化类型

`[T; N]` 表示固定长度数组：

```cp
let data: [i32; 4] = [1, 2, 3, 4];
```

固定数组是同构类型，所有元素具有统一元素类型，长度 `N` 是类型的一部分。它内联拥有存储，不退化为指针。

`[T; N]` 的类型参数规则：

- 第一个参数 `T` 必须是类型。
- 第二个参数 `N` 必须是非负整数编译期常量。
- 长度位置不是普通表达式位置；只能写非负整数字面量或当前可见的整数 const 泛型参数名。`[T; N + 1]`、`[T; 2 * N]`、`[T; -1]`、`[T; value]` 和 `[T; enum_case]` 都不是当前能力。
- `N` 是数组类型的一部分，`[i32; 3]` 和 `[i32; 4]` 是不同类型。
- `[T; 0]` 允许存在，但不能读取任何元素。
- 泛型函数可以声明 `N: usize` 或 `N: isize` 这样的整数 const 参数，并在数组类型中写 `[T; N]`。

固定数组的基础操作是内建能力：

- 数组字面量 `[a, b, c]` 可以在无上下文时推出元素类型和长度；数组类型初始化器 `Alias{...}` / `array<T, N>{...}` 显式给出目标类型，并允许尾部元素默认初始化。
- `array[index]` 要求 `index` 是整数类型，结果类型是 `T`。可写数组左值的下标结果是可写元素左值；const 数组或只读引用数组的下标结果只读；临时数组的下标结果不能作为可写赋值目标。
- 常量整数下标会做静态边界检查，负数或 `index >= N` 报错；其它整数下标只检查类型，运行时越界属于调用者违反前置条件。checked 第一版会在运行时触发 panic。
- 数组下标始终走内建规则。即使当前作用域或元素类型附近存在用户 `operator []`，`[T; N]` 的索引类型错误、常量越界或只读赋值错误也不会回退到用户下标 operator。
- 固定数组不会隐式退化为 `T*`，也没有裸内建 `.len` / `.data` 字段。需要指针或范围接口时应通过标准库封装、显式 helper，或在有明确生命周期约束的底层代码里使用相应数组/指针 API。

`storage T` 和 `storage [T; N]` 表示内联原始存储：

```cp
let one = storage i32{};
let many = storage [i32; 4]{};
```

storage 类型只保证大小和对齐，不表示其中已经存在 `T` 对象。它默认初始化时不调用 `T{}`，析构时不调用 `T` 析构函数，也不支持普通数组索引。对象生命周期必须通过 `construct_at(storage.slot(...), value)` 和 `destroy_at(storage.slot(...))` 显式管理；完整生命周期和底层内存契约见 [memory_allocation.md](memory_allocation.md)。const storage 只能取到 `T const*`，普通解引用不能写入；底层生命周期原语仍可以用 `T const*` 开始或结束 const 对象生命周期。

`storage T` 是单 slot storage；`storage [T; N]` 是同一元素类型的 `N` 个连续 slot。`storage [T; N]` 中的 `[T; N]` 只用来表达 storage 的元素类型和 slot 数，不表示这些 slot 中已经存在一个完成初始化的数组对象。`N` 可以是整数 literal，也可以是当前可见的 `usize` / `isize` 整数 const 泛型参数；`storage [T; 0]` 可以表示零 slot storage，但 `slot()` 省略下标只允许用于长度已知为 1 的 storage。

`data()` 和 `slot()` 是 storage 上的编译器 builtin 成员调用，不是普通方法：

- receiver 必须是可取地址的 lvalue；`let s = storage T{}; s.slot()` 合法，`(storage T{}).slot()` 不合法。
- `data()` 不接受参数，返回第一个 slot 的指针；编译器不证明 storage 长度大于 0。
- `slot()` 不带下标时只允许用于单 slot storage；多 slot storage 必须写 `slot(i)`。
- `slot(i)` 的下标必须是整数表达式。长度已知且下标是编译期整数常量时，负数或越界会被拒绝；运行时下标边界由调用者保证。
- 这些 builtin 不接受显式类型实参，也不会通过首参 UFCS 把 `data(storage)` 或 `slot(storage, i)` 普通调用改写成 storage builtin；如果当前作用域里有同名自由函数，这类普通调用仍按自由函数解析。
- 点号调用中只有 `data` 和 `slot` 这两个名字会被 storage builtin 抢先处理。其它成员名仍按普通成员函数和点号 UFCS 查找，例如 `helper(value: storage i32)` 可以通过 `value.helper()` 调用；但 `value.data()` / `value.slot(...)` 不会因为当前作用域里存在同名自由函数而改走 UFCS。需要调用自由函数 `data(value: storage i32)` 时必须写普通调用 `data(value)`。

storage 是普通值类型，可以作为字段、局部变量、参数、返回值和泛型实参使用。可写 storage binding 支持整体赋值：

```cp
let slots = storage [i32; 2]{};
slots = storage [i32; 2]{};
```

整体赋值只复制原始 storage 值，不为其中可能存在的对象调用赋值、构造或析构。已经在 storage slot 中手动构造对象的代码，必须在覆盖 storage 前自行维护对象生命周期。

`(T1, T2)` 表示元组类型：

```cp
let triple: (i32, f64, char) = (1, 0.5, 'x');
let single: (i32,) = (1,);
```

元组是异构类型。普通 `(T)` 是类型分组，普通 `(x)` 是分组表达式；一元 tuple 必须写成 `(T,)` / `(x,)`。空 tuple 类型 `()` 不是当前语法，parser 会按“不支持空 tuple type”拒绝；需要表达“无值”时使用函数返回 `void` / 内部 `unit`，不要把 `()` 当成可命名类型。

元组值的基础操作是字面量构造、默认初始化、整体赋值、简单解构和编译期字段访问。读取元素使用 `.0`、`.1` 这类数字字段；`tuple[0]` 不是内建 tuple 访问，也不会作为 tuple 专用语法处理。完整值层规则见下文“元组操作”。

`array` 和 `tuple` 也是类型位置中的编译器小内建：

```cp
type ints = array<i32, 4>;          // 等价于 [i32; 4]
type pair = tuple<i32, bool>;       // 等价于 (i32, bool)
```

规则：

- `array<T, N>` 必须且只能带两个类型实参位置。第一个实参必须是类型，第二个实参必须是非负整数 literal，或当前可见的 `usize` / `isize` 整数 const 泛型参数名；不能写运行时表达式、普通类型名、`bool`、负数字面量或 `N + 1` 这类常量表达式。
- `array<i32>`、`array<i32, 4, 8>`、`array<1, 2>` 这类参数数量或实参种类不匹配都会报 `invalid_type_argument`。`array<1, 2>` 中第一个实参不是类型；`array<i32, bool>` 中第二个实参不是整数长度；`array<i32, N>` 只有在 `N` 是当前可见的 `usize` / `isize` 整数 const 泛型参数时才合法。
- `array<T, N>` 和 `[T; N]` 产生同一个数组类型。后缀 `*`、`&`、`const&`、`move&` 等按普通类型后缀规则作用在整个数组类型上。
- `tuple<T1, T2, ...>` 的每个实参都必须是类型，且至少有一个元素。空 `tuple<>` 不是当前合法类型；需要“无值”时使用内部 `unit` 返回或普通空块语义。
- `tuple<1>`、`tuple<i32, 1>` 这类非类型实参会报 `invalid_type_argument`。`tuple` 不接受整数长度参数、字段名、运行时值或类型参数包展开来补齐元素；每个元素都必须直接写成一个类型实参。
- `tuple<T1, T2>` 和 `(T1, T2)` 产生同一个元组类型。`tuple<i32>` 是一元元组类型，等价于 `(i32,)`，不是分组类型。
- `array` / `tuple` 是类型位置的小内建名，不是 `std` 中的普通泛型类型构造器，也不是用户可重载或替换的名字。即使用户声明了同名 `struct`、`variant` 或 type alias，类型位置的 `array<...>` / `tuple<...>` 仍按内建数组 / 元组检查。当前不要定义同名类型来表达不同语义。
- 类型参数包不能通过普通 `array` / `tuple` 实参任意展开成新结构。`array<Args...>`、`tuple<Args...>` 或 `tuple<box<Args>...>` 都不是当前公开能力；参数包展开边界见 [generic.md](generic.md)。

`struct` 是名义类型。结构体规则见 [struct.md](struct.md)。

`enum` 是名义整数类型。case 通过 `Type::case` 访问，不隐式转整数；规则见 [enum.md](enum.md)。

`type A = opaque T` 是名义封装类型，layout/ABI 与底层 `T` 一致，但不继承底层操作；规则见 [opaque_alias.md](opaque_alias.md)。

`variant` 是名义类型，表示若干个 case 中恰好一个。variant 规则见 [variant.md](variant.md)。

### 函数类型与函数指针

函数类型写作：

```cp
f(i32, i32) -> i32
f(left: i32, right: i32) -> i32
```

`f(...) -> R` 表示非空函数值。普通命名函数和无捕获 lambda 可以绑定到函数类型。

函数指针类型写作：

```cp
f*(i32, i32) -> i32
f*(left: i32, right: i32) -> i32
```

`f*(...) -> R` 表示运行时函数地址，主要用于 C ABI 和底层回调。它接近 C/C++ 函数指针，可以默认初始化为空函数指针；调用空函数指针属于底层 unsafe 契约。普通命名函数和无捕获 lambda 在目标类型是同签名函数指针时可以隐式转换到 `f*(...) -> R`；有捕获 lambda 不能绑定到函数类型或函数指针类型。

函数指针比较走普通指针比较规则：同一函数签名的 `f*(...) -> R` 可以做 `==`、`!=` 和有序比较，也可以和 `nullptr` 比较；不同参数列表或不同返回类型的函数指针不是同一指针目标类型，不能直接比较。函数指针不支持内建 `<=>`，需要三路比较时应在用户层封装成普通值或显式比较空值 / 地址来源。

函数类型参数可以只写类型，也可以写参数名。参数名由 parser 识别并保存在 AST 中，用于诊断、文档和可读性；它不参与类型等价、ABI 或重载选择。

`f` 是类型位置中的上下文 marker。只有在类型位置看到 `f(` 或 `f*(` 时，parser 才按函数类型解析；表达式位置的 `f(...)` 优先是普通调用或 lambda 识别流程，不会因为名字是 `f` 自动变成函数类型。

函数类型必须完整写出返回类型：

- `f() -> void` 是零参数、返回内部 `unit` 的函数类型。
- `f() -> i32` 是零参数、返回 `i32` 的函数类型。
- `f()` 不是函数类型语法，也不是“返回类型推导”的函数类型。
- `f(i32,) -> i32` 这种尾随逗号当前 parser 不接受。
- 参数名只能写成 `name: Type`。函数类型参数没有默认值、`self` receiver、参数包或省略类型参数语法；需要可变数量调用能力时使用泛型函数/lambda 或 `callable<Args...>` 这类 concept。

`f*(...) -> R` 是函数指针类型，因此可以接收 `nullptr`，也可以默认初始化为空函数指针。这里的 `*` 是函数指针语法的一部分，不是给 `f(...) -> R` 再套普通类型后缀；当前没有函数引用类型，也不能写 `(f(...) -> R)*`、`f(...) -> R&` 或 `f(...) -> R const&` 来修饰整个函数类型。源码表达式位置当前不会把裸 `f*(...) -> R{}` 识别成类型初始化器；需要空函数指针值时，写显式类型上下文的 `nullptr`，或先给函数指针类型取别名再写 `callback{}`。`f(...) -> R` 本身不是指针类型，不能由 `nullptr` 初始化，也不可默认初始化。

函数类型是普通类型表达式，可以出现在局部类型别名、函数返回类型、参数类型、泛型类型实参、`requires` 约束和 `std.meta` 查询中：

```cp
type unary = f(i32) -> i32;
type result = call_result<f(i32, bool) -> i32, i32, bool>;

make<F, Args...>() -> call_result<F, Args...>
requires
    F: callable<Args...>
{
    return 0;
}

main() -> i32
{
    return make<f(i32, bool) -> i32, i32, bool>();
}
```

在类型实参列表里，`f(i32, bool) -> i32` 仍然按完整函数类型解析，`->` 属于函数类型的一部分，不结束外层泛型调用。`f(...) -> R` 作为类型实参只表示类型，不会创建 lambda 或函数值；需要值时仍要传入普通函数名、lambda 或闭包对象。

lambda、普通函数绑定、捕获和闭包规则见 [lambda.md](lambda.md)。

### decltype 类型表达式

`decltype(expr)` 是类型位置表达式，用于取得表达式的静态类型：

```cp
let x = 1;
type X = decltype(x);      // i32
type Y = decltype(x + 1);  // i32
```

`decltype(expr)` 返回 `expr` 被读取后的值类型，不保留 C++ 的 value category 细节。`expr` 不是纯语法 quote：它仍按当前作用域和普通表达式规则做语义检查，名字、成员、调用和操作符解析错误都会正常报错。

`decltype` 检查内部表达式时不提供目标类型。也就是说，它只观察表达式自己能推导或检查出的类型，不会像变量声明、函数实参或返回语句那样把外层期望类型传进去。需要目标类型才能成立的表达式不能靠 `decltype` 补上下文，例如裸 `nullptr`、空数组字面量、空 tuple/聚合字面量或需要目标 callable 类型的 lambda，都不是稳定的 `decltype` 输入；应先在普通源码中建立带显式类型的值，再对那个名字使用 `decltype`。

```cp
let x = 1;
let ref r = x;
let p: i32* = &x;

type A = decltype(x);   // i32
type B = decltype(r);   // i32
type C = decltype(*p);  // i32
```

显式借用表达式是例外：`decltype(ref expr)` 和 `decltype(const ref expr)` 保留引用类型。这给成员函数和泛型代码提供查询当前借用形态的方式。

```cp
let x = 1;

type R = decltype(ref x);        // i32&
type C = decltype(const ref x);  // i32 const&
```

其它表达式级值类别标记不保留成引用类型：

```cp
let x = 1;
let ref r = x;

type M = decltype(move x);   // i32，不是 i32 move&
type K = decltype(const x);  // i32，不是 i32 const 或 i32 const&
type V = decltype(r);        // i32，不是 i32&
```

字段访问、下标访问、解引用和普通括号也按普通读出规则处理。即使某个表达式结果是左值或引用元素，`decltype(expr)` 仍得到读出后的值类型；需要保留借用形态时必须把 `ref` / `const ref` 写在 `decltype` 的顶层表达式里。普通括号不会触发 C++ 式引用保留，但也不会取消顶层显式借用：`decltype((x))` 是 `T`，`decltype((ref x))` 仍是 `T&`。

`forward` 表达式也是例外：在泛型函数体内，`decltype(forward value)` 保留当前实例中 `forward value` 的引用或移动引用形态。这里的 `value` 可以是当前函数的 `forward&` 参数，也可以是 `template for` 从 `T forward&...` 值参数包元素建立的展开绑定。标准库 ranges 使用它区分可写左值、const 左值和右值。

规则：

- `decltype(expr)` 只能出现在类型位置，例如类型别名、变量类型标注、函数返回类型、泛型实例类型实参或 `template for` body 中的类型位置。
- `expr` 只进行语义类型检查，不产生运行时代码，也不会执行副作用；但它必须是语义有效表达式，不能用来查询不存在的名字、成员、调用结果或非法操作。
- `expr` 没有来自外层的 expected type。`decltype(nullptr)`、`decltype([])`、`decltype({})` 或依赖目标函数类型的 lambda 形状不能通过 `decltype` 自己获得上下文。
- `decltype(expr)` 返回表达式的静态读出类型，不保留左值、引用或可写性类别。
- `decltype(ref expr)` 的结果是 `T&`。
- `decltype(const ref expr)` 的结果是 `T const&`。
- `decltype(move expr)` 和 `decltype(const expr)` 不保留 move/const 表达式类别，结果仍按普通读出类型计算。
- `decltype(forward name)` 的结果按当前实例的 forward 绑定类别保留引用或移动引用形态；`name` 必须是 forward-capable binding，具体规则见 [ownership.md](ownership.md)。
- `self` 在成员函数体中是普通 receiver 变量，因此 `decltype(ref self)` 合法。
- 如果 `expr` 依赖泛型参数，`decltype(expr)` 可以形成依赖类型，等实例化后再确定。
- `decltype(expr)` 本身是完整类型语法分支，后面不再接受普通类型后缀；`decltype(x)*`、`decltype(x)&` 或 `decltype(x) const&` 不是当前写法。需要从查询结果派生指针或引用类型时，先写局部类型别名，例如 `type X = decltype(x); type XP = X*;`。
- 不支持 C++ 的 `decltype(auto)`。
- 不支持 `decltype((x))` 这类通过额外括号保留引用类别的特殊规则。

`decltype` 与参数包配合时，可以取得 `template for` 当前展开值的类型：

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

## 函数返回类型

函数声明和定义写作：

```cp
add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

显式返回类型存在时：

- 所有 `return value;` 都必须能转换到声明返回类型。
- `return;` 只允许用于 `void` / 内部 `unit` 返回。

函数可以省略 `-> type`：

```cp
main()
{
    let x = 1;
}
```

省略返回类型时，语义分析收集函数体中的所有 `return` 并统一出返回类型。`return value;` 贡献 `value` 的表达式类型，`return;` 贡献内部 `unit`。函数体中没有任何 `return` 时，返回类型同样推导为 `unit`。

返回类型推导不通过额外括号保留引用。`return (x);` 与 `return x;` 等价，都会按普通表达式读出规则推导返回类型。需要返回引用时，显式写 `return ref x;` 或 `return const ref x;`，借用表达式规则见 [ownership.md](ownership.md)。

当前实现的推导规则：

- 可以从当前函数之后声明的函数调用继续推导；`main() { return f(); } f() { return 1; }` 合法。
- 已显式声明返回类型的递归函数不需要推导；`f() -> i32 { return f(); }` 合法。
- 没有显式返回类型的直接递归或互递归函数不能只靠自身调用推出类型，会报 `cannot_infer_return_type`。
- 多个正常返回观察值必须能统一。相同类型直接统一；整数族按当前实现的 `integer_rank` 统一；浮点族按更高 rank 的浮点类型统一；不同族、不同名义类型或 `unit` 与普通值类型混用不能统一。
- `integer_rank` 当前为 `i8/u8 < i16/u16 < i32/u32 < i64/u64/isize/usize`。整数返回统一只比较 rank，rank 更高者成为结果类型；rank 相同时保留先观察到的返回类型。因此 `return 1 as i32; ... return 2 as u32;` 推导为 `i32`，反过来先观察 `u32` 时推导为 `u32`。这不是完整的 C++ usual arithmetic conversion。
- `return;` 作为 `unit` 参与统一。只有全部正常返回都是 `unit`，或者函数体没有任何 `return` 时，才推导为 `unit`。`if(flag) { return; } return 1;` 不能推导成 `i32`，会报 `cannot_infer_return_type`。
- `if`、`while`、`do while`、范围 `for`、块表达式、`match`、`template for` 展开体和合法具体实例中被选中的 `template if` 分支中的 `return` 都参与推导。
- 合法 `template if` 必须能在当前实例中选出确定分支；未被选中的分支不参与推导，也不要求其中的名字和类型有效。条件 dependent、不能折叠成 constexpr bool 或语义检查出错时，后续返回推导可能为了错误恢复保守观察多个分支，但这不是可依赖的语言能力；完整条件边界见 [generic.md](generic.md)。
- 省略返回类型的函数不应依赖 enum case 条件来让返回推导排除未选中 `template if` 分支；需要这种分支时应显式写返回类型，或使用类型相等 / concept 条件。完整 `template if` 条件边界见 [flow.md](flow.md#template-if)。
- `!` never 表达式不决定普通返回类型；例如 `match` 的一个分支返回 `panic(...)`，其它分支返回 `i32` 时，整体可推导为 `i32`。如果所有观察到的返回都是 `!`，则函数返回类型推导为 `!`。
- 普通函数、关联函数、成员函数、扩展方法、函数类型值、函数指针值和闭包调用都会按调用表达式的结果类型参与推导；例如 `return callback();` 可以从 `callback: f() -> i32` 推导为 `i32`。
- 泛型函数、泛型关联函数和泛型成员函数会先按调用实参实例化具体函数实例，再使用该实例的返回类型；例如 `return id(1);` 会通过 `id<i32>` 推导为 `i32`，`return box::make(1);` 会推导为具体 `box<i32>`。
- 编译器内建调用和内建表达式按自身固定结果参与推导：`builtin(expr)` 推导为 `expr` 的类型，`alloc<T>(count)` 和 `new T{...}` 推导为对应的 `T*`，`free(ptr)`、`construct_at(ptr, value)`、`destroy_at(ptr)`、`delete ptr` 和 `assert(condition)` 推导为 `unit`，`panic(message)` 和 `unreachable()` 推导为 `!`。
- `storage` 的 `data()` / `slot()` 是成员形式的编译器 builtin，也按固定结果参与推导。`return value.data();` 和 `return value.slot(i);` 推导为元素指针；receiver 为 const storage 时结果是 `T const*`，否则是 `T*`。这些调用仍受 storage 成员规则限制，例如多 slot storage 的 `slot()` 必须带整数下标。
- 赋值表达式在普通表达式语义中按实际选中的赋值 operator 决定结果类型，见 [operator.md](operator.md#赋值运算)。省略返回类型时，不建议让 `return (left = right);` 这类赋值表达式决定公共 API 返回类型；需要稳定接口时显式写返回类型。
- `return ref x;` 和 `return const ref x;` 才能推导引用返回；普通 `return x;` 读取出值类型。

返回推导不是只扫源码中的 `return` token。为了给 `return value;` 求表达式类型，当前实现会按语句顺序建立一套临时语义作用域：

- 函数参数在函数体开始前可见；块语句、块表达式、范围 `for` body 和 `match` arm 都建立嵌套作用域，兄弟分支中的局部名字不会互相可见。
- 局部变量声明先检查 initializer，再把声明名放入后续作用域。因此声明名不能在自己的 initializer 里参与返回推导；`let value = { return value; };` 中的 `return value;` 不会绑定到这个新声明的 `value`。
- 普通值名字只在当前词法层级检查重复；同一 block 中重复声明 `let value` 会报 `duplicate_symbol`，内层 block、`for` body、`match` arm 或 lambda 体里的同名 binding 会遮蔽外层值名。局部值名也可以遮蔽当前可见的顶层函数名或导入函数名；遮蔽后，裸 `name` 按局部值解析，`name(args...)` 也先按这个局部值是否可调用检查，不会跳过它去调用同名顶层函数。
- 局部 `type` 别名会进入后续类型位置；后续变量声明、聚合初始化或返回表达式中的类型查询可以使用它。局部类型别名使用独立的类型名字作用域：同一层级内重复声明同名 `type` 会报 `duplicate_symbol`，内层块可以遮蔽外层类型别名；类型名字和普通值名字不互相占用，因此 `type item = i32; let item: item = 1;` 中右侧类型位置的 `item` 和左侧 binding 名可以同时存在。
- 元组解构声明会把每个解构名按对应 tuple 元素类型放入后续作用域；`let ref` / `const ref` 解构按普通引用和 const 规则记录引用类型。initializer 不是 tuple、元素数量不匹配或多余名字等错误仍由普通语义检查报告，返回推导不会把错误解构当成可依赖语义。
- 范围 `for` 的循环变量只在循环体内可见；`for(let ref item : range)` 依赖 range 元素本身是引用类型，否则普通语义检查仍会报错。裸 iterator 不能作为合法 range；范围表达式必须是数组、`iterable` 或 `const_iterable`。
- `match` case payload 绑定只在对应 arm 右侧表达式内可见。错误 case、payload 数量不匹配或多余绑定名不产生可依赖的局部名字。

返回推导不会吞掉调用表达式本身的语义错误。`return builtin();` 仍然报 `argument_count_mismatch`，`return alloc(1);` 仍然报 `invalid_type_argument`，`return construct_at(pointer, true);` 仍然按 pointer 目标类型检查右侧值，`return item.data(0);` 仍然报 storage `data()` 参数数量错误，`return maker::missing();` 仍然报未知关联函数，而不是把当前函数模糊地推导成 `error` 或 `unit`。

泛型函数省略返回类型时，推导发生在每个具体函数实例上，而不是为泛型定义本身产生一个全局返回类型。也就是说，先确定类型实参、替换参数类型、选择 `template if` 分支并展开 `template for`，再收集该实例中的 `return`。固有 `impl` 中的泛型成员函数同样按具体成员函数实例推导；`impl<T> box<T> { value_or(self const&, fallback: T) { ... } }` 在 `box<i32>` 上调用时推导的是该 `i32` 实例的返回类型。

concept 函数要求、concept 默认函数和 `impl Concept for Type` 中的函数不走这套返回类型推导。它们的签名是 concept 协议的一部分；省略 `-> R` 表示返回内部 `unit`，而不是根据函数体推导。实现非 `unit` 要求时必须显式写出返回类型。完整边界见 [concept.md](concept.md)。

如果当前实例的返回表达式依赖另一个省略返回类型的泛型函数调用，编译器先推导被调泛型函数的具体实例返回类型，再把这个类型用于当前表达式检查。这个顺序对泛型 `variant` 的 `match` 尤其重要：被调函数可以先推导出 `choice<T>` 这样的具体 variant 类型，随后当前函数的 `match` 才能把 `.case(payload)` 绑定成替换后的 payload 类型，并用 arm 表达式继续推导当前函数返回类型。

参数包相关规则：

- `template for` 展开体中的 `return` 按具体展开后的语句参与推导；`return;` 仍贡献 `unit`。
- 值参数包或类型参数包为空时，展开体执行零次，因此展开体内的 `return` 不贡献返回类型。若函数只有一个空展开且没有其它 `return`，则该实例返回 `unit`；如果后面有 fallback `return 0;`，则按 fallback 推导。
- `match` 出现在 `template for` 内时，先按当前 pack 元素替换泛型 `variant` payload 类型，再按普通 `match` arm 类型统一规则推导表达式类型。
- 同一个具体函数实例中，多个展开产生的返回类型仍必须统一；例如 `first<T...>(values: T...) { template for(let value : values...) { return value; } }` 只有在该实例所有 `value` 的读出类型能统一时才合法。
- 泛型 lambda 当前不做返回类型推导，必须显式写 `-> R`，见 [lambda.md](lambda.md)。

### 返回值消除与目标位置初始化

语言保证一类 NRVO（named return value optimization）：当 `return local;` 满足下面条件时，`local` 直接作为函数返回对象本身，不发生语言级 copy 或 move。普通分组括号会被剥掉，因此 `return (local);` 与 `return local;` 等价。

第一版 NRVO 条件：

- `local` 必须是当前函数、lambda 或构造函数体内的非引用 local binding。
- `local` 不能是参数、`self`、捕获变量、字段、索引结果、函数调用结果或引用别名。
- `local` 不能是 `let static` / `const static` 局部；static local 有独立静态存储，不会被当作当前调用的返回对象存储。
- `return` 表达式不能是 `move local`、`ref local`、`const ref local`，也不能是成员访问、索引、调用或字面量。
- `local` 的读出类型必须等于声明/推导后的函数返回读出类型；如果需要类型转换，则不走 NRVO。
- 同一函数中所有带值 `return` 必须返回同一个 NRVO candidate；否则回退普通 `return` 规则。

`move` 是显式所有权转移。写 `return move local;` 表示用户选择移动，并明确放弃 NRVO。

NRVO 只识别源码上的整体局部 binding。引用别名、解构得到的名字、字段访问、下标访问和函数返回值即使最终指向同一块存储，也不会反向归并成这个 local 的 NRVO candidate。例如 `let ref alias = local; return alias;` 会按普通返回规则读取 `alias` 指向的值并需要 copy / move / 转换；它不是 `return local;`。如果函数返回引用，应显式写 `return ref local;` 或 `return const ref local;`，这走引用返回规则，不是 NRVO。

局部变量声明另有目标位置初始化规则。`let a = func();`、数组字面量、元组字面量和 `Type{...}` 结构化初始化会直接构造到局部变量的存储位置，不先产生用户可观察的中间对象再整体复制。这不是 NRVO，因为它不发生在 `return local;` 上；完整边界见 [初始化](initial.md#初始化表达式)。

赋值语句不属于目标位置直接初始化。`a = func();` 仍按普通赋值和转换规则处理。

构造函数不参与普通函数返回类型推导；构造函数返回类型固定为当前结构体类型，见 [struct.md](struct.md)。

## 默认初始化

默认初始化用于 `Type{}`、聚合初始化缺省字段、数组和元组元素递归初始化等场景。

| 类型 | 默认初始化结果 |
| --- | --- |
| `bool` | `false` |
| `i8 i16 i32 i64` | `0` |
| `u8 u16 u32 u64` | `0` |
| `isize` | `0` |
| `usize` | `0` |
| `f32 f64` | `0.0` |
| `char` | `'\0'` |
| `str` | `""` |
| `T*` | 空指针值 |
| `T&` | 不可默认初始化 |
| `f(...) -> R` | 不可默认初始化 |
| `f*(...) -> R` | 空函数指针值 |
| `[T; N]` | 每个元素按 `T` 默认初始化 |
| `storage T` / `storage [T; N]` | 创建有效 raw storage 对象；槽内对象生命周期未开始，字节内容未指定 |
| `(T1, T2)` | 每个元素按对应元素类型默认初始化 |
| `struct` | 按结构体初始化规则默认初始化 |
| `enum` | 不可默认初始化 |
| `type A = opaque T` | 按底层表示默认初始化，但仍保持 opaque 名义类型 |
| `variant` | 不可默认初始化，必须显式选择 case |
| `!` | 不可默认初始化 |

如果某个类型不可默认初始化，那么依赖它的默认初始化也失败。例如引用字段没有字段缺省表达式或显式初始化时，包含该字段的结构体不能完成默认初始化。

泛型参数本身不提供默认初始化能力。`T{}` 只有在泛型函数实例化并且当前实例中的 `T` 已经替换成可默认初始化的具体类型后才成立；不能把“任意 `T` 都可默认初始化”当作无约束泛型规则。

默认初始化只定义值如何产生，不改变 `let` / `const` 的初始化要求。需要默认值时，仍然必须显式写初始化表达式，例如：

```cp
let value: i32 = i32{};
let point = vec2{};
```

`Type{}` 产生的表达式类型就是花括号左侧写出的类型；它不会先默认初始化一个值，再自动按外层声明或参数的目标类型做隐式转换。`let wide: i64 = i32{};` 因此不是合法的“默认成 i32 再提升到 i64”，应写成 `let wide: i64 = i64{};`，或显式写 `(i32{}) as i64`。更完整的初始化表达式规则见 [initial.md](initial.md)。

源码层的 `Type{}` 初始化器在普通表达式位置必须从类型名或 `storage` 开始。内建标量名、`struct` 名、`variant` 名、opaque alias 名、类型别名名、带显式实参的泛型类型名，以及这些名字后的 `const`、`*` 和普通 `&` 后缀都属于这一路；例如 `i32{}`、`box<i32>{}`、`int_ptr{}` 或 `i32*{}` 可以进入类型初始化器解析。`move&` / `forward&` 这类引用后缀不是当前普通表达式位置的 `Type{}` 启动形状；引用类型本身也不可默认初始化，需要绑定已有左值或移动引用目标。裸复合类型语法不会在普通表达式位置直接启动初始化器：`[i32; 2]{}`、`(i32, bool){}` 和 `f*(i32) -> i32{}` 不是当前稳定写法。需要初始化数组、tuple 或函数指针这类复合类型时，先用 `type` 取别名再写 `alias{}`，或使用已有的上下文表达式，例如显式指针上下文里的 `nullptr`。`storage T{}` / `storage [T; N]{}` 是专门语法例外。

`Type{...}` 里的初始化器列表由同一个 parser 入口读取。列表可以全是位置项 `expr`，也可以全是命名项 `.field = expr`，但不能在同一个列表中混用两种形式；`T{1, .field = 2}` 和 `T{ .field = 1, 2 }` 都会在 parser 阶段报 `unexpected_token`，不会进入后续的 `struct` 构造函数匹配、字段聚合、数组或 `str` 专用语义检查。各具体类型是否接受位置项、命名项、尾随逗号和省略项，再由下面的类型专门规则决定。

除 `struct`、数组、`str` 和 `storage` 的专门初始化路径外，`Type{}` 只表示空列表默认初始化，不接受位置项或命名项。也就是说，`i32{1}`、`i32*{nullptr}`、`callback{nullptr}`、`opaque_id{1}` 这类写法当前都会按“非 struct 默认初始化只能为空”拒绝；要从已有值构造这些类型，应使用普通表达式、目标类型上下文、显式 `as` 或对应类型自己的公开构造 API。

裸 `{}` 不表示默认初始化。裸块的表达式规则见 [控制流](flow.md#函数返回和正常完成)。

数组默认初始化写作：

```cp
type i32x4 = [i32; 4];

let values = i32x4{};
let prefix = i32x4{1, 2};
```

数组类型初始化器创建长度为 `N` 的数组，每个元素按 `T` 的默认初始化规则初始化。如果 `T` 不可默认初始化，则 `[T; N]` 也不可默认初始化。

数组类型初始化器也可以带位置元素：

```cp
type i32x3 = [i32; 3];
type empty_i32_array = [i32; 0];

let exact = i32x3{1, 2, 3};
let padded = i32x3{1};
let empty = empty_i32_array{};
```

规则：

- 位置元素数量不能超过数组长度 `N`。
- 位置元素列表允许尾随逗号，例如 `i32x3{1, 2,}`。
- 每个显式元素按数组元素类型 `T` 做上下文检查。
- 显式元素少于 `N` 时，剩余元素按 `T` 默认初始化；如果 `T` 不可默认初始化，则初始化失败。
- `[T; 0]{}` 是显式零长数组初始化器，不需要初始化任何元素，因此即使 `T` 本身不可默认初始化也可以成立。这个边界只描述数组初始化器本身；结构体缺省字段、元组元素递归默认初始化等“默认初始化能力”检查仍按元素类型判断 `[T; N]` 是否可默认初始化。
- 不支持 named field 形式，例如 `[i32; 2]{ .0 = 1 }`。
- 数组类型初始化器复用通用 `{...}` 初始化器列表解析；位置项和命名项不能混用，例如 `i32x3{1, .field = 2}` 或 `i32x3{ .field = 1, 2 }` 会在 parser 阶段作为混合初始化器列表报错，不进入数组语义检查。
- 普通表达式位置的类型初始化器当前必须以类型名或 `storage` 开头；裸 `[T; N]{...}` 不作为普通表达式解析。
- `array<T, N>{...}` 是当前可写的命名形态数组类型初始化器，和先给 `[T; N]` 取别名后写 `alias{...}` 等价。这里的 `array` 仍只是类型位置小内建，不是运行时构造函数；长度必须写成类型实参中的整数 literal 或整数 const 泛型参数，不能省略，也不能来自运行时表达式。
- `new [T; N]{...}` 是例外，因为 `new` 会先解析后面的 raw array type，再把 `{...}` 当作数组类型初始化器。
- `storage [T; N]{}` 不是数组元素初始化器；它只能写空 `{}`，表示创建 raw storage。

数组字面量 `[1, 2, 3]` 和数组类型初始化器 `i32x3{1, 2, 3}` 都能构造数组，但用途不同：前者可以在无上下文时自行推导元素类型和长度，后者显式写出目标数组类型，并允许省略后缀元素由默认初始化补齐。

## 聚合字面量

数组字面量使用 `[ ... ]`：

```cp
let data: [i32; 4] = [1, 2, 3, 4];
```

有上下文类型时：

- 目标类型必须是 `[T; N]`。
- 字面量长度必须等于 `N`。
- 每个元素按目标元素类型 `T` 做上下文检查。
- 如果长度不等于 `N`，当前语义层仍会继续用目标元素类型检查所有显式元素，并把该字面量按目标数组类型恢复，用于后续诊断和推导；这只是错误恢复，不表示长度不匹配的数组字面量合法。
- 如果上下文存在但不是数组类型，例如 `let value: i32 = []`，会先报“array literal requires array context”。空数组在这种错误上下文中仍没有元素类型可推导，因此还会报“empty aggregate literal requires a contextual type”。

没有上下文类型时：

- 非空数组字面量自行推导为 `[T; N]`。
- `N` 为元素个数。
- `T` 由元素类型统一得到：完全相同的类型直接统一；全是整数内建类型时按 `integer_rank` 取较高 rank 的整数类型，rank 相同时保留较早元素的类型；全是浮点内建类型时取较高 rank 的浮点类型。
  整数和浮点不会混合统一，`[1, 2.0]` 会报异构聚合错误；`bool`、`char`、`str`、指针、数组、tuple、`struct`、`variant` 等非数值或不同名义类型也必须逐项完全相同。
- 空 `[]` 报错，因为无法推导元素类型。

元组字面量使用 `(a, b, c)`，单元素元组必须写 trailing comma：

```cp
let pair = (1, true);       // (i32, bool)
let single: (i32,) = (1,);  // 单元素 tuple
let grouped = (1);          // 分组表达式，类型仍是 i32
```

元组字面量的上下文规则：

- 有上下文类型时，目标类型必须是 tuple。
- 字面量元素数量必须等于目标 tuple arity。
- 每个元素按目标位置的元素类型做上下文检查。
- 如果目标是 tuple 但长度不匹配，当前语义层仍按共同前缀逐元素使用目标元素类型检查；多余的字面量元素按无上下文表达式检查，缺失的目标元素不做默认初始化补齐。整个 tuple 字面量随后按目标 tuple 类型恢复，用于继续诊断。
- 如果上下文存在但不是 tuple，例如 `let value: i32 = (1, true);`，会先报“tuple literal requires tuple context”，然后按无上下文 tuple 字面量推导出 `(i32, bool)`，外层声明再报告它不能初始化 `i32`。
- 没有上下文类型时，字面量类型就是各元素读出类型组成的具体 tuple；不同位置可以是不同类型，不需要像数组那样统一为同一个元素类型。
- 当前不支持空 tuple 类型或空 tuple 字面量；`()` 不是值表达式，`(T,)` / `(value,)` 才是一元 tuple。

元组字面量没有命名字段、rest 元素、展开元素或数组式补零规则。需要访问元素时使用编译期数字字段 `.0`、`.1` 等；`tuple[0]` 不是内建 tuple 访问。

tuple 类型别名可以用于空的默认初始化，例如 `type pair_t = (i32, bool); let pair = pair_t{};`，前提是每个元素都可默认初始化。但 tuple 没有数组那样的专门位置初始化器；`pair_t{1, true}`、`pair_t{ .0 = 1 }` 或 `tuple<i32, bool>{1, true}` 都不是当前构造 tuple 值的公开写法。需要提供元素值时，使用 tuple 字面量 `(1, true)`，并让声明类型、参数类型或返回类型提供上下文。

结构体初始化使用 `type_name{ ... }`，见 [struct.md](struct.md)。它和数组字面量是不同语法，不引入 C++ 式 `initializer_list` 特权。

## 数组操作

数组提供基础内建操作：下标访问、默认初始化和范围遍历。数组本身没有内建 `size()`、`data()`、`front()`、`back()`、`as_slice()`、`fill()`、`map()` 或 `iter()` 成员；未导入相关标准库模块时，`values.size()` / `values.data()` 会按普通未知成员或 UFCS 查找失败处理。当前 `std.memory.span` 给 `[T; N]` 实现 `contiguous_mutable_range`，导入 `std.memory`、`std.algorithm` 或更高层 `std` 后，标准库算法可以通过这个协议使用数组的 `.data()` / `.size()`；`front`、`back`、slice、fill 和 map 仍不是第一版数组标准库能力。

数组是普通值类型。可写数组 binding 支持整体赋值，右侧必须能转换为完全相同的 `[T; N]`：

```cp
let values = [1, 2];
values = [3, 4];
```

数组长度是类型的一部分，因此 `[i32; 2]` 不能整体赋值为 `[i32; 3]`。数组不会退化为指针；需要指针时显式取址或使用标准库 view/slice。

元组同样暂时保持内建或编译器认识的异构聚合，因为当前用户层还不能表达 pack 字段 `fields: T...`。未来支持 pack field expansion 后，可以再考虑迁移为 compiler-recognized std type。

### 下标访问

数组下标表达式写作：

```cp
let x = values[0];
values[1] = x + 10;
```

规则：

- 内建 `a[i]` 要求 `a` 的类型是 `[T; N]`。
- 下标 `i` 必须是整数类型。
- `a[i]` 的结果类型是 `T`。
- 如果 `a` 是可写左值，则 `a[i]` 是可写左值。
- 如果 `a` 是 const 值或 const 引用，则 `a[i]` 只能读取，不能赋值。
- 当前静态越界检查只识别语法上直接可见的整数 literal 下标，以及一层前缀 `-` 包住的整数 literal。例如 `values[3]` 和 `values[-1]` 可以在语义阶段按 `[0, N)` 检查；`values[1 + 2]`、`values[(3)]`、`values[index_const]` 即使人眼能判断，也不会走这条静态边界检查。
- 其它整数下标的越界行为定义为调用者违反前置条件；checked 第一版会在运行时触发 panic。

这一节只描述数组内建路径。完整下标表达式还先处理 `str` 和指针内建下标；只有目标类型不是数组、`str` 或指针时，才查找用户 `operator []`。如果目标已经是这些内建下标类型，索引类型错误、数组常量越界或 `str` 只读结果都按内建规则诊断，不会回退到用户 `operator []`。

单独的 `a[]` 不是表达式。数组访问必须写成带下标的 `a[index]`。用户自定义类型可以通过 `operator []` 支持下标访问，见 [operator.md](operator.md)。

## 元组操作

元组提供基础内建操作：字面量构造、默认初始化、编译期字段访问和简单解构声明。第一版标准库还没有给 tuple 提供 `size()`、`front()`、`back()`、`get<N>`、`apply`、`map`、`zip`、`fold`、比较或 hash 这类高层接口；当前也没有把 tuple 自动视为 range 或可比较对象。需要读取元素时使用 `.0` / `.1` 等编译期字段，或用解构声明把元素拆成局部名字。

元组是普通值类型。可写元组 binding 支持整体赋值，右侧必须能转换为相同 arity、逐元素兼容的元组类型：

```cp
let pair = (1, 2);
pair = (3, 4);
```

不同长度的元组不是同一类值；`(i32, i32)` 不能整体赋值为 `(i32, i32, i32)`。

### 编译期字段访问

元组字段访问使用 `.0` / `.1` 语法：

```cp
let pair: (i32, f64) = (1, 2.0);
let first = pair.0;
let second = pair.1;
pair.0 = first + 1;
```

规则：

- `t.N` 要求 `t` 是元组类型。
- 字段编号 `N` 必须是成员名位置的十进制数字，并且必须在 `[0, tuple_length)` 范围内。`pair.0` 可用，`pair.02` 会先按数字 token 报前导零错误，`pair.first` 不走 tuple 字段路径，当前会继续按普通成员访问报告“field access requires a struct value”。
- `t.N` 的结果类型是第 `N` 个元素类型。
- 如果 `t` 是可写左值，则 `t.N` 是可写左值。
- 如果 `t` 是 const 值或 const 引用，则 `t.N` 只能读取，不能赋值。
- 如果第 `N` 个元素类型本身是引用类型，字段访问会保留这个引用元素的左值性，而不是只看 tuple 对象本身。例如 `make(value).0 = 3` 在 `make` 返回 `(i32&,)` 时当前可以写入 `value`，即使 `make(value)` 这个 tuple 结果不是可写局部；若元素类型是 `i32 const&`，则字段仍按只读引用处理。

因为元组是异构类型，元组不支持运行时整数下标。运行时选择异构值应使用 `variant` 或标准库抽象，而不是元组字段访问。

### 简单解构声明

元组可以在 `let` / `const` 声明中做简单解构：

```cp
let pair = (1, 2.0);
let (count, ratio) = pair;
const (x, y, z) = (1, 2, 3);
let ref (first, second) = pair;
const ref (readonly_first, readonly_second) = pair;
```

规则：

- 解构目标必须是元组类型。
- 绑定个数必须和元组元素个数完全一致。
- 每个绑定的类型由对应元素类型推导。
- 绑定名进入当前局部作用域，按普通局部名字规则检查重复和 shadow。`let (x, x) = pair;` 会按重复局部名报错，而不是把两个元素合并成同一个 binding。
- 不支持 `static` 解构声明。
- `let` 解构产生可重新赋值的 binding；`const` 解构产生不可重新赋值的 binding。
- 没有 `ref` 时，解构绑定是新 binding，不是引用别名。
- 带 `ref` 时，解构要求初始化表达式是左值，并为每个元素产生引用 binding。
- `let ref` 解构保留元素的 target const：可写元素得到 `T&`，只读元素得到 `T const&`。
- `const ref` 解构为每个元素产生只读引用，结果类型为 `T const&`。
- 只支持简单标识符列表，不支持嵌套解构、忽略绑定、剩余绑定、字段重命名或 pattern guard。

错误恢复边界：

- 初始化表达式不是元组时，整条解构声明只报类型错误，不建立任何解构 binding。
- 绑定个数和元组元素个数不一致时会报聚合长度错误；当前实现仍按二者共同前缀建立可用 binding，多余元素没有名字，多余绑定名也不会凭空进入作用域。
- `ref` 解构的 initializer 不是左值时会报赋值目标错误；类型检查仍按元素类型继续建立引用形态 binding，以便后续诊断能继续进行。这是错误恢复，不是合法程序语义。

## 引用、指针与 const

引用和指针类型写作：

```cp
i32&
i32*
i32**
i32 const&
i32 const*
i32 const**&
i32*&
i32**&
i32 like*
i32 like**&
i32 like&
i32 move&
```

`const` 分为 binding const 和 target const。

### binding const

`const name = ...` 或函数参数中的 `const name: T` 是 binding const，表示这个名字绑定不可重新赋值：

```cp
const value = 1;

view(const p: i32 const*) {
}
```

局部声明可以在 `let` / `const` 后添加 `ref`，表示引用 binding：

```cp
let value = 1;
let ref writable = value;
const ref readonly = value;
```

`let ref name = expr` 要求 `expr` 是左值，并按表达式的可写性推导为 `T&` 或 `T const&`。`const ref name = expr` 同样要求 `expr` 是左值，但结果总是只读引用 `T const&`。

表达式位置也可以显式借用：

```cp
foo(ref value);
bar(const ref value);
```

`ref expr` 和 `const ref expr` 的匹配规则见 [ownership.md](ownership.md)。

### target const

`type const*` / `type const&` 中的 `const` 是 target const，表示指针或引用最终指向的基础值不可写。不管有多少级 `*` / `&`，它都约束最终目标值，而不是中间指针本身。

```cp
update(p: i32*) {
}

view(const p: i32 const*) {
}

borrow(value: i32 const&) {
}
```

类型语法约束为：

```text
Parameter   -> const? identifier : Type
Declaration -> (let | const) RefMode? BindingPattern (: Type)? = Expression
RefMode     -> ref
BindingPattern -> identifier | ( identifier (, identifier)* )

Type            -> TypeBase TargetQualifier? TypeSuffix
TargetQualifier -> const | like | forward
TypeSuffix      -> *+ &? | & | move & | forward &
```

这套后缀语法描述普通 `TypeBase`，不描述函数类型分支。函数类型使用单独的 `f(...) -> R` / `f*(...) -> R` 语法；`f*` 是唯一的函数指针拼写。

`TargetQualifier` 只有在 `TypeSuffix` 非空时合法。因此 `i32 const`、`i32 like`、`i32* const`、`i32& const` 都不是合法类型写法。

`like` 是 receiver-const 转发限定符，可以写作 `T like*`、`T like**`、`T like*&` 或 `T like&`。它只负责把当前 `self like&` receiver 的 constness 转发到对应指针/引用目标，不表达 move，也不改变基础类型。`move &` 只允许写作 `T move&`，表示移动引用；`forward &` 只允许写作依赖类型上的 `T forward&`，表示泛型转发引用。第一版不允许 `T const move&`、`T like move&` 或 `T const forward&`。具体规则见 [ownership.md](ownership.md)。当前不支持 C++ 式指针自身 const，也不支持 `volatile` / `restrict`。

类型后缀作用在完整的 `TypeBase` 上，不只作用于普通命名类型。因此数组、storage、元组和分组类型也遵守完全相同的后缀校验：

```cp
let array_ref: [i32; 2]& = ref values;
let storage_ptr: storage [i32; 2]* = &slots;
let tuple_ref: (i32, bool) const& = const ref pair;
let grouped_ref: (i32) const& = const ref value;
```

这些写法中的 `&`、`*`、`const`、`like`、`move&` 和 `forward&` 都修饰整个复合类型。`[i32; 2] like`、`storage [i32; 2] like` 或 `(i32) like` 没有指针/引用后缀，因此不合法；`[i32; 2] forward&`、`(i32, bool) forward&` 和 `i32 forward&` 的被引用类型不是依赖类型，因此也不合法。泛型上下文里如果被引用的完整类型仍依赖泛型参数，则可以使用同一规则表达转发引用，例如 `[T; N] forward&` 或 `(T, U) forward&`。

`move&` 和 `forward&` 都不能和 `const` 或 `like` 同时出现在同一个类型后缀组里。`forward` 只能写成 `forward&`，不能写成 `forward*`、`forward**` 或 `forward*&`。分组类型只改变语法结合，例如 `(i32) const&` 与 `i32 const&` 表示同一类引用；它不会把 `i32 const` 变成合法类型。

### 指针运算与解引用

指针算术、比较和解引用语义保持接近 C++。裸指针是底层 unsafe 能力，编译器只做类型检查，不完整证明对象生命周期、边界、悬垂或别名安全。

取址和解引用：

```cp
let value = 1;
let p: i32* = &value;
let x = *p;
*p = 2;
```

规则：

- `&expr` 要求 `expr` 是左值，结果类型为 `T*`。如果 `expr` 只能只读访问，结果类型为 `T const*`。
- `*ptr` 要求 `ptr` 是 `T*` 或 `T const*`。
- `*ptr` 的结果是左值。`T*` 解引用得到可写 `T` 左值；`T const*` 解引用得到只读 `T` 左值。
- 解引用空指针、悬垂指针、未对齐指针、未开始对象生命周期的存储或不满足目标类型别名规则的地址，属于底层 unsafe 契约。

指针算术：

```cp
let next = p + 1;
let prev = next - 1;
let distance: isize = next - p;
let item = p[0];
```

规则：

- `p + n`、`n + p` 和 `p - n` 要求 `p` 是 `T*`，`n` 是整数类型，结果为 `T*`。
- 指针加减的步长是 `T` 的元素大小，不是字节大小。
- `p2 - p1` 要求两侧都是相同目标类型的指针，结果类型为 `isize`，表示元素距离。
- `p[i]` 要求 `p` 是 `T*` 或 `T const*`，`i` 是整数类型，地址计算等价于 `*(p + i)`。
- `T*` 下标通常得到可写 `T` 左值；`T const*` 下标得到只读 `T` 左值。`const p: T*` 只是绑定不可重新赋值，不会让 `p[i]` 只读；但若下标目标表达式本身是只读引用，例如参数 `p: (T*) const&`，当前实现会让 `p[i]` 只读，而 `*(p + i)` 仍可通过新产生的指针值写入 `T`。
- 不支持 `i[p]`。整数表达式不能作为指针下标目标。
- 指针差值只对同一数组对象或同一连续分配对象内的两个元素位置，以及 one-past 位置有定义。
- 产生或使用越出同一数组对象允许范围的指针值，行为按 C++ 指针算术边界处理；编译器不尝试静态证明。

指针比较：

- `p == q`、`p != q` 可比较相同目标类型的指针，结果为 `bool`。
- `< <= > >=` 可比较相同目标类型的指针，结果为 `bool`。
- 具体指针可以和 `nullptr` 做 `== != < <= > >=`；`nullptr` 会按另一侧的具体指针类型参与比较。
- 裸 `nullptr` 和裸 `nullptr` 之间没有目标指针类型，不能写 `nullptr == nullptr` 或 `nullptr < nullptr`。
- 指针不支持内建 `<=>`，即使两侧目标类型相同；需要三路比较时必须通过用户抽象或可见 operator 另行建模。
- 指针有序比较只对同一数组对象或同一连续分配对象内的位置有定义；其它来源的指针有序比较属于底层 unsafe 契约。

## 转换

显式转换只有 `as` 一种写法：

```cp
value as i32
```

显式转换的完整白名单见 [cast.md](cast.md)：已可隐式转换的类型、整数/浮点数值互转、指针到指针、`enum` 到声明的精确底层整数，以及 opaque alias 声明源文件内部的底层互转。

隐式转换只允许当前实现明确列出的目标位置转换，不采用宽泛的 C++ 式隐式转换。目标位置包括变量初始化、赋值、函数实参、返回语句、聚合字段、构造参数和 operator 候选匹配。

隐式转换有两条入口：

- 目标是引用类型时，先看源表达式的值类别、是否是显式借用、是否只读，再决定能否绑定到目标引用。
- 目标不是引用类型时，先拒绝顶层显式借用表达式，然后按源类型和目标类型的读出类型做值转换检查。

这里的“读出类型”会剥掉一层或多层引用。例如普通引用 binding `alias: T&` 在按值目标位置使用时按 `T` 参与检查；`T&&` 这类多层引用形状也会继续剥到最终被引用类型。读出只影响类型检查，不表示产生新的左值，也不保留原引用的可写性。

当前隐式转换白名单：

- 完全相同类型，或按值目标位置的读出类型相同。`let value: T = alias;` 可以从 `alias: T&` 读出 `T`，但 `let value: T = ref object;` 不合法，因为 `ref object` 是显式借用表达式，只能匹配引用目标位置。
- `!` never 源表达式到任意目标类型。这只表示控制流不会正常到达使用点；普通值不能反向隐式转换成 `!`。
- `nullptr` 到任意具体指针类型，包括数据指针 `T*` / `T const*` 和函数指针 `f*(...) -> R`。裸 `nullptr` 没有目标类型时不能自行推导，也不会变成非空函数类型 `f(...) -> R`。
- 非空函数类型 `f(...) -> R` 到目标为同一函数类型 pointee 的 `f*(...) -> R` 函数指针。参数列表、返回类型和指针目标必须完全相同；不会做参数转换、返回类型转换、调用约定转换，也不会把函数指针反向隐式转换成函数类型。
- 指针和引用的 const / like 资格转换：可以给目标增加只读资格，不能通过隐式转换去掉只读资格；嵌套指针会递归检查 pointee 资格。
- 目标位置是引用类型时，按表达式类别绑定，而不是先读出值再转换：可写左值可以绑定到 `T&` 或 `T const&`，只读左值只能绑定到 `T const&`，临时值和普通返回值只能绑定到 `T const&` 或 `T move&`，`move expr` 只能绑定到 `T move&`。
- 目标位置是按值类型时，`move expr` 会读出被移动对象的值类型并继续按普通转换检查；例如 `move value` 可以传给按值 `T` 参数或返回 `T`，后续 copy/move constructor 选择见 [ownership.md](ownership.md)。这不表示 `move expr` 能绑定到 `T const&`，引用目标仍按上一条规则处理。
- `T forward&` 目标位置可以接受可写左值、const 左值、临时值、普通返回值和 `move expr`，前提是目标 `T` 是依赖类型或省略参数类型引入的隐藏类型参数。实际调用实例会记录当前实参的转发类别。
- `T like&` 目标位置只由 `self like&` receiver 规则产生；它要求源表达式是左值，并按当前 receiver 的 constness 物化成 `T&` 或 `T const&`。
- 整数类型之间互相隐式转换，包括窄化和有符号/无符号变化。这里的“整数类型”只包括 `i8/i16/i32/i64/u8/u16/u32/u64/isize/usize`；`bool` 和 `char` 虽然是内建标量类型，但当前不属于整数转换族。
- 浮点类型只允许向不低于源 rank 的浮点类型隐式转换，例如 `f32 -> f64` 合法，`f64 -> f32` 不合法。

不属于隐式转换：

- `ref expr` 或 `const ref expr` 到按值 `T` 参数、变量或返回类型；显式借用结果只能匹配引用目标位置。
- 整数和浮点之间的目标位置隐式转换；需要写 `as`。
- `bool` / `char` 和整数之间的目标位置隐式转换；例如 `let x: i32 = 'a';`、`let flag: bool = 1;` 都不是当前合法转换。
- 普通值到 `!` never；`let x: ! = 1;` 和 `return 1;` 到显式 `-> !` 函数都不合法。需要发散结果时应使用 `panic(...)`、`unreachable()` 或其它不会正常返回的控制流。
- 指针和整数之间互转。
- 普通指针之间的地址重解释隐式转换。`i32*` 可以隐式流向 `i32 const*` 这类只增加 target const 的形状，但不能隐式流向 `u8*`、`void*`、其它数据指针、或签名不同的函数指针；需要地址重解释时必须写 `as`，且仍不证明目标地址满足对象生命周期、对齐或调用契约。
- `str` 到 `string`，或其它通过构造函数实现的转换；需要写 `string{text}` 这类显式构造。
- `enum` 到整数作为隐式转换；需要写 `value as underlying_type`。
- opaque alias 和底层类型之间的隐式转换；即使在声明该 alias 的源文件内部，也必须写 `as`。
- 数组、元组、结构体、variant 之间按字段或形状自动转换。

运算表达式内部的共同数值类型不是目标位置隐式转换。例如 `1 + 2.0` 可以按数值运算规则产生浮点结果，但 `let x: f64 = 1;` 仍不会因为整数到浮点的目标位置隐式转换而成立。

## 控制流类型规则

`if`、`while` 和 `do while` 的条件表达式必须是 `bool`。

`for` 当前仅支持范围形式：

```cp
for(let value : values) {
}
```

范围表达式的目标语义基于 [iteration.md](iteration.md)：表达式必须是内建数组、实现 `iterable`，或在只读上下文中实现 `const_iterable`。实现 `iterator` 的游标本身不能直接作为 range-for 的范围表达式。

range-for 的绑定推导与声明绑定保持同一方向：没有 `ref` 时按值绑定，`const` 只约束循环变量本身；显式 `ref` 时才保留 iterator item 的引用语义。也就是说 `for(const value : values)` 的 `value` 是 `read_type(iter_item)` 的 const 值，而 `for(const ref value : values)` 才是只读引用。

`break` 和 `continue` 必须位于循环中；带 label 时，label 必须能解析到外层带标签的 `for`。

## 运算符

运算符优先级由 parser 的共享表定义，整体与 C++ 接近，但逻辑运算使用 `and` / `or` / `not`。

算术运算要求数值类型：

- 整数族内部按 `integer_rank` 取 rank 较高的一侧；rank 相同时保留左侧类型。`i32 + i64` 的结果是 `i64`，`u32 + i32` 这类同 rank 混合保留左侧类型。
- 浮点族内部按 `float_rank` 取 rank 较高的一侧，`f32 + f64` 的结果是 `f64`。
- 整数和浮点混合时按数值运算规则走浮点结果。整数在 `float_rank` 中按 rank 0 处理，因此 `i32 + f32` 和 `i64 + f32` 的结果都是 `f32`，`i32 + f64` 的结果是 `f64`。
- 结果类型为统一后的类型。

这个共同数值类型只服务当前运算表达式，不开放新的目标位置隐式转换。`let value: f64 = 1;` 仍不合法；需要把整数值放进 `f64` 目标位置时必须写 `1 as f64`，或让它先参与一个真正的数值运算表达式。

`== != < <= > >=` 这类二值比较结果为 `bool`。`<=>` 是三路比较 operator，不属于这个 `bool` 结果规则：同类型 `enum` 的内建 `<=>` 返回 `weak_ordering`，其它类型需要用户或标准库提供可见 operator，完整边界见 [operator.md](operator.md#比较运算)。逻辑运算要求 `bool` 操作数，结果为 `bool`。

下标运算 `value[index]` 是后缀表达式。内建下标支持 `[T; N]`、`str` 和指针；用户自定义类型可以通过 `operator []` 提供下标能力，具体规则见 [operator.md](operator.md)。

自增和自减支持前置 `++value` / `--value` 与后置 `value++` / `value--`：

- 操作数必须是可写左值。
- 不能作用于 `const` binding 或 const target。
- 内建路径只允许整数类型。
- 非内建类型通过 `operator prefix ++` / `operator postfix ++`、`operator prefix --` / `operator postfix --` 参与重载解析；声明时必须写出 `prefix` 或 `postfix`。
- 前置形式先更新并产生更新后的自身引用；内建前置结果为 `T&`。
- 后置形式先产生旧值再更新；内建后置结果为 `T`，结果本身不是左值。

赋值规则：

- 赋值左侧必须是左值。
- 不能给 `const` binding 重新赋值。
- 复合赋值优先查找对应 `operator +=`、`operator -=` 等；没有用户定义 operator 时，只尝试内建二元运算再写回左侧，例如数值、整数位运算和指针加减整数。用户自定义类型不自动退化为 `left = left op right`。
