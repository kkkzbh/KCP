# builtin Type

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
function不对外使用，name(){}天然是函数字面量
函数字面量在局部作用域内使用则为闭包，自动智能按需捕获

## 序列类型
sequence<type,num> 序列字面量 { x,y,z }

## 数组类型
array<type,num> 数组字面量 [x,y,z]

## 元组类型
tuple<type,type,type> 元组字面量 (x,y,z)

## 引用类型
type& type const& type*& type**&

## 指针类型
type* type** type const*
