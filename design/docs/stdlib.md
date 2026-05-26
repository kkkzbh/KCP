# 标准库导览

cp 标准库位于仓库根目录 `std/`，使用普通 cp 模块实现。语言本身只提供必要的语法和少量内建能力，集合、字符串、范围、格式化和文件 IO 都通过标准库模块组织。

## 导入方式

学习阶段可以直接导入聚合入口：

```cp
import std;
```

`std` 会重导出第一批公共领域模块。需要更清晰的依赖边界时，也可以导入具体模块，例如 `std.collections`、`std.ranges` 或 `std.fs`。

## 模块分层

| 模块 | 内容 |
| --- | --- |
| `std.core` | `optional<T>`、`expected<T,E>`、`iterator`、`iterable` |
| `std.memory` | `raw_buffer<T>`、`span<T>` 和连续内存基础类型 |
| `std.collections` | `vector<T>`、`map<K,V>`、`set<K>` |
| `std.text` | compiler-recognized `str` 的扩展和拥有字符串 `string` |
| `std.ranges` | 可被范围 `for` 消费的范围对象，第一版提供 `iota` |
| `std.compare` | 三路比较分类、比较器和泛型算法所需 concept |
| `std.algorithm` | 泛型算法，第一版提供 `sort` |
| `std.io` | `format`、`print`、`println`、`eprint`、`eprintln` |
| `std.fs` | 同步文件打开、读取、写入和关闭 |

## optional 和 expected

`optional<T>` 表示“可能没有值”，`expected<T,E>` 表示“值或错误”。它们常用于可恢复失败路径。

```cp
main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let value = expected<i32,str>::value(12);
    let error = expected<i32,str>::unexpected("bad");

    if(some.has_value() and not none.has_value() and value.has_value() and not error.has_value()) {
        return some.value_or(0) + none.value_or(10) + value.value_or(0);
    }

    return 1;
}
```

相关参考：[错误处理](error_handling.md)。

## raw_buffer、span、vector

`raw_buffer<T>` 拥有原始连续存储，负责容量和释放；元素构造和析构由上层容器或调用者控制。`span<T>` 借用连续区间，不拥有内存。`vector<T>` 是基于 `vector_storage<T>` 的动态数组，`string` 通过 `string_storage` 封装 trailing `'\0'` 所需的物理容量。

```cp
main() -> i32
{
    let storage = raw_buffer<i32>{2};
    construct_at(storage.data(), 5);
    construct_at(storage.data() + 1, 7);

    let values = vector<i32>{};
    values.push_back(3);
    values.push_back(4);
    values.insert(1 as usize, 5);

    let sum = *storage.data() + *(storage.data() + 1) + values[0 as usize];

    destroy_at(storage.data() + 1);
    destroy_at(storage.data());
    return sum;
}
```

相关参考：[底层内存分配](memory_allocation.md)、[标准库 collections](std_collections.md)。

## map 和 set

`map` / `set` 是有序唯一键容器，底层基于维护 subtree size 的 B-tree。重复插入不会覆盖已有 key，结果会标记 `inserted = false`。查询或插入返回的引用在下一次容器修改后可能失效。

```cp
main() -> i32
{
    let ids = map<i32, i32>{};
    let keys = set<i32>{};

    ids.insert(2, 20);
    ids[1] = 10;
    ids.insert(2, 99);

    keys.insert(4);
    keys.insert(2);
    keys.insert(4);

    return ids.at(1) + ids.at(2) + ids.nth(0 as usize).value + keys.nth(0 as usize);
}
```

相关参考：[标准库 collections](std_collections.md)。

## str 和 string

`str` 是 compiler-recognized 的只读字符串视图，标准库为它提供长度、数据指针、迭代和字典序比较。`string` 拥有字符存储，维护 trailing `'\0'`，可以借出 `str`。

```cp
main() -> i32
{
    let text = string{"hi"};
    text.push_back('!');
    text.append(" cp");
    text[0 as usize] = 'H';
    println("owned string = {}", text);
    return text.size() as i32;
}
```

相关参考：[类型系统](type_system.md)、[迭代](iteration.md)。

## iota 和范围 for

`iota(begin, end)` 产生半开范围 `[begin, end)`，元素类型需要支持 `==` 和前置 `++`。最常见用法是整数循环。

```cp
main() -> i32
{
    let total = 0;

    for(let index : iota(0, 4)) {
        total += index;
    }

    return total;
}
```

相关参考：[标准库 ranges](std_ranges.md)、[迭代](iteration.md)。

## 格式化输出

`std.io` 第一版只提供输出，不提供标准输入。格式字符串支持 `{}` 占位，以及双左花括号 / 双右花括号输出字面量花括号。

```cp
main() -> i32
{
    println("std = {}, stored = {}", "ok", 12);
    eprintln("error code = {}", 1);
    return 0;
}
```

内置 `display` 实现覆盖 `bool`、整数、浮点、`char`、`str` 和标准库 `string`。

## 文件 IO

`std.fs` 提供同步文件 IO。`file::open` 返回 `expected<file, file_error>` 风格结果，常用 `match` 处理成功和失败分支。

```cp
main() -> i32
{
    let path: str = "/tmp/cp-example-fs.txt";

    match file::open(path, open_options{}.write().create().truncate()) {
        .value(out) => {
            out.write_str("cp");
            out.close();
        },
        .unexpected(error) => {
            return 1;
        },
    };

    return 0;
}
```

相关参考：[标准库 fs](std_fs.md)。
