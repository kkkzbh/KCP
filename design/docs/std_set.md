# 标准库 set

`set<K>` 是 `std.collections.set` 提供的有序唯一键容器。它只保存 key，不保存额外 value，适合表达去重集合、按序遍历和排名查询。

```cp
import std.collections.set;

main() -> i32
{
    let keys = set<i32>{};
    keys.insert(4);
    keys.insert(2);
    keys.insert(4);

    return keys.nth(0 as usize) + keys.nth(1 as usize);
}
```

## 模块与类型

```cp
import std.collections.set;
```

公开类型：

```cp
set<K, Order: ordering<K> = asc<K>>
set_node<K>
set_node_ref<K>
set_insert_result<K>
set_iter<K, Order: ordering<K> = asc<K>>
const_set_iter<K, Order: ordering<K> = asc<K>>
```

`std.collections` 聚合模块也会重导出 `set` 相关类型。`std.collections.detail.btree` 和 `std.collections.detail.btree_storage` 是内部实现，不属于稳定公共 API。

## 排序与唯一性

`set` 与 `map` 使用同一套有序唯一 key 语义。默认 `Order` 是 `asc<K>`，因此 key 类型只要提供可用 `<=>`，且结果能转成 `weak_ordering`，就能直接使用默认 set。

set 的唯一性、`contains`、`find`、`at`、`insert`、`remove`、`nth` 顺序和 `rank` 全部由 `Order` 决定。`operator ==` 不参与查找或重复 key 判断；两个 key 是否“相同”取决于 `Order` 是否返回 `weak_ordering::equivalent`。

`Order` 是 set 保存的比较器对象，默认构造 `set` 时会使用 `Order{}`。第一版没有接收 comparator/order 实例的构造入口。

## 查询

```cp
size(self const&) -> usize;
empty(self const&) -> bool;
contains(self const&, key: K const&) -> bool;
find(self like&, key: K const&) -> optional<K like&>;
at(self like&, key: K const&) -> K like&;
nth(self like&, index: usize) -> K like&;
nth_node(self like&, index: usize) -> set_node<K>;
rank(self const&, key: K const&) -> usize;
```

查询接口要区分语义：

- `contains` 只返回 key 是否存在。
- `find` 表达可能没有的查询，缺 key 返回 `optional::none`。
- `at` 是“调用者保证 key 存在”的前置条件访问。
- `nth(index)` 使用 0-based 中序下标访问按序 key。
- `nth_node(index)` 按值返回第 `index` 个 key 包装成的 `set_node<K>`，原 set 保持不变。
- `rank(key)` 返回严格小于 `key` 的元素数量；如果 key 不存在，返回它的插入位置。

```cp
let keys = set<i32>{};
keys.insert(10);
keys.insert(20);

if(keys.contains(10)) {
    let found = keys.at(10);
}

let pos = keys.rank(15);
let first = keys.nth(0 as usize);
```

`at`、`nth` 和 `nth_node` 都是前置条件访问；当前实现用 `assert` 检查缺 key 或越界，debug/check 模式下会 panic，`--release` 下断言会被移除，调用者仍必须自行保证 key 存在且下标小于 `size()`。

## 插入、删除与节点结果

```cp
clear(self&) -> void;
insert(self&, key: K) -> set_insert_result<K>;
insert_node(self&, node: set_node<K>) -> set_insert_result<K>;
remove(self&, key: K const&) -> bool;
```

`insert` / `insert_node` 不覆盖已有 key。重复 key 返回已有 node，`inserted == false`。

```cp
let keys = set<i32>{};
let first = keys.insert(7);
let again = keys.insert(7);

if(not again.inserted) {
    let live_key = again.node.key;
}
```

`insert` / `insert_node` 按值接收 key/node。重复 key 时不会替换已有 key，但传入的 key/node 已经被消费；不要在调用后依赖这些实参仍保留原值。

`set_insert_result.node` 是 `set_node_ref<K>`；其中 `node.key` 是树内 key 的可写引用。当前没有 `set_node_const_ref` 或 const 版插入结果类型。

`nth_node(index)` 不是 extract/remove。它会复制第 `index` 个 key 形成 `set_node<K>`，原 set 保持不变；因此当前用法要求 `K` 能从 `K&` 复制构造。需要转移所有权时，先复制或保存 key，再显式 `remove`。

## 迭代

`set<K>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `K&`，只读迭代元素为 `K const&`。

```cp
let keys = set<i32>{};
keys.insert(3);
keys.insert(1);
keys.insert(2);

let total = 0;
for(ref key : keys) {
    total += key;
}
```

set iterator 与 map iterator 一样是 live index cursor，不是快照。稳定用法仍然是通过 `for` 或 `keys.iter()` 取得 iterator。遍历期间 `insert`、`remove`、`clear` 或修改迭代得到的 `K&` 都可能改变后续下标对应的元素。

## 不变量与引用失效

非 const `set` 的 `find`、`at`、`nth` 和迭代器 item 都是 `K&`。当前类型系统允许原地修改这个 key，但 set 不会自动重排；修改 key 会破坏唯一性和有序不变量。需要改 key 时，按旧 key `remove`，再插入新 key。

`find`、`at`、`nth`、`insert` 和 `insert_node` 返回的引用在下一次容器修改或 `clear()` 后可能失效；`nth_node` 按值返回 node。

## 当前限制

第一版 set 没有 `lower_bound`、`upper_bound`、`equal_range`、从 key 开始的 iterator、`extract`、node handle、按 iterator 删除或区间删除。需要定位插入位置时只能用 `rank(key)` 得到下标，再按需要调用 `nth(index)`；容器修改后这个下标不再稳定。
