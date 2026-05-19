# 模块

模块用于组织源文件和控制文件之间的可见性。不引入 namespace 概念，`import` 会把目标模块导出的名字直接引入当前文件作用域。

## 模块声明

语法总览：

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
export import std.option;
```

规则：

- 导入语句位于模块声明之后、顶层声明之前。
- `import` 只导入目标模块导出的名字。
- `export import` 导入目标模块导出的名字，并把这些名字作为当前模块的导出名字继续导出。
- 使用导入名字时直接写名字。

```cp
import math;

main()
{
    let x = add(1, 2);
}
```

`export import` 只能出现在具名模块中：

```cp
export module std.iter;

export import std.option;
```

上例表示：

- 当前模块 `std.iter` 可以直接使用 `std.option` 导出的名字。
- 导入 `std.iter` 的其它模块也能看见 `std.option` 经由 `std.iter` 重导出的名字。
- `export import` 不导出目标模块的非导出声明。

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

未标记 `export` 的顶层声明只在当前模块内部可见。`concept` 也遵循同一导出规则；需要跨模块使用的公共 concept 应写作 `export concept`。匿名模块没有可导入的模块名，因此不能导出声明。

## 名字冲突

语义分析阶段应报告以下冲突：

- 同一模块中出现重复顶层名字。
- 导入符号与当前文件可见的顶层名字冲突。
- 多个导入模块导出同名符号。
- 重导出符号与当前模块已有导出名字冲突。

不使用限定名来消解冲突。
