# opaque alias

本文档记录第一版 `type A = opaque T`。普通局部 `type` 类型别名和 concept 关联类型见 [concept.md](concept.md)；C ABI 边界见 [extern_c.md](extern_c.md)。

## 语法总览

```text
OpaqueAliasDecl -> export? type identifier = opaque Type ;
```

示例：

```cp
export type file_handle = opaque u8*;
export type open_options = opaque u8;
```

`opaque` 只在顶层声明、固有 `impl` / concept 相关的 `type` 声明解析路径中作为 contextual marker 识别；普通块语句里的局部 `type name = Type;` 不支持 `opaque`。因此 `main() { type handle = opaque i32; }` 不是当前能力，parser 会把 `opaque` 当作类型名开始解析，而不是创建局部名义封装类型。

## 语义

`type A = opaque T` 创建一个新的名义类型 `A`：

- `A` 的底层 layout 和 ABI 与 `T` 相同。
- 类型检查中 `A` 和 `T` 不等价。
- `A` 不继承 `T` 的运算符、解引用、下标或成员。
- `A` 可以拥有自己的 `impl` 方法和关联函数。
- 只有声明该 opaque alias 的同一个 translation unit / 源文件允许通过显式 `as` 在 `A` 和 `T` 之间转换。
- 其它源文件只能通过声明源文件导出的函数和方法使用 `A`，即使它们属于同一个具名模块。

opaque alias 是顶层类型声明，不带泛型参数。`type handle<T> = opaque T;` 不是当前语法；使用处也不能写 `handle<i32>`。普通非 opaque `type name = T;` 是别名，不产生新名义类型；`type name = opaque T;` 才产生独立名义类型。

当前语义层只要求 opaque alias 的底层类型能被解析为某个类型，不额外限制它必须是整数、指针或可默认初始化类型。也就是说，`type raw_ref = opaque i32&;` 这类底层形状当前可能通过声明收集，但这不是推荐公开 API。实际设计应优先使用 layout/ABI 清晰的整数、指针或简单值类型作为底层表示；如果底层类型本身有引用、函数、`variant` 或复杂生命周期语义，opaque alias 不会自动补出正确构造、析构或借用协议。

opaque alias 的顶层名字和同一作用域内的其它类型、函数、concept 名字共享冲突检查。已经存在同名 struct、variant、enum、concept、顶层函数或另一个 alias 时，不能再声明同名 opaque alias。

示例：

```cp
type open_options = opaque u8;

impl open_options {
    bits(self) -> u8
    {
        return self as u8;
    }
}
```

第一版不引入 `unsafe` 关键字。opaque alias 的意义不是提供内存安全证明，而是在类型系统层面把裸表示封装起来，避免用户代码把文件句柄、flag bitset 等值当成普通指针或整数随意操作。

## 显式转换边界

opaque alias 与底层类型之间没有隐式转换，即使在声明该 alias 的源文件内部也必须写 `as`：

```cp
type handle = opaque u8*;

make(raw: u8*) -> handle
{
    return raw as handle;
}

raw(value: handle) -> u8*
{
    return value as u8*;
}
```

这两个方向的显式转换只在声明该 opaque alias 的同一个 translation unit / 源文件内部合法。当前实现用 alias 声明所在的 `unit_index` 检查权限，不用模块名检查权限。因此，同一具名模块拆成多个文件时，只有写出 `type handle = opaque u8*;` 的那个文件能直接做 `handle <-> u8*`：

```cp
export module handles;
export type handle = opaque u8*;

export make_handle(raw: u8*) -> handle
{
    return raw as handle; // 合法：同一源文件
}
```

```cp
export module handles;

export leak(value: handle) -> u8*
{
    return value as u8*; // 错误：同一模块的另一个源文件也没有底层转换权限
}
```

模块外即使能导入 `handle` 类型名，也不能直接把 `handle` 转回 `u8*`，也不能把 `u8*` 转成 `handle`：

```cp
import handles;

main()
{
    let value = make_handle(nullptr as u8*);
    let raw = value as u8*; // 错误：不在 handle 的声明源文件内
}
```

opaque alias 之间也不会因为底层类型相同而互相转换。`type file_handle = opaque u8*;` 和 `type socket_handle = opaque u8*;` 是两个不同名义类型；需要转换时必须由拥有相应底层权限的源文件提供显式 API。

如果把一个 opaque alias 用作另一个 opaque alias 的底层类型，当前显式转换检查只看声明时记录的直接底层类型，不递归剥开多层 opaque：

```cp
type raw = opaque i32;
type wrapped = opaque raw;

unwrap(value: wrapped) -> raw
{
    return value as raw; // 合法：外层 wrapped 显式解到它声明时记录的直接底层 raw
}

wrap(value: raw) -> wrapped
{
    return value as wrapped; // 合法：raw 正好是 wrapped 声明时记录的直接底层
}
```

也不能直接把 `i32` 写成 `wrapped`，也不能直接把 `wrapped` 写成 `i32`，因为 `wrapped` 的直接底层是 `raw`，不是 `i32`。如果 `raw` 和 `wrapped` 声明在同一个源文件中，可以显式写两步转换，例如 `bits as raw as wrapped` 或 `value as raw as i32`；如果它们声明在不同源文件中，每一步仍分别受对应 opaque alias 声明源文件权限限制。这类嵌套 opaque 形状虽然可以声明并默认初始化，但会让转换 API 分层变复杂；第一版应避免用 opaque alias 包 opaque alias，优先让每个公开 opaque alias 直接包装整数、指针或其它清晰表示类型。

## 操作能力

opaque alias 不继承底层类型的操作。底层是指针时，opaque 值不能直接解引用、下标、做指针算术，也不能隐式传给需要底层指针值的函数参数；底层是整数时，opaque 值不能直接参与算术、比较或 bitwise 运算。需要这些能力时，应在声明该 alias 的同一源文件内部显式转换到底层类型，再通过导出的函数或方法暴露受控操作。

```cp
type flags = opaque u8;

impl flags {
    bits(self) -> u8
    {
        return self as u8;
    }

    with(self, bit: u8) -> flags
    {
        return ((self as u8) | bit) as flags;
    }
}
```

当前固有 `impl` 支持在 opaque alias 上声明成员函数和关联函数：

```cp
impl flags {
    empty() -> flags
    {
        return 0 as flags;
    }

    contains(self, bit: u8) -> bool
    {
        return ((self as u8) & bit) != 0;
    }
}
```

如果 `impl flags { ... }` 写在声明 `flags` 的另一个同模块源文件中，方法本身可以注册到 `flags` 上，但方法体不能直接写 `self as u8` 或 `0 as flags`。关联函数也是同样规则：`flags::empty()` 可以作为 opaque alias 的 associated function 注册和调用，但函数体里的 `0 as flags` 只有在 `flags` 的声明源文件内合法。需要底层互转的方法、关联函数和 helper 应放在声明 `flags` 的同一源文件中，或调用那里导出的低层 API。

opaque alias 可以作为固有 `impl` 目标，但不能作为 concept impl 目标。也就是说，`impl flags { contains(...) { ... } }` 是当前支持的扩展形式，`impl some_concept for flags { ... }` 当前会被拒绝；需要 concept 能力时应为可支持的目标类型提供实现，或通过普通函数/方法暴露所需行为。

固有 `impl` 中的关联类型声明按普通 impl 收集规则处理，但 opaque alias 自身不因此变成泛型类型，也不会获得底层类型的关联能力。外部代码只能通过已导入的类型名、方法和关联函数使用 opaque alias。

opaque alias 不能声明构造函数或析构函数。写 `impl flags { flags(...) { ... } }`、`= default` 构造函数或 `~flags()` 都会被拒绝；构造 opaque 值应通过声明源文件内的普通关联函数或 helper 完成。

当前实现也不把 opaque alias 的 `impl operator` 注册为可用 operator。写在 `impl flags { operator ... }` 里的 operator 不会进入后续 operator 查找，也不应被当作可调用、可删除或可导出的函数实现；即使 parser 能记录这类声明，它也不是当前公开能力。需要 operator 时使用顶层 operator，并让至少一个参数类型是该 opaque alias：

```cp
operator ==(left: flags, right: flags) -> bool
{
    return (left as u8) == (right as u8);
}
```

顶层 operator 本身按普通顶层函数规则检查。上面这种直接把 `flags` 转回底层 `u8` 的实现仍必须写在声明 `flags` 的同一源文件内；如果 operator 写在同模块其它源文件或导入模块中，它不能直接使用 `left as u8`，只能调用声明源文件导出的 `bits()`、`raw()` 等 helper。顶层 operator 是否对其它模块可见，按普通模块导出/导入规则决定。

## 默认初始化

当前语义把 opaque alias 视为可默认初始化，并使用底层表示的默认零值。这使 `open_options{}` 可以表示空 bitset，`file_handle{}` 可以表示空指针形态的句柄。是否允许外部用户依赖这个零值语义，应由标准库 API 文档说明。

这条默认初始化判定当前不递归检查底层类型是否本身可默认初始化。换言之，opaque alias 一旦声明成功，`Alias{}` 在语义层会被当作可默认初始化处理；如果底层表示不是明确有零值语义的整数或指针，就不应把默认初始化后的运行时含义作为稳定语言能力使用。

实践中 opaque alias 应优先选择整数或指针这类清晰可零初始化的底层表示，并通过方法说明零值语义；不要用 opaque alias 去包装需要复杂构造协议的对象。

## 模块和 ABI

`export type handle = opaque u8*;` 会导出 `handle` 这个名义类型名，但不会导出它和 `u8*` 之间的转换权限。转换权限仍只属于声明该 alias 的同一 translation unit / 源文件。

opaque alias 与底层类型共享表示，不代表可以自动穿过 `extern "C"` 边界。`extern "C"` 参数和返回类型仍按 [extern_c.md](extern_c.md) 的 C-compatible 白名单检查：opaque alias by value 不合法；`handle*` 这类 pointer-to-opaque 合法，因为当前 ABI 检查只浅层检查指针本身。需要对 C 按值暴露句柄或 bitset 时，应在 ABI 函数签名中使用底层指针或整数类型，在 KCP 模块内部再封装成 opaque alias。
