# 错误处理

本文档记录 KCP 第一版错误处理边界：可恢复失败通过普通库类型表达，调用者违反前置条件通过 `assert` 表达，不可恢复终止通过 `panic` 表达。`optional<T>`、`expected<T,E>` 的类型规则见 [variant.md](variant.md)，函数返回和控制流规则见 [flow.md](flow.md)。

## 错误分类

外部状态导致、调用者无法在调用前可靠确定的失败，应使用 `optional<T>`、`expected<T,E>` 或领域内等价结果类型。

```cp
file::open(path, options) -> expected<file, io_error>;
parse_i32(text) -> expected<i32, parse_error>;
map.find(self like&, key: K const&) -> optional<V like&>;
```

调用者应在调用前保证成立的前置条件，使用 `assert` 表达。第一版 `assert` 是 checked contract：条件为 false 时调用 `panic`。

```cp
span.operator [](self like&, index: usize) -> T like&
{
    assert(index < len, "span index out of bounds");
    return ref ptr[index];
}
```

不可恢复的程序错误使用 `panic`。`panic` 不返回，返回类型是 `!`。

```cp
panic(message: str) -> !;
unreachable() -> !;
```

这些名字是编译器识别的 builtin 调用，不需要导入标准库，也不会解析成普通用户函数重载。它们都不能带显式类型实参。

## `!` never type

`!` 表示表达式不会正常产生值。它是内建类型，不是标准库类型。

规则：

- `panic(...)` 和 `unreachable()` 的类型是 `!`。
- `!` 可以隐式转换到任意类型，包括引用返回和 `void` / 内部 `unit` 位置。
- 任何普通类型都不能隐式转换到 `!`。
- 显式返回 `!` 的函数不能有正常完成路径。
- `return;` 不能用于 `-> !` 函数。
- `return value;` 只有在 `value` 本身类型为 `!` 时才能用于 `-> !` 函数。

```cp
fail() -> !
{
    panic("failed");
}

value_or_panic<T>(value: optional<T>) -> T
{
    return match value {
        .some(item) => item,
        .none => panic("none"),
    };
}
```

## assert

`assert` 是语言内建函数式调用，不需要导入标准库。

```cp
assert(condition);
assert(condition, message);
```

规则：

- `condition` 必须检查为 `bool`；没有整数、指针或其它类型的 truthiness。
- `message` 存在时必须检查为 `str`。
- `assert` 只接受 1 个或 2 个实参，返回内部 `unit`。
- 第一版 `assert` 总是 checked：失败时调用 `panic`。
- 未写 `message` 时，当前 IR 使用默认消息 `"assertion failed"`。
- IR lowering 内部有 `elide_asserts` 选项，但第一版语言和命令行不把它作为用户可依赖的编译模式；默认语义仍是 checked。

`panic` 和 `unreachable` 的调用边界：

- `panic(message)` 只接受 1 个实参，`message` 必须检查为 `str`，表达式类型是 `!`。
- `unreachable()` 不接受实参，表达式类型是 `!`。
- `unreachable()` 当前 lower 为带固定消息的 `panic`，用于标记理论上不可达的控制流。

后端把 panic 统一 lower 到 runtime ABI：

```text
declare void @cp_panic(ptr, i64) noreturn
```

字符串实参以 `ptr + len` 形式传给 runtime。

## 容器访问

顺序容器和连续视图的下标访问表达前置条件访问，不返回 `optional`：

```cp
span.operator [](self like&, index: usize) -> T like&;
vector.operator [](self like&, index: usize) -> T like&;
string.operator [](self like&, index: usize) -> char like&;
```

这些接口内部使用 `assert(index < size())` 维护 checked 第一版语义。`front()`、`back()`、`pop_back()` 等要求非空容器的接口也使用 `assert`。

关联容器按语义区分查询和前置条件访问：

```cp
map.find(self like&, key: K const&) -> optional<V like&>;
map.at(self like&, key: K const&) -> V like&;
map.operator [](self&, key: K) -> V&;
```

`find` 表示“可能不存在”的查询；`at` 表示调用者保证 key 存在，缺 key 时 panic；`operator[]` 在 key 不存在时默认构造并插入 value。

## optional 和 expected 的前置条件访问

`optional<T>` 和 `expected<T,E>` 的成功值访问使用 `operator *`，不使用 `unwrap`。

```cp
optional<T>.operator *(self like&) -> T like&;
expected<T,E>.operator *(self like&) -> T like&;
```

规则：

- `*optional` 要求当前 case 是 `.some`，否则 panic。
- `*expected` 要求当前 case 是 `.value`，否则 panic。
- 返回值使用 `like&`，可变性跟随 `self`，不会按值复制 payload。
