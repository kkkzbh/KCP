# KCP

[English](README.md) | 简体中文

KCP 是 KCP 编程语言的编译器项目。仓库包含语言设计、前端、语义分析、IR 与 LLVM 降级、运行时支持、标准库、示例、文档和 CLion 集成。

![KCP in CLion](docs/assets/readme/clion-demo.png)

## 概览

编译器按原生代码工具链的阶段组织：

- `source`、`preprocessor` 和 `lexer` 保留源码位置，并生成适合诊断的 token 流。
- `parser` 从模块、声明、语句、表达式和类型构建 KCP 语法树。
- `semantic` 解析名称、检查类型、实例化泛型、验证 concept，并把语言规则降级到带类型的编译器状态。
- `codegen` 将检查后的程序转换为 IR 和 LLVM。
- `runtime` 定义生成程序使用的 ABI 边界。
- `std` 提供第一批用 KCP 编写的标准库模块。

项目还包含文档站点、可运行示例、回归测试和 IntelliJ/CLion 语言插件。

## 安装

KCP 会从带 tag 的 release 发布 Linux 包和 CLion 语言插件。当前 release 页面：

<https://github.com/kkkzbh/KCP/releases/tag/0.1.0>

编译器包会安装：

- `/usr/bin/kcp`
- `/usr/lib/kcp/libcp_runtime.a`
- `/usr/lib/kcp/std`

安装后的编译器会自动找到打包的运行时和标准库。生成二进制时仍然使用 `clang` 生成 object 和链接；如果包管理器没有自动安装，请手动安装 `clang`。

### Fedora

使用 Fedora 仓库：

```bash
sudo curl -L -o /etc/yum.repos.d/kcp.repo \
  https://raw.githubusercontent.com/kkkzbh/KCP/master/docs/install/kcp-fedora.repo
sudo dnf install kcp
```

也可以从 release 直接安装 RPM：

```bash
sudo dnf install ./kcp-0.1.0-1.fc44.x86_64.rpm
```

### Debian 和 Ubuntu

从 release 页面下载 `.deb` 后安装：

```bash
sudo apt install ./kcp_0.1.0-1_amd64.deb
```

### Arch Linux

从 release 页面下载 pacman 包后安装：

```bash
sudo pacman -U ./kcp-0.1.0-1-x86_64.pkg.tar.zst
```

### CLion 插件

插件已发布到 JetBrains Marketplace：

<https://plugins.jetbrains.com/plugin/32138-kcp-language-support>

它提供 `.cp` 文件类型、语法高亮、语义诊断、导航、重命名、类型提示，以及由内置 KCP 原生工具驱动的运行配置。

release 中也会附带 `kcp-clion-plugin-<version>.zip`。

## 快速开始

创建 `hello.cp`：

```cp
import std;

main() -> i32
{
    println("hello from {}", "KCP");
    return 0;
}
```

构建并运行：

```bash
kcp hello.cp -o hello
./hello
```

常用编译器选项：

```bash
kcp --help
kcp hello.cp --emit ll -o hello.ll
kcp hello.cp --emit obj -o hello.o
kcp hello.cp --release -o hello
kcp hello.cp --verbose -o hello
```

多文件模块只需要传入入口文件，编译器会从当前目录、输入文件目录和已安装标准库中解析 import：

```bash
kcp main.cp -o app
```

如果使用解包后的自定义标准库或运行时，可以覆盖安装路径：

```bash
CP_STDLIB_ROOT_PATH=/path/to/std \
CP_RUNTIME_LIBRARY_PATH=/path/to/libcp_runtime.a \
kcp main.cp -o app
```

## 语言

KCP 是一门静态类型系统语言，提供显式模块、值导向聚合、泛型编程和可预测的底层互操作。

当前语言能力包括：

- 模块、导入、导出和名称可见性
- struct、impl block、构造函数、析构函数、成员函数和 UFCS
- 强整数枚举、variant、带 payload 的 case 和 `match`
- 引用、指针、数组、tuple、所有权、move 和 `like&`
- lambda、函数值、捕获和闭包检查
- 泛型函数、泛型类型、参数包和 `template for`
- concept、关联类型、默认实现和受约束 API
- 运算符重载、显式 cast、不透明 alias 和 `extern "C"`

示例：

```cp
import std.core.option;

variant event {
    quit;
    key(char);
    resize(i32, i32);
}

score(value: event) -> i32
{
    return match value {
        .resize(width, height) => width + height,
        .key(code) => 1,
        .quit => 0,
    };
}

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    return some.value_or(0) + none.value_or(10) + score(event::resize(5, 7));
}
```

更多示例见 `design/examples/`。

## 标准库

标准库以普通 KCP 模块实现，目前包括：

- `std.core`：`optional`、`expected`、迭代器协议
- `std.memory`：原始 buffer 和 span
- `std.collections`：`vector`、有序 `map`、有序 `set`
- `std.text`：`str` 和拥有所有权的 `string`
- `std.ranges`：source、惰性 adapter 和 terminal
- `std.meta`：类型查询和 callable concept
- `std.compare`：比较类别和 ordering 对象
- `std.algorithm`：排序算法
- `std.io`：格式化和输出
- `std.fs`：同步文件 IO

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `compiler/` | 命令行编译器入口 |
| `source/` | 源码文本、span 和位置工具 |
| `preprocessor/` | lexing 前的源码规范化 |
| `lexer/` | tokenization 和词法诊断 |
| `parser/` | parser、语法树和 parse 诊断 |
| `semantic/` | 语义分析、类型、泛型和语言规则 |
| `codegen/` | IR 和 LLVM 代码生成 |
| `runtime/` | 运行时 ABI 文档和支持 |
| `std/` | KCP 标准库源码 |
| `design/` | 语言文档和示例 |
| `clion-plugin/` | KCP 的 CLion 插件 |
| `test/` | 编译器、库、parser、lexer 和集成测试 |

## 从源码构建

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

只构建编译器：

```bash
cmake --build build --target kcp
```

从已配置的 CMake tree 构建 release 包：

```bash
scripts/package-linux.sh \
  --build-dir build \
  --version 0.1.0 \
  --out-dir dist/release
```

运行确定性的风格检查：

```bash
python3 .codex/skills/cp-code-style/scripts/check_cp_style.py
```

## 文档

公开语言文档：

<https://cp.kkkzbh.cn/>

文档站点源码位于 `design/`，可以本地预览：

```bash
cd design
npm run dev
```

常用入口：

- [语言指南](design/docs/index.md)
- [示例](design/docs/examples.md)
- [标准库说明](std/README.md)
- [运行时 ABI](runtime/abi.md)
