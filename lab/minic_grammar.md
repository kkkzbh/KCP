# miniC 超轻量级文法

目标：保留 miniC 的函数、变量、分支、循环、返回和整数表达式，去掉数组、指针、结构体、全局变量、函数声明和复杂类型，方便后续分别实现词法分析、递归下降 LL(1)、LR(1)、语义分析和四元式生成。

## 实验路线

本次实验选择 miniC 子集作为目标语言，按多遍编译结构组织：

```text
源程序字符流
  -> 词法分析
  -> token 序列
  -> LL(1) 递归下降语法分析
  -> AST
  -> 语义分析
  -> 四元式
```

同时单独实现一条 LR(1) 语法分析路线，用于实验四展示项目集规范族、DFA、ACTION/GOTO 表和分析过程：

```text
token 序列
  -> LR(1) 项目集规范族
  -> LR(1) DFA
  -> ACTION/GOTO 表
  -> 移进/规约分析过程
```

因此，LL(1) 递归下降是语义分析和中间代码生成的主入口；LR(1) 是独立语法分析实验路线，不承担后续语义分析入口。

## 模块边界

本文档同时给出词法规则、语法规则和语义约束，但它们属于不同实验模块：

- 词法分析模块使用“词法单元”一节。该节描述的是正规文法/正则表达式，输入是源程序字符流，输出是 token 序列。
- 语法分析模块使用“语法记号”“程序结构”“语句”“表达式”几节。这里描述的是上下文无关文法，输入是词法分析模块产生的 token 序列，输出是语法树或分析过程。
- 语义分析模块使用各节中的“约束”。这些规则不靠词法分析或语法分析单独完成，而是在语义分析阶段检查，例如函数是否存在、`void` 是否误用、返回值是否匹配。
- 中间代码模块使用语义分析后的表达式和语句结构，生成四元式或其他中间代码。

因此，实验一报告里写“描述词法的正规文法或正则表达式”时，只引用词法规则；实验二、三、四报告里写文法分析时，只引用语法规则；实验五再引用语义约束和中间代码规则。

## 目标文法与上下文无关文法

本文档的目标文法是 miniC 子集文法。它定义本次实验要识别和处理的源程序集合，包括函数定义、局部变量、分支、循环、返回语句和整数表达式。

目标文法分为两层：

- 词法层：使用正则表达式描述关键字、标识符、整数、运算符、界符、空白和注释。该层属于正规文法范围，用于实验一。
- 语法层：使用上下文无关文法描述 token 序列如何组成函数、语句和表达式。该层用于实验二的 LL(1) 递归下降分析和实验四的 LR(1) 分析。

因此，实验要求中的“构造该语言的上下文无关文法”指的是本文档“程序结构”“语句”“表达式”三节中的 EBNF 规则；“描述词法的正规文法或正则表达式”指的是“词法单元”和“词法理论说明”两节。

当前文法已经按递归下降 LL(1) 的需要改写：

- 表达式文法已经消除左递归。
- 标识符开头的语句已经提取左公因子。
- 函数调用和普通标识符表达式已经提取左公因子。
- `if` 和 `while` 主体强制使用块，避免悬挂 `else` 二义性。

## 词法单元：lexer 模块

关键字：

```text
int void if else while return
```

标识符：

```text
identifier = [A-Za-z_][A-Za-z0-9_]*
```

整数：

```text
integer = 0 | [1-9][0-9]*
```

界符和运算符：

```text
( ) { } , ; + - * / % = == != < <= > >= && ||
```

空白和注释：

- 空白字符只分隔 token。
- 支持 `// line comment`。
- 支持 `/* block comment */`。

词法分析模块不判断 token 序列是否满足语法。例如 `int if return` 都可以被切成合法 token，但是否构成合法程序交给语法分析模块判断。

## 词法理论说明

词法分析属于有限自动机能够处理的范围。关键字、标识符、整数、界符、运算符、空白和注释都可以用正规文法或正则表达式描述，再转换为 NFA、DFA，并可进一步最小化。

本 miniC 子集的词法分类如下：

| 类别 | 正则描述 | token 类型 |
|---|---|---|
| 关键字 | `int|void|if|else|while|return` | 对应关键字 token |
| 标识符 | `[A-Za-z_][A-Za-z0-9_]*` | `identifier` |
| 整数 | `0|[1-9][0-9]*` | `integer` |
| 单字符界符 | `[(){},;]` | 对应界符 token |
| 单字符运算符 | `[+\-*/%=<>]` | 对应运算符 token |
| 双字符运算符 | `==|!=|<=|>=|&&|\|\|` | 对应运算符 token |
| 行注释 | `//[^\n]*` | 跳过 |
| 块注释 | `/* ... */` | 跳过 |
| 空白 | 空格、制表符、换行 | 跳过或作为 token 分隔 |

关键字和标识符的识别采用最长匹配规则。先按标识符正则读出最长词素，再查关键字表；如果词素等于某个关键字，则输出关键字 token，否则输出 `identifier`。

运算符也采用最长匹配规则。例如读到 `=` 时，需要向前看一个字符；若下一个字符仍是 `=`，则输出 `==`，否则输出 `=`。`!=`、`<=`、`>=`、`&&`、`||` 同理。

词法错误属于局部字符错误，例如未知字符、非法数字形式、未闭合块注释。词法分析只报告这些字符层面的错误，不负责报告语句结构错误。

## 语法记号：parser 模块

- 终结符用字面量表示，例如 `"int"`、`"("`、`identifier`。
- `ε` 表示空串。
- 表达式文法已消除左递归，适合递归下降实现。
- 这里的 `identifier`、`integer`、关键字、界符和运算符都是词法分析模块已经输出的 token 类型，不再直接处理字符。

## 程序结构：parser 模块

```ebnf
Program         -> FunctionList EOF
FunctionList    -> Function FunctionList | ε

Function        -> ReturnType identifier "(" ParamListOpt ")" Block
ReturnType      -> "int" | "void"

ParamListOpt    -> ParamList | ε
ParamList       -> Param ParamListTail
ParamListTail   -> "," Param ParamListTail | ε
Param           -> "int" identifier
```

约束：

- 程序由零个或多个函数组成。
- `void` 只允许作为函数返回类型，不作为变量或参数类型。
- 函数没有前置声明，调用是否存在交给语义分析检查。

## 语句：parser 模块

```ebnf
Block           -> "{" StmtList "}"
StmtList        -> Stmt StmtList | ε

Stmt            -> VarDecl
                 | IdentifierStmt
                 | IfStmt
                 | WhileStmt
                 | ReturnStmt
                 | Block

VarDecl         -> "int" identifier VarInitOpt ";"
VarInitOpt      -> "=" Expr | ε

IdentifierStmt  -> identifier IdentifierStmtTail
IdentifierStmtTail
                -> "=" Expr ";"
                 | "(" ArgListOpt ")" ";"

IfStmt          -> "if" "(" Expr ")" Block ElseOpt
ElseOpt         -> "else" Block | ε

WhileStmt       -> "while" "(" Expr ")" Block

ReturnStmt      -> "return" ReturnValueOpt ";"
ReturnValueOpt  -> Expr | ε
```

约束：

- `if` 和 `while` 的主体强制使用 `{ ... }`，避免悬挂 `else`。
- 赋值语句只允许 `identifier = Expr;`，不把赋值放进普通表达式。
- 普通表达式不能单独作为语句；表达式语句第一版只保留函数调用 `f(...);`。
- 空语句暂不支持；如果需要，可以后续加入 `Stmt -> ";"`。

## 表达式：parser 模块

```ebnf
Expr            -> LogicalOr

LogicalOr       -> LogicalAnd LogicalOrTail
LogicalOrTail   -> "||" LogicalAnd LogicalOrTail | ε

LogicalAnd      -> Equality LogicalAndTail
LogicalAndTail  -> "&&" Equality LogicalAndTail | ε

Equality        -> Relational EqualityTail
EqualityTail    -> "==" Relational EqualityTail
                 | "!=" Relational EqualityTail
                 | ε

Relational      -> Additive RelationalTail
RelationalTail  -> "<" Additive RelationalTail
                 | "<=" Additive RelationalTail
                 | ">" Additive RelationalTail
                 | ">=" Additive RelationalTail
                 | ε

Additive        -> Multiplicative AdditiveTail
AdditiveTail    -> "+" Multiplicative AdditiveTail
                 | "-" Multiplicative AdditiveTail
                 | ε

Multiplicative  -> Unary MultiplicativeTail
MultiplicativeTail
                -> "*" Unary MultiplicativeTail
                 | "/" Unary MultiplicativeTail
                 | "%" Unary MultiplicativeTail
                 | ε

Unary           -> "+" Unary
                 | "-" Unary
                 | Primary

Primary         -> integer
                 | identifier PrimarySuffix
                 | "(" Expr ")"

PrimarySuffix   -> CallSuffix | ε
CallSuffix      -> "(" ArgListOpt ")"

ArgListOpt      -> ArgList | ε
ArgList         -> Expr ArgListTail
ArgListTail     -> "," Expr ArgListTail | ε
```

优先级从高到低：

| 优先级 | 运算符 | 结合性 |
|---|---|---|
| 1 | 函数调用 `f(...)` | 左结合 |
| 2 | 一元 `+` `-` | 右结合 |
| 3 | `*` `/` `%` | 左结合 |
| 4 | `+` `-` | 左结合 |
| 5 | `<` `<=` `>` `>=` | 左结合 |
| 6 | `==` `!=` | 左结合 |
| 7 | `&&` | 左结合 |
| 8 | `||` | 左结合 |

约束：

- 所有表达式类型第一版都按 `int` 处理。
- 关系运算和逻辑运算结果也是 `int`，`0` 表示 false，非 `0` 表示 true。
- 函数调用只支持普通标识符调用，不支持函数指针和成员访问。

## LL(1) 实现边界

这套文法刻意做了左因子提取，方便写成严格的递归下降 LL(1) 版本：

- `Stmt` 看到 `"int"` 进入变量声明。
- `Stmt` 看到 `"if"`、`"while"`、`"return"`、`"{"` 分别进入对应语句。
- `Stmt` 看到 `identifier` 进入 `IdentifierStmt`，再由 `IdentifierStmtTail` 用一个 lookahead 区分：
  - `"="`：赋值语句。
  - `"("`：函数调用语句。

因此，严格写实验二报告时可以说明：本 miniC 子集通过消除左递归和提取左因子，使语句和表达式主体适合 LL(1) 递归下降实现。

## 左递归与左公因子处理

原始表达式文法常写成左递归形式：

```text
Expr -> Expr "+" Term | Term
Term -> Term "*" Factor | Factor
```

这种形式不适合递归下降，因为 `Expr` 会在没有消耗任何 token 的情况下再次调用 `Expr`，导致无限递归。本文档把表达式按优先级拆成多层，并用 tail 非终结符消除左递归：

```text
Additive     -> Multiplicative AdditiveTail
AdditiveTail -> "+" Multiplicative AdditiveTail
              | "-" Multiplicative AdditiveTail
              | ε
```

乘法、关系、相等、逻辑与、逻辑或各层使用同样方式处理。

左公因子方面，语句和表达式中最容易冲突的是 `identifier` 开头的结构：

```text
Stmt -> identifier "=" Expr ";"
      | identifier "(" ArgListOpt ")" ";"
```

提取左公因子后写成：

```text
IdentifierStmt     -> identifier IdentifierStmtTail
IdentifierStmtTail -> "=" Expr ";"
                    | "(" ArgListOpt ")" ";"
```

表达式中的标识符也同理：

```text
Primary       -> identifier PrimarySuffix
PrimarySuffix -> CallSuffix | ε
CallSuffix    -> "(" ArgListOpt ")"
```

这样递归下降程序在看到 `identifier` 后，只需要再看一个 token：`=` 表示赋值语句，`(` 表示函数调用，其他可跟随表达式的 token 表示普通变量引用。

## LL(1) 理论说明

LL(1) 的含义是：从左到右扫描输入，构造最左推导，并且每次只用 1 个 lookahead token 选择产生式。

递归下降版本把每个非终结符实现为一个分析子程序：

| 非终结符 | 递归下降子程序职责 |
|---|---|
| `Program` | 分析完整程序，并检查最终到达 EOF |
| `Function` | 分析返回类型、函数名、参数列表和函数体 |
| `Block` | 分析 `{ ... }` 包围的语句列表 |
| `Stmt` | 根据 lookahead 分派到变量声明、赋值、调用、分支、循环、返回或块 |
| `Expr` | 分析表达式入口 |
| `Primary` | 分析整数、标识符、函数调用和括号表达式 |

FIRST 集用于判断一个非终结符可能以哪些 token 开头。当前文法中最关键的 FIRST 集如下：

| 非终结符 | FIRST 集 |
|---|---|
| `Function` | `{ "int", "void" }` |
| `Block` | `{ "{" }` |
| `Stmt` | `{ "int", identifier, "if", "while", "return", "{" }` |
| `VarDecl` | `{ "int" }` |
| `IdentifierStmt` | `{ identifier }` |
| `IfStmt` | `{ "if" }` |
| `WhileStmt` | `{ "while" }` |
| `ReturnStmt` | `{ "return" }` |
| `Expr` | `{ integer, identifier, "(", "+", "-" }` |

FOLLOW 集用于处理可空产生式。例如 `StmtList -> Stmt StmtList | ε` 中，`StmtList` 可以为空；当 lookahead 是 `"}"` 时，表示当前块的语句列表结束，应选择空产生式。

关键 FOLLOW 集如下：

| 非终结符 | FOLLOW 集 |
|---|---|
| `FunctionList` | `{ EOF }` |
| `ParamListOpt` | `{ ")" }` |
| `ParamListTail` | `{ ")" }` |
| `StmtList` | `{ "}" }` |
| `VarInitOpt` | `{ ";" }` |
| `IdentifierStmtTail` | 由语句内部直接消费 `";"` |
| `ElseOpt` | `{ "int", identifier, "if", "while", "return", "{", "}" }` |
| `ReturnValueOpt` | `{ ";" }` |
| `Expr` | `{ ")", ";", "," }` |
| `PrimarySuffix` | `{ "*", "/", "%", "+", "-", "<", "<=", ">", ">=", "==", "!=", "&&", "||", ")", ";", "," }` |

SELECT 集用于判断某个产生式在当前 lookahead 下是否应被选择。公式为：

```text
SELECT(A -> α) = FIRST(α)                         当 α 不能推出 ε
SELECT(A -> α) = FIRST(α) - {ε} ∪ FOLLOW(A)       当 α 能推出 ε
```

主要产生式的 SELECT 集如下：

| 产生式 | SELECT 集 |
|---|---|
| `FunctionList -> Function FunctionList` | `{ "int", "void" }` |
| `FunctionList -> ε` | `{ EOF }` |
| `ReturnType -> "int"` | `{ "int" }` |
| `ReturnType -> "void"` | `{ "void" }` |
| `ParamListOpt -> ParamList` | `{ "int" }` |
| `ParamListOpt -> ε` | `{ ")" }` |
| `ParamListTail -> "," Param ParamListTail` | `{ "," }` |
| `ParamListTail -> ε` | `{ ")" }` |
| `StmtList -> Stmt StmtList` | `{ "int", identifier, "if", "while", "return", "{" }` |
| `StmtList -> ε` | `{ "}" }` |
| `Stmt -> VarDecl` | `{ "int" }` |
| `Stmt -> IdentifierStmt` | `{ identifier }` |
| `Stmt -> IfStmt` | `{ "if" }` |
| `Stmt -> WhileStmt` | `{ "while" }` |
| `Stmt -> ReturnStmt` | `{ "return" }` |
| `Stmt -> Block` | `{ "{" }` |
| `VarInitOpt -> "=" Expr` | `{ "=" }` |
| `VarInitOpt -> ε` | `{ ";" }` |
| `IdentifierStmtTail -> "=" Expr ";"` | `{ "=" }` |
| `IdentifierStmtTail -> "(" ArgListOpt ")" ";"` | `{ "(" }` |
| `ElseOpt -> "else" Block` | `{ "else" }` |
| `ElseOpt -> ε` | `{ "int", identifier, "if", "while", "return", "{", "}" }` |
| `ReturnValueOpt -> Expr` | `{ integer, identifier, "(", "+", "-" }` |
| `ReturnValueOpt -> ε` | `{ ";" }` |
| `Primary -> integer` | `{ integer }` |
| `Primary -> identifier PrimarySuffix` | `{ identifier }` |
| `Primary -> "(" Expr ")"` | `{ "(" }` |
| `PrimarySuffix -> CallSuffix` | `{ "(" }` |
| `PrimarySuffix -> ε` | `{ "*", "/", "%", "+", "-", "<", "<=", ">", ">=", "==", "!=", "&&", "||", ")", ";", "," }` |
| `ArgListOpt -> ArgList` | `{ integer, identifier, "(", "+", "-" }` |
| `ArgListOpt -> ε` | `{ ")" }` |
| `ArgListTail -> "," Expr ArgListTail` | `{ "," }` |
| `ArgListTail -> ε` | `{ ")" }` |

表达式各层 tail 产生式的 SELECT 集如下：

| 产生式 | SELECT 集 |
|---|---|
| `LogicalOrTail -> "||" LogicalAnd LogicalOrTail` | `{ "||" }` |
| `LogicalOrTail -> ε` | `{ ")", ";", "," }` |
| `LogicalAndTail -> "&&" Equality LogicalAndTail` | `{ "&&" }` |
| `LogicalAndTail -> ε` | `{ "||", ")", ";", "," }` |
| `EqualityTail -> "==" Relational EqualityTail` | `{ "==" }` |
| `EqualityTail -> "!=" Relational EqualityTail` | `{ "!=" }` |
| `EqualityTail -> ε` | `{ "&&", "||", ")", ";", "," }` |
| `RelationalTail -> "<" Additive RelationalTail` | `{ "<" }` |
| `RelationalTail -> "<=" Additive RelationalTail` | `{ "<=" }` |
| `RelationalTail -> ">" Additive RelationalTail` | `{ ">" }` |
| `RelationalTail -> ">=" Additive RelationalTail` | `{ ">=" }` |
| `RelationalTail -> ε` | `{ "==", "!=", "&&", "||", ")", ";", "," }` |
| `AdditiveTail -> "+" Multiplicative AdditiveTail` | `{ "+" }` |
| `AdditiveTail -> "-" Multiplicative AdditiveTail` | `{ "-" }` |
| `AdditiveTail -> ε` | `{ "<", "<=", ">", ">=", "==", "!=", "&&", "||", ")", ";", "," }` |
| `MultiplicativeTail -> "*" Unary MultiplicativeTail` | `{ "*" }` |
| `MultiplicativeTail -> "/" Unary MultiplicativeTail` | `{ "/" }` |
| `MultiplicativeTail -> "%" Unary MultiplicativeTail` | `{ "%" }` |
| `MultiplicativeTail -> ε` | `{ "+", "-", "<", "<=", ">", ">=", "==", "!=", "&&", "||", ")", ";", "," }` |
| `Unary -> "+" Unary` | `{ "+" }` |
| `Unary -> "-" Unary` | `{ "-" }` |
| `Unary -> Primary` | `{ integer, identifier, "(" }` |

从 SELECT 集可以看出，同一非终结符的不同产生式 SELECT 集互不相交，所以该文法满足 LL(1) 递归下降分析要求。

本 miniC 子集满足递归下降 LL(1) 的关键原因：

- `Stmt` 各候选分支的 FIRST 集互不冲突。
- 标识符开头的语句统一进入 `IdentifierStmt`，再通过下一个 token 区分赋值和函数调用。
- `if` 主体强制使用块，避免悬挂 `else` 造成的二义性。
- 表达式按优先级拆成多层非终结符，消除了直接左递归。
- 空产生式只在明确的结束 token 上触发，例如 `")"`、`"}"`、`";"`、`,` 或 EOF。

递归下降分析过程可以在报告中写成“进入非终结符、匹配终结符、退出非终结符”的轨迹。对于一个函数：

```c
int add(int a, int b) {
    return a + b;
}
```

分析过程的高层结构是：

```text
Program
  Function
    ReturnType -> "int"
    identifier -> add
    ParamList -> int a, int b
    Block
      ReturnStmt
        Expr -> a + b
  EOF
```

## LR(1) 理论说明

LR(1) 的含义是：从左到右扫描输入，构造最右推导的逆过程，并且每个项目带 1 个向前看符号。相比 LL(1)，LR(1) 更适合自底向上分析，能够处理更广泛的上下文无关文法。

LR(1) 项目的形式为：

```text
[A -> α . β, a]
```

含义是：当前正在识别产生式 `A -> α β`，点号左边 `α` 已经识别完成，点号右边 `β` 还未识别，`a` 是该规约可接受的 lookahead。

LR(1) 分析需要构造以下对象：

- 增广文法：加入 `S' -> Program EOF` 作为唯一开始产生式。
- 项目闭包 `closure(I)`：如果项目中点号后是非终结符，则加入该非终结符的相关产生式项目，并计算对应 lookahead。
- 转移函数 `goto(I, X)`：项目集 `I` 在文法符号 `X` 上移动点号后形成的新项目集。
- 项目集规范族：从初始项目集开始，反复计算 `closure` 和 `goto` 得到所有状态。
- DFA：每个项目集是一个状态，`goto` 是状态转移。
- ACTION/GOTO 表：终结符列进入 ACTION，非终结符列进入 GOTO。

实验四要求中的三步可以对应为：

### 计算项目集规范族

先把文法增广为：

```text
S' -> Program EOF
```

初始 LR(1) 项目为：

```text
[S' -> . Program EOF, EOF]
```

对初始项目求闭包得到初始项目集 `I0`。然后对每个项目集 `I` 和每个可能的文法符号 `X` 计算 `goto(I, X)`；如果得到的新项目集非空且还没有出现过，就加入项目集规范族。反复执行，直到不再产生新项目集。

LR(1) 闭包中 lookahead 的传播规则是：若项目为

```text
[A -> α . B β, a]
```

则对 `B` 的每个产生式 `B -> γ`，加入：

```text
[B -> . γ, b]
```

其中 `b` 属于 `FIRST(β a)`。这就是 LR(1) 比 LR(0) 更精确的地方：项目不仅记录点号位置，还记录该项目未来允许规约的向前看符号。

### 构造 LR(0) 的 DFA

LR(0) 项目忽略 lookahead，形式为：

```text
A -> α . β
```

LR(0) DFA 的状态是 LR(0) 项目集，边由 `goto(I, X)` 给出。构造步骤和 LR(1) 类似：

- 从增广项目 `S' -> . Program EOF` 的闭包开始。
- 对每个项目集和每个文法符号计算 `goto`。
- 每个不同项目集对应 DFA 的一个状态。
- `goto(I, X) = J` 对应 DFA 中一条从 `I` 到 `J`、标号为 `X` 的边。

该 DFA 描述的是“点号在产生式右部如何移动”。当边标号是终结符时，对应输入 token 的移进；当边标号是非终结符时，对应规约后查 GOTO 表进入的新状态。

### 改造 DFA 适合 LR(1)

LR(1) 可以理解为在 LR(0) DFA 的核心结构上附加 lookahead 信息：

- LR(0) core 相同：产生式和点号位置相同。
- LR(1) item 额外带 lookahead：`[A -> α . β, a]`。
- 规约动作不再对所有终结符都有效，只在对应 lookahead 上有效。

构造 ACTION/GOTO 表时：

- 若状态 `I` 中存在 `[A -> α . t β, a]`，且 `t` 是终结符，`goto(I, t) = J`，则 `ACTION[I, t] = shift J`。
- 若状态 `I` 中存在 `[A -> α ., a]`，且 `A` 不是增广开始符号，则 `ACTION[I, a] = reduce A -> α`。
- 若状态 `I` 中存在 `[S' -> Program EOF ., EOF]`，则 `ACTION[I, EOF] = accept`。
- 若 `A` 是非终结符且 `goto(I, A) = J`，则 `GOTO[I, A] = J`。

如果同一个表格位置出现多个动作，则说明存在冲突：

- shift/reduce 冲突：同一位置既要移进又要规约。
- reduce/reduce 冲突：同一位置有多个可规约产生式。

无冲突时，该文法可按 LR(1) 方式分析。

### LALR(1) 说明

LALR(1) 是 LR(1) 的状态压缩形式。它把 LR(0) core 相同的 LR(1) 项目集合并，并把 lookahead 集合取并集。

例如：

```text
[A -> α ., ";"]
[A -> α ., ")"]
```

如果它们处在 LR(0) core 相同的状态中，LALR(1) 会合并成同一个 core 状态，并保留 lookahead 集 `{ ";", ")" }`。

LALR(1) 的状态数通常接近 LR(0)，比完整 LR(1) 少；但合并 lookahead 后可能引入新的 reduce/reduce 冲突。因此能力关系是：

```text
SLR(1) < LALR(1) < LR(1)
```

本实验选择实现 LR(1)，报告中可以说明：如果要改造成 LALR(1)，就在 LR(1) 项目集规范族构造完成后，合并 LR(0) core 相同的状态，并重新生成 ACTION/GOTO 表。

LR(1) 中 lookahead 的理论作用是限制规约发生的条件。对于同一个核心项目：

```text
[A -> α ., ";"]
[A -> α ., ")"]
```

它们的点号位置和产生式相同，但 lookahead 不同，因此在 LR(1) 中是不同项目。只有输入符号匹配对应 lookahead 时才允许规约，这比 LR(0) 和 SLR(1) 更精确。

分析表动作分为四类：

| 动作 | 含义 |
|---|---|
| shift | 当前 token 可以移入状态栈 |
| reduce | 根据某条产生式规约 |
| accept | 输入被完整接受 |
| error | 当前状态和 token 组合不合法 |

LR(1) 分析栈通常保存状态栈和符号栈。每一步根据栈顶状态和当前输入 token 查询 ACTION 表：

- shift：把 token 和新状态压栈，输入前进。
- reduce：弹出产生式右部长度对应的符号和状态，再根据左部非终结符查 GOTO 表压入新状态。
- accept：分析成功。
- error：分析失败并输出错误位置。

本实验中 LR(1) 路线用于展示自底向上语法分析能力，可以输出项目集、DFA 边、ACTION/GOTO 表和移进/规约过程。语义分析不依赖 LR(1) 结果，而是使用 LL(1) 递归下降得到的 AST。

## 语义分析理论说明

语义分析接收 LL(1) 递归下降生成的 AST，检查上下文相关规则。语法分析只能判断结构是否符合文法，不能判断名字是否声明、函数参数是否匹配、返回值是否合理。

本 miniC 子集的语义对象包括：

- 函数表：记录函数名、返回类型、参数数量和参数类型。
- 作用域栈：进入函数体或块时压入新作用域，离开块时弹出。
- 变量表：记录当前作用域内的局部变量名和类型。
- 表达式类型：第一版所有表达式都归为 `int`。

需要检查的语义规则：

- 函数不能重复定义。
- 函数调用的目标必须存在。
- 函数调用实参数量必须和形参数量一致。
- 局部变量不能在同一作用域内重复定义。
- 使用变量前必须已经声明。
- 变量声明、赋值和表达式使用的类型必须一致。
- `void` 不能作为变量或参数类型。
- `int` 函数应返回表达式。
- `void` 函数可以 `return;`，不能返回表达式。
- `main` 函数应存在，并作为程序入口。

对于作用域，内层块可以拥有自己的变量表。查找变量时从当前作用域向外层逐级查找；插入变量时只检查当前作用域是否重复。

语义错误示例：

```c
int main() {
    x = 1;
    return 0;
}
```

该程序词法和语法都合法，但 `x` 未声明，所以应在语义分析阶段报错。

## 四元式理论说明

四元式是常用的中间代码形式，每条指令由四个字段组成：

```text
(op, arg1, arg2, result)
```

表达式 `z = a + b * 2` 可以翻译为：

```text
(*, b, 2, t1)
(+, a, t1, t2)
(=, t2, _, z)
```

本 miniC 子集可以使用临时变量 `t1`、`t2` 表示中间结果。表达式翻译遵循语法树结构：先翻译子表达式，再为当前运算生成四元式。

控制流语句需要生成跳转四元式：

```text
if (cond) { then_block } else { else_block }
```

可翻译为：

```text
jnz cond, _, L_then
jmp _, _, L_else
label _, _, L_then
then_block
jmp _, _, L_end
label _, _, L_else
else_block
label _, _, L_end
```

`while` 语句可翻译为：

```text
label _, _, L_begin
cond_code
jz cond, _, L_end
body_code
jmp _, _, L_begin
label _, _, L_end
```

函数调用可以生成参数传递和调用四元式：

```text
param _, _, x
param _, _, y
call add, 2, t1
```

返回语句生成：

```text
ret value, _, _
```

实验五中可以同时输出四元式序列和表达式语义值。对于纯算术表达式，可以在语义分析阶段或四元式解释阶段计算结果；对于包含变量、函数和控制流的程序，则主要展示四元式生成过程。

## 最小样例

```c
int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 1;
    int y = 2;
    int z = add(x, y);

    if (z > 2) {
        z = z * 10;
    } else {
        z = 0;
    }

    while (z > 0) {
        z = z - 1;
    }

    return z;
}
```

## 固定范围

本实验实现不计划继续扩展语言特性。当前范围已经覆盖编译原理实验需要的核心结构：

- 变量：局部 `int` 声明、初始化和赋值。
- 分支：`if` / `else`。
- 循环：`while`。
- 函数：定义、参数、调用和返回。
- 表达式：整数、标识符、函数调用、算术、关系和逻辑运算。

非目标：

- 数组、指针、结构体。
- `for`、`break`、`continue`。
- 全局变量和函数声明。
- 多类型系统和隐式转换。
