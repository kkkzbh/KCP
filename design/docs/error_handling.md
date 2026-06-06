# 错误处理

本文档记录 KCP 第一版错误处理边界：可恢复失败通过普通库类型表达，调用者违反前置条件通过 `assert` 表达，不可恢复终止通过 `panic` 表达。`optional<T>`、`expected<T,E>` 的类型规则见 [variant.md](variant.md)，函数返回和控制流规则见 [flow.md](flow.md)。

## 错误分类

外部状态导致、调用者无法在调用前可靠确定的失败，应使用 `optional<T>`、`expected<T,E>` 或领域内等价结果类型。

```cp
file::open(path, options) -> expected<file, io_error>;
parse_i32(text) -> expected<i32, parse_error>;
map.find(self like&, key: K const&) -> optional<V like&>;
```

调用者应在调用前保证成立的前置条件，使用 `assert` 表达。默认编译模式下，`assert` 是 checked contract：条件为 false 时调用 `panic`。命令行 `--release` 会移除 `assert` 检查，见下文。

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

本页只讨论控制与诊断相关的 builtin。`alloc`、`free`、`construct_at` 和 `destroy_at` 是内存生命周期原语，规则见 [memory_allocation.md](memory_allocation.md)；`builtin(expr)` 是内建 operator 逃逸语法，规则见 [operator.md](operator.md)。这些名字同样走裸 callee builtin 识别，但不属于错误处理机制。

`panic` 没有零参数形式；需要没有额外上下文的理论不可达路径时写 `unreachable()`，需要空消息的不可恢复终止时也要显式传入一个 `str`，例如 `panic("")` 或 `panic(str{})`。

`panic` 和 `assert` 的 message 参数目标类型是 `str`，不会把拥有型 `string` 隐式转换成视图。字符串字面量本身是 `str`，因此 `panic("bad")` 可用；如果已有 `string`，应显式写 `panic(message.as_str())` 或先保存为 `str` 视图。`panic(message)` 中 `message: string` 会按上下文类型检查失败，而不是自动调用 `as_str()`、借用底层 buffer 或复制字符串。

这些 builtin 名字在调用表达式检查时优先于普通函数查找。即使当前作用域中存在名为 `panic`、`assert` 或 `unreachable` 的普通函数、局部变量或导入符号，`panic(...)`、`assert(...)` 和 `unreachable()` 这些调用形状仍按 builtin 规则处理；不能通过声明同名函数来重载或替换它们。这里的识别只覆盖普通名字作为 callee 的调用表达式，不是成员调用、关联函数调用或函数值。

这些 builtin 不是一等函数值。裸 `panic`、`assert` 或 `unreachable` 不会解析成可传递的 builtin 函数；`let f = panic;`、`run(panic);` 或 `panic as f(str) -> !` 这类写法只会按普通名字查找处理。若当前作用域没有同名普通 binding，会得到未知名字；若存在同名普通 binding，后续是否可调用由那个 binding 自己的类型决定，但直接写 `panic(...)` 仍会被 builtin 调用规则截获。

## `!` never type

`!` 表示表达式不会正常产生值。它是内建类型，不是标准库类型。

规则：

- `panic(...)` 和 `unreachable()` 的类型是 `!`。
- `!` 可以隐式转换到任意类型，包括引用返回和 `void` / 内部 `unit` 位置。
- 任何普通类型都不能隐式转换到 `!`。
- 显式返回 `!` 的函数不能有正常完成路径。
- `return;` 不能用于 `-> !` 函数。
- `return value;` 只有在 `value` 本身类型为 `!` 时才能用于 `-> !` 函数。
- `!` 只能作为裸类型写出，不能带 `const`、`like`、`forward`、`*`、`&`、`move&` 或 `forward&`。`type bad = ! const;`、`let p: !* = ...` 和 `take(value: !&)` 都会按非法类型参数报错；需要“不会返回”的语义时直接使用裸 `!`。

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
- `message` 存在时必须能隐式转换为 `str`。
- `assert` 只接受 1 个或 2 个实参，返回内部 `unit`。
- `assert` 的类型固定为 `unit`，不根据条件表达式做静态发散推导。即使写 `assert(false, "bad")`，语义层也不会把该表达式当作 `!`，后续语句和返回类型推导仍按可能继续执行处理。需要表达不可恢复终止或理论不可达路径时，使用 `panic(message)` 或 `unreachable()`。
- `assert(false, "bad")` 也不是编译期错误。checked 构建会生成运行时条件分支并在执行到该调用时 panic；release 构建按下面的 elide 规则移除整个调用。
- 默认编译模式下，`assert` 是 checked：失败时调用 `panic`。
- 未写 `message` 时，当前使用默认消息 `"assertion failed"`。
- `cp --release` 会移除 `assert` 调用，且不会求值 `condition` 或 `message`。因此 `assert(mark(x), "msg")` 在 release 模式下不会调用 `mark`，也不会保留这条 assert 的消息文本。release 模式不会跳过语义检查：`condition` 仍必须是 `bool`，`message` 仍必须能隐式转换为 `str`。
- `--release` 同时启用 release 编译参数；当前命令行会给 clang 传 `-O3` 和 `-DNDEBUG`。
- 不要把有副作用的必要计算放在 `assert` 条件或消息表达式里；debug/check 模式会求值，release 模式不会求值。

这意味着 `assert` 适合表达前置条件和内部不变量，不适合表达必须执行的运行时校验。需要在 release 中也保留的校验应写成普通控制流：

```cp
if(not valid(input)) {
    return parse_error::invalid;
}
```

`panic` 和 `unreachable` 的调用边界：

- `panic(message)` 只接受 1 个实参，`message` 必须能隐式转换为 `str`，表达式类型是 `!`。
- `unreachable()` 不接受实参，表达式类型是 `!`。
- `unreachable()` 当前以固定消息 `"entered unreachable code"` 触发 panic，用于标记理论上不可达的控制流。

显式类型实参或参数数量错误不会让这些 builtin 退回普通函数查找。当前语义检查会先报告 `invalid_type_argument` 或 `argument_count_mismatch`，再保留固定 builtin 类型用于后续错误恢复：

- `panic()` 和 `panic(a, b)` 都非法；若至少有一个实参，编译器仍检查第一个实参是否可作为 `str` message，表达式恢复为 `!`。
- `assert()` 和 `assert(a, b, c)` 都非法；若实参存在，编译器仍检查第一个实参为 `bool`，第二个实参可隐式转换为 `str`，表达式恢复为 `unit`。
- `unreachable(extra)` 非法；表达式恢复为 `!`。
- `panic<T>(message)`、`assert<T>(condition)` 和 `unreachable<T>()` 都非法；显式类型实参本身报 `invalid_type_argument`，调用仍按对应 builtin 的固定结果类型恢复。

这些恢复类型只用于继续产生更具体的诊断、控制流和返回推导信息。合法程序不能依赖错误恢复后的 `!` 或 `unit` 来通过编译。

显式返回 `!` 的函数必须保证不会正常完成。函数体内部仍必须通过 `panic(...)`、`unreachable()`、返回另一个 `!` 表达式或其它发散控制流满足这条要求；它不是“普通函数结束时自动终止”的标记。

`panic` 运行时入口使用 `ptr + len` 消息 ABI：

```text
declare void @cp_panic(ptr, i64) noreturn
```

字符串实参以 `ptr + len` 形式传给 runtime。当前 runtime 会向 stderr 写出 `panic: <message>\n` 后 abort；这不是可捕获异常，也不会恢复执行。`str` 不按 C string 解释，消息中间的 `'\0'` 也按普通字节写出；如果传入默认空 `str{}` 这类 `ptr == nullptr` 或长度为 0 的消息，runtime 只写出 `panic: ` 和换行，不读取 message 指针。

## 容器访问

顺序容器和连续视图的下标访问表达前置条件访问，不返回 `optional`：

```cp
span.operator [](self like&, index: usize) -> T like&;
vector.operator [](self like&, index: usize) -> T like&;
string.operator [](self like&, index: usize) -> char like&;
```

这些接口内部使用 `assert(index < size())` 维护 checked 第一版语义。`front()`、`back()`、`pop_back()` 等要求非空容器的接口也使用 `assert`。

固定数组 `[T; N]` 和 `str` 的内建下标也是前置条件访问，不返回 `optional`。非静态拒绝的越界路径会 panic，消息分别是 `"array index out of bounds"` 和 `"string index out of bounds"`；这条检查是内建下标语义的一部分，不是用户可写 `assert(...)` 调用，也不会随 `--release` 对显式 `assert` 的 elide 一起移除。

关联容器按语义区分查询和前置条件访问：

```cp
map.find(self like&, key: K const&) -> optional<V like&>;
map.at(self like&, key: K const&) -> V like&;
map.operator [](self&, key: K) -> V&;
```

`find` 表示“可能不存在”的查询；`at` 表示调用者保证 key 存在，缺 key 时 panic；`operator[]` 在 key 不存在时默认构造并插入 value。

## optional 和 expected 的成功值访问

`optional<T>` 和 `expected<T,E>` 是普通标准库 `variant`，不是异常机制。它们当前只提供查询、按值回退和成功值解引用这几个成员操作：

```cp
optional<T>.has_value(self const&) -> bool;
expected<T,E>.has_value(self const&) -> bool;
optional<T>.value_or(self const&, fallback: T) -> T;
expected<T,E>.value_or(self const&, fallback: T) -> T;
```

`has_value()` 没有前置条件：`optional::some` / `expected::value` 返回 `true`，`optional::none` / `expected::unexpected` 返回 `false`。`value_or(fallback)` 也不会 panic；它在成功 case 中从 `T const&` 构造返回值，在失败 case 中返回按值传入的 `fallback`。`fallback` 是普通调用实参，调用点必须能构造并通过类型检查，不会因为当前对象已有成功值而跳过。`value_or` 不会 move 出保存的成功 payload，不是 lazy fallback，也不提供访问 `expected` 错误 payload 的路径。

需要前置条件访问成功值时使用 `operator *`，不使用 `unwrap`：

```cp
optional<T>.operator *(self like&) -> T like&;
expected<T,E>.operator *(self like&) -> T like&;
```

规则：

- `*optional` 要求当前 case 是 `.some`，否则 panic。
- `*expected` 要求当前 case 是 `.value`，否则 panic。
- 返回值使用 `like&`，可变性跟随 `self`，不会按值复制 payload。
- panic 消息来自标准库实现：`optional::none` 上解引用会使用 `"optional dereference on none"`，`expected::unexpected` 上解引用会使用 `"expected dereference on unexpected"`。
- `*expected` 只访问成功 payload，不提供错误 payload 访问路径。需要读取或移动 `unexpected(E)` 中的错误值时，直接用 `match` 解开 `.unexpected(error)` case。
- 当前没有 `unwrap`、`expect`、`map`、`and_then`、`error()` 或 `unexpected()` 成员访问器。需要移动成功值、按需构造 fallback、检查或转换错误值时，直接对 `.some` / `.none` 或 `.value` / `.unexpected` 做 `match`。
