# 标准库 collections

本文档记录 `std.collections` 第一版公共容器接口。连续容器基于 `vector<T>`；有序关联容器基于 collections 专用的 size-augmented 红黑树内核，并维护子树大小以支持按序访问和 rank 查询。

## 模块布局

```text
std.collections                 聚合集合公共模块
std.collections.vector          动态数组
std.collections.map             有序唯一键 map
std.collections.set             有序唯一键 set
std.collections.detail.rb_tree  collections 内部红黑树内核
```

`std.collections.detail.rb_tree` 是容器实现细节，不由 `std.collections` 重导出。公开容器通过组合持有内部树。

## map

```cp
map<K, V, Compare: strict_weak_order<K> = less<K>>
map_node<K, V>
map_insert_result<K, V>
```

`map` 按 `Compare` 维护唯一 key。默认 `Compare` 是 `less<K>`。比较器返回 `bool`，表示左侧 key 是否应排在右侧 key 前。

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
nth(self like&, index: usize) -> map_node<K, V> like&;
rank(self const&, key: K const&) -> usize;
```

规则：

- `find` 表达可能没有的查询，缺 key 返回 `optional::none`。
- `at` 表达前置条件访问，调用者保证 key 存在；缺 key 时 panic。
- `operator[]` 在 key 不存在时插入 `V{}`，并返回对应 value 引用。
- `insert` 和 `insert_node` 不覆盖已有 key；重复 key 返回已有 node，`inserted == false`。
- `nth` 使用 0-based 中序下标，越界 panic。
- `rank(key)` 返回严格小于 `key` 的元素数量；如果 key 不存在，返回它的插入位置。

## set

```cp
set<K, Compare: strict_weak_order<K> = less<K>>
set_node<K>
set_insert_result<K>
```

`set` 与 `map` 使用同一棵内部红黑树，只保存唯一 key。

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
nth_node(self like&, index: usize) -> set_node<K> like&;
rank(self const&, key: K const&) -> usize;
```

`set.insert` 的重复 key 语义与 `map.insert` 一致：返回已有 node，`inserted == false`。
