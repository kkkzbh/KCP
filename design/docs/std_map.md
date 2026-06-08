# 标准库 map

`map<K, V>` 是 `std.collections.map` 提供的有序唯一键关联容器。它按比较器维护 key 的顺序，支持查找、插入、删除、按序遍历、按序下标访问和 `rank(key)` 查询。

```cp
import std.collections.map;

main() -> i32
{
    let ids = map<i32, i32>{};
    ids.insert(2, 20);
    ids[1] = 10;
    ids.insert(2, 99);

    return ids.at(1) + ids.at(2);
}
```

## 模块与类型

```cp
import std.collections.map;
```

公开类型：

```cp
map<K, V, Order: ordering<K> = asc<K>>
map_node<K, V>
map_node_ref<K, V>
map_node_const_ref<K, V>
map_insert_result<K, V>
map_iter<K, V, Order: ordering<K> = asc<K>>
const_map_iter<K, V, Order: ordering<K> = asc<K>>
```

`std.collections` 聚合模块也会重导出 `map` 相关类型。`std.collections.detail.btree` 和 `std.collections.detail.btree_storage` 是内部实现，不属于稳定公共 API。

## 排序与唯一性

默认 `Order` 是 `asc<K>`。key 类型只要提供可用 `<=>`，且结果能转成 `weak_ordering`，就能直接使用默认 map。结构体可用显式 `operator <=> = default;` 按字段声明顺序生成比较。

`contains`、`find`、`at`、`insert`、`remove` 和 `rank` 都通过 `Order` 比较 key，不会再调用 `operator ==` 做相等判定。两个 key 只要 `order(left, right)` 返回 `weak_ordering::equivalent`，就被视为同一个唯一 key。

```cp
import std;

struct point {
    x: i32;
    y: i32;
}

impl point {
    operator <=>(self const&, rhs: point const&) -> weak_ordering = default;
}

main() -> i32
{
    let points = map<point, i32>{};
    points.insert(point{ .x = 1, .y = 2 }, 12);
    return points.at(point{ .x = 1, .y = 2 });
}
```

`Order` 是 map 保存的比较器对象，默认构造 `map` 时会使用 `Order{}`。第一版没有接收 comparator/order 实例的构造函数；需要自定义排序时，`Order` 类型本身必须能默认初始化，并把所需排序状态编码在类型或默认值中。

## 查询

```cp
size(self const&) -> usize;
empty(self const&) -> bool;
contains(self const&, key: K const&) -> bool;
find(self like&, key: K const&) -> optional<V like&>;
at(self like&, key: K const&) -> V like&;
operator [](self&, key: K) -> V&;
rank(self const&, key: K const&) -> usize;
nth(self&, index: usize) -> map_node_ref<K, V>;
nth(self const&, index: usize) -> map_node_const_ref<K, V>;
```

查询接口要区分语义：

- `contains` 只返回 key 是否存在。
- `find` 表达可能没有的查询，缺 key 返回 `optional::none`。
- `at` 是“调用者保证 key 存在”的前置条件访问。
- `operator[]` 在 key 不存在时插入 `V{}`，并返回对应 value 引用。
- `rank(key)` 返回严格小于 `key` 的元素数量；如果 key 不存在，返回它的插入位置。
- `nth(index)` 使用 0-based 中序下标访问按序节点。

```cp
let scores = map<i32, i32>{};
scores.insert(10, 100);

if(scores.contains(10)) {
    scores.at(10) += 1;
}

scores[20] = 200;
let first = scores.nth(0 as usize);
```

`at` 和 `nth` 当前用 `assert` 检查前置条件。debug/check 模式下缺 key 或越界会 panic；`--release` 下断言会被移除，调用者仍必须自行保证 key 存在或 `index < size()`。

## 插入、删除与节点结果

```cp
clear(self&) -> void;
insert(self&, key: K, value: V) -> map_insert_result<K, V>;
insert_node(self&, node: map_node<K, V>) -> map_insert_result<K, V>;
remove(self&, key: K const&) -> bool;
```

`insert` 和 `insert_node` 不覆盖已有 key；重复 key 返回已有 node，`inserted == false`。

```cp
let ids = map<i32, i32>{};
let first = ids.insert(7, 70);
let again = ids.insert(7, 99);

if(not again.inserted) {
    again.node.value = 71;
}
```

`map_node<K, V>` 是普通值结构体，只保存一个可移动/可复制的 `key` 和 `value`。`insert_node(node)` 是把这个值结构拆开后按唯一 key 插入的便捷入口，不是 node-handle、extract 或 splice 协议。

`map_insert_result.node` 是 `map_node_ref<K, V>`。`inserted == true` 时它指向新插入节点；`inserted == false` 时它指向已有节点。`node.key` 和 `node.value` 都是树内存储的 live 引用，不是插入参数的副本。

`insert` / `insert_node` 按值接收 key/value/node。即使 key 重复、最终返回已有 node，传入的 key/value/node 也已经被消费；不要在调用后依赖这些实参仍保留原值。

## 迭代

`map<K, V>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `map_node_ref<K, V>`，只读迭代元素为 `map_node_const_ref<K, V>`。

```cp
let items = map<i32, i32>{};
items.insert(2, 20);
items.insert(1, 10);

let total = 0;
for(node : items) {
    total += node.key + node.value;
}
```

map iterator 是 live index cursor，不是节点句柄或快照。稳定用法是通过 `for` 或 `items.iter()` 取得 iterator。遍历期间 `insert`、`remove`、`clear` 或修改 `map_node_ref.key` 都可能改变后续下标对应的元素。只修改 `map_node_ref.value` 不改变排序。

## 不变量与引用失效

非 const `map` 的 `map_node_ref.key` 是 `K&`，`value` 是 `V&`；const `map` 的 `map_node_const_ref` 两者都是 const 引用。当前类型系统不会阻止你通过 `map_node_ref.key` 原地修改 key，但容器不会因此重新排序或重新计算插入位置；修改 key 会破坏 `contains`、`find`、`rank`、`nth` 和后续插入/删除依赖的有序不变量。需要改变 key 时，先 `remove` 旧 key，再用新 key 插入。

`find`、`at`、`nth`、`insert` 和 `insert_node` 返回的引用在下一次容器修改或 `clear()` 后可能失效。

## 当前限制

第一版 map 没有 `lower_bound`、`upper_bound`、`equal_range`、从 key 开始的 iterator、`extract`、node handle、按 iterator 删除或区间删除。需要按序扫描时只能从 range-for 开始遍历，或用 `rank(key)` 得到下标后手动调用 `nth(index)`；这两步之间如果容器被修改，rank 得到的下标不再有稳定意义。
