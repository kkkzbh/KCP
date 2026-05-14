# 从编译原理理论读懂 cp Parser

这份教程的目标不是重新讲一遍教材，而是帮你把 `/home/kkkzbh/code/cp/教材ppt` 里的理论概念，逐步对应到本仓库 `parser/` 的真实代码。

建议阅读时开着三个窗口：

- 本文档
- `parser/syntax/recursive_descent.cppm`
- `parser/ast/` 下的 AST 定义

先不要急着记所有函数名。parser 的核心问题只有一个：**输入是一串 token，输出是一棵语法结构树，期间要判断这串 token 是否符合 cp 语言的语法规则。**

## 0. 先建立总地图

本仓库前端主线是：

```text
source -> preprocessor -> lexer -> parser.syntax -> ast_arena + translation_unit_id
```

对应理论分工如下：

| 编译阶段 | 教材理论 | 仓库代码 | 产物 |
|---|---|---|---|
| 源文件管理 | 源程序输入、位置记录 | `source/` | `source_manager`, `file_id`, `source_span` |
| 预处理 | 实验一扩展里的注释处理 | `preprocessor/` | 去除注释后的文本 |
| 词法分析 | 正规文法、正则式、NFA、DFA | `lexer/` | `std::vector<token>` |
| 语法分析 | CFG、LL(1)、递归下降、Pratt、算符优先 | `parser/` | `ast_arena + translation_unit_id` |
| 语义分析 | 属性文法、语法制导翻译 | 未来 `semantic/` 更合适 | 符号表、类型表、中间代码 |

你现在要熟悉的是第四行：`lexer` 已经把字符流切成 token，parser 不再关心 `"while"` 是怎么识别出来的，只关心当前 token 是不是 `token_kind::kw_while`。

教材 PPT 对应路线：

- `CHP-02-TCH.md`：理解 lexer 为什么输出 token，正规文法/有限自动机主要对应 `lexer/`。
- `CHP-04-TCH.md`：本文主线，LL(1)、递归下降、FIRST/FOLLOW/SELECT、左递归、左公因子。
- `CHP-06-TCH-2026春.md`：理解算符优先、移进-归约、LR，它们是 parser 的实验和对照路线。
- `CHP-07-TCH-2026春.md`：后续语义分析，解释为什么 AST 之后还需要符号表、类型检查、中间代码。

## 1. Parser 不从字符开始，而从 token 开始

理论上，词法分析把字符流变成单词串；语法分析判断单词串是否属于某个上下文无关文法。

代码入口是：

```cpp
auto parse_translation_unit(source_manager const& sources, file_id file) -> parse_result
```

位置：`parser/syntax/recursive_descent.cppm` 文件末尾。

它做了三件事：

1. 用 `lexer` 把 `file` 转为 `tokens`。
2. 如果 lexer 已经报错或产生 `invalid` token，就返回 `lexical_failure`，parser 不继续硬解析。
3. 构造 `recursive_descent_parser`，调用 `parser.parse()`。

对应理论：

```text
字符流 --词法分析--> token stream --语法分析--> 语法树 / 拒绝
```

所以读 parser 时，不要从字符、正则、DFA 开始；那是 lexer 的职责。parser 的最小输入单位是：

```cpp
token {
    token_kind kind;
    source_span span;
    flags;
}
```

其中 `kind` 用来判断语法，`span` 用来记录 AST 节点和诊断位置。

## 2. `parse_result` 就是语法分析的输出合同

先看 `parse_result`：

```cpp
export struct parse_result
{
    bool accepted{};
    ast_arena ast{};
    translation_unit_id root{};
    std::vector<parser_diagnostic> parser_diagnostics{};
    std::vector<lexer_diagnostic> lexer_diagnostics{};
};
```

它对应实验要求里的几个点：

| 字段 | 理论/实验含义 |
|---|---|
| `accepted` | 输入符号串是否是该文法的句子 |
| `parser_diagnostics` | 为什么不是合法句子 |
| `lexer_diagnostics` | 上游词法错误 |
| `ast` | 语法树节点仓库 |
| `root` | 整棵语法树的根，也就是开始符号 `TranslationUnit` 的结果 |

教材里常说“从开始符号 S 推导输入串”。在代码里，这个开始符号就是：

```cpp
parse_translation_unit_node()
```

它成功时返回：

```cpp
std::optional<translation_unit_id>
```

这相当于说：我成功识别了一个 `TranslationUnit`，并把它放进了 `ast_arena`。

## 3. 递归下降的基本形态：一个非终结符对应一个函数

`CHP-04` 讲递归下降 LL(1) 时，核心规则是：

```text
每个非终结符写成一个子程序。
根据当前输入符号，选择产生式。
匹配终结符时，向前读一个 token。
```

本仓库主线 parser 就是这个结构。你可以直接做如下对应：

| 文法非终结符 | 代码函数 |
|---|---|
| `TranslationUnit` | `parse_translation_unit_node()` |
| `ModuleHeader` | `parse_module_header()` |
| `Import` | `parse_import_declaration()` |
| `Function` | `parse_function()` |
| `Parameter` | `parse_parameter()` |
| `Statement` | `parse_statement()` |
| `Block` | `parse_block_statement()` |
| `Type` | `parse_type()` |
| `ModuleName` | `parse_module_name()` |
| `Expression` | `parse_expression()` |
| `Assignment` | `parse_assignment()` |
| `Unary` | `parse_unary()` |
| `Postfix` | `parse_postfix()` |
| `Primary` | `parse_primary()` |

先看最小模式：

```cpp
[[nodiscard]]
auto expect(token_kind kind) -> std::optional<token>
{
    if(not at(kind)) {
        report_current(...);
        return std::nullopt;
    }

    return consume();
}
```

这就是教材里的“匹配终结符”：

```text
if lookahead == terminal:
    match terminal
else:
    syntax error
```

`at(kind)` 查看当前 token，`consume()` 消耗当前 token 并前进，`expect()` 则是“必须匹配，否则报错”。

这三个函数是读 parser 的钥匙：

| 函数 | 含义 |
|---|---|
| `peek()` | 查看当前或向前若干 token，不消耗 |
| `at(...)` / `at_any(...)` | 判断当前 token 是否符合某类终结符 |
| `consume()` | 消耗当前 token，输入指针前移 |
| `expect(...)` | 必须匹配某终结符；失败就记录诊断 |

## 4. 从顶层文法读 `parse_translation_unit_node`

cp 的顶层结构大致是：

```text
TranslationUnit
    -> ModuleHeader? Import* Function*
```

代码是：

```cpp
if(at(token_kind::kw_export) and peek(1).kind == token_kind::kw_module) {
    unit.module_header = parse_module_header();
}

while(at(token_kind::kw_import)) {
    auto item = parse_import_declaration();
    ...
}

while(not at(token_kind::eof)) {
    auto function = parse_function();
    ...
}
```

这一段非常适合作为第一个阅读点，因为它几乎就是文法的直接翻译：

| 代码 | 文法含义 |
|---|---|
| `if(export module)` | 可选模块头 `ModuleHeader?` |
| `while(import)` | 零个或多个导入 `Import*` |
| `while(not eof)` | 直到文件结束，解析函数列表 |

这里的 `peek(1)` 是 LL(1) 之外的一点工程处理：`export module` 和 `export function` 都以 `export` 开头，只看一个 token 不够区分，所以向前看第二个 token。理论上这叫处理左公因子；工程里直接用 lookahead 写出来。

文法可写成：

```text
TopLevelAfterExport -> module ModuleName ;
                    | identifier FunctionAfterName
```

代码写成：

```cpp
if(at(kw_export) and peek(1).kind == kw_module) ...
```

这就是理论到代码的第一座桥。

## 5. 左公因子：为什么 `export module` 要特殊看

教材说左公因子会让预测分析无法只靠当前 token 选产生式。比如：

```text
TopLevel -> export module ModuleName ;
          | export identifier FunctionAfterName
```

两条产生式都以 `export` 开头。只看当前 token，无法决定走哪条。

理论变换是提取左公因子：

```text
TopLevel -> export TopLevelAfterExport
TopLevelAfterExport -> module ModuleName ;
                     | identifier FunctionAfterName
```

仓库中有两处体现：

1. 实验 LL(1) 文法：`parser/ll1/grammar.cppm` 里的 `TranslationUnitAfterExport`。
2. 主线递归下降：`parse_translation_unit_node()` 和 `parse_function()` 用 `peek(1)` 分辨。

为什么主线可以这么写？

因为真实编译器 parser 不一定要机械地照表驱动 LL(1) 写。它的目标是清晰、可靠地解析语言。理论告诉你“这里存在共同前缀，要分流”，代码可以选择显式 lookahead。

## 6. 语句层：`parse_statement()` 是 FIRST 集的手写版

教材中 FIRST 集回答的是：

```text
某个非终结符可能以哪些终结符开头？
```

`parse_statement()` 就是把 `Statement` 的 FIRST 集写成 `switch`：

```cpp
switch(peek().kind) {
case token_kind::l_brace:
    return parse_block_statement();
case token_kind::kw_let:
case token_kind::kw_const:
    return parse_declaration_statement();
case token_kind::kw_if:
    return parse_if_statement();
case token_kind::kw_while:
    return parse_while_statement();
case token_kind::kw_do:
    return parse_do_while_statement();
case token_kind::kw_for:
    return parse_for_statement();
case token_kind::kw_break:
    return parse_break_continue_statement(true);
case token_kind::kw_continue:
    return parse_break_continue_statement(false);
case token_kind::kw_return:
    return parse_return_statement();
default:
    return parse_expression_statement();
}
```

理论对应：

```text
Statement
    -> Block
     | DeclarationStmt
     | IfStmt
     | WhileStmt
     | DoWhileStmt
     | ForStmt
     | BreakStmt
     | ContinueStmt
     | ReturnStmt
     | ExprStmt
```

也就是：

| 当前 token | 选择的产生式 |
|---|---|
| `{` | `Statement -> Block` |
| `let` / `const` | `Statement -> DeclarationStmt` |
| `if` | `Statement -> IfStmt` |
| `while` | `Statement -> WhileStmt` |
| `do` | `Statement -> DoWhileStmt` |
| `for` | `Statement -> ForStmt` |
| `break` | `Statement -> BreakStmt` |
| `continue` | `Statement -> ContinueStmt` |
| `return` | `Statement -> ReturnStmt` |
| 表达式开头 token | `Statement -> ExprStmt` |

所以你读到 `switch(peek().kind)` 时，要在脑中自动翻译成：

```text
这是 SELECT 集 / FIRST 集的工程实现。
```

## 7. 块语句：递归结构如何自然出现

块语句文法：

```text
Block -> { Statement* }
```

代码：

```cpp
auto open = expect(token_kind::l_brace, "'{'");

auto block = block_statement_syntax{};
while(not at_any({ token_kind::r_brace, token_kind::eof })) {
    auto statement = parse_statement();
    if(statement) {
        block.statements.push_back(*statement);
        continue;
    }
    ...
}

auto close = expect(token_kind::r_brace, "'}'");
return arena_.add(statement_syntax{ std::move(block) });
```

这里的递归不是“函数自己直接调用自己”才算递归，而是语法结构递归：

```text
parse_block_statement()
    -> parse_statement()
        -> parse_if_statement()
            -> parse_block_statement()
```

比如：

```cp
if(value < 0) {
    continue;
}
```

解析路线是：

```text
parse_statement
-> parse_if_statement
-> parse_expression      解析 value < 0
-> parse_block_statement 解析 { continue; }
-> parse_statement
-> parse_break_continue_statement(false)
```

这就是“语法树的层次结构”在代码调用栈上的体现。

## 8. AST：为什么不是每个节点 `new` 一个对象

教材常画分析树或语法树，节点之间有父子关系。很多初学实现会写成：

```cpp
struct Expr {
    virtual ~Expr() = default;
};

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};
```

本仓库没有这么做，而是使用 Rust 风格的：

```text
typed id + arena + std::variant
```

核心文件：

- `parser/ast/ids.cppm`
- `parser/ast/arena.cppm`
- `parser/ast/expr.cppm`
- `parser/ast/stmt.cppm`
- `parser/ast/item.cppm`
- `parser/ast/type.cppm`

### 8.1 typed id

`ids.cppm` 定义：

```cpp
export using expr_id = syntax_id<expr_tag>;
export using stmt_id = syntax_id<stmt_tag>;
export using type_id = syntax_id<type_tag>;
export using function_id = syntax_id<function_tag>;
export using translation_unit_id = syntax_id<translation_unit_tag>;
```

这表示：

- 表达式节点用 `expr_id`
- 语句节点用 `stmt_id`
- 类型节点用 `type_id`
- 函数节点用 `function_id`
- 顶层根节点用 `translation_unit_id`

它们底层都是整数下标，但类型不同，避免把 `stmt_id` 错传给 `arena.expression()`。

### 8.2 arena

`arena.cppm` 里有：

```cpp
std::vector<expr_syntax> expressions{};
std::vector<statement_syntax> statements{};
std::vector<type_syntax> types{};
std::vector<function_syntax> functions{};
std::vector<translation_unit_syntax> translation_units{};
```

添加节点时：

```cpp
auto id = expr_id{ static_cast<std::uint32_t>(expressions.size()) };
expressions.push_back(std::move(node));
return id;
```

这相当于：

```text
节点本体集中放在 arena 里。
节点之间只保存 id。
```

例如二元表达式：

```cpp
export struct binary_expr_syntax
{
    source_span full_span{};
    token_kind operator_kind{ token_kind::eof };
    expr_id left{};
    expr_id right{};
};
```

不是保存 `unique_ptr<Expr>`，而是保存左右子表达式的 `expr_id`。

### 8.3 variant

表达式总类型是：

```cpp
export using expr_syntax = std::variant<
    name_expr_syntax,
    literal_expr_syntax,
    unary_expr_syntax,
    binary_expr_syntax,
    assignment_expr_syntax,
    call_expr_syntax,
    cast_expr_syntax,
    array_literal_expr_syntax,
    sequence_literal_expr_syntax,
    tuple_literal_expr_syntax,
    grouped_expr_syntax>;
```

理论里的非终结符：

```text
Expression -> Name
            | Literal
            | Unary
            | Binary
            | Call
            | ...
```

代码里就是：

```cpp
std::variant<name_expr_syntax, literal_expr_syntax, ...>
```

这是一一对应的。

## 9. 函数定义：从文法到节点构造

cp 函数示例：

```cp
export sum_non_negative(values: array<i32,4>) -> i32
{
    let total: i32 = 0;
    return total;
}
```

文法可以简化理解为：

```text
Function
    -> export? identifier ( ParameterList? ) ReturnType? Block
```

代码 `parse_function()` 的顺序就是这个顺序：

1. 处理可选 `export`
2. `expect_identifier("function name")`
3. `expect(l_paren)`
4. 循环解析参数列表
5. `expect(r_paren)`
6. 如果看到 `->`，解析返回类型
7. 解析函数体 block
8. 构造 `function_syntax`
9. `arena_.add(function)`

构造出的节点：

```cpp
function_syntax{
    .full_span = ...,
    .exported = exported,
    .name = name->span,
    .parameters = std::move(parameters),
    .return_type = return_type,
    .body = *body,
}
```

注意：`name` 存的是 `source_span`，不是字符串。因为 parser 阶段只负责结构，后续如果要拿真实文本，可以通过 `source_manager + span` 查。这样可以避免 parser 过早把语义信息塞进 AST。

## 10. 类型解析：上下文让同一个 token 有不同意义

类型文法大致是：

```text
Type -> identifier TypeArguments? const? (& | *)*
TypeArgument -> Type | integer_literal
```

例子：

```cp
array<array<i32,3>,2>
```

代码入口是 `parse_type()`。

关键步骤：

```cpp
auto name = context.expect_identifier("type name");

if(at(token_kind::less)) {
    consume();
    parse_type_argument();
    while(at(token_kind::comma)) ...
    expect_closing_angle();
}

if(at(token_kind::kw_const)) ...

while(at_any({ token_kind::amp, token_kind::star })) ...
```

这里有一个很工程化的点：`expect_closing_angle()` 会处理 `>>`。

为什么？

词法分析时，`array<array<i32,3>,2>` 里的两个连续 `>` 可能被 lexer 识别成一个 `greater_greater` token。可是在类型参数上下文里，它应该表示两个嵌套泛型的右尖括号。

所以 parser 做了这个修正：

```cpp
split_current_double_greater()
```

它把当前 `>>` 临时拆成两个 `>` 注入回 token 流。

理论上这说明一件事：lexer 只做局部最长匹配，parser 可以利用上下文修正语法含义。这个例子能帮你理解“词法和语法边界不是完全没有交互，但职责仍然不同”。

## 11. 表达式为什么没有完全照 LL(1) 写

表达式是 parser 中最容易卡住的部分，因为教材常用这种文法：

```text
E -> E + T | T
T -> T * F | F
F -> (E) | id
```

这个文法很直观，但有直接左递归：

```text
E -> E + T
T -> T * F
```

递归下降如果照写：

```cpp
parse_E() {
    parse_E();
    expect('+');
    parse_T();
}
```

会无限递归。

教材的 LL(1) 处理方式是消除左递归：

```text
E  -> T E'
E' -> + T E' | ε
T  -> F T'
T' -> * F T' | ε
```

本仓库实验 LL(1) 文法在 `parser/ll1/grammar.cppm` 中确实体现了类似思想。

但主线 parser 没有为每层优先级都写一个 `parse_additive()`、`parse_multiplicative()`，而是使用 Pratt parser：

```cpp
parse_expression()
-> parse_assignment()
-> parse_expression_pratt(0)
-> parse_unary()
-> parse_postfix()
-> parse_primary()
```

这是一种工程上更简洁的表达式解析方式。

## 12. Pratt parser：用绑定力解决优先级

Pratt parser 的核心思想：

```text
先解析一个左操作数。
看当前二元运算符的优先级是否足够高。
如果足够高，就吃掉运算符，递归解析右操作数。
否则停止，把左边结果返回给上层。
```

绑定力表在 `parser/expression/precedence.cppm`：

```cpp
{ token_kind::plus,  "plus",  110, 111 },
{ token_kind::star,  "star",  120, 121 },
```

`*` 的绑定力高于 `+`，所以：

```cp
a + b * c
```

解析时会成为：

```text
a + (b * c)
```

主循环在 `parse_expression_pratt(int min_bp)`：

```cpp
auto left = parse_unary();

while(true) {
    auto entry = find_binary_operator(peek().kind);
    if(not entry or entry->left_bp < min_bp) {
        break;
    }

    auto operation = consume();
    auto right = parse_expression_pratt(entry->right_bp);

    left = arena_.add(binary_expr_syntax{
        .operator_kind = operation.kind,
        .left = *left,
        .right = *right,
    });
}
```

把 `a + b * c` 走一遍：

1. `parse_unary()` 读到 `a`，作为 `left`。
2. 当前是 `+`，绑定力够，吃掉 `+`。
3. 递归解析右边，最低要求变成 `111`。
4. 右边先读到 `b`。
5. 当前是 `*`，`*` 的 `left_bp = 120`，大于 `111`，所以它应该属于右边。
6. 吃掉 `*`，解析 `c`，构造 `b * c`。
7. 返回外层，构造 `a + (b * c)`。

这个过程对应理论里的“优先级”和“结合性”，但代码不需要写十几层递归函数。

## 13. 赋值为什么单独处理

`parse_assignment()` 在 Pratt 前面：

```cpp
auto left = parse_expression_pratt(0);

if(not is_assignment_operator(peek().kind)) {
    return left;
}

auto operation = consume();
auto right = parse_assignment();
```

注意右边调用的是 `parse_assignment()`，不是 `parse_expression_pratt(0)`。

这表示赋值是右结合：

```cp
a = b = c
```

应解析为：

```text
a = (b = c)
```

而普通二元算符大多是左结合：

```cp
a - b - c
```

解析为：

```text
(a - b) - c
```

这就是理论里的结合性，在代码中的直接体现。

## 14. Primary、Postfix、Unary：从最小表达式往外包

表达式层可以这样理解：

```text
Primary  是最小原子：名字、字面量、括号、数组/序列/元组字面量
Postfix  在原子后面加调用、后缀 ++/--
Unary    在前面加前缀 + - ! ~ & * ++ --
Binary   用 Pratt 组合左右表达式
Assign   处理赋值
```

对应代码：

| 层级 | 函数 | 示例 |
|---|---|---|
| Primary | `parse_primary()` | `x`, `123`, `(a)`, `[1,2]` |
| Postfix | `parse_postfix()` | `f(x)`, `i++` |
| Unary | `parse_unary()` | `-x`, `*p`, `++i` |
| Binary | `parse_expression_pratt()` | `a + b * c` |
| Assignment | `parse_assignment()` | `x = y + 1` |

这条链很好记：

```text
越靠下，越贴近 token；
越靠上，越接近完整表达式。
```

## 15. 括号表达式与元组：左公因子的第二个例子

cp 同时支持：

```cp
(x)
(x, y)
```

它们都以 `(` 开始。理论上存在共同前缀：

```text
ParenPrimary -> ( Expression ParenTail
ParenTail -> )
           | , Expression TupleTail )
```

代码在 `parse_paren_expression()`：

```cpp
auto first = parse_expression();

auto elements = std::vector<expr_id>{};
if(at(token_kind::comma)) {
    elements.push_back(*first);

    while(at(token_kind::comma)) {
        consume();
        auto next = parse_expression();
        elements.push_back(*next);
    }
} else {
    elements.push_back(*first);
}

expect(token_kind::r_paren, "')'");

if(elements.size() > 1uz) {
    return tuple_literal_expr_syntax{...};
}

return grouped_expr_syntax{...};
```

这不是“读完才瞎猜”，而是左公因子的工程实现：

1. 共同前缀是 `(` 和第一个 `Expression`。
2. 之后看有没有 `,`。
3. 有逗号就是 tuple。
4. 没逗号就是 grouped expression。

## 16. 错误恢复：为什么不是一错就退出

实验要求通常只说输出“是否为句子”，但真实 parser 更需要“报错后继续找更多错误”。

本仓库有两个同步函数：

```cpp
synchronize_statement()
synchronize_top_level()
```

思想是：发生错误后，跳过一些 token，直到遇到比较可靠的边界。

语句级边界：

```text
; 或 }
```

顶层边界：

```text
;、import、export、identifier、eof
```

这对应 `CHP-04` 里的预测分析出错处理。理论上常说同步集合；代码里就是“跳到下一个比较像边界的位置”。

例如函数体中少了分号：

```cp
let x = 1
return x;
```

parser 发现 `let` 声明缺少 `;` 后，不能立刻放弃整个文件。它会同步到合适位置，继续尝试解析后面的 `return x;`。

## 17. LL(1) 实验模块和主线 parser 的关系

`parser/ll1/grammar.cppm` 保存的是实验二需要的通用文法分析能力：

- `grammar_definition`
- `grammar_production`
- FIRST 集
- FOLLOW 集
- SELECT 集
- LL(1) 冲突判断

它的作用不是替代主线 `parser.syntax`，而是让实验要求中的“构造 CFG、消除左递归和左公因子、计算 SELECT、判断 LL(1)”有代码落点。

主线 parser：

```text
token -> AST
```

LL(1) 实验模块：

```text
grammar -> FIRST/FOLLOW/SELECT/conflicts
```

两者关系：

| 模块 | 更像什么 |
|---|---|
| `parser.syntax` | 真正编译器前端 |
| `parser.ll1` | 实验二理论验证器 |

所以阅读顺序上，建议先读 `parser.syntax`，知道实际语言怎么被解析；再读 `parser.ll1`，理解这些结构如何满足实验要求。

## 19. 算符优先模块：它不是主线，但能解释表达式优先级

实验三要求算符优先分析。仓库里是：

```text
parser/op/operator_precedence.cppm
```

它做的是：

1. 输入文法。
2. 判断是否为算符文法。
3. 计算 FIRSTVT / LASTVT。
4. 构造算符优先矩阵。
5. 用 shift-reduce 方式分析输入。

它和主线 Pratt parser 的共同点在这里：

```text
parser/expression/precedence.cppm
```

`cp_binary_operator_table` 是二元运算符优先级的事实源。主线 Pratt 用它判断绑定力；算符优先实验用它生成表达式文法和矩阵。

对应关系：

| 理论 | 主线代码 | 实验代码 |
|---|---|---|
| 运算符优先级 | Pratt binding power | 算符优先矩阵 |
| 移进/归约 | 递归何时继续/返回 | shift/reduce |
| 表达式合法性 | 构造 AST | 接受/拒绝 token 串 |

运行：

```bash
./build/parser/op_precedence_demo "a + b * c"
./build/parser/op_precedence_demo "a + b * c" --trace
```

如果你想从理论上理解 `a + b * c` 为什么不是 `(a + b) * c`，可以先看 `op_precedence_demo` 的 shift/reduce trace；再回头看 `parse_expression_pratt()`，就会更顺。

## 20. LR 模块现在只是边界

`parser/lr/README.md` 目前是预留边界，不是已经接入的主线实现。

这和实验要求里的“实验四 LR”对应，但当前仓库还没有完整 LR(0)/SLR(1)/LR(1)/LALR(1) 实现。

理解上可以这样放：

```text
CHP-04 -> 解释当前主线递归下降 parser
CHP-06 -> 解释未来 LR 和当前 op 算符优先实验
```

不要用 LR 的状态机思路硬读 `recursive_descent.cppm`。这个文件是自顶向下，不是自底向上。

## 21. 语义分析：AST 之后才是属性文法

`CHP-07` 讲属性文法、综合属性、继承属性、语法制导翻译。

在当前代码里，parser 只做语法结构，不做类型判断。例如：

```cp
let x: i32 = true;
```

parser 可能认为它语法合法，因为结构是：

```text
声明语句 -> let identifier : Type = Expression ;
```

但类型上不一定合法。这个要交给后续 semantic：

```text
AST -> symbol table / type table / semantic diagnostics / IR
```

这也是为什么 AST 节点里只保存：

- `source_span`
- 子节点 id
- 运算符 token kind
- 是否 `const` / 是否 `exported`

而没有直接保存：

- 变量最终类型
- 符号解析结果
- 函数重载结果
- 常量折叠结果

这些更像属性文法里的属性，适合放在后续 side table 中，而不是塞进 parser 的 syntax node。

## 22. 推荐阅读顺序

### 第一步：只看 parser 总入口

读：

- `parser/README.md`
- `parser/docs/parser_pipeline.md`
- `parser/parser.cppm`
- `parser/syntax/recursive_descent.cppm` 文件末尾的 `parse_translation_unit()`

目标：

```text
知道 parser 接收什么、返回什么、主线从哪里开始。
```

不要急着读表达式，也不要急着读 LL(1) 实验模块。

### 第二步：读顶层和函数

读：

- `parse_translation_unit_node()`
- `parse_module_header()`
- `parse_import_declaration()`
- `parse_function()`
- `parse_parameter()`

配合例子：

- `design/examples/math.cp`
- `design/examples/main.cp`

目标：

```text
能把 export module、import、function definition 对应到 AST。
```

### 第三步：读语句

读：

- `parse_statement()`
- `parse_block_statement()`
- `parse_declaration_statement()`
- `parse_if_statement()`
- `parse_while_statement()`
- `parse_do_while_statement()`
- `parse_for_statement()`
- `parse_return_statement()`

配合例子：

- `design/examples/flow_demo.cp`

目标：

```text
看到 switch(peek().kind) 时，能想到 FIRST/SELECT 集。
```

### 第四步：读类型

读：

- `parse_type()`
- `parse_type_argument()`
- `expect_identifier("type name")`
- `expect_closing_angle()`

配合例子：

- `design/examples/types_demo.cp`

目标：

```text
理解类型名、泛型参数、const 最终目标标记、引用/指针后缀如何落到 type_syntax。
```

### 第五步：读表达式

读：

- `parser/expression/precedence.cppm`
- `parse_expression()`
- `parse_assignment()`
- `parse_expression_pratt()`
- `parse_unary()`
- `parse_postfix()`
- `parse_primary()`

配合例子：

```cp
a + b * c
x = y = 1
foo(a, b + c)
(x)
(x, y)
value as i32
```

目标：

```text
理解优先级、结合性、调用、前后缀、括号/元组如何构造 expr_syntax。
```

### 第六步：读 AST

读：

- `parser/ast/ids.cppm`
- `parser/ast/arena.cppm`
- `parser/ast/expr.cppm`
- `parser/ast/stmt.cppm`
- `parser/ast/item.cppm`
- `parser/ast/type.cppm`

目标：

```text
理解 syntax node 存储结构：variant 节点 + typed id + arena。
```

### 第七步：读实验模块

读：

- `parser/docs/ll1_experiment.md`
- `parser/ll1/grammar.cppm`
- `parser/docs/op_precedence_experiment.md`
- `parser/op/operator_precedence.cppm`

目标：

```text
知道实验二/三怎么和理论报告对应，但不要把它们误认为主线 parser。
```

## 23. 用一个例子串起全流程

以 `flow_demo.cp` 中的片段为例：

```cp
export sum_non_negative(values: array<i32,4>) -> i32
{
    let total: i32 = 0;

    for(let value : values) {
        if(value < 0) {
            continue;
        }

        total = total + value;
    }

    return total;
}
```

高层解析路线：

```text
parse_translation_unit_node
-> parse_function
   -> parse_parameter
      -> parse_type
   -> parse_type                 返回类型 i32
   -> parse_block_statement
      -> parse_declaration_statement
      -> parse_for_statement
         -> parse_expression     values
         -> parse_block_statement
            -> parse_if_statement
               -> parse_expression value < 0
               -> parse_block_statement
                  -> parse_break_continue_statement(false)
            -> parse_expression_statement
               -> parse_assignment total = total + value
      -> parse_return_statement
```

最终 AST 大致是：

```text
translation_unit
└── function sum_non_negative
    ├── parameter values: array<i32, 4>
    ├── return_type i32
    └── body block
        ├── declaration total: i32 = literal(0)
        ├── for value in name(values)
        │   └── block
        │       ├── if binary(value < 0)
        │       │   └── block
        │       │       └── continue
        │       └── expr_stmt
        │           └── assignment(total = binary(total + value))
        └── return name(total)
```

注意这里不是 parser_demo 一定会按这个 ASCII 树打印，而是帮助你在脑中建立 AST 形状。

## 24. 理论到代码速查表

| 理论名词 | 你在代码里看什么 |
|---|---|
| 终结符 | `token_kind` |
| 非终结符 | `parse_xxx()` 函数，或 AST 结构名 |
| 开始符号 | `parse_translation_unit_node()` |
| 产生式 | 某个 `parse_xxx()` 内部的顺序逻辑 |
| FIRST 集 | `switch(peek().kind)` / `starts_expression()` |
| FOLLOW / 同步集合 | `synchronize_statement()` / `synchronize_top_level()` |
| 匹配终结符 | `expect()` / `consume()` |
| 左公因子 | `peek(1)` 分流，或先解析共同前缀再判断后续 token |
| 左递归 | 表达式不用直接递归，改用 Pratt / 实验 LL(1) 中消除 |
| LL(1) 判断 | `parser/ll1/grammar.cppm::analyze_grammar()` |
| 算符优先 | `parser/op/operator_precedence.cppm` |
| 优先级和结合性 | `parser/expression/precedence.cppm` |
| 分析树/AST | `parser/ast/*` |
| 语义属性 | 后续 semantic side table，而不是 parser AST 本体 |
| 语法错误 | `parser_diagnostic` |

## 25. 最小实践任务

按下面顺序练，比直接通读代码更不容易卡住。

### 任务 1：跑一次 parser demo

```bash
cmake --build build --target parser_demo
./build/parser/parser_demo design/examples/flow_demo.cp
```

观察：

- 是否 accepted
- 是否输出 lexer/parser diagnostic
- AST 行为是否和对应测试一致

### 任务 2：手工对应一个函数

选 `design/examples/math.cp` 中一个函数，手写三行：

```text
Function -> export? identifier (...) ReturnTypeOpt Block
Block -> { Statement* }
ReturnStmt -> return Expression ;
```

然后对照 `parse_function()`、`parse_block_statement()`、`parse_return_statement()`。

目标不是写得完整，而是练“文法和函数调用顺序一一对应”。

### 任务 3：解释一个表达式

用：

```cp
total = total + value * 2;
```

自己画出：

```text
assignment
├── left: total
└── right: binary(+)
    ├── total
    └── binary(*)
        ├── value
        └── 2
```

然后在 `parse_assignment()` 和 `parse_expression_pratt()` 中找对应构造位置。

### 任务 4：看一个错误如何恢复

构造一个缺分号文件：

```cp
export bad() -> i32
{
    let x = 1
    return x;
}
```

运行 parser_demo，看诊断和 trace。重点观察 parser 是否尝试继续解析后续语句。

## 26. 读代码时的常见卡点

### 卡点 1：为什么 `std::optional<id>` 到处都是

因为解析可能失败。失败时返回 `std::nullopt`，调用方决定报错、同步还是继续。

这对应理论里的“当前输入不能由该非终结符推出”。

### 卡点 2：为什么 AST 节点不保存文本

多数节点保存 `source_span`。真实文本可以从 `source_manager` 取。这样 AST 更轻，也不会复制大量字符串。

### 卡点 3：为什么 `parse_statement()` 里有 `starts_expression()`

表达式语句的 FIRST 集不像 `if`、`while` 那样只有一个关键字。它可能以 identifier、literal、`(`、`[`、一元运算符等开始，所以单独写成函数。

### 卡点 4：为什么 `{ ... }` 不能随便作为表达式语句开头

在语句位置，`{` 已经表示 block。如果允许 `{1,2,3};` 作为表达式语句开头，就会和 block 的 FIRST 集冲突。实验 LL(1) 文档也把“以序列字面量开头的独立表达式语句”排除在 2.1 文法外。

### 卡点 5：为什么 `kw_as` 在 Pratt 表里，却不进算符优先实验文法

`as` 的右边是 Type，不是 Expression：

```cp
x as i32
```

Pratt 可以在主线里特殊处理：

```cpp
if(op_kind == token_kind::kw_as) {
    parse_type();
}
```

但算符优先实验文法主要处理表达式二元算符，所以它跳过 `as`。

## 27. 你最终应该形成的心智模型

读 `parser/syntax/recursive_descent.cppm` 时，按这个模型看：

```text
parse_xxx() 是一个非终结符。
expect/consume 是匹配终结符。
switch(peek()) 是 FIRST/SELECT 决策。
while(...) 是 A* 或列表产生式。
std::optional 是解析失败通道。
arena_.add(...) 是构造 AST 节点。
id 字段是 AST 边。
diagnostics 是语法错误。
```

这样你再看任何一个函数，都可以问同一组问题：

1. 这个函数对应哪个非终结符？
2. 它会匹配哪些终结符？
3. 它调用哪些子非终结符？
4. 它成功后构造什么 AST 节点？
5. 它失败后如何报错或同步？

只要能回答这五个问题，就说明你已经把编译原理理论和当前 parser 代码接上了。
