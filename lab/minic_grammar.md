# miniC 超轻量级文法

目标：保留 miniC 的函数、标量变量、固定长度 `int` 数组、分支、循环、返回和整数表达式，去掉指针、结构体、全局变量、函数声明和复杂类型，使用规范 LR(1) 语法分析作为后续 AST、语义分析和四元式生成的主入口。

## 实验路线

本次实验选择 miniC 子集作为目标语言，按多遍编译结构组织：

```text
源程序字符流
  -> 词法分析
  -> token 序列
  -> LR(1) 项目集规范族
  -> LR(1) DFA
  -> ACTION/GOTO 表
  -> 移进/规约分析并同步生成 AST
  -> 语义分析
  -> 四元式
```

因此，LR(1) parser 是 miniC 的主线语法分析入口，不再把 LR(1) 放在独立展示目录中。语义分析和中间代码生成都接收 LR(1) 规约动作构造出来的 AST。

## 实现语法概览

当前实现的 miniC 语法围绕函数和块内语句展开。一个程序由若干函数定义组成，函数返回类型支持 `int` 和 `void`，参数支持 `int` 标量形参和按引用传递的 `int name[]` 数组形参，函数体使用 `{ ... }` 块包围。

语句支持局部 `int` 变量声明、固定长度 `int` 数组声明、数组初始化列表、逗号分隔的变量声明列表、标量赋值、数组元素赋值、函数调用语句、`if` / `else` 分支、`while` 循环、C 风格 `for` 循环、`break`、`continue`、`return` 语句和嵌套块。`if`、`while` 与 `for` 的主体都要求写成块，方便语法分析阶段直接处理结构边界。

表达式支持整数、变量引用、数组下标读取、函数调用、括号表达式、一元 `+` / `-`、算术运算、关系运算和逻辑运算。赋值不作为普通表达式的一部分，而是单独作为语句处理。

## 示例程序：lab/test.c

`lab/test.c` 是当前 miniC 子集的综合示例，完成一个常见的“成绩排序与统计”小任务，覆盖函数定义、数组形参、数组初始化、数组下标读写、函数调用、分支、`for` 循环、`break`、`continue`、返回和复合表达式：

```c
/* miniC score sorting and statistics sample */
void selection_sort(int values[], int n) {
    for (int i = 0; i < n; i = i + 1) {
        int min = i;
        for (int j = i + 1; j < n; j = j + 1) {
            if (values[j] < values[min]) {
                min = j;
            }
        }
        if (min != i) {
            int temp = values[i];
            values[i] = values[min];
            values[min] = temp;
        }
    }
}

int main() {
    int scores[7] = {72, 55, 88, 91, 66, 79, -1};
    selection_sort(scores, 6);
    return scores[3];
}
```

实际文件还包含 `count_passing` 和 `checksum` 两个函数：先排序前 6 个有效成绩，再用第 7 个负数哨兵演示 `break`，用低于 60 分的分支演示 `continue`，最后返回一个由排序结果、及格人数和校验值组合出的可验证整数。

## 词法单元：lexer 模块

关键字：

```text
int void if else while for break continue return
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
( ) { } [ ] , ; + - * / % = == != < <= > >= && ||
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
| 关键字 | `int|void|if|else|while|for|break|continue|return` | 对应关键字 token |
| 标识符 | `[A-Za-z_][A-Za-z0-9_]*` | `identifier` |
| 整数 | `0|[1-9][0-9]*` | `integer` |
| 单字符界符 | `[(){}\[\],;]` | 对应界符 token |
| 单字符运算符 | `[+\-*/%=<>]` | 对应运算符 token |
| 双字符运算符 | `==|!=|<=|>=|&&|\|\|` | 对应运算符 token |
| 行注释 | `//[^\n]*` | 跳过 |
| 块注释 | `/* ... */` | 跳过 |
| 空白 | 空格、制表符、换行 | 跳过或作为 token 分隔 |

关键字和标识符的识别采用最长匹配规则。先按标识符正则读出最长词素，再查关键字表；如果词素等于某个关键字，则输出关键字 token，否则输出 `identifier`。

运算符也采用最长匹配规则。例如读到 `=` 时，需要向前看一个字符；若下一个字符仍是 `=`，则输出 `==`，否则输出 `=`。`!=`、`<=`、`>=`、`&&`、`||` 同理。

词法错误属于局部字符错误，例如未知字符、非法数字形式、未闭合块注释。词法分析只报告这些字符层面的错误，不负责报告语句结构错误。

## parser 模块：目标文法分析

parser 模块处理的对象不是源程序字符，而是 lexer 已经输出的 token 序列。目标文法需要描述这些 token 如何组合成函数、语句和表达式，并为后续 AST、语义分析和四元式生成提供结构。

从当前实现的 miniC 语法出发，目标语言的核心结构可以分成三层：

- 程序层：一个程序由零个或多个函数定义组成，函数由返回类型、函数名、形参列表和函数体构成。
- 语句层：函数体和块内部包含变量声明、数组声明、赋值、数组元素赋值、函数调用、分支、循环、跳转、返回和嵌套块。
- 表达式层：表达式包含整数、标识符、数组下标读取、函数调用、括号表达式、一元运算、算术运算、关系运算和逻辑运算。

语法分析阶段只判断 token 序列的结构是否符合文法。例如变量是否已经声明、函数调用参数数量是否正确、`return` 是否匹配函数返回类型，这些属于语义分析阶段。

## 转化为上下文无关文法

按照 `教材ppt/CHP-04-TCH.md` 中的上下文无关文法记号，可以把 parser 的目标文法写成：

```text
G = (VN, VT, P, S)
```

其中：

- `VN` 是非终结符集合，例如 `Program`、`Function`、`Stmt`、`Expr`、`Primary`。
- `VT` 是终结符集合，对应 lexer 输出的 token，例如 `"int"`、`identifier`、`integer`、`"if"`、`"+"`、`";"`。
- `P` 是产生式集合，用来定义非终结符如何展开为终结符和非终结符序列。
- `S` 是开始符号，本实验取 `Program`。

因此，本实验 parser 使用的上下文无关文法为：

```text
GminiC = (VN, VT, P, Program)
```

下面“程序结构”“语句”“表达式”三节给出产生式集合 `P`。这些产生式直接服务于规范 LR(1) 项目集、DFA、ACTION/GOTO 表和规约时 AST 构造。

## 上下文无关文法记号：parser 模块

- 终结符用字面量表示，例如 `"int"`、`"("`、`identifier`。
- `ε` 表示空串。
- 表达式文法保留左递归，用 LR(1) 规约自然表达左结合。
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
                 | "int" identifier "[" "]"
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
                 | ForStmt
                 | BreakStmt
                 | ContinueStmt
                 | ReturnStmt
                 | Block

VarDecl         -> "int" VarDeclarator VarDeclaratorTail ";"
VarDeclarator  -> identifier VarInitOpt
                 | identifier "[" integer "]"
                 | identifier "[" integer "]" "=" InitializerList
VarDeclaratorTail
                -> "," VarDeclarator VarDeclaratorTail
                 | ε
VarInitOpt      -> "=" Expr | ε
InitializerList -> "{" InitializerValuesOpt "}"
InitializerValuesOpt
                -> InitializerValues | ε
InitializerValues
                -> InitializerValues "," Expr
                 | Expr

IdentifierStmt  -> identifier IdentifierStmtTail
IdentifierStmtTail
                -> "=" Expr ";"
                 | "[" Expr "]" "=" Expr ";"
                 | "(" ArgListOpt ")" ";"

IfStmt          -> "if" "(" Expr ")" Block ElseOpt
ElseOpt         -> "else" Block | ε

WhileStmt       -> "while" "(" Expr ")" Block
ForStmt         -> "for" "(" ForInitOpt ";" Expr ";" ForStepOpt ")" Block
ForInitOpt      -> "int" VarDeclarator VarDeclaratorTail
                 | identifier "=" Expr
                 | identifier "[" Expr "]" "=" Expr
                 | ε
ForStepOpt      -> identifier "=" Expr
                 | identifier "[" Expr "]" "=" Expr
                 | ε

BreakStmt       -> "break" ";"
ContinueStmt    -> "continue" ";"

ReturnStmt      -> "return" ReturnValueOpt ";"
ReturnValueOpt  -> Expr | ε
```

约束：

- `if`、`while` 和 `for` 的主体强制使用 `{ ... }`，避免悬挂 `else` 并统一循环边界。
- 同一条 `int` 变量声明语句可以包含多个声明项，例如 `int a = 2, c = 0;`，每个声明项独立决定是否带初始化表达式。
- 固定长度数组只支持 `int name[N]`，`N` 必须是正整数字面量；数组形参写作 `int name[]`，按引用传递。
- 赋值语句只允许 `identifier = Expr;`，不把赋值放进普通表达式。
- 数组元素赋值只允许 `identifier[Expr] = Expr;`；数组名本身不能作为 `int` 值使用，只能作为数组实参传递。
- `break` 和 `continue` 只能出现在 `while` 或 `for` 内部。
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
                 | identifier "[" Expr "]"
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
| 1 | 函数调用 `f(...)`、数组下标 `a[i]` | 左结合 |
| 2 | 一元 `+` `-` | 右结合 |
| 3 | `*` `/` `%` | 左结合 |
| 4 | `+` `-` | 左结合 |
| 5 | `<` `<=` `>` `>=` | 左结合 |
| 6 | `==` `!=` | 左结合 |
| 7 | `&&` | 左结合 |
| 8 | `||` | 左结合 |

约束：

- 普通表达式类型为 `int`；数组名是单独的 `int[]` 引用类型，不能直接参与算术、关系或逻辑运算。
- 关系运算和逻辑运算结果也是 `int`，`0` 表示 false，非 `0` 表示 true。
- 函数调用只支持普通标识符调用，不支持函数指针和成员访问；数组实参按引用传递，不复制数组。
- 四元式生成新增 `array_decl`、`array_load`、`array_store`，用于描述数组声明、下标读取和下标写入。

## LR(1) 主线构造说明

parser 主线采用规范 LR(1) 分析，不再把预测分析作为后续阶段入口。实现结构如下：

- `parser/symbol.cp`: 定义终结符、非终结符和 epsilon。
- `parser/grammar.cp`: 在 `make_minic_grammar()` 中直接写出 miniC 产生式。
- `parser/table.cp`: 计算 FIRST 集、closure、goto、规范 LR(1) 项目集族、DFA 转移以及 ACTION/GOTO 表。
- `parser/state.cp`: 执行表驱动移进/规约分析，并在每条 reduce 动作上构造现有 AST。
- `parser/parser.cp`: 暴露 `parse(tokens, options = parse_options{})`，供语义分析和 IR 继续使用。

LR(1) 项目写作 `[A -> alpha . beta, a]`。其中 `A -> alpha beta` 是一条产生式，点表示已经识别到的位置，`a` 是 1 个向前看终结符。closure 遇到 `[A -> alpha . B beta, a]` 时，会加入 `B` 的所有候选产生式，并用 `FIRST(beta a)` 决定新项目的 lookahead。goto 把点前为某个符号的项目统一右移一格，再做 closure，得到自动机下一状态。

填表规则为：

- DFA 的终结符边填入 `ACTION[state, terminal] = shift target`。
- DFA 的非终结符边填入 `GOTO[state, nonterminal] = target`。
- 完成项目 `[A -> alpha ., a]` 填入 `ACTION[state, a] = reduce A -> alpha`。
- 增广完成项目 `[S' -> Program ., EOF]` 填入 accept。

运行时只维护状态栈、语义值栈和输入指针。shift 时把 token 对应的值压入语义值栈；reduce 时弹出产生式右部长度个值，按产生式编号构造 AST 节点或中间列表，再把左部非终结符的值压回栈。最终 accept 时，栈顶值就是完整 `program_syntax` 对应的 `program_id`。

## 实验三：算符优先分析理论与实现

实验三采用要求中的 (a) 方式：在代码中直接写出算符优先关系矩阵。实现代码只有一个文件，位于 `parser/op/main.cp`。

算符优先分析属于自底向上的移进/规约分析方法。它不按递归下降方式一层层调用非终结符子程序，而是维护一个分析栈，并根据“栈顶最近终结符”和“当前输入终结符”之间的优先关系决定下一步动作：

- 若关系为 `<` 或 `=`，说明当前输入还应继续进入句柄，执行移进。
- 若关系为 `>`，说明栈顶已经形成一个可规约句柄，执行规约。
- 若没有关系，说明当前 token 序列不符合该算符优先文法。

算符优先分析路线只选取 miniC 表达式中最简单的算术子集：

- 支持整数、标识符、括号和二元运算 `*` `/` `%` `+` `-`。
- 暂不支持函数调用表达式。
- 暂不支持关系、相等、逻辑运算。
- 暂不支持一元 `+`、一元 `-`，只演示普通二元算术表达式。

使用的算符优先文法如下，其中 `identifier` 和 `integer` 都是 lexer 已经识别出的 token：

```text
E -> E "+" E
   | E "-" E
   | E "*" E
   | E "/" E
   | E "%" E
   | "(" E ")"
   | identifier
   | integer
```

该文法用于实验三演示，优先级不靠文法层次表达，而是直接写在算符优先关系矩阵中。核心矩阵含义如下：

| 栈顶终结符 | 当前输入 | 关系含义 |
|---|---|---|
| `EOF` | `id`、`integer`、`(`、运算符 | 移进 |
| `id`、`integer` | 运算符、`)`、`EOF` | 规约 |
| `+`、`-` | `id`、`integer`、`(`、`*`、`/`、`%` | 移进 |
| `+`、`-` | `+`、`-`、`)`、`EOF` | 规约 |
| `*`、`/`、`%` | `id`、`integer`、`(` | 移进 |
| `*`、`/`、`%` | 运算符、`)`、`EOF` | 规约 |
| `(` | `id`、`integer`、`(`、运算符 | 移进 |
| `(` | `)` | 等于，用于匹配括号 |
| `)` | 运算符、`)`、`EOF` | 规约 |

其中 `EOF < 运算符` 和 `( < 运算符` 用于处理已经规约出的左操作数后继续读取二元运算符；如果表达式一开始就是 `+ value` 这类缺左操作数形式，后续句柄规约会失败并报错。

当前实现接受例如 `value + 2 * (limit - 3)`，拒绝 `value + * 3` 和 `+ value`。

按代码执行顺序，本实验三路线如下：

```text
parser/op/main.cp
  -> lex_expression()
  -> parse_expression()
  -> relation_between()
  -> shift 或 reduce_handle()
```

`parser/op/main.cp` 是唯一实现文件，内部包含：

- `to_op_terminal()`：把 lexer token 转成实验三使用的终结符。
- `relation_between()`：人工填写的算符优先关系矩阵。
- `reduce_handle()`：识别 `id`、`E op E` 和 `(E)` 三种句柄并规约成 `E`。
- `parse_expression()`：执行移进/规约主循环，并输出每一步动作。

分析器运行时的核心循环是：

1. 初始栈为 `EOF`，输入 token 串末尾也补 `EOF`。
2. 找到栈顶最近的终结符 `a`，读取当前输入终结符 `b`。
3. 查优先关系表 `relation_between(a, b)`。
4. 若 `a < b` 或 `a = b`，把 `b` 移进栈，输入指针前进。
5. 若 `a > b`，向左回溯找到句柄起点，再用某条产生式右部匹配该句柄并规约为左部非终结符。
6. 当输入为 `EOF` 且栈为 `EOF Nonterminal` 时接受。

这条路线只用于实验三展示“人工矩阵、移进规约分析过程”，不生成 AST，也不作为后续语义分析入口。后续语义分析统一使用主线 LR(1) parser 生成的 AST。

## LR(0) 与 LR(1) 理论方法

本节只说明 LR(0) 和 LR(1) 的构造方法。完整项目集、完整 DFA 转移和完整 ACTION/GOTO 表单独放在 [minic_lr_complete_construction.md](minic_lr_complete_construction.md)，避免理论说明被几千行状态表打断。

### LR 分析的基本思想

LR 分析是一种自底向上的语法分析方法。它从左到右读取 token，并通过“移进”和“规约”逐步把输入串还原成开始符号。

分析栈中保存两类信息：

- 状态：表示当前已经识别到文法的哪个位置。
- 文法符号：表示已经移进或规约出来的终结符、非终结符。

分析过程中每一步都看两个东西：

- 栈顶状态 `I`。
- 当前输入 token，也叫 lookahead。

然后查 ACTION 表决定下一步是 shift、reduce、accept 还是 error。reduce 以后，再用 GOTO 表决定规约出的非终结符应该进入哪个新状态。

### 增广文法

LR 构造前要先加入一个新的开始符号：

```text
S' -> Program
```

`S'` 只出现在这一条产生式左侧。这样做的作用是让“分析成功”有唯一判断条件：当状态中出现增广产生式完成项目，并且当前输入是 `EOF` 时，分析器执行 accept。

本文档里的 `Program` 已经包含 `EOF`：

```text
Program -> FunctionList EOF
```

所以增广产生式不能再写成 `S' -> Program EOF`，否则就会多要求一个结束符。

### LR(0) 项目

LR(0) 项目是在产生式右部插入一个点号。点号表示当前产生式已经识别到的位置。

例如产生式：

```text
FunctionList -> Function FunctionList
```

对应三个 LR(0) 项目：

```text
FunctionList -> . Function FunctionList
FunctionList -> Function . FunctionList
FunctionList -> Function FunctionList .
```

含义分别是：

- 第一个项目：还没有识别 `Function`。
- 第二个项目：已经识别出一个 `Function`，接下来需要识别 `FunctionList`。
- 第三个项目：右部已经识别完成，可以按 `FunctionList -> Function FunctionList` 规约。

如果产生式右部为空，例如：

```text
FunctionList -> ε
```

它对应的完成项目写成：

```text
FunctionList -> .
```

这表示不需要移进任何 token，就可以把空串规约成 `FunctionList`。

### LR(0) 闭包

`closure0(I)` 用来补全一个 LR(0) 项目集。这里的 `I` 是当前已有的一组项目。

闭包规则是：如果项目集中有项目：

```text
A -> α . B β
```

并且点号后面的 `B` 是非终结符，那么说明接下来要识别一个 `B`。而 `B` 可以由它自己的任意产生式推出，所以必须把 `B` 的所有产生式初始项目加入当前集合：

```text
B -> . γ
```

完整计算过程是：

1. 令结果集合 `C` 等于初始项目集 `I`。
2. 扫描 `C` 中每个项目。
3. 如果某个项目的点号后面是非终结符 `B`，就把 `B` 的所有产生式初始项目加入 `C`。
4. 新加入的项目也可能让点号后面出现新的非终结符，所以继续扫描。
5. 直到没有任何新项目可以加入，`C` 就是 `closure0(I)`。

闭包只负责补全“接下来可能展开什么”，不移动点号。

### LR(0) 转移函数

`goto0(I, X)` 表示：当前在状态 `I`，如果识别了一个文法符号 `X`，会进入哪个状态。

这里的 `X` 可以是终结符，也可以是非终结符。

计算 `goto0(I, X)` 时，只处理点号后面正好是 `X` 的项目。也就是找出所有形如：

```text
A -> α . X β
```

的项目，然后把点号越过 `X`：

```text
A -> α X . β
```

这些移动后的项目组成一个集合 `M`。如果 `M` 是空集，说明当前状态没有标号为 `X` 的转移。如果 `M` 非空，就对它求闭包：

```text
goto0(I, X) = closure0(M)
```

注意这里不是把状态 `I` 里所有项目都移动一格，而是只移动点号后面同为 `X` 的那一批项目。`goto0(I, X)` 本身只针对一个固定的 `X` 计算一次；真正构造 DFA 时，才会把当前状态里所有可能的 `X` 都取出来，逐个调用 `goto0(I, X)`。DFA 的一条边只能有一个边标号。

### LR(0) DFA 构造

LR(0) DFA 的每个状态都是一个 LR(0) 项目集。构造过程如下：

1. 从初始项目开始：

   ```text
   S' -> . Program
   ```

2. 对初始项目求闭包，得到初始状态 `I0`。
3. 把 `I0` 放入状态集合和待处理队列。
4. 取出一个未处理状态 `I`。
5. 找出 `I` 中所有出现在点号后面的文法符号。
6. 对每个这样的符号 `X` 计算 `goto0(I, X)`。
7. 如果结果项目集非空，并且以前没有出现过，就创建一个新状态。
8. 记录一条 DFA 边：

   ```text
   I --X--> J
   ```

9. 重复处理，直到没有新状态产生。

判断两个 LR(0) 状态是否相同，只比较产生式编号和点号位置。LR(0) 项目没有 lookahead，所以不需要比较向前看符号。

LR(0) DFA 的意义是描述“点号如何随着识别到的文法符号向右移动”。终结符边对应 shift，非终结符边对应 reduce 以后查 GOTO 表。

### LR(0) 的局限

LR(0) 完成项目没有 lookahead。例如：

```text
A -> α .
```

在 LR(0) 中，这个项目只说明 `A -> α` 已经识别完成，但没有说明当前输入 token 是什么时才应该规约。

因此 LR(0) 构造分析表时容易遇到冲突：

- shift/reduce 冲突：同一个状态里既有可移进项目，又有完成项目。
- reduce/reduce 冲突：同一个状态里有多个完成项目。

LR(1) 的改进就是给每个项目加一个 lookahead，让规约只在指定 token 上发生。

### LR(1) 项目

LR(1) 项目形式是：

```text
[A -> α . β, a]
```

其中 `a` 是 lookahead。它表示：当前正在识别 `A -> α β`，点号左侧 `α` 已经识别完成；如果这个项目最后变成：

```text
[A -> α β ., a]
```

那么只有当前输入 token 是 `a` 时，才允许按 `A -> α β` 规约。

LR(1) 和 LR(0) 的区别是：

- LR(0) 项目只记录产生式和点号位置。
- LR(1) 项目记录产生式、点号位置和 lookahead。
- LR(1) 中点号位置相同但 lookahead 不同的项目，仍然是不同项目。

### FIRST(β a)

LR(1) 闭包需要计算 `FIRST(β a)`。

在项目：

```text
[A -> α . B β, a]
```

中，点号后面是非终结符 `B`。如果要展开 `B`，就要知道 `B` 推导出来的内容后面可能跟哪些 token。

这些 token 来自 `β a`：

- 如果 `β` 能推出某些开头终结符，就用这些终结符作为 `B` 新项目的 lookahead。
- 如果 `β` 可以推出空串，就还要把原项目的 lookahead `a` 传给 `B`。
- 如果 `β` 本来就是空串，也直接把 `a` 传给 `B`。

计算 `FIRST(β a)` 的步骤是：

1. 从 `β` 的第一个符号开始扫描。
2. 如果遇到终结符 `t`，把 `t` 加入结果并停止。
3. 如果遇到非终结符 `N`，把 `FIRST(N)` 中除 `ε` 以外的符号加入结果。
4. 如果 `N` 不能推出 `ε`，停止。
5. 如果扫描完 `β` 后仍然可能为空，把 `a` 加入结果。

### LR(1) 闭包

`closure1(I)` 是 LR(1) 项目集闭包。

如果项目集中有：

```text
[A -> α . B β, a]
```

并且 `B` 是非终结符，那么对 `B` 的每条产生式：

```text
B -> γ
```

以及每个：

```text
b ∈ FIRST(β a)
```

都加入一个新项目：

```text
[B -> . γ, b]
```

完整计算过程是：

1. 令结果集合 `C` 等于初始 LR(1) 项目集 `I`。
2. 扫描 `C` 中每个项目。
3. 如果某个项目点号后面是非终结符，就根据 `FIRST(β a)` 加入带 lookahead 的新项目。
4. 新加入的项目也可能继续展开，所以继续扫描。
5. 直到没有任何新项目可以加入，`C` 就是 `closure1(I)`。

LR(1) 闭包和 LR(0) 闭包的主要差别是：LR(1) 展开非终结符时必须同时传播 lookahead。

### LR(1) 转移函数

`goto1(I, X)` 和 `goto0(I, X)` 的结构相同，也是沿着一个文法符号 `X` 移动点号。

计算时仍然只找点号后面正好是 `X` 的项目：

```text
[A -> α . X β, a]
```

移动点号以后保留原 lookahead：

```text
[A -> α X . β, a]
```

所有移动后的项目组成集合 `M`。如果 `M` 为空，就没有这条转移。如果 `M` 非空，则：

```text
goto1(I, X) = closure1(M)
```

所以 `goto1` 可以理解为两步：

1. 沿边标号 `X` 统一移动点号。
2. 对移动后的项目集合做 LR(1) 闭包。

这里同样要区分“转移函数”和“构造 DFA”：`goto1(I, X)` 是固定一个 `X` 的计算；构造某个状态 `I` 的所有出边时，要对 `I` 中所有出现在点号后面的符号 `X` 都计算一遍 `goto1(I, X)`。

### LR(1) DFA 构造

规范 LR(1) DFA 的构造过程如下：

1. 从初始 LR(1) 项目开始：

   ```text
   [S' -> . Program, EOF]
   ```

2. 对它求 `closure1`，得到初始状态 `I0`。
3. 把 `I0` 放入状态集合和待处理队列。
4. 取出一个未处理状态 `I`。
5. 找出 `I` 中所有出现在点号后面的文法符号。
6. 对每个这样的符号 `X` 计算 `goto1(I, X)`。
7. 如果结果项目集非空，并且以前没有出现过，就创建一个新状态。
8. 记录 DFA 边：

   ```text
   I --X--> J
   ```

9. 重复处理，直到没有新状态产生。

判断两个 LR(1) 状态是否相同，必须比较完整项目集合。产生式编号、点号位置和 lookahead 都相同，两个状态才相同。

不能先构造 LR(0) DFA，再给每个状态随便补 lookahead。规范 LR(1) 必须从 `[S' -> . Program, EOF]` 开始，让 lookahead 在 `closure1` 和 `goto1` 中逐步传播。

### LR(1) ACTION/GOTO 表

LR(1) DFA 构造完成后，根据状态项目和 DFA 转移生成分析表。

如果状态 `I` 有终结符转移：

```text
I --t--> J
```

则填入移进动作：

```text
ACTION[I, t] = shift J
```

如果状态 `I` 中有完成项目：

```text
[A -> α ., a]
```

并且 `A` 不是 `S'`，则填入规约动作：

```text
ACTION[I, a] = reduce A -> α
```

注意规约动作只填在这个项目自己的 lookahead `a` 上，不填在所有终结符上。

如果状态 `I` 中有增广完成项目：

```text
[S' -> Program ., EOF]
```

则填入接受动作：

```text
ACTION[I, EOF] = accept
```

如果状态 `I` 有非终结符转移：

```text
I --A--> J
```

则填入：

```text
GOTO[I, A] = J
```

如果同一个 ACTION 表格位置被填入多个不同动作，就说明发生冲突：

- 同时出现 shift 和 reduce，是 shift/reduce 冲突。
- 同时出现多个 reduce，是 reduce/reduce 冲突。

本 miniC 文法的完整构造结果见 [minic_lr_complete_construction.md](minic_lr_complete_construction.md)。其中完整列出了产生式编号、LR(0) DFA、LR(1) DFA 和 LR(1) ACTION/GOTO 表。

## 语义分析理论说明

语义分析接收 LR(1) parser 规约生成的 AST，检查上下文相关规则。语法分析只能判断结构是否符合文法，不能判断名字是否声明、函数参数是否匹配、返回值是否合理。

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

当前实验五实现按主编译器的模块边界拆分，但不提供聚合模块：

- `semantic/type.cp` 定义 `int`、`void`、`error` 三类语义类型。
- `semantic/symbol.cp` 定义函数、参数和局部变量符号。
- `semantic/result.cp` 保存函数表、符号表、表达式类型、常量值、语句绑定和诊断。
- `semantic/state.cp` 保存分析过程中的源码、AST、诊断收集器和作用域栈。
- `semantic/function.cp` 负责函数收集、`main` 检查、参数绑定和函数体入口。
- `semantic/statement.cp` 负责块、声明、赋值、调用、分支、循环和返回语句检查。
- `semantic/expression.cp` 负责名字解析、函数调用检查、表达式类型和常量值计算。
- `semantic/program.cp` 提供公共入口 `analyze_semantics(file, parsed)`。

语义分析采用两遍结构：第一遍收集所有函数定义，因此函数体内可以调用后面才出现的函数；第二遍进入每个函数体，先绑定参数，再按块级作用域检查语句和表达式。表达式常量值保存在 `semantic_result.expression_infos` 中，例如 `1 + 2 * 3` 会得到常量值 `7`。

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

当前四元式实现同样按职责拆分，不提供 `ir` 聚合模块：

- `ir/quad.cp` 定义四元式记录 `(op, arg1, arg2, result)`。
- `ir/result.cp` 定义四元式生成结果。
- `ir/state.cp` 保存临时变量、标签和输出序列。
- `ir/expression.cp` 负责表达式、参数传递和函数调用翻译。
- `ir/statement.cp` 负责声明、赋值、调用语句、`if`、`while` 和 `return` 翻译。
- `ir/program.cp` 提供公共入口 `emit_quads(file, parsed, semantics)`。
- `ir/format.cp` 提供 `dump_quads(quads)`，按 `(op, arg1, arg2, result)` 逐行输出。

四元式中的空位统一写作 `_`，临时变量命名为 `t1`、`t2`、...，标签命名为 `L1`、`L2`、...。函数边界输出为：

```text
(func, _, _, main)
...
(end, _, _, main)
```

`lab/ir/main.cp` 是实验五样例入口，会读取 `lab/test.c`，依次执行预处理、词法分析、LR(1) 语法分析、语义分析和四元式生成，并输出语义统计、可计算表达式值以及四元式序列。

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
