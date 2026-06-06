# Lambda 与函数值

本文档记录 KCP 的函数值、函数指针、lambda 和闭包设计。普通函数声明和函数类型见 [类型系统](type_system.md#函数返回类型)，运行时 `return`、返回类型推导、块表达式和正常完成规则见 [控制流](flow.md#函数返回和正常完成)。

## 函数类型

KCP 区分函数类型和函数指针类型：

```cp
f(i32, i32) -> i32
f*(i32, i32) -> i32
```

`f(...) -> R` 是函数类型，表示一个非空、可直接调用的函数实现。它可以由普通命名函数或非泛型无捕获 lambda 产生。

`f*(...) -> R` 是函数指针类型，表示运行时函数地址。它接近 C/C++ 的函数指针，主要用于 C ABI、底层表驱动和需要显式地址语义的场景。

函数类型参数可以只写类型，也可以写参数名：

```cp
f(i32, i32) -> i32
f(left: i32, right: i32) -> i32
```

参数名由 parser 识别并保存在 AST 中，用于诊断、文档和可读性。它不参与类型等价、不参与 ABI、不参与重载选择，也不会引入可在表达式中引用的名字。上面两个类型完全等价。

函数指针类型同理：

```cp
f*(i32) -> i32
f*(value: i32) -> i32
```

## 普通函数绑定

普通命名函数可以作为值绑定：

```cp
add(x: i32, y: i32) -> i32 {
    return x + y;
}

let op: f(i32, i32) -> i32 = add;
let result = op(1, 2);
```

这里 `add` 在值位置表示一个函数实现。它不是普通变量，也没有可修改状态；绑定到 `f(...) -> R` 后仍表示非空函数值。

普通命名函数也可以在上下文需要函数指针时转换为 `f*(...) -> R`：

```cp
let raw: f*(i32, i32) -> i32 = add;
```

`f(...) -> R` 和 `f*(...) -> R` 的语义边界：

- `f(...) -> R` 是首选回调类型，语义上非空，不暴露指针运算或地址整数转换。
- `f*(...) -> R` 是底层地址值，可以默认初始化为空函数指针。
- 调用空函数指针属于底层 unsafe 契约；编译器只做类型检查。
- 函数值语义上不是裸指针，不能做指针运算、空值检查或地址整数转换。

具体操作边界：

- `f(...) -> R` 不能由 `nullptr` 初始化，也不能写空的 `Type{}` 默认初始化；它只能来自可见的普通函数名或非泛型无捕获 lambda。
- `f*(...) -> R` 可以由同签名函数值、非泛型无捕获 lambda 或 `nullptr` 初始化，也可以通过函数指针类型别名写空的 `callback{}` 默认初始化。
- 函数指针比较走普通指针比较规则：同签名 `f*(...) -> R` 可以与同类型函数指针或 `nullptr` 做比较；`f(...) -> R` 函数值本身不作为可比较指针值使用。
- 函数指针不能反向隐式转换成 `f(...) -> R`。需要非空回调契约时，API 参数应写 `f(...) -> R`；只有需要表达可空地址或 C ABI 互操作时才写 `f*(...) -> R`。

## Lambda 语法

lambda 表达式写作：

```text
LambdaExpr -> f GenericParameterList? ( LambdaParamList? ) ReturnType? LambdaBody
LambdaBody -> Block
            | => Expr
```

`f` 是 lambda 表达式的上下文 marker，不是全局保留的普通函数名。parser 只有在 `f` 后面可选泛型参数列表、紧接参数列表，并且参数列表后直接出现 `->`、`{`，或出现 `=` 并随后按 lambda body 继续要求 `>` 组成源码拼写 `=>` 时，才把它识别成 lambda 表达式。其它形状仍按普通名字表达式或调用表达式处理，例如 `f(1)` 是调用名为 `f` 的值，而不是 lambda。

参数语法与普通函数参数一致：

```cp
let square = f(x: i32) -> i32 {
    x * x
};
```

lambda 参数不能带默认值。当前 parser 可以把 `f(value: i32 = 1) -> i32 { ... }` 读进 AST，但语义层会报告 `invalid_type_argument`，不会记录可供调用点补齐的默认实参。需要默认行为时，在外层函数或闭包体内显式处理。

返回类型可以省略，由 lambda body 推导：

```cp
let square = f(x: i32) {
    x * x
};
```

当上下文类型已知时，lambda 参数类型也可以省略：

```cp
let square: f(i32) -> i32 = f(x) {
    x * x
};
```

省略参数类型只允许在存在明确上下文函数类型时使用。没有上下文类型时，lambda 参数必须显式标注类型。

上下文参数推断只使用目标可调用类型的参数列表：

- 上下文必须是 `f(...) -> R` 或 `f*(...) -> R` 这类可调用类型。
- 上下文参数数量必须和 lambda 参数数量一致。
- 只填补省略的参数类型；lambda 的返回类型仍来自显式 `-> R` 或 body 返回值推导。
- 可以混合显式参数类型和省略参数类型；上下文只替换省略位置，已经显式标注的参数类型保持不变，后续再按普通 lambda 类型到目标函数类型的转换规则检查是否匹配。
- `f*(...) -> R` 上下文也可以提供省略参数类型，但这只解决参数类型推断，不会把任意 lambda 强制变成函数指针；有捕获 lambda 和泛型 lambda 后续仍按各自的函数值/闭包转换规则检查。
- 没有上下文、上下文不是可调用类型，或 arity 不一致时，省略参数类型报 `type_mismatch`；例如 `let f = f(x) => x;` 不能从右侧 lambda 自己反推出 `x` 的类型。

lambda 可以声明自己的泛型参数：

```cp
let id = f<T>(value: T) -> T {
    value
};

let count_types = f<T...>(values: T...) -> i32 {
    let total = 0;
    template for(let value : values...) {
        total = total + 1;
    }
    return total;
};
```

当前实现中，泛型 lambda 必须显式写返回类型；`f<T>(value: T) { value }` 在 parser 阶段就不构成合法 lambda 表达式。泛型 lambda 表达式本身是匿名可调用对象，即使没有捕获，也不会直接变成某个具体 `f(...) -> R` 函数类型。调用泛型 lambda 时，类型实参可以显式写出，也可以像泛型函数一样从普通调用实参推导：

```cp
let explicit = id<i32>(1); // 合法：显式 T = i32
let inferred = id(1);      // 合法：从普通实参推导 T = i32
```

泛型 lambda 的显式类型实参按它自己的源码泛型参数列表绑定，规则和泛型函数一致：默认泛型实参可以补齐尾部参数，类型参数包吸收剩余类型实参，显式实参与普通实参推导结果冲突时报错。没有显式类型实参时，编译器会从普通调用实参和闭包对象的具体签名推导类型实参；参数包、默认泛型实参、整数 const 泛型参数、`forward&` 绑定类别和约束检查沿用泛型函数调用规则。仍然不从返回类型、变量声明目标类型或其它上下文反推泛型实参。

泛型 lambda 不支持省略返回类型；当前不会为泛型 lambda 的每个具体调用实例做返回类型推导。需要返回 `T`、`unit` 或其它依赖类型时，都必须显式写 `-> R`。

泛型 lambda 总是按闭包对象处理。没有捕获的泛型 lambda 也不隐式转换成函数类型或函数指针；只有在类型实参由显式写出或普通实参推导选定后，调用表达式才实例化出一个具体 callable 签名并检查普通实参。

因此：

```cp
let id = f<T>(value: T) -> T {
    return value;
};

let ok = id<i32>(1);
let inferred = id(1);
let missing = f<T>() -> T { return T{}; }(); // 错误：T 不能从返回上下文反推

let bad = f<T>(value: T) {
    return value;
}; // 错误：泛型 lambda 必须显式写 -> T
```

## 泛型 lambda 与类型查询

泛型 lambda 表达式产生的是匿名闭包类型。要在类型层查询它对某组参数的返回类型，使用 `decltype` 取得闭包类型，再交给 `std.meta.call_result`：

```cp
import std.meta;

main() -> i32
{
    let count = f<T...>(values: T...) -> i32 {
        let total = 0;
        template for(let value : values...) {
            total = total + 1;
        }
        return total;
    };

    type result = call_result<decltype(count), i32, bool>;
    let value: result = count(1, true);
    return value;
}
```

这里 `decltype(count)` 是闭包对象类型，不是 `f(i32, bool) -> i32`。`call_result<decltype(count), i32, bool>` 会用类型参数 `i32, bool` 构造一次假想调用，从而实例化 `count` 的 `T...` 为 `<i32, bool>` 并得到返回类型 `i32`。

这条规则同样适用于捕获泛型 lambda 和 `T forward&...` 值参数包：

```cp
import std.meta;

read(value: i32) -> i32
{
    return value;
}

main() -> i32
{
    let bias = 1;
    let sum = f<T...>(values: T forward&...) -> i32 {
        let total = bias;
        template for(let value : values...) {
            total = total + read(forward value);
        }
        return total;
    };

    type by_ref = call_result<decltype(sum), i32&, i32 const&, i32>;
    let first = 1;
    const second = 2;
    return sum(first, second, 3);
}
```

`call_result` 中的 `i32&` 表示假想调用参数是可写左值，`i32 const&` 表示只读左值，普通 `i32` 表示非左值实参。对于 `T forward&...`，这些类别会像真实调用一样进入泛型 lambda 实例 key，并影响 `forward value` 的合法性和物化引用类型。

限制：

- `call_result` 接收的是类型实参列表，不接收运行时值；`call_result<decltype(count), x>` 中的 `x` 不是类型时不合法。
- `call_result` 只能通过 `Args...` 推导泛型 lambda 的调用实例；不能在 `call_result` 内单独给 lambda 写显式泛型实参。
- `call_result` 不绕过泛型 lambda 的显式返回类型要求。`f<T>(value: T) { value }` 仍然不合法，即使只在 `call_result` 中查询它。
- 参数数量、参数类型、引用绑定、`forward&` 绑定和 `requires` 检查都按普通调用规则执行；失败时 `call_result` 报类型查询错误，而不是产生一个占位返回类型。

## Lambda 函数体

lambda 支持三种自然写法。

常规函数体风格：

```cp
let abs = f(x: i32) -> i32 {
    if(x < 0) {
        return -x;
    }

    return x;
};
```

块表达式风格：

```cp
let abs = f(x: i32) -> i32 {
    if(x < 0) {
        return -x;
    }

    x
};
```

表达式体风格：

```cp
let inc = f(x: i32) -> i32 => x + 1;
```

`{ ... }` body 在源码层使用块表达式语法。lambda body 的尾表达式会作为该 lambda 正常路径的返回值处理：

- `return value;` 从当前 lambda 返回，不从外层函数返回。
- `return;` 只允许用于 `unit` 返回 lambda。
- 最后一项是无分号表达式时，它作为正常路径返回值。
- 没有尾表达式，或尾表达式后带分号时，正常路径返回 `unit`。
- 显式返回类型存在时，所有 `return value;` 和尾表达式都必须能转换到该返回类型。
- 省略返回类型时，语义分析统一所有 `return value;` 和尾表达式类型；没有任何带值返回路径时推导为 `unit`。
- 显式非 `unit` / 非 `!` 返回类型的 lambda 应保证所有可正常完成路径都返回对应值。`f() -> i32 { }` 这类没有返回值路径的 lambda 不属于合法公开能力。

这条 tail-return 规则只适用于 lambda body；普通块表达式仍按 [控制流](flow.md#函数返回和正常完成) 的块表达式规则保留 tail 语义。lambda body 中的 `return` 语句也不会穿透到外层函数。

表达式体：

```cp
f(x: i32) => x + 1
```

等价于：

```cp
f(x: i32) {
    x + 1
}
```

表达式体不需要也不允许在 `=>` 后直接写 `return` 语句；如果需要提前返回，应使用 `{ ... }` body。

## 捕获

lambda 可以自动捕获外层局部变量：

```cp
let bias = 10;

let add_bias = f(x: i32) {
    x + bias
};
```

捕获规则：

- lambda body 中引用外层局部变量时，编译器自动捕获该变量。
- 只读取的变量按只读捕获处理。
- 被赋值、取可写引用或传给需要可写引用的参数时，按可写捕获处理。
- 当前可写捕获标记只针对捕获名对应的表达式本身：`name = value`、`name += value`、`++name` / `name++`、`ref name`、`move name`、对非 const `name` 取地址 `&name`，以及把 `name` 放进需要可写 `T&` 的上下文都会把该捕获标记为 mutated。`const ref name`、按值读取、传给按值或 `T const&` 参数不会标记为 mutated。
- 捕获扫描会从 `object.field`、`object[index]`、`object.method(...)` 和 `object` 作为 UFCS receiver 的表达式里找到 `object` 捕获，但当前 mutability 标记不是成员/下标/方法 effect 分析。也就是说，`object.field = value`、`object[index] = value` 或调用接收 `self&` 的方法，不会仅因为修改了对象内部状态就把 `object` 捕获提升成可写捕获；需要依赖可写捕获模式时，应在源码中对捕获名本身使用明确的写入、可写借用、`move` 或 mutable-reference 参数上下文。

例如：

```cp
struct box {
    value: i32;
}

let state = box{ .value = 1 };
let local = f() -> i32 {
    state.value = state.value + 1;
    return state.value;
};
```

这里 `state.value = ...` 会捕获 `state`，但不会把 `state` 标记为 mutated。非逃逸闭包仍可能通过借到的聚合对象改到外层 `state.value`；如果这个闭包逃逸，`state` 会按只读捕获规则成为 `copy`，后续字段写入改的是闭包自己的快照，不会写回原局部，也不会触发“多个逃逸可写捕获不共享状态”的警告。需要让捕获模式明确表现为可写时，应在闭包体里对 `state` 本名取 `ref`，或把 `state` 传入需要 `box&` 的 helper，而不是只写它的字段。
- 捕获只发生在普通词法作用域变量上；模块项、普通函数名、类型名和 concept 名不算捕获。
- lambda 参数、局部 `let`、解构绑定、`for` 绑定、`template for` 绑定和 `match` pattern 绑定都会引入局部名字；同名局部名字会遮蔽外层变量，不触发捕获。
- `template for(type U : T...)` 的 `U` 是展开作用域里的类型别名，不是运行时值；捕获扫描仍把它当作当前作用域名字来遮蔽同名外层局部值。因此在这类展开体里写表达式位置的 `U` 不会捕获外层变量，而会按普通名字/类型位置规则报错或解析为类型用途。需要同时使用外层值时，应避免让类型展开名和外层值同名。
- 捕获扫描会穿过成员访问、下标访问、数组/元组/结构体初始化、块表达式、`if`、循环、`match`、`template if`、`template for` 和嵌套调用等表达式/语句形态。
- `match` arm 中的 pattern 绑定是 arm 局部名字，不捕获同名外层变量。
- 泛型 lambda 也按同一套规则捕获外层变量；定义处先按语法结构收集外层捕获，实例化时再检查具体 body 和类型替换。这个定义点捕获扫描是保守的：`template if` 的所有分支都会参与捕获集合，即使某个实例最终只语义检查选中的分支。
- 泛型 lambda 的闭包类型和捕获字段也在定义点确定，不按每个类型实参实例重新生成一套捕获布局。某个捕获只出现在特定 `template if` 分支或特定 `template for` 路径中时，它仍会成为该泛型闭包对象的一部分；具体实例只决定函数签名、`forward&` 绑定和 body 检查结果，不会删除或新增捕获字段。
- 当前泛型 lambda 的定义点捕获收集只稳定覆盖“捕获了哪些外层名字”和读出类型；捕获是否在某个具体实例里被写入，需要等实例 body 语义检查时才知道。闭包字段布局已经在定义点固定，因此公开代码不要依赖泛型 lambda 修改外层捕获变量的写回语义，尤其不要把它当作和非泛型可写捕获完全等价的 `ref` / `owned_mut_copy` 机制。需要可写状态时，优先把状态作为显式参数传入，或使用非泛型闭包封装该状态。
- 嵌套 lambda 自己负责捕获它引用到的外层名字；内层 lambda body 中的名字不会被当作外层 lambda 的直接捕获。

非泛型无捕获 lambda 等价于普通命名函数：

```cp
let f: f(i32) -> i32 = f(x: i32) {
    x + 1
};

let p: f*(i32) -> i32 = f(x: i32) {
    x + 1
};
```

有捕获 lambda 生成匿名闭包类型：

```cp
let bias = 10;

let closure = f(x: i32) {
    x + bias
};
```

有捕获 lambda 不能绑定到函数类型或函数指针类型：

```cp
let bad_function: f(i32) -> i32 = f(x: i32) {
    x + bias
}; // error

let bad_pointer: f*(i32) -> i32 = f(x: i32) {
    x + bias
}; // error
```

这条规则避免把需要环境对象的闭包误认为裸代码地址。

## 捕获与逃逸

为了让默认捕获自然，同时避免悬垂引用，逃逸规则按上下文区分：

- lambda 只在当前作用域内调用时，捕获按引用实现。
- lambda 被返回、赋给外层存储、放入结构体字段或传给可能保存它的参数时，视为逃逸。
- 简单局部绑定本身不让 lambda 逃逸；如果该绑定后续被返回、赋值到其他位置或作为调用实参传出，则绑定里关联的 lambda 按对应上下文逃逸。
- 当前逃逸别名传播只覆盖简单局部名和穿过括号、显式 cast、成员访问、下标、数组、元组、结构体初始化、`match` arm、块表达式 tail 的值传播。`(callback)`、`callback as SomeCallable` 这类只改变表达式形态或目标类型的包装不会阻断逃逸标记。
- 解构声明会先扫描 initializer，因此 initializer 里直接出现的 `return`、赋值、调用实参、`new` 初始化值或块表达式 tail 仍会按对应规则标记其中的 lambda。但解构不会把 initializer 关联到每个解构名上，后续返回或传出解构出来的 `left` / `right` 之类名字时，不会继续传播原 initializer 中的 lambda 集合。需要让闭包逃逸时，应直接返回/存储原表达式，或先用普通名字绑定整体值。

```cp
make_pair()
{
    let value = 1;
    let (left, right) = (f() { value }, f() { value + 1 });
    return left; // 当前逃逸分析不会把 left 继续关联回 initializer 里的 lambda
}
```

上面这种写法不应作为公开 API 模式使用。需要返回其中一个闭包时，写 `let pair = (...)` 后返回 `pair.0`，或直接返回目标 lambda 表达式，才能落在当前逃逸传播覆盖的路径里。

- `return` 的值中包含 lambda 时，逃逸原因是 `returned`。
- 赋值右侧、`new` 初始化值和块表达式尾值中包含 lambda 时，逃逸原因是 `stored`。赋值左侧仍会被遍历以发现成员/下标对象中的 lambda 传播，但左侧不是本次被存入的值，不会因为出现在 assignment target 位置而被标记为 `stored`。块表达式 tail 使用保守规则：`let f = { let x = 0; f() { x } };` 中的内层 lambda 会按 `stored` 处理，即使外层只是普通局部绑定；这样闭包离开块作用域后仍持有需要的捕获。
- 调用实参中包含 lambda 时，逃逸原因是 `passed`；这是保守规则，普通语义分析不证明被调函数是否立即调用或保存回调。
- 调用表达式的 callee 位置不按 `passed` 处理。`f() { value }()` 这种立即调用，或 `let cb = f() { value }; cb();` 这种简单局部调用，仍按非逃逸捕获处理；只有把 lambda 放进实参、返回值、赋值右侧、`new` 初始化值或块表达式 tail 等值传播位置时，才进入对应逃逸路径。
- 同一个 lambda 如果出现在多个上下文，逃逸强度取最大者：`returned` 强于 `stored`，`stored` 强于 `passed`。
- 非逃逸只读捕获是 `const_ref`，闭包字段类型是 `T const&`，借用外层变量。
- 非逃逸可写捕获是 `ref`，闭包字段类型是 `T&`，写回外层变量。
- 非逃逸捕获 const 源变量时，即使语法上尝试写入，最终也只能是只读引用，并由赋值/参数检查报告 const 写入错误。
- 逃逸只读捕获是 `copy`，闭包字段类型是值类型 `T`，创建闭包时拷贝快照。
- 逃逸可写捕获是 `owned_mut_copy`，闭包字段类型是值类型 `T`，闭包拥有自己的可写副本。
- 非泛型无捕获 lambda 不需要环境对象，仍按普通函数值处理。
- 不隐式生成 shared cell 或 shared capture frame。

示例：

```cp
make_adder(bias: i32)
{
    return f(x: i32) {
        x + bias
    };
}
```

这里 `bias` 是只读捕获，lambda 逃逸，因此闭包对象保存 `bias` 的值。

```cp
make_counter()
{
    let count = 0;

    return f() {
        count = count + 1;
        count
    };
}
```

这里 `count` 是逃逸可写捕获，闭包对象保存自己的 `owned_mut_copy` 字段；多次调用同一个闭包会保留该字段状态。

多个逃逸闭包捕获同一个可写变量时，不共享状态，每个闭包各自保存独立副本：

```cp
make_pair()
{
    let count = 0;

    let inc = f() {
        count = count + 1;
        count
    };
    let get = f() {
        count
    };

    return (inc, get);
} // warning: count is copied into multiple escaping closures; mutations are not shared
```

语义层按源变量记录所有逃逸捕获；当同一个源变量被至少两个逃逸闭包捕获，并且其中至少一个捕获模式是 `owned_mut_copy` 时，发出 `independent_closure_capture` 警告。这个诊断只提示状态不会共享，不会把捕获改写成共享 cell，也不会阻止编译；各闭包仍然各自保存 `copy` 或 `owned_mut_copy` 字段。

闭包对象本身也是普通值。把有捕获闭包按值复制、放入 tuple / struct、作为参数传递或返回时，闭包字段按当前类型的普通 copy / move 规则处理；语言不会为闭包对象生成共享捕获框架。也就是说，一个带 `owned_mut_copy` 状态的闭包被复制后，两个闭包值拥有各自的字段副本，后续调用不会互相共享计数器或其它可写状态。需要共享状态时，应显式捕获指针、引用语义对象，或使用标准库/运行时提供的共享容器。

如果需要拆开变量身份，可以用已有块表达式创建新的局部变量，不需要额外捕获语法：

```cp
let counter = {
    let count = 0;
    f() {
        count = count + 1;
        count
    }
};
```

## 函数参数中的回调

普通回调优先写函数类型：

```cp
apply(value: i32, cb: f(i32) -> i32) -> i32 {
    return cb(value);
}

inc(x: i32) -> i32 {
    return x + 1;
}

let a = apply(1, inc);
let b = apply(1, f(x: i32) => x + 1);
```

`cb: f(i32) -> i32` 表示调用方必须提供普通函数或非泛型无捕获 lambda；语言层保证它是非空函数值。

如果 API 明确需要底层地址语义，才写函数指针：

```cp
apply_raw(value: i32, cb: f*(i32) -> i32) -> i32 {
    return cb(value);
}
```

有捕获 lambda 不能传给 `f(...) -> R` 或 `f*(...) -> R` 参数。它需要由类型推导、泛型参数或可调用对象协议接收。

```cp
let bias = 10;

let closure = f(x: i32) {
    x + bias
};

// apply(1, closure); // error if apply expects f(i32) -> i32
```

泛型函数可以通过 `callable<Args...>` concept 接收普通函数、非泛型无捕获 lambda 和有捕获闭包。lambda/闭包语义上满足调用表达式协议，因此可作为 comparator、projection 这类泛型算法对象使用：

```cp
apply<F>(value: i32, callback: F) -> call_result<F, i32>
requires
    F: callable<i32>
{
    return callback(value);
}
```

闭包可以作为普通值存入泛型字段，只要字段类型由泛型参数承载：

```cp
struct holder<F> {
    callback: F;
}

impl<F> holder<F> {
    apply(self&, value: i32) -> i32
    {
        return callback(value);
    }
}
```

这里 `callback(value)` 先按函数值、函数指针和 lambda/闭包调用规则检查；如果 `F` 是带捕获闭包类型，也会调用其闭包对象。

调用表达式对闭包类型有专门路径：如果 callee 的读出类型是闭包类型，就直接使用该 lambda 的 callable 签名检查参数；泛型闭包会先根据显式类型实参或普通调用实参实例化。只有当 callee 不是函数、函数指针或闭包类型时，编译器才尝试查找 `operator ()`，因此普通 `struct` / `variant` 可调用对象和 lambda 闭包都能被 `callable<Args...>` / `call_result<F, Args...>` 覆盖，但它们不能互相替代函数值或函数指针边界。

这也意味着 `operator ()` 不会把普通对象转换成函数值或函数指针。`struct comparator { operator ()(...) -> bool { ... } }` 这类对象可以传给泛型 `F: callable<...>` 参数，也可以被 `call_result` 查询；但不能作为 `f(...) -> R` 或 `f*(...) -> R` 实参，不能绑定到函数类型变量，也不能通过 `as f*(...) -> R` 变成裸函数地址。需要非空函数值时传普通函数名或非泛型无捕获 lambda；需要可保存状态的可调用对象时用泛型参数或 concept 约束接收。

闭包调用的错误恢复边界：

- 非泛型闭包后面写显式类型实参会报 `invalid_type_argument`，例如 `callback<i32>(1)`。编译器仍按这个闭包自己的非泛型 callable 签名检查普通实参，并使用该签名返回类型继续恢复；它不会把调用改写成对象 `operator ()`，也不会把类型实参传给其它候选。
- 非泛型闭包实参数量不匹配会报 `argument_count_mismatch`；当前实现只按签名长度检查前缀实参，多余实参在没有目标类型的上下文中继续检查。
- 泛型闭包若显式类型实参解析、类型实参校验或普通实参推导失败，调用结果恢复为错误类型；若实例化成功后实参数量或类型仍不匹配，则按实例化出的具体 callable 签名报告参数数量或类型错误。
- 这些恢复规则只用于错误源码中的后续诊断和返回推导，不是可依赖的重载或 fallback 能力。

## 支持内容

Lambda 支持：

- 函数类型 `f(...) -> R`。
- 函数指针类型 `f*(...) -> R`。
- 普通函数名在值位置绑定为函数值。
- 非泛型无捕获 lambda 可绑定为函数类型或函数指针类型。
- 有捕获 lambda 生成匿名闭包类型。
- 有捕获 lambda 不能绑定为函数类型或函数指针类型。
- 有捕获 lambda 可通过泛型参数传递，并用调用表达式 `callback(args...)` 调用。
- 逃逸闭包按 `copy` / `owned_mut_copy` 捕获外层值；闭包对象再按普通值语义复制或移动。
- 泛型 lambda：`f<T>(value: T) -> T { ... }`。
- 泛型 lambda 默认泛型参数和类型参数包：`f<T, U = T>(...) -> R { ... }`、`f<T...>(values: T...) -> R { ... }`。
- 泛型 lambda 调用点从普通实参推导类型实参，也支持显式类型实参。
- 类型参数包 lambda：`f<T...>(values: T...) -> R { ... }`。
- `std.meta.call_result<decltype(lambda), Args...>` 查询普通 lambda 和泛型 lambda 对某组参数类型的返回类型。
- 泛型 lambda 的 `T forward&...` 值参数包在真实调用和 `call_result` 假想调用中都保留每个元素的 forward 绑定类别。
- lambda `{ ... }` body 同时支持 `return` 和尾表达式。
- lambda `=> expr` 表达式体。
- 参数名可出现在函数类型中，但不参与类型等价。

Lambda 不支持：

- 显式捕获列表。
- lambda 参数默认值。
- 泛型 lambda 返回类型推导。
- 从返回上下文反推泛型 lambda 类型实参。
- 在 `call_result` 内显式写出泛型 lambda 的类型实参；只能通过查询参数类型列表推导。
- 捕获生命周期完整证明。
- 隐式 shared capture frame / shared cell；多个逃逸闭包捕获同一个可写变量时不会共享状态。
- 闭包类型的命名、结构反射或稳定 ABI。
- 函数重载和函数类型参与重载排序。
