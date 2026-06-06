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
- 模块声明必须位于文件开头；当前 parser 只在 translation unit 的第一个 token 处识别 `export module`。源码注释不产生 token，因此可以出现在文件最前面；空语句 `;` 不是注释，写在 `export module` 前会让该文件被当作匿名模块解析，后续 `export module` 只会按顶层错误恢复处理。
- 模块声明写作 `export module name;`。
- `module name;` 不是模块声明语法。模块声明也不能出现在 `import`、空语句或任何顶层声明之后；这些位置的 `module` / `export module` 会按普通顶层语法错误恢复。
- 没有模块声明的文件属于匿名模块。

匿名模块规则：

- 每个无模块声明文件都有独立匿名模块。
- 匿名模块不会按空名字合并。
- 不同匿名模块之间的顶层名字表彼此独立；两个无模块声明文件可以各自声明同名顶层函数或类型，语义层不会把它们当成同一模块内的重复声明。
- 匿名模块也不会自动看见其它匿名模块的未导出名字。把多个无模块声明文件一起传给编译器时，`main.cp` 不能直接调用 `helper.cp` 中的 `helper()`；需要跨文件共享名字时，应使用具名模块和 `export` / `import`。
- 匿名模块不能被 `import` 引用。
- 匿名模块中的顶层声明不能使用 `export`；语法支持的导出声明会进入语义阶段并报
  `export_requires_module`，例如 `export import`、`export struct`、`export enum`、`export type`、
  `export operator`、导出普通函数、`export extern "C"`、`export variant` 和 `export concept`。
  语法本身不支持的形状仍由 parser 诊断，例如 `export impl` 或 `export ;` 不会被当成可导出的声明。

## 模块名

模块名由标识符组成，可以用 `.` 分隔：

```text
ModuleName -> identifier ( . identifier )*
```

`.` 在模块声明和导入语句中用于分隔模块名组件。表达式中的字段访问和成员调用由 [struct.md](struct.md) 单独定义，不属于模块名语法。

每个模块名组件都必须是 lexer 产出的普通 `identifier` token。词法关键字不能作为组件，因此 `export module import;`、`export module std.module;` 或 `import for.loop;` 都不是合法模块名。`variant`、`type`、`extern` 这类当前按上下文识别的词仍是普通 identifier，可以作为模块名组件，但不建议公共模块用这些容易混淆的名字。组件之间只能有一个 `.` 分隔，不能写空组件、前导点或尾随点；`export module .math;`、`export module math.;`、`import math..core;` 都会在 parser 阶段按模块名语法错误处理。

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

同模块共享的是本模块自己的顶层声明和 extension 集，不是每个文件的 `import` 列表。当前语义层会给每个
translation unit 单独构建可见表：先放入同模块本地声明，再按这个文件源码里的 `import` / `export import` 顺序加入外部导出实体。因此同一个具名模块里的两个文件都能直接使用彼此的本地声明，但 `math_part_a.cp` 写了 `import std;` 不会让
`math_part_b.cp` 自动看见 `std` 导出的名字；需要在 `math_part_b.cp` 也写对应 `import`，或由当前模块显式
`export import` 后再让其它模块导入当前模块。

### 语义收集顺序

语义分析会先给每个 translation unit 分配模块 key：具名模块使用源码模块名，匿名模块使用内部唯一 key。之后再分阶段收集声明。当前实现的顺序是：

1. 按编译输入中的 translation unit 顺序处理每个文件；在单个文件内先收集顶层 `concept`、`struct` 和 `enum` 名字，再收集顶层 `type` / opaque alias，最后收集顶层 `variant` 名字。
2. 收集所有 concept 的父约束、关联类型和函数要求。
3. 收集所有 `struct` 字段、`enum` case 和 `variant` case。
4. 收集顶层函数、顶层 operator、固有 `impl` 和 concept `impl` 中的函数/operator/关联类型。
5. 校验所有 `requires` 子句，并解析 import / re-export 可见表。

因此，同一个具名模块内的声明不要求按源码顺序排列：

- 字段类型、case payload、函数签名、`requires` 和函数体都可以引用同模块中后面声明的导出或非导出类型。
- 函数体可以调用同模块中后面声明的函数；函数是否导出不影响同模块内部可见性。
- concept body 可以引用同模块中后面声明的 concept，因为所有 concept 名字会先进入模块 concept 表。
- 从导入模块来的导出类型和导出 concept 也可以用于当前文件的类型位置；解析时会沿 `export import` 递归查找公共类型/concept。
- 导入的函数、operator、extension method 和 extension operator 在 import 解析后进入表达式检查的可见表；它们仍只包含目标模块的导出实体。

顶层 `type` 和 opaque alias 是当前实现的重要例外：alias 右侧比字段、函数签名或 `variant` case payload 更早解析。因此 alias 右侧只应该依赖已经进入当前可见类型表的名字：

- 可以引用内建类型、当前文件或同模块更早输入文件中已经收集到的 `struct` / `enum`、以及当前 alias 之前已经收集到的普通类型别名或 opaque alias。
- 可以引用已经进入目标模块导出表的导入类型；但 import / re-export 可见表还没有构建完，跨模块 alias 解析会受编译输入顺序和导出表当前状态影响。
- 不能引用同一文件或同一模块稍后才收集的 `variant`，即使 `variant` 在源码文本中写在 alias 前面也不行，因为当前阶段总是先收集 alias、再收集 variant。
- 不能引用同一文件中后面声明的另一个 alias，也不能可靠引用编译输入中排在当前文件之后的同模块类型。

```cp
type late_alias = i32;
type value = late_alias; // 合法：late_alias 已经在当前文件中先收集

type bad_later_alias = later_alias; // 错误：later_alias 还没有收集
type later_alias = i32;

type bad_variant = option; // 错误：variant 名字在 alias 之后才进入类型表
variant option {
    none;
}
```

需要给 `variant` 或跨文件后置类型起别名时，当前应把 alias 放到一个在编译输入顺序上能看见目标类型的文件中，或直接在字段、函数签名、`requires`、函数体等较晚解析的位置使用目标类型。公共代码不要依赖“稍后会收集到这个类型名”的直觉来书写 alias。

多文件模块的可见性也不会扩大 opaque alias 的底层转换权限。`type handle = opaque u8*;` 可以让同模块其它文件看见 `handle` 名义类型，但当前 `handle <-> u8*` 的显式 `as` 只允许出现在声明该 alias 的同一个 translation unit / 源文件内；同模块其它文件应调用声明源文件导出的封装函数或方法。

这个顺序不是运行时初始化顺序，也不表示模块之间有隐式循环加载。语义分析只处理当前编译输入中已经解析出的文件；某个导入目标没有进入输入时，仍会在 import 处报告 `unknown_module`。

## 导入

```cp
import math;
import math.core;
export import std.core.option;
```

规则：

- 导入语句必须紧跟在模块声明之后、顶层声明之前。当前 parser 不会跳过空语句后继续识别 import list；如果在模块声明和 `import` 之间写 `;`，后面的 `import` 会被当作普通顶层 token 并进入错误恢复。
- `import` 只导入目标模块导出的公共实体。
- `export import` 导入目标模块导出的公共实体，并把这些实体作为当前模块的导出实体继续导出。
- 使用导入名字时直接写名字。
- 当前 `import` 语法只接受完整模块名，不支持选择性导入、别名导入或模块限定访问。`import math as m;`、`from math import add;`、`import math.{ add };`、`math::add(1, 2)` 和 `math.add(1, 2)` 都不是当前可用写法。

```cp
import math;

main()
{
    let x = add(1, 2);
}
```

### 编译输入与导入解析

模块可见性由语义分析检查，但语义分析只检查本次编译输入中的源文件集合。也就是说，`analyze_semantics` 看到的是已经解析好的 translation unit；如果某个 `import math;` 的目标模块没有进入这批输入，就会报告 `unknown_module`。

只要目标具名模块已经进入本次语义输入，`import` 本身就是已知模块，即使该模块没有任何导出实体。这样的导入不会引入名字，但也不会因为导出表为空而报 `unknown_module`。

命令行编译器和 CLion native helper 在进入语义分析前会做 import closure 解析，把入口文件直接或间接导入的 `.cp` 文件加入编译输入。解析规则是工程入口能力，不是源语言里的 namespace 或运行时机制；源语言层仍只关心“目标 `export module name;` 是否进入了本次编译输入”。

当前工具链按常见布局查找模块文件，例如：

- `import math.core;` 可以对应 `math/core.cp`、`math/core/math.cp`、`math/core/core.cp` 等项目布局。
- 标准库根目录下的 `std.cp` 是 `import std;` 的聚合入口；`std.` 前缀子模块也可按标准库根下的层级文件查找，例如 `std.io` 对应 `io.cp` 或相应子目录入口。
- 导入者所在目录和配置的 `import_roots` 都会参与搜索；同目录候选通常优先于外部 import root。
- `search_roots` 可在解析前扫描并索引已有 `.cp` 文件，使已经声明目标模块名的源码参与 import closure。

需要注意的边界：

- resolver 只跟随文件头 import list。写在空语句、顶层声明或其它 token 之后的 `import` 不会用于自动补全源文件输入。
- 文件路径只是查找候选；真正匹配仍以源码中的 `export module name;` 为准。路径叫 `math/core.cp` 但声明了别的模块时，`import math.core;` 仍可能在语义层表现为 `unknown_module`。
- 多文件模块要求每个参与文件进入当前编译输入或由 import closure 找到；只声明同一个模块名但没有进入本次输入的源文件，不会被语义分析自动看见。
- 可以关闭标准库 import closure 跟随；这种模式下源码里的 `import std.io;` 仍存在，但工具链不会自动把标准库源码加入输入。调用方若没有用其它方式提供 `std.io` 及其依赖源文件，语义层仍会报告未知模块或缺失名字。

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

- 顶层函数，包括 `extern "C"` 函数声明和定义。
- 顶层 `operator`。
- 导出的 `struct`、`enum`、opaque alias、普通 `type` 类型别名和 `variant`。
- 导出的 `concept`。
- 模块导出的 extension method。
- 模块导出的 extension operator。

同一模块内的本地顶层函数、类型和 concept 使用同一个声明名字空间：不能同时定义同名函数、类型或 concept。导入阶段的当前实现更接近“按类别可见表 + 冲突检查”，不是完全对称的单一名字空间：

- 构建某个文件的可见表时，编译器会先放入它所属模块的所有本地函数、operator、类型、concept 和 extension，再按该文件的 `import` / `export import` 源码顺序处理导入。因此本模块声明会先占住可见表，即使 import 语法写在顶层声明之前。
- 导入函数只检查当前已可见函数；它不会因为当前已可见类型或 concept 同名而报 `import_conflict`。
- 导入类型会检查当前已可见函数和类型；同名函数或不同类型符号会报 `import_conflict`。
- 导入 concept 会检查当前已可见函数、类型和 concept；同名不同符号会报 `import_conflict`。
- 导入语句按源码顺序处理。因此 `import types; import functions;` 可以让导入函数和已可见类型同名并同时可用，而 `import functions; import types;` 会在后导入类型时报告冲突。
- 同名函数和类型同时可见时，表达式调用按函数表查找，类型位置和类型初始化器按类型表查找；不支持用模块限定名消歧。

导入顺序示例：

```cp
// types.cp exports: struct item { value: i32; }
// functions.cp exports: item() -> i32

import types;
import functions;

main() -> i32
{
    let value = item{ 1 };
    return item() + value.value; // 当前可通过：函数表和类型表分别可见
}
```

反过来写时，后导入的类型会与已经可见的函数冲突：

```cp
import functions;
import types; // 错误：导入 item 类型时报 import_conflict
```

这个导入顺序边界不是鼓励 API 设计依赖导入顺序；公共模块应避免导出同名函数和类型。

顶层 `operator` 不使用文本名字冲突规则，而是按 operator kind 保存 overload 集，导入后继续参加普通 operator 候选选择。

当前实现把 `export import` 分成两个相关步骤：

1. 先在所有具名模块之间把 re-export 导出表传播到 fixed point。这个步骤只更新模块的公共导出表，让 `export import` 链可以传递导出；同一个原始符号会去重。
2. 再按每个文件的源码 `import` / `export import` 顺序构建该文件的可见表，并在这一步报告导入冲突。`export import` 在这一步也按普通导入规则把目标模块的导出名字放进当前文件可见表。

因此：

- fixed-point 传播本身不做“两个导入源是否同名冲突”的完整诊断；冲突诊断依赖后续为具体 translation unit 构建可见表时的源码导入顺序。
- 如果 `export import` 引入的函数、类型或 concept 与当前文件已经可见的不同符号冲突，会在这条导入语句处报 `import_conflict`，而不是等到其它模块导入当前模块时报错。
- 如果两个 `export import` 经由不同路径引入同一个原始符号，当前实现把它视为同一个符号并去重。
- 如果两个 `export import` 引入同名但不同的函数、类型或 concept，第一条导入成功，后一条按类别冲突规则报错。
- operator、extension method 和 extension operator 不按文本名字做 `import_conflict`；它们按 operator kind 或目标类型保存 overload 集。`export import` 传播到模块公共导出表时会对同一符号去重，extension method / extension operator 进入单个文件可见表时也会去重；但顶层 operator 的单文件可见表当前是直接追加候选，重复直接导入同一个 operator，或同时直接导入与经 re-export 间接导入同一个 operator，可能让同一 operator 符号在表达式重载解析中出现两次并产生歧义。公共模块应避免依赖重复导入来“合并”operator 候选。
- 当前模块没有模块限定访问语法，冲突后不能写 `base.visible`、`base::visible` 或 `wrap::visible` 来手工消歧。

循环 `export import` 也是按同一个 fixed-point 传播处理，不表示运行时循环加载，也不会无限递归。只要循环中的模块都进入当前语义输入，公共导出表会收敛；例如 `left` 重导出 `right`、`right` 又重导出 `left` 时，导入 `left` 的文件可以看见两边导出的类型。循环只传播公共导出实体，不会让私有声明外泄；如果循环中不同模块导出同名但不同的函数、类型或 concept，仍会在具体文件构建可见表时按导入顺序报告冲突。

同一个符号经由多个 `export import` 路径进入当前模块时会被去重。例如 `all` 同时 `import base` 和 `export import wrap`，而 `wrap` 又重导出 `base`，`base::marker` 只算同一个 concept 符号，不报冲突。不同模块导出同名但不同符号时才会报 `import_conflict`。

固有 `impl` 和 concept `impl` 本身没有 `export` 修饰位。对用户定义的 `struct` 和 `variant`，成员函数、关联函数和 operator 绑定在目标类型上；对 opaque alias，当前只有成员函数、关联函数和关联类型绑定到目标类型，`impl operator` 不注册为可用 operator。只要目标类型和对应 impl 都参与本次语义输入，方法查找就从目标类型取候选。

对 builtin、数组或依赖类型 pattern 这类没有用户名义类型承载的扩展方法/operator，编译器把它们放入模块 extension 表。具名模块中的这些 extension 当前会自动进入该模块的 extension 导出表，不需要也不能给单个 impl 成员写 `export`。匿名模块中的 extension 只进入该匿名模块自己的可见表，不可被其它模块 import。

concept 默认函数物化出来的 extension method 也遵守同一套 extension 表规则：如果物化发生在具名模块上下文中，它可以经由模块导出表被 `import` / `export import` 带到其它文件；如果目标类型本身已有同名成员导致默认函数不物化，则不会额外生成一个可导出的默认 extension。

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

导入 `wrap` 的文件可以直接使用 `value`、`marker`、`left + right` 和 `number.bump()` 这类 `i32` extension method。`impl marker for value` 本身不是可写 `export impl` 的声明；只要参与同一次编译输入，它就可以用于 concept 约束证明。

匿名模块不能使用 `export import`，因为它没有可被其它模块导入的模块名。当前语义检查在这种错误后仍会按普通 `import` 处理目标模块，把目标模块的导出名字放入这个匿名文件的可见表，用于继续检查后续表达式；但它不会生成任何可被其它文件导入的 re-export 表。合法程序不能依赖这种错误恢复，应在匿名文件中写普通 `import`，或先声明具名模块再写 `export import`。

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

未标记 `export` 的顶层声明只在当前模块内部可见。导入模块后，在表达式位置引用一个直接导入模块中存在但未导出的函数时，语义分析会报告未导出名字，而不是把它当成完全未知名字。这个特殊诊断当前只覆盖直接 `import` 的函数查找；未导出的类型、concept，以及只经由 `export import` 间接路径存在的私有名字，不进入公共导出表，使用处通常表现为未知类型、未知 concept 或未知名字。匿名模块没有可导入的模块名，因此不能导出声明。

支持 `export` 的顶层声明包括：

- 普通函数。
- `extern "C"` 函数声明和定义；`export` 只影响 KCP 模块可见性，C ABI 字符串、参数类型、返回类型和是否允许分号声明仍按 [extern_c.md](extern_c.md) 检查。
- `operator`。
- `struct`。
- `enum`。
- `type name = ...` 和 `type name = opaque ...`。
- `variant`。
- `concept`。
- `import`，形成 `export import`。

`export` 只出现在顶层声明起始位置，不能修饰字段、构造函数、析构函数、成员函数、关联函数、impl operator、关联类型、variant case、concept item 或 concept impl item。也不能写 `export impl` 或 `export impl Concept for Type`。这些位置的公共性由它们所属的导出类型、具名模块 extension 表、concept/impl 参与当前编译输入等规则决定，而不是由局部 `export` 修饰符决定。

不支持 `export impl`。当前 parser 在顶层看到 `export impl` 时直接按“impl blocks cannot be exported”这类 unexpected-token 诊断恢复；它不是具名模块中可通过语义阶段打开的导出形式。固有 `impl` 和 concept `impl` 的可见性由目标类型、目标 concept、所在模块是否参与编译输入以及导出的 extension method/operator 表决定。不能在 impl 头部或单个 impl 成员前写 `export` 来选择性导出某个成员；需要选择性公共 API 时，应把公共成员放在导出的目标类型或具名模块的 extension 集中，把私有 helper 留在未导出的顶层函数或未导入的模块内部。

匿名模块中写语法支持的导出声明都会报 `export_requires_module`，例如 `export import`、
`export struct`、`export enum`、`export type`、`export operator`、导出普通函数、
`export extern "C"`、`export variant` 或 `export concept`。这条规则只描述已经被 parser 识别为
导出声明的形状；`export impl`、`export ;` 或 `export` 后接其它非声明 token 会在 parser 阶段按
`unexpected_token` / `expected_token` 恢复，不是可以由语义阶段打开的导出能力。

## 入口函数与外部符号

模块系统首先定义源语言可见性；外部符号规则不提供模块限定访问语法。当前边界如下：

- `main` 在语义层仍是普通顶层自由函数名，参与同一套重复名字、导入和导出检查。
- 只有匿名模块中的顶层自由函数 `main` 会作为可执行程序入口使用。
- 具名模块中的 `main` 不会成为 C/OS 入口名。即使写 `export main`，导出的也是 KCP 模块函数，不是裸符号 `main`。
- 匿名模块中的其它普通顶层函数使用源码函数名；未导出函数是内部 linkage。匿名模块本身不能 `export`，因此这类名字只适合当前编译单元内部使用。
- 具名模块中的普通顶层函数不会按源码名直接暴露成稳定外部 ABI；`export` 只决定它是否进入模块导出表和是否能被其它 KCP 模块导入，不提供 `module::name` 这样的源码限定访问。
- `extern "C"` 函数声明和定义使用源码函数名作为外部符号名。它是否能被其它 KCP 模块导入，仍由 `export extern "C"` 决定。
- 成员函数、关联函数、构造函数、析构函数、impl operator、lambda 和泛型实例都是实现细节符号，当前使用内部 linkage。泛型函数定义本身不直接生成可链接实体，只有具体单态实例在使用点生成内部函数。

需要稳定外部符号时，应显式设计 `extern "C"` 边界，规则见 [extern_c.md](extern_c.md)。不要把普通 KCP 模块函数的当前符号拼写当作跨版本 ABI。

## impl 目标和模块边界

固有 `impl` 的目标类型当前可以是：

- `struct`。
- `variant`。
- opaque alias。
- 编译器内建标量类型，例如 `i32`。
- 固定数组类型，例如 `[T; N]`。

其它类型不能作为固有 `impl` 目标，例如 `storage T` / `storage [T; N]`、指针、引用、函数类型、函数指针、元组和未识别类型都会报错。`storage` 仍然可以作为字段、局部变量、参数、返回值和泛型实参使用；这里限制的是“不能给 storage 类型本身挂方法或 operator”。

`struct` impl 可以声明构造函数、析构函数、成员函数、关联函数、关联类型和 operator。其它目标有更窄的边界：

- `variant`、opaque alias、内建类型和数组类型不能声明构造函数或析构函数；variant 只能通过 case 构造器构造，opaque alias 的构造边界见 [opaque_alias.md](opaque_alias.md)。
- 内建类型和数组类型的固有 impl 函数必须使用 `self` receiver，因此只能作为 extension method；不能声明无 `self` 的 associated function。固有 `impl` 的关联类型收集不受这条函数 receiver 限制影响，当前内建类型和数组类型也可以声明关联类型别名。
- opaque alias 可以声明成员函数、关联函数和关联类型；但 opaque alias 的 `impl operator` 当前不会注册为可用 operator，需要时使用顶层 operator。
- 数组类型可以声明 extension method，但数组 `impl operator` 当前不会注册为可用 operator。
- `struct`、`variant` 和内建类型的 `impl operator` 会注册到对应类型或模块 extension operator 表；operator 详细查找规则见 [operator.md](operator.md)。

concept `impl` 的目标类型当前可以是 `struct`、`variant`、数组类型或内建类型。opaque alias 当前不作为 concept `impl` 目标；需要给 opaque 包装能力时，第一版应通过顶层函数/operator 或显式暴露的底层类型边界设计，而不是假定它自动继承底层 concept 实现。

concept `impl` 函数也受目标类型限制：`struct` 和 `variant` 目标可以实现带 `self` 的成员要求，也可以实现无 `self` 的关联函数要求；数组类型和内建类型目标的 concept impl 函数必须使用 `self` receiver。也就是说，`impl measurable for [i32; 2] { measure(self const&) -> i32 { ... } }` 是当前支持能力，`impl maker for [i32; 2] { make() -> i32 { ... } }` 会报错。concept 默认函数也不绕过这条规则；如果默认函数没有 `self`，在数组或内建类型 impl 上物化时同样会被拒绝。

模块边界上，`struct` 和 `variant` 的成员函数、关联函数、构造函数、析构函数、operator 和关联类型别名直接挂在目标类型记录上；opaque alias 也把成员函数、关联函数和关联类型别名挂在目标类型记录上，但它的 `impl operator` 当前不会注册为可用 operator。只要目标类型通过导入变为可见，且定义该类型的模块参与本次语义输入，这些类型成员就随目标类型参与查找，不存在给单个成员再写 `export` 的语法。内建类型和数组类型没有可导出的目标类型声明，因此它们的 extension method 通过具名模块的 extension 导出表跨模块可见；内建类型的 extension operator 也走这张表，数组类型当前没有可用的 extension operator 注册路径。匿名模块里的这类 extension 只对当前匿名模块可见。

内建类型和数组类型上的关联类型别名不是 extension method/operator，不进入模块 extension 导出表。当前实现按 owner type 在全局关联类型表中查找它们：只要声明该 alias 的源文件参与本次语义输入，内建类型上的 `i32::item` 这类关联类型就可以被解析；`import` / `export import` 不提供选择性开关。数组类型也可以拥有关联类型别名，但类型语法只允许命名类型路径接 `::name`，不能直接写 `[T; N]::item`；需要从外部引用数组目标上的关联类型时，应先给数组类型取别名，例如 `type pair = [i32; 2]; impl pair { type item = i32; }` 后写 `pair::item`。这个行为是当前实现边界，公共 API 仍应把这类别名放在清晰的标准库或编译器约定模块里，避免依赖“任意参与输入文件都能给内建类型或数组别名增加关联类型”的全局效果。

`impl` 和 concept `impl` 可以显式写泛型参数，也可以在省略泛型参数列表时从目标类型模式收集类型参数。例如 `impl box<T> { ... }` 会把 `T` 作为 impl 级泛型参数；如果显式写了 `impl<T> box<T> { ... }`，则以显式列表为准。类型参数包不能用于 `impl` 或 concept `impl` 的泛型参数列表。

## 名字冲突

语义分析阶段应报告以下冲突：

- 同一模块中出现重复顶层函数、类型或 concept 名字。
- 导入函数与当前可见函数冲突。
- 导入类型与当前可见函数或类型冲突。
- 导入 concept 与当前可见函数、类型或 concept 冲突。
- 多个导入模块导出同名实体且不是同一个 re-export 来源；函数导入只和函数表中的同名符号冲突，类型和 concept 按上面的类别规则检查。
- 重导出实体先按普通导入检查当前可见表；检查通过后才加入当前模块导出表。

不使用限定名来消解冲突。
