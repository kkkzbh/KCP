# `extern "C"`

本文档记录 cp 与 C ABI 互操作的最小设计。`extern "C"` 用来声明或定义使用 C 符号名和 C ABI 边界的顶层自由函数。

`extern` 是上下文关键字，只在顶层函数声明或定义前具有特殊含义。ABI 字符串第一版只接受 `"C"`。

第一版目标是支持标准库和 runtime 的低层互操作，例如 `std.io` 需要的 `putchar`、`puts` 或 `cp_put_char`。它不是完整 FFI 系统，也不试图一次性支持所有 C 类型、结构体布局或平台 ABI 细节。

## 设计目标

`extern "C"` 解决两件事：

```text
符号名: 使用 C 符号名，不使用 cp 模块名 mangling
ABI 边界: 只允许一组明确的 C-compatible 参数和返回类型
```

`export` 和 `extern "C"` 是两个独立概念：

- `export` 控制 cp 模块可见性。
- `extern "C"` 控制链接符号名和 ABI 边界。

因此：

```cp
extern "C" putchar(ch: i32) -> i32;
export extern "C" cp_put_char(ch: char) -> i32;
```

第一个声明只在当前 cp 模块内部可见，但链接时仍引用外部 C 符号 `putchar`。

第二个声明会把 `cp_put_char` 暴露给导入当前模块的 cp 代码，同时它的链接符号名仍是 `cp_put_char`。

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

声明形式以分号结束，不生成函数体，只在 IR 中产生外部函数声明。

定义形式带函数体，由 cp 编译器生成函数体，但函数使用 C 符号名和 C ABI 边界。

## 名字和链接

普通 cp 顶层函数仍按模块规则 lower：

```cp
export module math;

export add(x: i32, y: i32) -> i32
{
    return x + y;
}
```

上例导出的函数供 cp 模块系统使用，后端符号名可以是实现定义的 cp mangled name，例如 `cp.math.add`。`export` 不表示 C ABI。

`extern "C"` 函数的后端符号名默认就是源码中的函数名：

```cp
export module math;

export extern "C" add_i32(x: i32, y: i32) -> i32
{
    return x + y;
}
```

该函数的后端符号名是 `add_i32`，C 代码可以按同名函数声明调用它。

同一链接单元内不能出现两个相同 C 符号名的 `extern "C"` 定义。多个模块中重复声明同一个外部 C 函数是允许的，只要声明签名一致。

## 模块可见性

`extern "C"` 声明仍参与 cp 模块系统：

```cp
export module std.io.c;

extern "C" putchar(ch: i32) -> i32;

export write_char(ch: char)
{
    putchar(i32(ch));
}
```

这里 `putchar` 只是 `std.io.c` 模块内部实现细节，导入者看不到它。

如果希望把外部声明继续暴露给其它 cp 模块，可以显式写 `export extern "C"`：

```cp
export module c.stdio;

export extern "C" putchar(ch: i32) -> i32;
export extern "C" getchar() -> i32;
```

导入 `c.stdio` 的 cp 文件可以直接调用 `putchar` 和 `getchar`。

## 第一版类型边界

第一版只允许 C-compatible 类型：

```text
unit
bool
i8 i16 i32 i64
u8 u16 u32 u64
isize usize
f32 f64
char
T*
```

其中：

- `unit` 在返回类型位置 lower 为 C `void`。
- `char` 按 1 字节整数传递。
- `T*` 只检查指针本身的 ABI；`T` 指向的对象布局仍由调用者契约负责。

第一版不允许：

```text
str
array<T,N> by value
tuple<T...>
struct by value
variant
T&
f(...) -> R
generic extern function
lambda / closure
concept method
impl member function
```

这些类型需要更完整的 ABI 设计后再开放。特别是 `str` 的语言设计是运行时长度字符串视图，而不是稳定的 C `char*`；因此不能直接把 `str` 当作 C 字符串 ABI 暴露。

第一版 `extern "C"` 只能用于顶层自由函数，不能用于 `impl` 内成员函数、构造函数、析构函数、lambda 或 concept requirement。

## 函数体规则

外部声明没有函数体：

```cp
extern "C" puts(text: i8*) -> i32;
```

它只提供类型检查和后端声明。链接器必须能从 libc、runtime 或显式链接参数中找到同名符号。

外部定义有函数体：

```cp
export extern "C" answer() -> i32
{
    return 42;
}
```

它由 cp 编译器生成函数体，并以 C 符号名 `answer` 导出。

`extern "C"` 定义内部仍按普通 cp 函数规则检查返回类型、局部变量、控制流、析构和表达式。区别只在函数边界的符号名和 ABI 约束。

## 与标准库的关系

`std.io` 不应直接把 libc 细节暴露成公共接口。推荐分层：

```cp
export module std.io.c;

extern "C" putchar(ch: i32) -> i32;

export module std.io;

import std.io.c;

export write_char(ch: char)
{
    putchar(i32(ch));
}
```

更稳定的做法是通过 cp runtime 包一层：

```cp
export module std.io.runtime;

extern "C" cp_put_char(ch: char) -> i32;
extern "C" cp_put_cstr(text: i8*) -> i32;
```

这样标准库依赖的是 cp 自己的 runtime ABI，而不是不同平台 libc 的细节。

## 后续 ABI 议题

以下内容不属于第一版实现范围，需要单独设计：

- `str` 的实际内存布局和 C 边界表示。
- `struct` 的 C-compatible 布局声明。
- by-value 聚合参数和返回值的 ABI。
- 函数指针 `f*(...) -> R` 与 C function pointer 的等价条件。
- C 头文件导入或绑定生成。
- variadic C 函数，例如 `printf`。
- 符号别名，例如源码名和 C 符号名不一致时的 `link_name`。

第一版应保持小而明确：只支持标准库和 runtime 需要的少量外部 C 函数声明与简单 C ABI 导出。
