# enum

本文档记录 cp 第一版强类型 `enum`。`enum` 是单个整数命名值集合；`variant` 是 tagged union，见 [variant.md](variant.md)。

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

## 语义

`enum` 接近现代 C++ `enum class`：

- case 必须通过作用域访问，例如 `open_flag::read`。
- `enum` 值不隐式转换为整数。
- 整数不隐式转换为 `enum`。
- `enum` 默认不支持普通 bitwise 组合；如果需要位集合，先显式转换到底层整数。
- 第一版必须显式写底层整数类型。
- case 值必须是整数常量表达式，支持整数字面量、一元 `-` / `~`、整数算术、位运算和 `1 << 0` 这类表达式。
- 默认初始化 `open_flag{}` 不合法。

显式转换：

```cp
let raw: u8 = open_flag::read as u8;
```

第一版只允许 `enum_value as UnderlyingInteger`。反向整数到 enum 的构造暂不作为公共能力开放。

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
