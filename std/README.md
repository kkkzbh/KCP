# 标准库源码

`std/` 放置 cp 标准库源码。标准库使用普通 cp 模块表达，不引入语言关键字特权。

编译器驱动需要提供标准库搜索路径、自动导入策略和最终链接规则。

第一批标准库按领域分层。根目录只保留聚合模块，具体实现放到同名目录中：

- `std`: 总聚合入口，重导出第一批标准库公共领域模块。
- `std.core`: 基础协议和结果类型，重导出 `std.core.option`、`std.core.expected`、`std.core.iter`。
- `std.memory`: 连续内存基础类型，重导出 `std.memory.raw_buffer`、`std.memory.span`。
- `std.collections`: 集合类型，重导出 `std.collections.vector`、`std.collections.map`、`std.collections.set`。
- `std.text`: 文本类型，重导出 `std.text.str`、`std.text.string`。
- `std.ranges`: ranges 聚合入口，重导出 `std.ranges.*` 的公共范围对象。
- `std.compare`: 比较协议和默认比较器，定义 `partial_ordering`、`weak_ordering`、`strong_ordering`、`mutable_object`、`strict_weak_order<T>`、`equality_comparable`、`three_way_comparable`、`less<T>`、`greater<T>`。
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
- `std.collections.map`: 定义基于 size-augmented B-tree 的有序唯一键 `map<K, V, Compare = less<K>>`。
- `std.collections.set`: 定义基于同一 B-tree 内核的有序唯一键 `set<K, Compare = less<K>>`。
- `std.collections.detail.btree_storage` / `std.collections.detail.btree`: collections 内部 B-tree 存储和算法内核，维护 subtree size 以支持 `nth` / `rank`。
- `std.text.str`: 定义 compiler-recognized `str` 的 `size` / `data` 方法、`char` 迭代器、`iterable` 实现和按显式长度比较的字典序 operator。
- `std.text.detail.string_storage`: 定义 string 专用存储壳，封装 trailing `'\0'` 所需的物理容量。
- `std.text.string`: 定义拥有字符存储的 `string`，维护文本长度，并可通过 `as_str()` 借出 `str`。
- `std.ranges.iota`: 定义可被范围 `for` 消费的泛型半开范围 `iota(begin, end)`，要求元素类型满足 `equality_comparable<T>` 和 `incrementable`。
- `std.compare`: `partial_ordering`、`weak_ordering`、`strong_ordering` 表达三路比较结果；`strict_weak_order<T>` 要求比较器可用 `compare(left, right)` 形式调用；`equality_comparable<Rhs = this>` 要求 `==` 返回 `bool`；`three_way_comparable<Rhs = this, Category = weak_ordering>` 要求 `<=>` 返回指定比较分类；`incrementable` 要求前置 `++` 返回 `this&`；`less<T>` 和 `greater<T>` 分别基于 `<` 和 `>`。第一版 `sort` / `map` / `set` 默认比较器仍基于 `strict_weak_order`，不把 `partial_ordering` 作为默认有序容器比较结果。
- `std.algorithm.sort`: `sort<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{}) -> void` 使用非稳定 in-place hybrid quicksort；`stable_sort` 保留稳定 Powersort 风格 run-adaptive merge 实现。`span<T>` 必须借用可写元素，`T: mutable_object` 会拒绝只读元素视图。

`map` 和 `set` 的插入不覆盖已有 key。重复插入返回已有 node，结果中的 `inserted` 为 `false`。`find` 返回 `optional`，`at` 和 `nth` 是前置条件访问，违反时通过 `assert`/`panic` 终止。关联容器的 node/key/value 引用在下一次容器修改后可能失效。

## `std.io` 第一版边界

`std.io` 只做输出，不做标准输入。格式字符串只支持：

- `{}`：消费一个满足 `display` 的参数。
- `{{` / `}}`：输出字面量花括号。

第一版 `display` 使用 `display(self const&, formatter&) -> display_result` 协议，不直接写 stdout/stderr。
内置 `display` 实现覆盖 `bool`、整数、浮点、`char`、`str`，并覆盖标准库 `string`。`format` 返回
`format_result`，其它写入入口返回 `display_result`。不做编译期格式字符串检查，也不支持宽度、精度、进制、位置参数、命名参数或 locale。
格式扫描按 `str.size()` 的长度语义进行，字符串中的中间 `'\0'` 不会截断输出。
