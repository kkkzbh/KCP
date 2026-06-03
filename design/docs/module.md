# 模块

模块用于组织源文件和控制文件之间的可见性。不引入 namespace 概念，`import` 会把目标模块导出的名字直接引入当前文件作用域。

## 模块声明

### 语法总览

```text
ModuleHeader -> export module ModuleName ;
ImportDecl   -> export? import ModuleName ;
ModuleName   -> identifier ( . identifier )*
```

```cp
export module math;
export module math.core;
```

规则：

- 一个文件最多声明一个模块。
- 模块声明必须位于文件开头。
- 模块声明写作 `export module name;`。
- 没有模块声明的文件属于匿名模块。

匿名模块规则：

- 每个无模块声明文件都有独立匿名模块。
- 匿名模块不会按空名字合并。
- 匿名模块不能被 `import` 引用。
- 匿名模块中的顶层声明不能使用 `export`。

## 模块名

模块名由标识符组成，可以用 `.` 分隔：

```text
ModuleName -> identifier ( . identifier )*
```

`.` 在模块声明和导入语句中用于分隔模块名组件。表达式中的字段访问和成员调用由 [struct.md](struct.md) 单独定义，不属于模块名语法。

## 多文件模块

多个文件可以声明同一个具名模块。这些文件会合并到同一个模块作用域中。

同一具名模块内的所有顶层声明彼此可见，即使没有使用 `export` 修饰：

```cp
// math_part_a.cp
export module math;

helper(x: i32) -> i32
{
    return x + 1;
}
```

```cp
// math_part_b.cp
export module math;

export add_one(x: i32) -> i32
{
    return helper(x);
}
```

上例中 `helper` 没有导出，但 `math_part_b.cp` 与 `math_part_a.cp` 属于同一个具名模块 `math`，因此可以直接调用 `helper`。

## 导入

```cp
import math;
import math.core;
export import std.core.option;
```

规则：

- 导入语句位于模块声明之后、顶层声明之前。
- `import` 只导入目标模块导出的公共实体。
- `export import` 导入目标模块导出的公共实体，并把这些实体作为当前模块的导出实体继续导出。
- 使用导入名字时直接写名字。

```cp
import math;

main()
{
    let x = add(1, 2);
}
```

### 编译输入与导入解析

模块可见性由语义分析检查，但语义分析只检查本次编译输入中的源文件集合。也就是说，`analyze_semantics` 看到的是已经解析好的 translation unit；如果某个 `import math;` 的目标模块没有进入这批输入，就会报告 `unknown_module`。

命令行编译器和 CLion native helper 在进入语义分析前会做 import closure 解析，把入口文件直接或间接导入的 `.cp` 文件加入编译输入。解析规则是工程入口能力，不是源语言里的 namespace 或运行时机制：

- 先考虑导入者所在目录，再按 `import_roots` 的顺序查找。
- 已经显式提供或搜索发现、且声明了目标 `export module name;` 的源文件会被复用。
- 普通层级路径把 `math.core` 映射到 `math/core.cp`。
- 聚合布局也支持，例如 `parser.lr` 可以解析到 `parser/lr/parser.cp`，`math.core` 也可以按聚合布局解析到 `math/core/core.cp`。
- 标准库布局对 `std.` 前缀有专门处理；例如 `std.io` 可以在标准库根下解析到 `io.cp`。
- 搜索发现会跳过 `.git`、`.gradle`、`.idea`、`build`、`build-clion-plugin-native`、`cmake-build-*`、`node_modules` 和 `out` 等目录。
- 可以关闭标准库 import closure 跟随；这种模式下 `import std.io;` 不会自动把标准库源码加入输入。

源文件路径解析成功仍不等于模块匹配成功。被加入的文件必须声明相同的 `export module name;`，否则语义层看不到目标模块。多文件模块同样要求每个参与文件进入当前编译输入或由 import closure 找到。

`export import` 只能出现在具名模块中：

```cp
export module std;

export import std.core.option;
```

上例表示：

- 当前模块 `std` 可以直接使用 `std.core.option` 导出的名字。
- 导入 `std` 的其它模块也能看见 `std.core.option` 经由 `std` 重导出的名字。
- `export import` 不导出目标模块的非导出声明。

重导出只转发目标模块的公共导出表，不转发目标模块的私有顶层声明：

```cp
export module base;

export visible() -> i32
{
    return hidden();
}

hidden() -> i32
{
    return 42;
}
```

```cp
export module wrap;

export import base;
```

```cp
import wrap;

main() -> i32
{
    let a = visible(); // 合法
    let b = hidden();  // 错误：hidden 没有经由 wrap 重导出
    return a + b;
}
```

如果当前文件直接 `import base;` 后引用 `hidden()`，语义分析能识别它来自被导入模块但未导出，并报告未导出名字。只通过 `wrap` 间接导入时，`hidden` 没有进入 `wrap` 的导出表，对导入者来说就是不可见名字。

当前实现中，模块导入/重导出的公共实体包括：

- 顶层函数。
- 顶层 `operator`。
- 导出的 `struct`、`enum`、opaque alias、普通 `type` 类型别名和 `variant`。
- 导出的 `concept`。
- 模块导出的 extension method。
- 模块导出的 extension operator。

函数、类型和 concept 使用同一个顶层可见名字空间：同一模块内不能同时定义同名函数、类型或 concept，导入时也不能让函数名、类型名或 concept 名互相覆盖。顶层 `operator` 不使用文本名字冲突规则，而是按 operator kind 保存 overload 集，导入后继续参加普通 operator 候选选择。

同一个符号经由多个 `export import` 路径进入当前模块时会被去重。例如 `all` 同时 `import base` 和 `export import wrap`，而 `wrap` 又重导出 `base`，`base::marker` 只算同一个 concept 符号，不报冲突。不同模块导出同名但不同符号时才会报 `import_conflict`。

固有 `impl` 和 concept `impl` 本身没有 `export` 修饰位。对用户定义的 `struct`、`variant` 和 opaque alias，成员函数、关联函数和 operator 绑定在目标类型上；只要目标类型和对应 impl 都参与本次语义输入，方法查找就从目标类型取候选。对 builtin、数组或依赖类型 pattern 这类没有用户名义类型承载的扩展方法/operator，编译器把它们放入模块 extension 表；具名模块中的这些 extension 会进入模块导出表，并可被普通 `import` / `export import` 带到其它文件。

因此下面的 re-export 会同时转发类型、concept、operator 和扩展方法：

```cp
export module base;

export concept marker {
}

export struct value {
    item: i32;
}

impl marker for value {
}

impl i32 {
    bump(self&) -> void
    {
        self += 1;
    }
}

export operator +(left: value const&, right: value const&) -> value
{
    return value{ left.item + right.item };
}
```

```cp
export module wrap;

export import base;
```

导入 `wrap` 的文件可以直接使用 `value`、`marker`、`left + right` 和 `counter.bump()`。`impl marker for value` 本身不是可写 `export impl` 的声明；只要参与同一次编译输入，它作为全局 concept 实现事实被使用。

匿名模块不能使用 `export import`，因为它没有可被其它模块导入的模块名。

不支持：

- 选择性导入。
- 别名导入。
- `math::add` 这样的模块限定访问。

限定访问需要单独设计 namespace 或模块路径表达式规则，当前模块系统不提供限定访问。

## 导出

具名模块内使用 `export` 修饰顶层声明，使其可被其他文件导入：

```cp
export module math;

export add(x: i32, y: i32) -> i32
{
    return x + y;
}

export struct vec2 {
    x: f64;
    y: f64;
}

export concept measurable {
    size(self const&) -> u64;
}
```

未标记 `export` 的顶层声明只在当前模块内部可见。导入模块后引用一个存在但未导出的函数时，语义分析会报告未导出名字，而不是把它当成完全未知名字。匿名模块没有可导入的模块名，因此不能导出声明。

支持 `export` 的顶层声明包括：

- 普通函数。
- `operator`。
- `struct`。
- `enum`。
- `type name = ...` 和 `type name = opaque ...`。
- `variant`。
- `concept`。
- `import`，形成 `export import`。

不支持 `export impl`。固有 `impl` 和 concept `impl` 的可见性由目标类型、目标 concept、所在模块是否参与编译输入以及导出的 extension method/operator 表决定。不能在 impl 头部或单个 impl 成员前写 `export` 来选择性导出某个成员；需要选择性公共 API 时，应把公共成员放在导出的目标类型或具名模块的 extension 集中，把私有 helper 留在未导出的顶层函数或未导入的模块内部。

匿名模块中写任何导出形式都会报错，例如 `export import`、`export struct`、`export enum`、`export type`、`export operator` 或导出普通函数。

## 名字冲突

语义分析阶段应报告以下冲突：

- 同一模块中出现重复顶层函数、类型或 concept 名字。
- 导入函数与当前可见函数冲突。
- 导入类型与当前可见函数或类型冲突。
- 导入 concept 与当前可见函数、类型或 concept 冲突。
- 多个导入模块导出同名实体且不是同一个 re-export 来源。
- 重导出实体与当前模块已有导出名字冲突。

不使用限定名来消解冲突。
