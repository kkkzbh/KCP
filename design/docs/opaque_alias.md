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

## 语义

`type A = opaque T` 创建一个新的名义类型 `A`：

- `A` 的底层 layout 和 ABI 与 `T` 相同。
- 类型检查中 `A` 和 `T` 不等价。
- `A` 不继承 `T` 的运算符、解引用、下标或成员。
- `A` 可以拥有自己的 `impl` 方法和关联函数。
- 只有定义该 opaque alias 的模块内部允许通过显式 `as` 在 `A` 和 `T` 之间转换。
- 模块外只能通过该模块导出的函数和方法使用 `A`。

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

## 默认初始化

opaque alias 的默认初始化使用底层类型的默认零值。这使 `open_options{}` 可以表示空 bitset，`file_handle{}` 可以表示空指针形态的句柄。是否允许外部用户依赖这个零值语义，应由标准库 API 文档说明。
