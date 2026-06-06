# `extern "C"`

本文档记录 KCP 与 C ABI 互操作的最小设计。`extern "C"` 用来声明或定义使用 C 符号名和 C ABI 边界的顶层自由函数。

`extern` 是上下文关键字，只在顶层函数声明或定义前具有特殊含义。ABI 字符串第一版只接受 `"C"`。

第一版目标是支持标准库和 runtime 的低层互操作，例如 `std.io` 的字符输出和 `std.fs` 的文件读写。它不是完整 FFI 系统，也不试图一次性支持所有 C 类型、结构体布局或平台 ABI 细节。

## 设计目标

`extern "C"` 解决两件事：

```text
符号名: 使用 C 符号名，不使用 KCP 模块名 mangling
ABI 边界: 只允许一组明确的 C-compatible 参数和返回类型
```

`export` 和 `extern "C"` 是两个独立概念：

- `export` 控制 KCP 模块可见性。
- `extern "C"` 控制链接符号名和 ABI 边界。

因此：

```cp
extern "C" putchar(ch: i32) -> i32;
export extern "C" cp_put_char(ch: char) -> i32;
```

第一个声明只在当前 KCP 模块内部可见，但链接时仍引用外部 C 符号 `putchar`。

第二个声明会把 `cp_put_char` 暴露给导入当前模块的 KCP 代码，同时它的链接符号名仍是 `cp_put_char`。

和其它 `export` 声明一样，`export extern "C"` 只允许出现在具名模块中。匿名模块里可以写普通 `extern "C"` 声明或定义，但不能写 `export extern "C"`；否则会报 `export_requires_module`。

## 语法总览

```text
ExternFunctionDecl -> export? extern "C" FunctionHeader ;
ExternFunctionDef  -> export? extern "C" FunctionHeader Block
FunctionHeader     -> identifier FunctionParameterList ReturnType?
```

示例：

```cp
extern "C" putchar(ch: i32) -> i32;
extern "C" getchar() -> i32;

export extern "C" cp_put_char(ch: char) -> i32;

export extern "C" add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

声明形式以分号结束，不生成函数体；它只提供 KCP 调用点类型检查，并要求链接时存在同名 C 符号。

定义形式带函数体，由 KCP 编译器生成函数体，但函数使用 C 符号名和 C ABI 边界。

当前 parser 只用形状识别 `extern` 函数：顶层看到上下文标识符 `extern` 后紧跟字符串字面量，就按 extern 函数解析。字符串具体是不是 `"C"` 到语义阶段才检查；例如 `extern "Rust" item(...) -> ...;` 可以进入 AST，但会报告 `invalid_type_argument`。如果 `extern` 后没有字符串字面量，它只是普通 identifier 参与其它语法。

分号函数声明只对带 `extern_abi` 的顶层自由函数开放。普通 KCP 顶层函数没有 body 时不会被当作声明；`foo(x: i32) -> i32;` 在 parser 阶段会因为缺少 `{ ... }` 被拒绝。`extern "C"` 也不是 impl item 语法，`impl T { extern "C" f(...) { ... } }` 不会解析为成员函数。

## 名字和链接

普通 KCP 顶层函数仍按模块规则命名：

```cp
export module math;

export add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

上例导出的函数供 KCP 模块系统使用，不表示 C ABI，也不保证函数签名可被 C 代码直接调用。需要稳定的外部 C 符号时，必须显式写 `extern "C"`。

`extern "C"` 函数的外部 C 符号名默认就是源码中的函数名：

```cp
export module math;

export extern "C" add_i32(x: i32, y: i32) -> i32
{
    return x + y;
}
```

该函数的外部 C 符号名是 `add_i32`，C 代码可以按同名函数声明调用它。

当前源码层应依赖的符号边界：

- 匿名模块中的 `main` 对应外部符号 `main`。
- `extern "C"` 声明和定义都使用源码函数名作为外部 C 符号；声明要求链接时找到同名符号，定义会生成同名函数体。
- 普通 KCP 函数、成员函数、关联函数、构造函数、析构函数、impl operator、lambda 和泛型实例都不是稳定 C ABI surface。
- `export` 只控制 KCP 模块可见性；它不把普通 KCP 函数变成 C ABI，也不把泛型实例变成公共 C 符号。

需要稳定外部 ABI 时，只依赖 `extern "C"` 边界，不依赖普通 KCP 符号名。

`extern "C"` 函数在源码层仍是顶层函数声明，因此它的 KCP 名字参与当前模块的普通顶层重复检查。不能在同一模块里同时声明同名普通函数、同名 `extern "C"` 函数、同名类型或同名 concept；也不能用 `extern "C"` 绕过普通函数名冲突。`extern "C"` operator 不是当前语法。

同一链接单元内不应出现两个相同 C 符号名的 `extern "C"` 定义。多个模块中可以重复声明同一个外部 C 函数，但调用方必须自行保持声明签名一致。当前语义层不比较跨模块同名 C 声明的签名，也不做完整的全链接单元 C 符号查重；签名不一致属于外部 ABI 契约错误，重复定义通常会在链接阶段表现为符号冲突，而不是源语言层的专门诊断。

## 模块可见性

`extern "C"` 声明仍参与 KCP 模块系统：

```cp
export module std.io.c;

extern "C" putchar(ch: i32) -> i32;

export write_char(ch: char)
{
    putchar(ch as i32);
}
```

这里 `putchar` 只是 `std.io.c` 模块内部实现细节，导入者看不到它。

如果希望把外部声明继续暴露给其它 KCP 模块，可以显式写 `export extern "C"`：

```cp
export module c.stdio;

export extern "C" putchar(ch: i32) -> i32;
export extern "C" getchar() -> i32;
```

导入 `c.stdio` 的 KCP 文件可以直接调用 `putchar` 和 `getchar`。

## 第一版类型边界

第一版只允许当前语义层明确接受的 C-compatible 边界形状：

```text
return: void 或下列非 str 内建标量
parameter: 下列非 str 内建标量

bool char
i8 i16 i32 i64
u8 u16 u32 u64
isize usize
f32 f64

parameter/return: T*
parameter/return: f*(...) -> R  // 函数指针按“传递地址”浅层通过
```

其中：

- `void` 只在返回类型位置表示 C `void`；KCP 侧把它当作内部 `unit` 返回处理。
- 无函数体声明若省略返回类型，也按普通函数声明规则记录为内部 `unit`，对应 C `void`。
- 参数位置不允许 `void` / 内部 `unit`。
- `char` 按 1 字节整数传递。
- `nullptr` 是表达式字面量，不是可写 ABI 类型。需要传递空指针时，边界类型必须写成具体 `T*`，调用点传入 `nullptr` 或已经有具体指针类型的空指针值。
- `T*` 只检查指针本身的 ABI；`T` 指向的对象布局仍由调用者契约负责。
- 这里的 `T` 必须是 KCP 源码中能合法写出的具体类型。当前 `void` 只在返回类型位置表示 C `void`，不是普通类型，因此 `void*` 不是合法 KCP 类型；需要 C `void*` 风格的无类型地址边界时，第一版应使用约定好的字节指针类型，例如 `u8*` / `i8*`，或在 runtime 约定中自行说明。
- 指针的 pointee 可以是非 C-compatible 类型；这只表示“传地址”，不表示该对象可以按 C layout 解释。
- `T const*`、`T like*` 等 pointee 限定只影响 KCP 侧类型检查；ABI 检查仍把它们看作“传一个指针值”。C 侧是否把地址当作可写、只读或某种平台特定指针类型使用，不由当前语义层证明。
- `f*(...) -> R` 按函数指针值通过浅层 ABI 检查；它不是额外的深层 C prototype 校验入口。
- 语义层不会递归检查函数指针 pointee 的参数和返回类型是否都是 C-compatible。`f*(i32) -> i32`、`f*(str) -> str` 和其它函数指针在 C ABI 边界都只是传递一个函数地址；源码里的参数和返回类型只服务 KCP 侧调用点检查，不会自动生成或验证 C 侧 prototype。
- 因此 `extern "C"` 函数指针参数/返回值的真实调用约定需要边界双方保持一致。公开 ABI 时，应把函数指针类型限制在双方都明确约定的 C-compatible 参数和返回值上。
- 普通非 opaque `type` 别名和已经解析出的关联类型别名在 ABI 检查前按目标类型处理；`type fd = i32; extern "C" close(fd: fd) -> i32;` 按 `i32` 检查，因此合法。`enum`、`struct`、`variant` 和 opaque alias 是名义类型，按值不会因为底层或字段里有 C-compatible 类型就自动通过。

可以写的边界形状示例：

```cp
struct token {
    value: i32;
}

type handle = opaque usize;

extern "C" tick();
extern "C" truth(value: bool) -> bool;
extern "C" choose(callback: f*(i32) -> i32) -> f*(i32) -> i32;
extern "C" observe(callback: f*(i32) -> i32, token: token*, handle: handle*) -> void;
extern "C" add_default(value: i32, delta: i32 = 1) -> i32;
```

`token*` 和 `handle*` 合法，因为 ABI 上传递的是地址；`token` 和 `handle` by value 仍不合法。`add_default` 的默认参数只影响 KCP 调用点如何补齐实参；C ABI 签名仍是两个 `i32` 参数。

`extern "C"` 函数的默认参数沿用普通 KCP 函数默认参数规则：只能给尾部参数提供默认值，默认表达式按参数类型检查，并在 KCP 调用点补成真实实参。默认参数不进入函数类型或函数指针类型；如果把外部函数作为 `f*(...) -> R` 值传递或由 C 代码调用，调用方必须提供 ABI 签名中的完整参数列表。

当前指针检查是浅层的：

```cp
extern "C" raw_string_view(value: str*) -> void;          // 合法：只传 str 对象地址
extern "C" callback(callback: f*(str) -> str) -> void;    // 语义层合法：只把 callback 当作函数指针
extern "C" bad(value: str) -> void;                       // 错误：str by value 不是 C-compatible
```

上面第二个声明能够通过语义检查，不表示 `str` 已经拥有 C ABI，也不表示 C 侧会看到一个带 `str` prototype 的函数指针。它只说明边界上传递的是一个函数地址；该函数指针实际指向的函数是否能被 C 正确调用，是调用约定层面的外部契约。写公共 ABI 时应优先使用只含 C-compatible 参数和返回值的函数指针类型。

第一版不允许：

```text
str
[T; N] by value
(T1, T2)
struct by value
variant
enum by value, unless the value is explicitly cast to its underlying integer
opaque alias by value
!
T&
f(...) -> R
generic extern function
lambda / closure
concept method
impl member function
```

这些类型需要更完整的 ABI 设计后再开放。特别是 `str` 的语言设计是运行时长度字符串视图，而不是稳定的 C `char*`；因此不能直接把 `str` 当作 C 字符串 ABI 暴露。

opaque alias 当前也是名义类型，不能按底层类型自动穿过 `extern "C"` 边界。标准库需要跨 C ABI 暴露句柄或 bitset 时，应在 ABI 函数边界使用底层整数或指针类型，在 KCP 内部再用 opaque alias 封装。

普通 KCP `struct` 第一版不承诺 C ABI layout。当前没有 `repr(C)`、`extern struct` 或其它稳定结构体布局声明；需要跨 C 传递结构体时，只能传指针并由边界双方约定布局，或把字段拆成标量参数。

第一版 `extern "C"` 只能用于顶层自由函数，不能用于 `impl` 内成员函数、构造函数、析构函数、lambda 或 concept requirement。

典型不合法形式：

```cp
extern "C" abort_now() -> !;                    // 错误：never 不是 C ABI 返回类型
extern "C" call(callback: f(i32) -> i32) -> i32; // 错误：函数值不是 C 函数指针
extern "C" read(value: i32&) -> i32;             // 错误：KCP 引用不是 C ABI 类型
extern "C" take(value: token) -> i32;            // 错误：struct by value 不承诺 C layout
extern "C" read_array(value: [i32; 4]) -> i32;   // 错误：数组 by value 不开放 ABI
extern "C" read_pair(value: (i32, i32)) -> i32;  // 错误：tuple by value 不开放 ABI
```

`extern "C"` 函数边界必须是完全确定的具体类型：

- ABI 字符串只接受 `"C"`。
- 显式泛型参数不允许。
- `requires` 子句不允许。
- 返回类型不能依赖函数体推导；省略返回类型的带函数体 `extern "C"` 定义会在 ABI 检查时因为返回类型仍是 inferred 而报错；省略返回类型的无函数体声明则记录为 `void`。
- 参数类型和返回类型不能保留类型参数、推导类型或其它非 C-compatible 类型。
- 普通默认参数按 KCP 调用规则在调用点补全，不改变 C ABI 签名；C 调用方看不到默认参数，也不能省略 ABI 参数。

## 函数体规则

外部声明没有函数体：

```cp
extern "C" puts(text: i8*) -> i32;
```

它只提供类型检查和外部符号声明。链接器必须能从 libc、runtime 或显式链接参数中找到同名符号。

外部定义有函数体：

```cp
export extern "C" answer() -> i32
{
    return 42;
}
```

它由 KCP 编译器生成函数体，并以 C 符号名 `answer` 导出。

`extern "C"` 定义内部仍按普通 KCP 函数规则检查返回类型、局部变量、控制流、析构和表达式。区别只在函数边界的符号名和 ABI 约束。

无函数体 extern 声明没有函数体作用域，不会做局部变量、返回路径、析构或 `-> !` 正常完成检查；它只提供 KCP 调用点签名和外部符号需求。有函数体 extern 定义会完整进入普通函数体检查：`return` 必须能转到声明返回类型，非 `unit` 返回类型不能正常落出函数体，局部对象仍按普通清理规则处理。`extern "C"` 不会让函数体内部变成 C 语法，也不会关闭 KCP 的 ownership / control-flow 语义。

## 与标准库的关系

`std.io` 不应直接把 libc 细节暴露成公共接口。推荐分层：

```cp
export module std.io.c;

extern "C" putchar(ch: i32) -> i32;

export module std.io;

import std.io.c;

export write_char(ch: char)
{
    putchar(ch as i32);
}
```

更稳定的做法是通过 KCP runtime 包一层：

```cp
export module std.detail.runtime;

export extern "C" cp_io_write_char(stream: i32, ch: char) -> i32;
```

这样标准库依赖的是 KCP 自己的 runtime ABI，而不是不同平台 libc 的细节。
当前 `std.io.raw.write_char(out, ch)` 调用的就是 `cp_io_write_char(out.fd, ch)`；`std.io.raw.write_str(out, text)` 不走单独的 C string runtime 入口，而是按 `str` 迭代顺序逐字符调用 `write_char`。因此第一版 runtime 没有 `cp_put_cstr` 这类把 `str` 当 C string 暴露的符号。

`std.fs` 使用 runtime 的 `ptr + len` 路径 ABI：

```cp
extern "C" cp_file_open(path: char const*, path_len: usize, flags: u8) -> u8*;
extern "C" cp_file_close(handle: u8*) -> i32;
extern "C" cp_file_read(handle: u8*, data: u8*, len: usize, out_len: usize*) -> i32;
extern "C" cp_file_write(handle: u8*, data: u8 const*, len: usize, out_len: usize*) -> i32;
```

这里 `path` 不是 C string；runtime 内部负责复制并补 trailing nul，内部 nul 路径打开失败。`cp_file_read` / `cp_file_write` 要求 `handle`、数据指针和 `out_len` 都非 null，即使 `len == 0` 也一样；任一为空都会返回非 0。`cp_file_read` 把 `fread` 实际字节数写入 `out_len`，EOF 和短读在没有 `ferror` 时仍是成功返回。`cp_file_write` 也会写入实际 `fwrite` 字节数，但只有完整写入 `len` 字节才返回成功；短写返回失败。

## 当前不支持的 ABI 议题

以下内容不属于当前实现范围，需要单独设计后才能作为语言能力使用：

- `str` 的实际内存布局和 C 边界表示。
- `struct` 的 C-compatible 布局声明，例如 `repr(C)` 或 `extern struct`。
- by-value 聚合参数和返回值的 ABI。
- 函数指针 `f*(...) -> R` 与 C function pointer 的完整等价条件和平台调用约定细节。
- C 头文件导入或绑定生成。
- variadic C 函数，例如 `printf`。
- 符号别名，例如源码名和 C 符号名不一致时的 `link_name`。

第一版应保持小而明确：只支持标准库和 runtime 需要的少量外部 C 函数声明与简单 C ABI 导出。
