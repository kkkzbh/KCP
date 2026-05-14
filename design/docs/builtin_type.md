# builtin Type

完整类型规则见 [type_system.md](type_system.md)。本文只保留内建类型速览。

## 整数类型

bool: 1字节 (字面量false true)
i8 i16 i32 i64
u8 u16 u32 u64
f32 f64
char: 1字节 (字面量 'c')

字面量默认i32与f64

## 字符串类型
"Hello"是str(字符串视图)
- 支持'\n'等转义字符
- 行为类似C++的string_view

## 函数类型
name() -> type {} 是function，其中->省略表示推导返回类型
当前阶段 function 不作为用户可写类型。闭包尚未进入主线 semantic 实现。

## 序列类型
sequence<type,num> 序列字面量 { x,y,z }

## 数组类型
array<type,num> 数组字面量 [x,y,z]

## 元组类型
tuple<type,type,type> 元组字面量 (x,y,z)

## 引用类型
type& type const& type*& type const**&

## 指针类型
type* type** type const*

`type const` 不是合法类型形态。类型中的 `const` 只作为指针或引用的最终目标不可变标记使用，例如 `type const*`、`type const&`、`type const**&`。
