# 标准库 vector

`vector<T>` 是 `std.collections.vector` 提供的连续动态数组。它适合保存数量会变化、需要按下标访问、需要交给 `sort` / `stable_sort` 这类连续区间算法处理的元素。

```cp
import std.collections.vector;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(1);
    values.insert(1 as usize, 2);

    return values[0 as usize] + values[1 as usize] + values.back();
}
```

需要同时使用 `sort`、`span`、`optional` 或输出时，导入对应模块，或在学习阶段直接导入 `std`。

## 模块与类型

```cp
import std.collections.vector;
```

公开类型：

```cp
vector<T>
```

`std.collections` 聚合模块也会重导出 `vector<T>`。`std.collections.detail.vector_storage` 是内部连续存储壳，公共代码不要直接依赖它。当前语言没有字段私有性，源码层能看到 `storage` 和 `len`，但这两个字段是实现细节。

## 构造与生命周期

```cp
vector() = default;
vector(count: usize);
vector(count: usize, value: T const&);
```

- `vector<T>{}` 构造空数组。
- `vector<T>{count}` 默认构造 `count` 个 `T{}`。
- `vector<T>{count, value}` 复制构造 `count` 个 `value`。
- copy 构造和 copy 赋值逐元素复制。
- move 构造和 move 赋值接管底层 storage，并把源对象长度置为 0。
- 析构时会销毁当前所有元素，再释放底层 storage。

`vector(count)` 要求 `T{}` 可用；`vector(count, value)` 要求 `T` 能从 `T const&` 构造。对没有默认值的类型，优先构造空 `vector`，再用 `push_back` / `move_back` 逐个放入元素。

## 查询与访问

```cp
data(self like&) -> T like*;
begin(self like&) -> T like*;
end(self like&) -> T like*;
size(self const&) -> usize;
capacity(self const&) -> usize;
empty(self const&) -> bool;
operator [](self like&, index: usize) -> T like&;
front(self like&) -> T like&;
back(self like&) -> T like&;
```

`data`、`begin`、`end` 和 `operator[]` 使用 `self like&`，因此同一套接口会按 receiver 自动得到可写或只读结果：

- 可写 `vector<T>` 返回 `T*` / `T&`。
- const `vector<T>` 返回 `T const*` / `T const&`。

```cp
let values = vector<i32>{};
values.push_back(10);
values[0 as usize] = 20;
let first = values.front();
```

`operator[]`、`front` 和 `back` 都是前置条件访问。debug/check 模式下越界或空容器访问会 panic；`--release` 会移除这些 assert，调用者必须自行保证 `index < size()` 或 `not empty()`。

## 容量与修改

```cp
reserve(self&, new_capacity: usize) -> void;
clear(self&) -> void;
push_back(self&, value: T) -> void;
move_back(self&, value: T move&) -> void;
pop_back(self&) -> void;
pop(self&) -> T;
resize(self&, new_size: usize, value: T const&) -> void;
insert(self&, index: usize, value: T) -> void;
erase(self&, index: usize) -> void;
erase_range(self&, first: usize, last: usize) -> void;
```

`reserve(new_capacity)` 只保证容量至少达到 `new_capacity`，不改变 `size()`，也不构造新元素。`clear()` 把长度变为 0，但保留容量。

```cp
let values = vector<i32>{};
values.reserve(8 as usize);
values.push_back(1);
values.push_back(4);
values.insert(1 as usize, 2);
values.erase(0 as usize);
```

修改规则：

- `push_back(value)` 按值接收并 move 到尾部。
- `move_back(value)` 从显式 move 引用构造尾部元素。
- `pop()` move 出尾部元素并销毁原槽位。
- `pop_back()` 只销毁尾部元素，不返回值。
- `resize(new_size, value)` 缩小时销毁尾部元素，扩张时复制 `value` 填充新位置。
- `insert(index, value)` 允许 `index == size()`，此时等价于尾部插入。
- `erase(index)` 删除一个元素并把后续元素前移。
- `erase_range(first, last)` 要求 `first <= last <= size()`。

当前实现没有专门处理 `first == last < size()` 的空区间；公开代码不要把任意空区间当作 no-op。需要条件删除时，先判断 `first != last`，或只在 `first == last == size()` 时把它当作尾部空区间。

## 迭代与算法

`vector<T>` 实现 `iterable`、`const_iterable` 和 `contiguous_mutable_range`：

```cp
vector<T> implements iterable
    type iter_type = ptr_iter<T>;
    type iter_item = T&;
    iter(self&) -> ptr_iter<T>;

vector<T> implements const_iterable
    type const_iter_type = const_ptr_iter<T>;
    type const_iter_item = T const&;
    iter(self const&) -> const_ptr_iter<T>;
```

```cp
import std;

main() -> i32
{
    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(1);
    values.push_back(2);

    sort(values);

    let total = 0;
    for(ref item : values) {
        total += item;
    }
    return total;
}
```

`for(const value : values)` 是按值绑定；需要避免拷贝时写 `for(const ref value : values)`。

## 失效规则

`vector` iterator 是指针快照。迭代开始后追加的新元素不会被已有 iterator 自动纳入遍历范围。只要容器发生可能移动、销毁或重排元素的修改，就不要继续使用旧 iterator、旧元素引用或旧 `begin()` / `end()` 指针。

这些操作都可能使旧引用、指针和 iterator 失效：

- `reserve`
- `push_back`
- `move_back`
- `insert`
- `erase`
- `erase_range`
- `resize`
- `clear`
- `pop_back`
- `pop`

## 当前限制

第一版 `vector` 没有 `shrink_to_fit`、`assign`、`append`、`extend`、`insert_range`、`erase_if`、`remove`、`contains`、`find`、`sort` 成员、`span()` / `as_slice()` 或 initializer-list 构造。范围转换也不由 `vector` 提供；`std.ranges` 当前没有 `to_vector()` / `to<vector>()` terminal。
