# 标准库 string

`string` 是 `std.text.string` 提供的拥有型字符容器。它保存一段可修改字符序列，并维护一个额外的 trailing `'\0'` 槽位，方便当前运行时互操作。只读字符串字面量的类型是 `str`；需要拥有内容或逐步拼接文本时使用 `string`。

```cp
import std.text;

main() -> i32
{
    let text = string{"hi"};
    text.push_back('!');
    text.append(" cp");
    text[0 as usize] = 'H';
    return text.size() as i32;
}
```

## 模块与类型

```cp
import std.text.string;
```

公开类型：

```cp
string
```

`std.text` 和 `std` 聚合模块也会重导出 `string`。`std.text.detail.string_storage` 是内部存储，公共代码不要直接依赖。当前语言没有字段私有性，源码层能看到 `storage` 和 `len`，但它们是实现细节。

## string 与 str

`str` 是 compiler-recognized 的只读字符串视图，只借用一段 `{ptr, len}`。`string` 拥有字符存储，并能通过 `as_str()` 借出当前内容：

```cp
let owned = string{"hello"};
let view = owned.as_str();
```

`string{text: str}` 会复制 `text` 的当前内容。字符串字面量是 `str`，所以 `string{"abc"}` 是拥有式构造，不是隐式转换。当前 `"abc" as string` 不可用；`as` 是窄显式转换，不会调用构造函数。

## 构造与生命周期

```cp
string();
string(text: str);
```

- `string{}` 构造空串。
- `string{text}` 拷贝 `text` 的字符内容。
- copy 构造和 copy 赋值复制字符内容。
- move 构造和 move 赋值接管底层 storage，并把源对象长度置为 0。
- move 后的源对象可以析构、重新赋值、`clear()` 或继续追加内容，但在重新分配前不要依赖它的 `data()` 非空或带 trailing `'\0'`。

## 查询与访问

```cp
data(self like&) -> char like*;
begin(self like&) -> char like*;
end(self like&) -> char like*;
as_str(self const&) -> str;
size(self const&) -> usize;
capacity(self const&) -> usize;
empty(self const&) -> bool;
operator [](self like&, index: usize) -> char like&;
front(self like&) -> char like&;
back(self like&) -> char like&;
```

`data`、`begin`、`end` 和 `operator[]` 使用 `self like&`，会随 receiver 自动返回 `char*` / `char&` 或 `char const*` / `char const&`。

```cp
let text = string{"abc"};
text[1 as usize] = 'B';
let view = text.as_str();
```

`operator[]`、`front()`、`back()` 和 `pop_back()` 都使用 `assert` 表达前置条件。debug/check 模式下越界或空串访问会 panic；`--release` 会移除这些 assert，调用者必须保证 `index < size()` 或 `not empty()`。

## 修改文本

```cp
reserve(self&, new_capacity: usize) -> void;
clear(self&) -> void;
push_back(self&, ch: char) -> void;
pop_back(self&) -> void;
resize(self&, new_size: usize, ch: char) -> void;
append(self&, text: str) -> void;
```

```cp
let text = string{};
text.reserve(16 as usize);
text.append("KCP");
text.push_back(' ');
text.append("lang");
text.resize(3 as usize, '?');
```

修改规则：

- `reserve(new_capacity)` 只保证容量至少达到传入值。
- `clear()` 把长度置 0 并写 trailing `'\0'`，不释放容量。
- `push_back(ch)` 在尾部追加字符。
- `pop_back()` 删除最后一个字符但不返回值。
- `resize(new_size, ch)` 缩小时截断，扩张时用 `ch` 填充。
- `append(text)` 按 `text.size()` 追加字节级字符序列。

当前实现只特殊处理 `text.data() == data()` 的整串自追加，保证 `s.append(s.as_str())` 在可能扩容后仍从新的自身缓冲读取。对部分重叠的外部 `str` 视图没有额外重叠处理；不要把指向同一 `string` 中间区域的视图传给 `append` 后再依赖具体复制结果。

## 存储与 nul 字符

`string` 的内部 storage 额外保留一个 trailing `'\0'` 槽位。`capacity()` 返回可保存的字符数，不包含这个终止符槽位；`size()` 也不包含它。

`as_str()` 返回 `{ data(), size() }`，不把 trailing `'\0'` 计入 `str.len`。这个终止符是实现细节，不表示 `string` 文本内容不能包含中间 `'\0'` 字符。

## 迭代与输出

`string` 实现 `iterable`、`const_iterable` 和 `contiguous_mutable_range`：

```cp
impl iterable for string {
    type iter_type = ptr_iter<char>;
    type iter_item = char&;
}

impl const_iterable for string {
    type const_iter_type = const_ptr_iter<char>;
    type const_iter_item = char const&;
}
```

```cp
import std;

main() -> i32
{
    let text = string{"cab"};
    sort(text);

    for(ref ch : text) {
        if(ch == 'a') {
            ch = 'A';
        }
    }

    println("text = {}", text);
    return text.size() as i32;
}
```

`std.io` 给 `str` 和 `string` 都实现了 `display`，因此可以直接传给 `{}`；这个能力来自 `std.io.format`，不是 `std.text.string` 自身的成员方法。

## 失效规则

`reserve`、`push_back`、`append` 和扩张式 `resize` 可能重新分配 storage；之前取得的 `data()` / `begin()` / `end()` 指针、`as_str()` 视图、下标引用或迭代器在这些修改后可能失效。

`clear()`、`pop_back()` 和缩短式 `resize` 不释放 storage，但会改变长度和 trailing 终止符，旧的越界引用或视图长度不能继续使用。

## 当前限制

`string` 当前没有 `c_str()`、内建或标准库比较 operator、`find`、`contains`、`starts_with`、切片、split、trim、parse、`operator +` 或插值。需要比较两个拥有字符串时，先比较它们的 `as_str()`；需要组合文本时，用 `append` 或格式化输出。
