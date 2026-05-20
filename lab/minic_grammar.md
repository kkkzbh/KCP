# miniC 超轻量级文法

目标：保留 miniC 的函数、变量、分支、循环、返回和整数表达式，去掉数组、指针、结构体、全局变量、函数声明和复杂类型，方便后续分别实现词法分析、递归下降 LL(1)、算符优先/LR、语义分析和四元式生成。

## 词法单元

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

## 语法记号

- 终结符用字面量表示，例如 `"int"`、`"("`、`identifier`。
- `ε` 表示空串。
- 表达式文法已消除左递归，适合递归下降实现。

## 程序结构

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

## 语句

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

## 表达式

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

因此，严格写实验二报告时可以说明：本 miniC 子集通过消除左递归和提取左因子，使语句和表达式主体适合 LL(1) 递归下降实现。表达式也可以在实验三中复用同一组运算符优先级做算符优先分析。

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
