# 流程控制

本文档记录 KCP 流程控制语句。整体语法接近 C++，但逻辑运算使用 `and` / `or` / `not`，范围 `for` 采用当前语言自己的绑定语法。

## 运行时条件

`if`、`while`、`do while` 和 `assert` 的运行时条件都必须检查为 `bool`。当前语言不采用 C/C++ 式 truthiness：

```cp
if(ok) {
    run();
}

// 错误：整数、指针、enum、nullptr、str 和用户对象都不会自动当作 bool
// if(1) { run(); }
// if(pointer) { run(); }
// if(nullptr) { run(); }
```

需要把非 bool 状态用于条件时，应显式写比较或调用返回 `bool` 的 API：

```cp
if(count != 0) {
    run();
}

if(pointer != nullptr) {
    run();
}
```

`condition` 位置会给表达式提供 `bool` 目标类型，但这只允许已有的 `bool` 表达式或能按普通隐式转换到 `bool` 的表达式通过；当前隐式转换白名单不包含整数到 bool、指针到 bool、enum 到 bool、`str` 到 bool 或用户自定义 truthiness。

诊断路径有一个实现差异：`if`、`while` 和 `do while` 使用语句条件检查，非 bool 条件报 `condition_not_bool`；`assert(condition)` 是 builtin 调用，条件参数按 `bool` 目标类型检查，非 bool 条件报 `type_mismatch`。二者的语言要求相同，都是“条件必须是 bool”，只是错误种类不同。

运行时控制流 header 必须保留圆括号。`if(ok) { ... }`、`while(ok) { ... }`、`do { ... } while(ok);` 和 `for(let item : range) { ... }` 是当前语法；`if ok { ... }`、`while ok { ... }`、`do { ... } while ok;`、`for let item : range { ... }` 都不会被 parser 当作合法控制流。条件表达式本身可以再用普通分组括号，例如 `if((left < right) and ok) { ... }`。

条件位置只能是表达式，不是声明或 pattern。当前不支持 C++17 风格的 init-statement，也不支持 `if let` / `while let` / `if(const value = ...)` 这类把名字声明放进条件头的语法。需要在条件里使用局部中间值时，先在外层 block 声明，再用普通 `if(value_has_property)`；需要根据 `variant` case 分支时，用 `match` 表达式，而不是在 `if` 条件里写 pattern。

## if

```cp
if(condition) {
    then_statement();
}
```

```cp
if(condition) {
    then_statement();
} else if(other_condition) {
    other_statement();
} else {
    fallback_statement();
}
```

规则：

- 条件表达式类型必须是 `bool`。
- `then` 分支必须是块语句。
- `else` 可以接另一个 `if`，也可以接块语句。
- `if` 是语句，不是表达式。

## while

```cp
while(condition) {
    step();
}
```

规则：

- 条件表达式类型必须是 `bool`。
- 循环体必须是块语句。

## do while

```cp
do {
    step();
} while(condition);
```

规则：

- 条件表达式类型必须是 `bool`。
- 循环体至少执行一次。
- 尾部 `while(condition)` 在 `do` 块的作用域之外检查。`do` 块内部声明的局部名不能在条件中使用；条件只能使用外层已经可见的名字。
- `do while` 末尾必须有分号。

## for

只支持范围 `for`：

```cp
for(let value : values) {
    use(value);
}
```

```cp
for(const value : values) {
    use(value);
}
```

```cp
for(let ref value : values) {
    value += 1;
}
```

```cp
for(const ref value : values) {
    use(value);
}
```

规则：

- 绑定语法是声明语法的子集：必须以 `let` 或 `const` 开头，随后才可以显式写 `ref`。`for(ref value : range)` 和 `for(let const value : range)` 都不是当前语法。
- 范围表达式的目标语义基于 [iteration.md](iteration.md)：表达式必须是内建数组、实现 `iterable`，或在只读上下文中实现 `const_iterable`。实现 `iterator` 的游标本身不能直接作为 range-for 的范围表达式。
- `for(let value : range)` 和 `for(const value : range)` 默认按值绑定，循环变量类型为 `read_type(iter_item)`。
- `for(let ref value : range)` 要求 `iter_item` 是可写引用，并把循环变量绑定为 `T&`。
- `for(const ref value : range)` 要求 `iter_item` 是引用，并把循环变量绑定为 `T const&`。
- `const` 循环变量或 `const ref` 循环变量不能被写入。
- 循环体必须是块语句。`for(let value : values) use(value);` 不是当前语法。

不支持 C++ 三段式 `for(init; condition; step)`。

## 循环标签

范围 `for` 可以带标签：

```cp
for: outer(let row : rows) {
    for(const value : row) {
        if(value < 0) {
            continue outer;
        }

        if(value == target) {
            break outer;
        }
    }
}
```

规则：

- 标签写在 `for` 后、循环条件前：`for: label(...)`。
- 标签只能标记范围 `for`。`while: label(...)`、`do: label { ... } while(...)` 和 `template for: label(...)` 都不是当前语法。
- 标签名必须是单个 identifier；不支持路径、字符串、数字或表达式标签。
- 带标签的 `break` / `continue` 必须解析到当前或外层范围 `for` 标签。
- `while` 和 `do while` 虽然可以作为无标签 `break` / `continue` 的最近运行时循环目标，但它们不能声明标签，也不能被 `break name;` / `continue name;` 选中。
- 不要在同一嵌套区域内重复使用 loop label；重复 label 当前不作为稳定语言行为。

## break 和 continue

```cp
break;
continue;
break outer;
continue outer;
```

规则：

- `break` 和 `continue` 必须出现在循环内部。
- 语法只允许 `break;`、`continue;`、`break label;` 和 `continue label;`。标签位置只能是裸 identifier；`break 1;`、`break value;`、`continue if(condition);` 或 `break outer.value;` 都不是当前控制流能力。若 `break` 后跟一个普通局部变量名，parser 会先把它当作 label 解析，随后语义层按找不到循环标签处理，而不是把它当作要返回给循环的值。
- 这条检查先于标签查找。也就是说，函数体顶层的 `break outer;` / `continue outer;` 会先报“不在循环内”的 `invalid_break` / `invalid_continue`，不会因为 `outer` 不存在而报 `unknown_label`。
- 不带标签时作用于最近的内层运行时循环，可以是 `while`、`do while` 或范围 `for`。
- 带标签时只能作用于对应标签的当前或外层范围 `for`；没有标签语法的 `while` / `do while` 不能作为带标签跳转目标。
- `template for` 不是运行时循环，不能作为 `break` / `continue` 的目标。
- 如果 `template for` 展开体内声明了新的运行时循环，展开体中的 `break` / `continue` 可以控制这个内层循环。
- `break` / `continue` 不能从 `template for` 展开体穿透到外层运行时循环；带标签写法同样不能跳到展开体外部的 `for: label(...)`。
- `break` / `continue` 跳转前会清理从当前位置到目标循环边界之间已经注册的局部对象；析构注册和清理边界见 [ownership.md](ownership.md#析构与清理路径)。

## template for

`template for` 是语句级编译期展开，不是运行时循环。完整泛型和参数包规则见 [generic.md](generic.md)。

```cp
template for(let value : values...) {
    use(value);
}

template for(type T : Args...) {
    use_type<T>();
}
```

规则：

- `template for(let value : values...)` 和 `template for(const value : values...)` 只能遍历当前函数、成员函数或 lambda 实例中的值参数包。
- 值参数包展开不会生成运行时循环变量，也不会额外复制 pack 元素。每次展开里的 `value` 是当前函数实例中对应参数元素的局部名字；按值参数包元素本身已经是被调函数的局部副本，`let ref` / `const ref` 展开则绑定为引用。
- `template for(let ref value : values...)` / `template for(const ref value : values...)` 使用引用绑定形态；如果参数包元素来自 `forward&` 参数包，展开出的局部绑定保留对应元素的 forward 资格。
- `template for(type T : Args...)` 只能遍历当前实例中的类型参数包；`T` 是每次展开体内的局部类型别名，不是运行时值。
- header 必须写在圆括号中，绑定名和 pack 名之间必须写 `:`。`template for let value : values... { ... }`、`template for(let value values...) { ... }` 都不是当前语法。
- 被遍历的名字后必须写 `...`，普通数组、tuple、range、局部变量和 concept 参数包都不能作为 `template for` 的范围。`template for(let value : values)` 会在 parser 阶段因为缺少 `...` 被拒绝。
- 展开体必须是块语句。`template for(let value : values...) use(value);` 不是当前语法。
- 参数包可以为空；为空时 body 展开零次。
- 每次展开都有自己的语义上下文和局部作用域；展开体按源码顺序重复检查，不生成运行时循环。
- `template for` 不建立运行时循环目标，没有运行时计数器，也不接收循环标签。
- `break` / `continue` 不能直接作用于 `template for`；展开体内新建的运行时循环可以被自己的 `break` / `continue` 控制。
- 展开体中的 `return` 返回当前函数或 lambda，和普通语句中的 `return` 一样参与返回类型推导。

## template if

`template if` 是语句级编译期分支，用于在具体泛型实例中选择一个语句体：

```cp
template if(T == i32) {
    return 1;
} else template if(T: display) {
    return 2;
} else {
    return 0;
}
```

规则：

- 条件支持类型相等、concept 条件、可折叠为 `bool` 的小型常量表达式，以及 `not` / `and` / `or` / 括号组合。
- 表达式条件必须先能按普通表达式规则检查为 `bool`，再能折叠出编译期 `bool` 常量；不能折叠时不是运行时 `if`，而是语义错误。
- 类型相等条件比较两侧规范化后的读出类型，不是源码形状的严格文本比较。普通别名会先展开，引用会被剥掉；因此 `T& == T`、`T const& == T` 和 `type alias = T; alias == T` 当前都会按同一个读出类型成立。指针、数组、tuple、struct、variant、opaque alias 等非引用形状仍按实际类型比较。
- 类型相等或 concept 条件如果在当前实例中仍然依赖未替换类型，会报错，不延迟到运行时。
- `and` / `or` 在条件求值中按编译期短路处理。
- 每个 `template if` / `else template if` 分支体都必须是块语句；最终 `else` 也只能接块语句。`template if(T == i32) return 1;` 不是当前语法。
- 只检查第一个命中的分支；未选中的分支只要求语法正确，不解析名字、不检查类型、不贡献返回类型，也不参与循环或借用状态。
- 没有分支命中且没有 `else` 时，等价于空语句。
- 被选中的分支按普通语句处理：其中的 `return`、`break`、`continue`、局部声明和块表达式规则都与运行时语句一致。

当前表达式条件的 constexpr 折叠范围：

- 可作为字面值输入的是 `true` / `false`、整数字面量、字符字面量，以及已经解析出的 `Enum::case`。
- 括号分组会递归折叠。
- 一元 `not` 只作用于可折叠 bool；一元 `-` 只作用于可折叠整数。
- `==` / `!=` 可以比较两个同类可折叠字面值；整数、字符、bool 和 enum case 都可以参与相等比较。
- `<` / `<=` / `>` / `>=` 当前只折叠整数比较。字符、bool、浮点和字符串不参与这些关系比较。
- `and` / `or` 只要求被实际求值的一侧能折叠为 bool；因此 `false and missing_name` 和 `true or missing_name` 不会检查右侧名字。
- 不折叠算术、位运算、cast、函数调用、局部变量、字段访问、数组/tuple/struct/variant 构造、match 或 block 表达式。表达式仍会先按 bool 条件做普通语义检查；类型不是 bool 时先报 `condition_not_bool`，类型是 bool 但不能折叠时才报 `invalid_operator`。
- `template if` 的普通语义检查可以把合法 enum case 条件折叠成 bool；但省略返回类型时，不要依赖 enum case 条件排除未选中分支。需要这种分支影响返回类型时，应显式写返回类型，或改用类型相等 / concept 条件。

enum case 参与折叠不绕过普通 enum 运算规则。只有普通表达式已经是合法 `bool` 条件时，constexpr 折叠才读取 case 的常量值。因此同一个 enum 类型的 case 可以做 `==` / `!=`，不同 enum 类型仍不能比较，enum 也不能靠这里使用 `<` / `<=` / `>` / `>=`：

```cp
enum mode : u8 {
    read = 1;
    write = 2;
}

template if(mode::read == mode::read) {
    // 合法：同类型 enum 相等比较，折叠为 true
}

template if(mode::read < mode::write) {
    // 错误：enum 没有内建关系比较；需要先显式转到底层整数
}
```

例如：

```cp
template if(1 < 2 and 'a' == 'a') {
    return 1;
}

template if(1 + 1 == 2) {
    return 2; // 当前不合法：template if 的 constexpr 折叠不支持算术表达式
}
```

## 空语句与空块

源码里单独写 `;` 会被 parser 当作空语句诊断并丢弃：

```cp
main()
{
    let value = 1;; // warning: empty_statement
    return value;
}
```

这条诊断是 warning，不会像语法 error 一样让整次解析失败；但 `;` 不会产生 AST 语句节点，也不是推荐的 no-op 语句能力。它可以出现在顶层、块语句和块表达式内部，都会被消费并从后续语义检查中消失。需要一个什么都不做的分支时，写空块 `{}`；需要表达“没有 template if 分支命中”时，语义上等价于不生成任何语句，而不是源码层要求写一个分号。

## 表达式语句

普通表达式后接分号可以作为语句：

```cp
counter = counter + 1;
log(counter);
panic("stop");
```

规则：

- `expr;` 会按普通表达式做完整语义检查，包括名字查找、重载选择、赋值目标、调用参数、借用和类型转换检查。
- 表达式结果会被丢弃；结果类型可以是普通值、引用、指针、内部 `unit` 或 `!`。当前没有 `[[nodiscard]]` 这类“结果必须使用”的语言规则。
- 分号是表达式语句的一部分。块中的最后一个表达式如果没有分号，会按块表达式尾表达式处理，而不是表达式语句；在普通函数体的块语句中不能写裸尾表达式来表示返回值。
- `unit` 表达式语句可以正常完成，例如 `free(ptr);`、`assert(condition);` 或调用返回 `void` 的函数。
- 类型为 `!` 的表达式语句不能正常完成，例如 `panic("bad");` 或 `unreachable();`。这会影响后续块、函数返回检查和 `!` 返回函数的正常完成判断。
- 表达式语句不能直接声明名字；需要局部名字时使用 `let` / `const` 声明。`type name = Type;` 是局部类型别名语句，不是表达式语句。

## return

```cp
return;
return value;
```

规则：

- `return value;` 的值必须能转换到当前函数返回类型。
- `return;` 只允许用于 `unit` 返回。
- 函数省略返回类型时，`return;` 会作为 `unit` 参与返回类型推导；它不能和普通带值返回混合推导成非 `unit` 类型。
- 在块表达式中写 `return` 时，仍然从所在函数返回，不是从块表达式返回。
- 块表达式内部的 `return value;` 会参与所在函数的返回类型推导；例如 `let ignored = { return 1; };` 会让所在函数推导为 `i32`。
- lambda 是新的函数边界。lambda body 中的 `return` 返回当前 lambda，不返回外层函数。
- 函数声明返回 `!` 时，函数体不能有正常完成路径；`return;` 不合法，`return value;` 只有在 `value` 本身类型为 `!` 时合法。

省略返回类型时，当前实现先收集函数体中所有可见 `return` 语句的表达式类型，再做一个有限的统一：

- 函数体没有任何 `return` 时，推导为 `unit`。
- `return;` 贡献 `unit`。它可以和其它 `return;` 统一，但不能和带值返回统一。
- `return;` 和带值 `return value;` 不能统一。`return;` 先出现、后面再出现 `return 1;`，或先出现 `return 1;`、后面再出现 `return;`，当前预推导都会让函数返回类型保持未推导状态并报
  `cannot_infer_return_type`；不会把二者合并成某个更宽的类型，也不会把 `unit` 当作可忽略分支。
- `return panic(...)`、`return unreachable()` 或其它类型为 `!` 的返回不决定普通返回类型；只要还有普通返回，推导结果由普通返回决定。所有返回都是 `!` 时，函数推导为 `!`。
- 多个整数返回可以统一成当前普通整数 common type；例如一个分支返回 `i32` 局部、另一个分支返回 `i64` 局部时，会按整数提升规则取共同类型。rank 相同时保留先观测到的返回类型，因此 `return 1 as i32;` 后再返回 `u32` 会推导为 `i32`，反过来先返回 `u32` 则推导为 `u32`。
- 多个浮点返回可以统一成较高 rank 的浮点类型。
- 除整数/浮点 common type 外，其它普通返回类型必须完全相同。`i32` 与 `bool`、`unit` 与 `i32`、不同名义 `struct` / `variant` 类型、数组长度不同的数组、不同 tuple 形状都不能自动统一；这些组合在省略返回类型函数中会导致
  `cannot_infer_return_type`，而不是选择第一个返回类型后再按后续 `return` 报普通 `return_type_mismatch`。
- `while`、`do while` 与范围 `for` 的循环体中的 `return` 会参与省略返回类型推导；这不表示循环已被证明一定执行。
  若函数体在循环后仍可能正常完成，后续正常完成检查仍会要求显式返回。
- 如果推导返回类型需要调用另一个也正在推导返回类型的函数，例如直接递归 `f() { return f(); }` 或互递归 `f() { return g(); } g() { return f(); }`，当前会报告不能推导返回类型；给其中至少一个函数写显式返回类型。
- 推导出的非 `unit` / 非 `!` 返回类型仍要通过正常完成检查。`f() { if(ok) { return 1; } }` 会先从 `return 1;` 推导出 `i32`，然后因为函数体仍可能不返回而报告缺少返回。

当前实现按语义检查后的控制流形状判断“函数体能否正常完成”：

- `return`、`break`、`continue` 语句自身不能正常完成。
- 类型为 `!` 的表达式语句不能正常完成，例如 `panic("bad");`。
- 声明初始化器若类型为 `!`，该声明不能正常完成，例如 `let value = panic("bad");`。
  初始化器是块表达式时也按块表达式最终类型判断；例如 `let ignored: i32 = { return 1; };` 中的 `return 1;`
  直接从所在函数返回，初始化器类型为 `!`，不会要求后续继续初始化 `ignored`。`assert(false)` 固定是 `unit`，
  不会因为实参是字面量 `false` 而让声明或后续语句发散。
- 一个块中的任意前序语句不能正常完成时，后续路径也被视为不能从该块正常到达。
- 块表达式按同样规则判断：先顺序检查块内语句；只要某条前序语句不能正常完成，块表达式整体不能正常完成，尾表达式即使存在也不可达。所有前序语句都可能正常完成时，没有尾表达式的块表达式可以正常完成；有尾表达式时，块表达式能否正常完成取决于尾表达式本身。
- `if` 没有 `else` 时始终可能正常完成；有 `else` 时，只要任一分支可能正常完成，整个 `if` 就可能正常完成。
- `match` 是表达式；如果所有 arm 的值都不能正常完成，整个 `match` 表达式不能正常完成；只要存在一个能正常完成的 arm，整个 `match` 表达式仍可能正常完成。
- `while`、`do while`、范围 `for` 和 `template for` 当前都按可能正常完成处理，即使条件字面量写成 `true`，循环体第一条语句就是 `continue`，或者范围 / 参数包在当前源码形状下看起来非空。语义层不会用“循环一定至少执行一次”“没有可达 break”“参数包长度非零”这类事实证明循环整体发散；需要让函数返回 `!` 或满足非 `unit` 返回检查时，应在循环之后显式写 `panic(...)` / `unreachable()` / `return ...`，或把发散表达成已经被语义层识别为 `!` 的表达式语句。
- 被选中的 `template if` 分支按普通语句判断；未选中的分支不参与。

因此：

```cp
bad() -> i32
{
    if(true) {
        return 1;
    }
} // 错误：没有 else，当前实现认为函数仍可能正常完成

ok() -> !
{
    panic("stop");
} // 合法：panic 的类型是 !

spin() -> !
{
    while(true) {
    }
} // 当前仍然错误：循环语句按可能正常完成处理，不把条件字面量 true 当作发散证明
```
