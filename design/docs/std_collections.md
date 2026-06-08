# 标准库 collections

本文档记录 `std.collections` 第一版公共容器接口。连续容器基于 `vector<T>`；有序关联容器提供唯一 key、有序遍历、按序访问和 rank 查询。

容器的完整教程和细节现在按类型拆分到单独页面：

- [标准库 vector](std_vector.md)：连续动态数组、容量、失效规则、迭代和算法入口。
- [标准库 map](std_map.md)：有序唯一键映射、node 引用、插入结果、按序访问和 rank。
- [标准库 set](std_set.md)：有序唯一键集合、重复插入语义、按序访问和 rank。

本页保留模块布局、聚合导入边界和公共接口速查。

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

`std.collections` 聚合模块当前只重导出 `std.collections.vector`、`std.collections.map` 和 `std.collections.set`。排序等泛型算法属于独立的 `std.algorithm` / `std.algorithm.sort`，不是 `std.collections` 的导出项，也不是 `vector` / `map` / `set` 成员函数。只写 `import std.collections;` 时不要期待 `sort`、`stable_sort`、`to_vector` 或其它 range collect 入口可见。

直接导入子模块时，只得到该子模块导出的容器类型、结果类型和 iterator 类型。`std.collections.vector`、`std.collections.map` 和 `std.collections.set` 对 `std.core.iter`、`std.core.option`、`std.compare`、`std.memory.span` 等依赖都是普通 `import`，不会继续重导出给用户。需要直接写 `optional`、`weak_ordering`、`asc` 或 `ordering` 时，应显式导入 `std.core` / `std.compare`，或导入重导出它们的 `std`。

当前语言没有字段私有性，因此 `vector<T>` 的 `storage` / `len`、`map<K, V>` 的 `tree` 和 `set<K>` 的 `tree` 在源码层仍可访问。这些字段是实现细节；公共代码应只使用下面列出的成员函数、operator 和迭代协议。

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

- `vector(count)` 会默认构造 `count` 个 `T{}`；`vector(count, value)` 会复制构造 `count` 个 `value`。因此第一种形式要求 `T` 可默认初始化，第二种形式要求 `T const&` 能构造 `T`。
- copy 构造和 copy 赋值逐元素复制；move 构造和 move 赋值接管底层 `vector_storage`，并把源对象长度置为 0。
- `data()` / `begin()` 返回第一个元素槽位指针，`end()` 返回 `data() + size()`；空 vector 上这三个指针只表达当前 storage 位置，不表示有可读元素。`end()` 不能解引用。
- 可写 `vector<T>` 的 range-for 选择 `iter(self&)`，iterator item 是 `T&`。
- 只读 `vector<T>` 的 range-for 选择同名 `iter(self const&)`，iterator item 是 `T const&`。
- `vector<T>` 实现 `contiguous_mutable_range`，其 `item` 是 `T`。
- `for(const value : values)` 仍然是按值绑定；需要避免拷贝时写 `for(const ref value : values)`。
- `vector` iterator 是指针快照，不像 map/set iterator 那样每轮重新查询容器大小；迭代开始后追加的新元素不会被已有 iterator 自动纳入遍历范围。只要容器发生可能移动、销毁或重排元素的修改，就不要继续使用旧 iterator、旧元素引用或旧 `begin()` / `end()` 指针。
- `operator[]`、`front`、`back`、`pop_back`、`pop`、`insert`、`erase` 和 `erase_range` 都用 `assert` 表达前置条件。debug/check 模式下越界或空容器访问会 panic；`--release` 会移除这些 assert，调用者必须自行保证条件成立。
- `reserve(new_capacity)` 只保证容量至少达到 `new_capacity`；`new_capacity <= capacity()` 时不做事。它不改变 `size()`，也不构造新元素。
- `push_back(value: T)` 按值接收并 move 到尾部；`move_back(value: T move&)` 从显式 move 引用构造尾部元素。
- `push_back`、`move_back` 和 `insert` 会按需自动增长容量；增长可能移动已有元素并使旧引用、指针和 iterator 失效。不要依赖具体增长倍率。
- `pop()` 把尾部元素 move 出来并销毁原槽位；`pop_back()` 只销毁尾部元素，不返回值。
- `clear()` 把 `size()` 变为 0，但不释放或缩小底层 storage；`capacity()` 保持不变。第一版没有 `shrink_to_fit`。
- `resize(new_size, value)` 缩小时销毁尾部元素，扩张时复制 `value` 填充新元素。当前没有只传 `new_size` 的默认填充重载。
- `insert(index, value)` 允许 `index == size()`，此时等价于尾部插入；`index > size()` 是前置条件错误。插入位置及之后的元素会向后移动。
- `erase(index)` 要求 `index < size()`，移除一个元素并把后续元素向前移动。`erase_range(first, last)` 要求 `first <= last <= size()`；当前实现没有专门处理 `first == last < size()` 的空区间，公开代码不要把任意空区间当作 no-op。需要条件删除时，先判断 `first != last`，或只在 `first == last == size()` 时把它当作尾部空区间。
- `reserve`、`push_back`、`move_back`、`insert`、`erase`、`erase_range`、`resize`、`clear`、`pop_back` 和 `pop` 都可能销毁或移动元素，已经取得的元素指针、引用、iterator、`data()` / `begin()` / `end()` 指针在容器修改后可能失效。
- 第一版没有 `shrink_to_fit`、`assign`、`append`、`extend`、`insert_range`、`erase_if`、`remove`、`contains`、`find`、`sort` 成员、`span()` / `as_slice()` 或 initializer-list 构造。范围转换也不由 `vector` 提供；`std.ranges` 当前没有 `to_vector()` / `to<vector>()` terminal。

## map

```cp
map<K, V, Order: ordering<K> = asc<K>>
map_node<K, V>
map_node_ref<K, V>
map_node_const_ref<K, V>
map_insert_result<K, V>
map_iter<K, V, Order: ordering<K> = asc<K>>
const_map_iter<K, V, Order: ordering<K> = asc<K>>
```

`map` 按 `Order` 维护唯一 key。默认 `Order` 是 `asc<K>`，因此 key 类型只要提供可用 `<=>` 即可直接使用；结构体可用显式 `operator <=> = default;` 按字段声明顺序生成比较。order object 返回 `weak_ordering`，等价结果表示同一个唯一 key。`partial_ordering` 不满足有序容器要求。

`Order` 是 map 保存的比较器对象，默认构造 `map` 时会使用 `Order{}`。第一版没有接收 comparator/order 实例的构造函数；需要自定义排序时，`Order` 类型本身必须能默认初始化，并把所需排序状态编码在类型或默认值中。

`contains`、`find`、`at`、`insert`、`remove` 和 `rank` 都通过 `Order` 比较 key，不会再调用 `operator ==` 做相等判定。两个 key 只要 `order(left, right)` 返回 `weak_ordering::equivalent`，就被视为同一个唯一 key；如果 `operator ==` 与 `Order` 的等价类不一致，map 仍以 `Order` 为准。

公开接口：

```cp
map() = default;
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
- `at` 表达前置条件访问，调用者保证 key 存在；当前实现用 `assert` 检查，debug/check 模式下缺 key 会 panic，`--release` 下断言会被移除，调用者仍必须自行保证 key 存在。
- `operator[]` 在 key 不存在时插入 `V{}`，并返回对应 value 引用。因此它要求 `V` 在当前实例中可默认初始化；如果 value 类型没有默认值，应使用 `insert(key, value)`、`insert_node(node)`、`find` 或 `at`，不要用 `operator[]` 表达“只查询不插入”。
- `insert` 和 `insert_node` 不覆盖已有 key；重复 key 返回已有 node，`inserted == false`。
- `map_node<K, V>` 是普通值结构体，只保存一个可移动/可复制的 `key` 和 `value`。`insert_node(node)` 是把这个值结构拆开后按唯一 key 插入的便捷入口，不是 node-handle、extract 或 splice 协议；当前 map 没有 `nth_node`、`extract` 或“从树中取出节点再插回”的拥有式节点接口。
- `map_insert_result.node` 是 `map_node_ref<K, V>`。`inserted == true` 时它指向新插入节点；`inserted == false` 时它指向已有节点。`node.key` 和 `node.value` 都是树内存储的 live 引用，不是插入参数的副本。
- `insert` / `insert_node` 按值接收 key/value/node。即使 key 重复、最终返回已有 node，传入的 key/value/node 也已经被消费；不要在调用后依赖这些实参仍保留原值。
- `remove` 返回是否真的移除了 key；缺 key 返回 `false`。
- copy 构造和 copy 赋值复制当前 key/value 和 order；move 构造和 move 赋值转移容器存储，并把源容器置为空。`clear()` 会销毁所有当前节点。
- `find`、`at`、`nth`、`insert` 和 `insert_node` 返回的引用在下一次容器修改或 `clear()` 后可能失效。
- 非 const `map` 的 `map_node_ref.key` 是 `K&`，`value` 是 `V&`；const `map` 的 `map_node_const_ref` 两者都是 const 引用。当前类型系统不会阻止你通过 `map_node_ref.key` 原地修改 key，但容器不会因此重新排序或重新计算插入位置；修改 key 会破坏 `contains`、`find`、`rank`、`nth` 和后续插入/删除依赖的有序不变量。需要改变 key 时，先 `remove` 旧 key，再用新 key 插入。
- `nth` 使用 0-based 中序下标；当前实现用 `assert` 检查下标，debug/check 模式下越界会 panic，`--release` 下断言会被移除，调用者仍必须自行保证 `index < size()`。
- `rank(key)` 返回严格小于 `key` 的元素数量；如果 key 不存在，返回它的插入位置。
- 第一版 map 没有 `lower_bound`、`upper_bound`、`equal_range`、从 key 开始的 iterator、`extract`、node handle、按 iterator 删除或区间删除。需要按序扫描时只能从 range-for 开始遍历，或用 `rank(key)` 得到下标后手动调用 `nth(index)`；这两步之间如果容器被修改，rank 得到的下标不再有稳定意义。
- `map<K, V>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `map_node_ref<K, V>`，只读迭代元素为 `map_node_const_ref<K, V>`。

map iterator 是 live index cursor，不是节点句柄或快照；稳定用法是通过 `for` 或 `items.iter()` 取得 iterator。遍历期间 `insert`、`remove`、`clear` 或修改 `map_node_ref.key` 都可能改变后续下标对应的元素。只修改 `map_node_ref.value` 不改变排序，但不要在同一次遍历里混用结构性修改。

## set

```cp
set<K, Order: ordering<K> = asc<K>>
set_node<K>
set_node_ref<K>
set_insert_result<K>
set_iter<K, Order: ordering<K> = asc<K>>
const_set_iter<K, Order: ordering<K> = asc<K>>
```

`set` 与 `map` 使用同一套有序唯一 key 语义，只保存 key。`Order` 的处理方式与 `map` 相同：set 内部保存一个默认构造出来的 `Order{}`，没有运行时传入 comparator 实例的构造入口。

set 的唯一性、`contains`、`find`、`at`、`insert`、`remove`、`nth` 顺序和 `rank` 同样全部由 `Order` 决定。`operator ==` 不参与查找或重复 key 判断；两个 key 是否“相同”取决于 `Order` 是否返回 `weak_ordering::equivalent`。

公开接口：

```cp
set() = default;
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

规则：

- `set.insert` 的重复 key 语义与 `map.insert` 一致：返回已有 node，`inserted == false`。
- `insert` / `insert_node` 按值接收 key/node。重复 key 时不会替换已有 key，但传入的 key/node 已经被消费；不要在调用后依赖这些实参仍保留原值。
- `set_insert_result.node` 是 `set_node_ref<K>`；其中 `node.key` 是树内 key 的可写引用。当前没有 `set_node_const_ref` 或 const 版插入结果类型，`inserted == false` 时它指向已有 key，`inserted == true` 时指向新插入 key。和 `find`、`at`、`nth`、迭代器返回的 `K&` 一样，不应通过这个引用原地修改 key；修改会破坏排序和唯一性不变量。
- `remove` 返回是否真的移除了 key；缺 key 返回 `false`。
- `at`、`nth` 和 `nth_node` 都是前置条件访问；当前实现用 `assert` 检查缺 key 或越界，debug/check 模式下会 panic，`--release` 下断言会被移除，调用者仍必须自行保证 key 存在且下标小于 `size()`。
- copy 构造和 copy 赋值复制当前 key 和 order；move 构造和 move 赋值转移容器存储，并把源容器置为空。`clear()` 会销毁所有当前节点。
- `find`、`at`、`nth`、`insert` 和 `insert_node` 返回的引用在下一次容器修改或 `clear()` 后可能失效；`nth_node` 按值返回 node。
- `nth_node(index)` 不是 extract/remove。它会复制第 `index` 个 key 形成 `set_node<K>`，原 set 保持不变；因此当前用法要求 `K` 能从 `K&` 复制构造。需要转移所有权时，先复制或保存 key，再显式 `remove`。
- 第一版 set 没有 `lower_bound`、`upper_bound`、`equal_range`、从 key 开始的 iterator、`extract`、node handle、按 iterator 删除或区间删除。需要定位插入位置时只能用 `rank(key)` 得到下标，再按需要调用 `nth(index)`；容器修改后这个下标不再稳定。
- 非 const `set` 的 `find`、`at`、`nth` 和迭代器 item 都是 `K&`。当前类型系统允许原地修改这个 key，但 set 不会自动重排；修改 key 会破坏唯一性和有序不变量。需要改 key 时，按旧 key `remove`，再插入新 key。

`set<K>` 实现 `iterable` 与 `const_iterable`。可写迭代元素为 `K&`，只读迭代元素为 `K const&`。

set iterator 与 map iterator 一样是 live index cursor，不是快照；稳定用法仍然是通过 `for` 或 `keys.iter()` 取得 iterator。遍历期间 `insert`、`remove`、`clear` 或修改迭代得到的 `K&` 都可能改变后续下标对应的元素。不要在 range-for 中结构性修改正在遍历的 set。
