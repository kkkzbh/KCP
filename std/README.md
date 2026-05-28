# 标准库源码

`std/` 放置 cp 标准库源码。标准库使用普通 cp 模块表达，不引入语言关键字特权。

编译器驱动需要提供标准库搜索路径、自动导入策略和最终链接规则。

第一批标准库按领域分层。根目录只保留聚合模块，具体实现放到同名目录中：

- `std`: 总聚合入口，重导出第一批标准库公共领域模块。
- `std.core`: 基础协议和结果类型，重导出 `std.core.option`、`std.core.expected`、`std.core.iter`。
- `std.memory`: 连续内存基础类型，重导出 `std.memory.raw_buffer`、`std.memory.span`。
- `std.collections`: 集合类型，重导出 `std.collections.vector`、`std.collections.map`、`std.collections.set`。
- `std.text`: 文本类型，重导出 `std.text.str`、`std.text.string`。
- `std.ranges`: ranges 聚合入口，重导出 `std.ranges.*` 的公共 view source、adapter 和 terminal。
- `std.meta`: 编译期类型查询和 callable concept，供泛型标准库提取类型能力。
- `std.compare`: 比较协议和默认 order object，定义 `partial_ordering`、`weak_ordering`、`strong_ordering`、`mutable_object`、`ordering<T>`、`equality_comparable`、`three_way_comparable`、`asc<T>`、`desc<T>`。
- `std.algorithm`: 泛型算法聚合入口，第一版重导出 `std.algorithm.sort`。
- `std.io`: 第一版格式化输出，提供 `format` / `format_to` / `print` / `println` / `eprint` / `eprintln`。
- `std.fs`: 第一版同步文件 IO，重导出 `std.fs.file`。

具体实现模块：

- `std.core.option`: 定义 `optional<T>` 和基础查询/回退/解引用方法；`operator*` 在空值上 panic。
- `std.core.expected`: 定义 `expected<T,E>` 和基础查询/回退/解引用方法；`operator*` 在 unexpected 上 panic。
- `std.core.iter`: 定义 `iterator` 和 `iterable` 迭代协议。
- `std.memory.raw_buffer`: 定义只拥有原始存储的 `raw_buffer<T>`，由上层容器负责元素构造和析构。
- `std.memory.span`: 定义借用连续区间的 `span<T>`。
- `std.collections.detail.vector_storage`: 定义基于 `raw_buffer<T>` 的 vector 专用存储壳，仍由 `vector<T>` 维护元素生命周期。
- `std.collections.vector`: 定义基于 `vector_storage<T>` 的动态数组。
- `std.collections.map`: 定义基于 size-augmented B-tree 的有序唯一键 `map<K, V, Order = asc<K>>`。
- `std.collections.set`: 定义基于同一 B-tree 内核的有序唯一键 `set<K, Order = asc<K>>`。
- `std.collections.detail.btree_storage` / `std.collections.detail.btree`: collections 内部 B-tree 存储和算法内核，维护 subtree size 以支持 `nth` / `rank`。
- `std.text.str`: 定义 compiler-recognized `str` 的 `size` / `data` 方法、`char` 迭代器、`iterable` 实现和按显式长度比较的字典序 operator。
- `std.text.detail.string_storage`: 定义 string 专用存储壳，封装 trailing `'\0'` 所需的物理容量。
- `std.text.string`: 定义拥有字符存储的 `string`，维护文本长度，并可通过 `as_str()` 借出 `str`。
- `std.meta`: 定义 `read_type`、`remove_reference`、`pointee`、`tuple_element`、`call_result` 类型查询，以及编译器识别的 `callable<Args...>`、reference 分类 concept。
- `std.ranges.iota`: 定义可被范围 `for` 和 ranges adapter 消费的泛型半开 view `iota(begin, end)`，要求元素类型满足 `equality_comparable<T>` 和 `incrementable`。
- `std.ranges.sources`: 定义 `view`、`to_view`、`ref_view` / `const_ref_view` / `owning_view`，以及 `empty<T>()`、`single(value)`、无限 `repeat(value)`。
- `std.ranges.adapters`: 定义 lazy adapter：`filter`、`transform`、`take`、`drop`、`enumerate`、`zip`、`concat`。
- `std.ranges.terminals`: 定义消费 range 的 `count`、`fold`、`any`、`all_of`、`find`。
- `std.compare`: `partial_ordering`、`weak_ordering`、`strong_ordering` 表达三路比较结果；`ordering<T>` 要求 order object 可用 `order(left, right)` 形式调用并返回 `weak_ordering`；`equality_comparable<Rhs = this>` 要求 `==` 返回 `bool`；`three_way_comparable<Rhs = this, Category = weak_ordering>` 要求 `<=>` 返回指定比较分类；`incrementable` 要求前置 `++` 返回 `this&`；`asc<T>` 基于 `<=>` 升序，`desc<T>` 基于 `asc<T>` 反向。`partial_ordering` 不满足 `sort` / `map` / `set` 的默认有序要求。
- `std.algorithm.sort`: `sort<T: mutable_object, Order: ordering<T> = asc<T>>(values: span<T>, order: Order = Order{}) -> void` 使用非稳定 in-place hybrid quicksort；`stable_sort` 保留稳定 Powersort 风格 run-adaptive merge 实现。`span<T>` 必须借用可写元素，`T: mutable_object` 会拒绝只读元素视图。

`map` 和 `set` 的插入不覆盖已有 key。重复插入返回已有 node，结果中的 `inserted` 为 `false`。`find` 返回 `optional`，`at` 和 `nth` 是前置条件访问，违反时通过 `assert`/`panic` 终止。关联容器的 node/key/value 引用在下一次容器修改后可能失效。

## `std.io` 第一版边界

`std.io` 只做输出，不做标准输入。格式字符串只支持：

- `{}`：消费一个满足 `display` 的参数。
- `{{` / `}}`：输出字面量花括号。

第一版 `display` 使用 `display(self const&, formatter&) -> display_result` 协议，不直接写 stdout/stderr。
内置 `display` 实现覆盖 `bool`、整数、浮点、`char`、`str`，并覆盖标准库 `string`。`format` 返回
`format_result`，其它写入入口返回 `display_result`。不做编译期格式字符串检查，也不支持宽度、精度、进制、位置参数、命名参数或 locale。
格式扫描按 `str.size()` 的长度语义进行，字符串中的中间 `'\0'` 不会截断输出。
