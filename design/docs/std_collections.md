# 标准库 collections

本文档记录 `std.collections` 第一版公共容器接口。连续容器基于 `vector<T>`；有序关联容器基于 collections 专用的 size-augmented B-tree 内核，并维护子树大小以支持按序访问和 rank 查询。

## 模块布局

```text
std.collections                 聚合集合公共模块
std.collections.vector          动态数组
std.collections.detail.vector_storage  vector 内部连续存储壳
std.collections.map             有序唯一键 map
std.collections.set             有序唯一键 set
std.collections.detail.btree_storage
std.collections.detail.btree    collections 内部 B-tree 内核
```

`std.collections.detail.vector_storage`、`std.collections.detail.btree_storage` 和 `std.collections.detail.btree` 是容器实现细节，不由 `std.collections` 重导出。公开容器通过组合持有内部存储结构。

## vector

```cp
vector<T>
```

`vector` 是连续动态数组。它的元素存储在一段连续内存中，`data`、`begin`、`end` 和 `operator[]` 使用 `self like&`，因此同一套接口会随 receiver 自动得到 `T*` / `T const*` 或 `T&` / `T const&`。

公开接口：

```cp
vector() = default;
vector(count: usize);
vector(count: usize, value: T const&);
data(self like&) -> T like*;
begin(self like&) -> T like*;
end(self like&) -> T like*;
size(self const&) -> usize;
capacity(self const&) -> usize;
empty(self const&) -> bool;
operator [](self like&, index: usize) -> T like&;
front(self like&) -> T like&;
back(self like&) -> T like&;
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

迭代接口：

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

规则：

- 可写 `vector<T>` 的 range-for 选择 `iter(self&)`，iterator item 是 `T&`。
- 只读 `vector<T>` 的 range-for 选择同名 `iter(self const&)`，iterator item 是 `T const&`。
- `vector<T>` 实现 `contiguous_mutable_range`，其 `item` 是 `T`。
- `for(const value : values)` 仍然是按值绑定；需要避免拷贝时写 `for(const ref value : values)`。
- `reserve`、`push_back`、`insert`、`erase`、`erase_range`、`resize` 等可能移动元素，已经取得的元素指针或引用在容器修改后可能失效。

## map

```cp
map<K, V, Order: ordering<K> = asc<K>>
map_node<K, V>
map_node_ref<K, V>
map_node_const_ref<K, V>
map_insert_result<K, V>
```

`map` 按 `Order` 维护唯一 key。默认 `Order` 是 `asc<K>`，因此 key 类型只要提供可用 `<=>` 即可直接使用；结构体可用显式 `operator <=> = default;` 按字段声明顺序生成比较。order object 返回 `weak_ordering`，等价结果表示同一个唯一 key。`partial_ordering` 不满足有序容器要求。

公开接口：

```cp
size(self const&) -> usize;
empty(self const&) -> bool;
clear(self&) -> void;
contains(self const&, key: K const&) -> bool;
find(self like&, key: K const&) -> optional<V like&>;
at(self like&, key: K const&) -> V like&;
operator [](self&, key: K) -> V&;
insert(self&, key: K, value: V) -> map_insert_result<K, V>;
insert_node(self&, node: map_node<K, V>) -> map_insert_result<K, V>;
remove(self&, key: K const&) -> bool;
nth(self&, index: usize) -> map_node_ref<K, V>;
nth(self const&, index: usize) -> map_node_const_ref<K, V>;
rank(self const&, key: K const&) -> usize;
```

规则：

- `find` 表达可能没有的查询，缺 key 返回 `optional::none`。
- `at` 表达前置条件访问，调用者保证 key 存在；缺 key 时 panic。
- `operator[]` 在 key 不存在时插入 `V{}`，并返回对应 value 引用。
- `insert` 和 `insert_node` 不覆盖已有 key；重复 key 返回已有 node，`inserted == false`。
- `find`、`at`、`nth` 和 `insert` 返回的引用在下一次容器修改后可能失效。
- `nth` 使用 0-based 中序下标，越界 panic。
- `rank(key)` 返回严格小于 `key` 的元素数量；如果 key 不存在，返回它的插入位置。
- `map<K, V>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `map_node_ref<K, V>`，只读迭代元素为 `map_node_const_ref<K, V>`。

## set

```cp
set<K, Order: ordering<K> = asc<K>>
set_node<K>
set_node_ref<K>
set_insert_result<K>
```

`set` 与 `map` 使用同一个内部 B-tree 内核，只保存唯一 key。

公开接口：

```cp
size(self const&) -> usize;
empty(self const&) -> bool;
clear(self&) -> void;
contains(self const&, key: K const&) -> bool;
find(self like&, key: K const&) -> optional<K like&>;
at(self like&, key: K const&) -> K like&;
insert(self&, key: K) -> set_insert_result<K>;
insert_node(self&, node: set_node<K>) -> set_insert_result<K>;
remove(self&, key: K const&) -> bool;
nth(self like&, index: usize) -> K like&;
nth_node(self like&, index: usize) -> set_node<K>;
rank(self const&, key: K const&) -> usize;
```

`set.insert` 的重复 key 语义与 `map.insert` 一致：返回已有 node，`inserted == false`。`find`、`at`、`nth` 和 `insert` 返回的引用在下一次容器修改后可能失效；`nth_node` 按值返回 node。

`set<K>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `K&`，只读迭代元素为 `K const&`。迭代器按当前中序下标访问元素，因此会按序看到遍历过程中插入到后续下标的新元素。
