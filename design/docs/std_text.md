# 标准库 text

拥有型字符容器 `string` 的完整教程见 [标准库 string](std_string.md)。本页保留 `std.text` 模块导览、`str` 视图规则以及 `string` 公共接口速查。

本文档记录 `std.text` 第一版公共接口。它把编译器认识的 `str` 字符串视图扩展成可比较、可迭代的标准库类型，并提供拥有型字符串 `string`。

`std.text` 聚合模块当前重导出：

```text
std.text.str
std.text.string
```

`std.text.detail.string_storage` 是实现细节，不由 `std.text` 重导出；公共代码不应直接依赖它的字段或方法。

## str 扩展

`str` 本身是编译器认识的字符串视图类型，基础字段和下标能力在 [type_system.md](type_system.md) 中定义。`std.text.str` 为它补充标准库方法、比较和迭代。

公开成员：

```cp
size(self) -> usize;
data(self) -> char const*;
operator <=>(self const&, rhs: str const&) -> weak_ordering;
operator ==(self const&, rhs: str const&) -> bool;
operator !=(self const&, rhs: str const&) -> bool;
operator <(self const&, rhs: str const&) -> bool;
operator <=(self const&, rhs: str const&) -> bool;
operator >(self const&, rhs: str const&) -> bool;
operator >=(self const&, rhs: str const&) -> bool;
```

规则：

- `size()` 返回 `str.len`。
- `data()` 返回 `str.ptr`，类型是 `char const*`。
- 比较按 `[ptr, ptr + len)` 中的 `char` 序列做字典序比较。
- 公共前缀相同后，短串小于长串；长度相同为 equivalent / equal。
- 比较和 `size()` 都按 `str.len` 工作，不按 C 字符串在中间 `'\0'` 截断。
- 这些方法和 operator 只有在 `std.text.str`、`std.text`、`std` 或其它重导出链可见后才参与普通查找；裸语言只内建 `ptr` / `len` 字段和只读下标。

`str` 迭代：

```cp
struct str_iter {
    text: str;
    index: usize;
}

str_iter implements iterator
    type iter_item = char;

str_iter.next(self&) -> optional<char>;

str implements iterable
    type iter_type = str_iter;
    type iter_item = char;

str implements const_iterable
    type const_iter_type = str_iter;
    type const_iter_item = char;
```

`str_iter::next()` 从 `text[index]` 读出一个 `char` 值，然后递增下标；结束时返回 `optional<char>::none`。迭代 item 是值类型 `char`，不是 `char const&`，因此 `for(let ch : text)` 不会借出可写字符引用。

`str_iter` 是 `std.text.str` 导出的普通结构体；字段当前也是公开字段。稳定用法是通过 `for` 或 `text.iter()` 取得迭代器。`std.text.str` 对 `std.core.option`、`std.core.iter` 和 `std.compare` 只是普通 `import`，不会把 `optional`、`iterator` 或 `weak_ordering` 这些名字继续重导出给用户。

这里的 `str` 迭代能力覆盖 range-for 和直接 `text.iter()`。当前不要把它进一步推断成 ranges adapter / terminal receiver：`text: str` 上的 `text.count()`、`text.filter(...)` 等 UFCS 组合不是公开可依赖能力；需要对文本做 ranges 组合时，先使用拥有型 `string`，或直接写 range-for / 手动循环。完整 ranges 边界见 [std_ranges.md](std_ranges.md#sources)。

## string

```cp
struct string {
    storage: string_storage;
    len: usize;
}
```

`string` 是拥有型、可变、连续字符缓冲区。它维护一个不计入 `size()` 的 trailing `'\0'` 槽位，方便与底层运行时或 C 风格接口配合；文本内容本身仍允许包含中间 `'\0'`。

当前语言没有字段私有性，因此 `storage` / `len` 在源码层是公开字段。但 `storage` 的类型来自 `std.text.detail.string_storage`，不是 `std.text` 的稳定公共入口；公共代码应使用 `string{}`、`string{text}` 和下面的成员函数，不要直接改写字段。

公开构造和特殊成员：

```cp
string();
string(text: str);
string(other: string const&);
string(other: string move&);
operator =(self&, rhs: string const&) -> string&;
operator =(self&, rhs: string move&) -> string&;
```

公开成员：

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
reserve(self&, new_capacity: usize) -> void;
clear(self&) -> void;
push_back(self&, ch: char) -> void;
pop_back(self&) -> void;
resize(self&, new_size: usize, ch: char) -> void;
append(self&, text: str) -> void;
```

规则：

- `string{}` 构造空字符串，并维护 trailing `'\0'`。
- `string{text}` 从 `str` 复制当前字符序列。
- copy 构造和 copy 赋值复制字符内容；copy 赋值显式处理自赋值。
- move 构造和 move 赋值接管底层 storage，并把源对象 `len` 置为 0；move 赋值也显式处理自赋值。
- `data()` / `begin()` 返回首字符指针；普通 receiver 为 `char*`，const receiver 为 `char const*`。
- `end()` 返回 `data() + size()`，不指向 trailing `'\0'` 的逻辑元素。
- `as_str()` 返回 `{ .ptr = data(), .len = size() }`，不拥有底层存储。
- `size()` 返回有效字符数，不包含 trailing `'\0'`。
- `capacity()` 返回不含 trailing `'\0'` 的有效字符容量。
- `empty()` 等价于 `size() == 0`。

访问和修改：

- `operator [](index)`、`front()`、`back()` 和 `pop_back()` 都使用 `assert` 表达前置条件。checked 模式下越界或空串访问会 panic；`--release` 会移除这些检查，调用者必须保证条件。
- 下标、`front()`、`back()` 使用 `self like&`：可写 `string` 返回 `char&`，const `string` 返回 `char const&`。
- `reserve(new_capacity)` 只保证容量至少达到 `new_capacity`；`new_capacity <= capacity()` 时不做事。增长可能替换底层 storage，并重新写 trailing `'\0'`。
- `clear()` 只把长度置 0 并写 trailing `'\0'`，不释放容量。
- `push_back(ch)` 追加一个字符，必要时按增长策略扩容。
- `pop_back()` 删除最后一个字符但不返回值。
- `resize(new_size, ch)` 缩小时截断，扩张时用 `ch` 填充新增位置。
- `append(text)` 按 `text.size()` 复制字符，不按 `'\0'` 截断。
- `push_back`、`append` 和扩张式 `resize` 会按需自动增长容量；不要依赖具体增长倍率。显式 `reserve(new_capacity)` 只保证容量至少达到传入值。
- `append(self.as_str())` 这类整串自追加受当前实现支持；指向同一 string 中间区域的旧 `str` 视图没有额外重叠保护，不要传给可能扩容的 `append` 后依赖具体复制结果。

失效规则：

- `reserve`、`push_back`、`append` 和扩张式 `resize` 可能替换底层 storage，使旧 `data()` / `begin()` / `end()` 指针、`as_str()` 视图、下标引用和迭代器失效。
- `clear()`、`pop_back()` 和缩短式 `resize` 不释放 storage，但会改变长度和 trailing terminator；旧视图的长度不会自动变化，越界引用不能继续使用。
- 不扩容的字符修改会让旧视图继续指向同一 buffer，但看到修改后的字节。

## 迭代和 range 协议

`string` 实现：

```cp
string implements iterable
    type iter_type = ptr_iter<char>;
    type iter_item = char&;

string implements const_iterable
    type const_iter_type = const_ptr_iter<char>;
    type const_iter_item = char const&;

string implements contiguous_mutable_range
    type item = char;
```

因此：

- `for(let ref ch : text)` 可以遍历并修改可写 `string` 的字符。
- `for(const ref ch : text)` 或 const receiver 遍历得到只读字符引用。
- `string` 可以作为 `std.ranges` adapter / terminal receiver，例如 `text.count()`、`text.filter(...)` 或 `text.enumerate()`，前提是对应 `std.ranges` 入口在当前文件可见。
- `string` 可作为 `sort` / `stable_sort` 这类要求 `contiguous_mutable_range` 的算法 receiver，按字符原地重排。
- `str` 只迭代 `char` 值，不是可写连续 range，不能直接排序；当前也不作为稳定 ranges UFCS 输入。

## 与 display / IO 的关系

`std.text` 自身不定义 `display`。`std.io.format` 为 `str` 和 `string` 实现显示能力：

- `str` 按 `str.len` 写出完整视图区间。
- `string` 先借出 `as_str()` 再显示。

因此只导入 `std.text` 时不会自动获得 `println("{}", text)`；需要导入 `std.io`、`std.io.format` 或更高层 `std`。

## 不支持内容

第一版 `std.text` 不提供：

- `string::c_str()`；需要裸指针时使用 `data()`，并理解失效边界。
- `find`、`contains`、`starts_with`、`ends_with`、`substring`、`slice`、`split`、`trim`、`replace`、`insert`、`erase` 或解析 API。
- `operator +`、字符串插值或自动格式化。
- Unicode 码点 / grapheme 语义、编码校验或大小写转换；当前操作按 `char` 字节序列工作。
- `str` 的拥有转换或 lifetime 延长；需要保存文本时显式构造 `string{text}`。
- `string` 自身的比较 operator。需要比较两个拥有字符串时，比较它们的 `as_str()`，或提供自己的比较 / order object。
