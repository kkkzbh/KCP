# cp 实验二 2.1 递归下降 LL(1) 语法分析设计

## 1. 目标文法分析

本次 2.1 选择 `design/` 中已经明确写清的 `cp` 语言子集，不再套用教材默认文法。分析范围如下：

- 顶层：可选 `export module` 文件头、若干 `import`、若干函数定义
- 语句：块、`let/const` 声明、`if / else if / else`、`while`、`do while`、范围 `for`、带标签 `break/continue`、`return`、表达式语句
- 类型：限定名类型、`array/sequence/tuple` 泛型形态、引用与指针后缀、`type const&` / `type const*`
- 表达式：限定名、字面量、数组/序列/元组字面量、函数调用、`as` cast、赋值、逻辑/位运算/比较/移位/算术、前后缀一元运算

本次明确不纳入：

- `struct / impl / trait`
- `?:`
- 成员访问 `.`
- 以序列字面量 `{ ... }` 开头的独立表达式语句

最后一条是为了避免语句位置上 `Block` 与 `SequenceLiteral` 共享前导 `{`，破坏 LL(1) 判定。它不影响 `let x = {1,2,3};` 或 `return {1,2,3};` 这种表达式上下文的序列字面量。

起始符号固定为：

```text
TranslationUnit
```

终结符来自 lexer 的 `token_kind`。为了让文法分析与实际 parser 完全一致，类型上下文里的 `< > & *` 在 LL(1) 文法中单独记为：

- `type_less`
- `type_greater`
- `type_amp`
- `type_star`

它们在词法层仍然是普通 token，只是在类型产生式里被当作上下文终结符解释。

## 2. 原始上下文无关文法

下面给出更接近语言设计直觉、尚未系统消除左递归和左公因子的原始 CFG。

```text
TranslationUnit -> ModuleHeaderOpt ImportList FunctionList
ModuleHeaderOpt -> export module QualifiedName ; | ε
ImportList -> Import ImportList | ε
Import -> import QualifiedName ;
FunctionList -> Function FunctionList | ε
Function -> export? identifier ( ParameterList? ) ReturnTypeOpt Block
ParameterList -> Parameter ( , Parameter )*
Parameter -> const? identifier : Type
ReturnTypeOpt -> -> Type | ε

Block -> { Statement* }
Statement -> Block
           | DeclarationStmt
           | IfStmt
           | WhileStmt
           | DoWhileStmt
           | ForStmt
           | BreakStmt
           | ContinueStmt
           | ReturnStmt
           | ExprStmt

DeclarationStmt -> (let | const) identifier ( : Type )? = Expression ;
IfStmt -> if ( Expression ) Block ( else IfStmt | else Block )?
WhileStmt -> while ( Expression ) Block
DoWhileStmt -> do Block while ( Expression ) ;
ForStmt -> for ( : label )? ( (let | const) identifier : Expression ) Block
BreakStmt -> break identifier? ;
ContinueStmt -> continue identifier? ;
ReturnStmt -> return Expression? ;
ExprStmt -> Expression ;

Type -> QualifiedName TypeArgs? ConstQualifier? PointerOrRef*
TypeArgs -> < TypeArg ( , TypeArg )* >
TypeArg -> Type | integer_literal
PointerOrRef -> & | *

QualifiedName -> identifier ( :: identifier )*

Expression -> Assignment
Assignment -> Assignment AssignOp LogicalOr | LogicalOr
LogicalOr -> LogicalOr or LogicalAnd | LogicalAnd
LogicalAnd -> LogicalAnd and BitwiseOr | BitwiseOr
BitwiseOr -> BitwiseOr pipe BitwiseXor | BitwiseXor
BitwiseXor -> BitwiseXor ^ BitwiseAnd | BitwiseAnd
BitwiseAnd -> BitwiseAnd & Equality | Equality
Equality -> Equality (== | !=) Relational | Relational
Relational -> Relational (< | <= | > | >=) Shift | Shift
Shift -> Shift (<< | >>) Additive | Additive
Additive -> Additive (+ | -) Multiplicative | Multiplicative
Multiplicative -> Multiplicative (* | / | %) Cast | Cast
Cast -> Unary ( as Type )*
Unary -> PrefixOp Unary | Postfix
Postfix -> Primary ( CallSuffix | ++ | -- )*
Primary -> Literal
         | QualifiedName
         | [ ElementList? ]
         | { ElementList? }
         | ( Expression )
         | ( Expression , Expression ( , Expression )* )
```

这个版本存在三类典型问题：

- 表达式存在直接左递归
- 顶层 `export module` 与 `export function` 共享前缀 `export`
- `(expr)` 与 `(expr, expr, ...)` 共享前缀 `(`

## 3. 消除左递归和左公因子后的 LL(1) 文法

实际程序与 `parser/core/grammar.cppm` 采用下列 LL(1) 版本。

### 3.1 顶层左公因子

```text
TranslationUnit -> TranslationUnitPrefix
TranslationUnitPrefix
    -> kw_export TranslationUnitAfterExport
    | kw_import QualifiedName ; ImportListTail FunctionList
    | identifier FunctionAfterName FunctionList
    | ε

TranslationUnitAfterExport
    -> kw_module QualifiedName ; ImportList FunctionList
    | identifier FunctionAfterName FunctionList

ImportList -> kw_import QualifiedName ; ImportList | ε
ImportListTail -> kw_import QualifiedName ; ImportListTail | ε
FunctionList -> FunctionLead FunctionList | ε
FunctionLead -> kw_export identifier FunctionAfterName
FunctionLead -> identifier FunctionAfterName
FunctionAfterName -> ( ParameterListOpt ) ReturnTypeOpt Block
```

### 3.2 语句层

```text
Block -> l_brace StatementList r_brace
StatementList -> Statement StatementList | ε
Statement -> Block
           | DeclarationStmt
           | IfStmt
           | WhileStmt
           | DoWhileStmt
           | ForStmt
           | BreakStmt
           | ContinueStmt
           | ReturnStmt
           | ExprStmt

DeclarationStmt -> LetOrConst identifier DeclTypeOpt equal Expression semicolon
IfStmt -> kw_if l_paren Expression r_paren Block ElseOpt
ElseOpt -> kw_else ElseBranch | ε
ElseBranch -> IfStmt | Block
WhileStmt -> kw_while l_paren Expression r_paren Block
DoWhileStmt -> kw_do Block kw_while l_paren Expression r_paren semicolon
ForStmt -> kw_for ForLabelOpt l_paren ForBinding colon Expression r_paren Block
ForLabelOpt -> colon identifier | ε
ForBinding -> LetOrConst identifier
BreakStmt -> kw_break LabelOpt semicolon
ContinueStmt -> kw_continue LabelOpt semicolon
ReturnStmt -> kw_return ReturnExprOpt semicolon
ExprStmt -> ExpressionNoBrace semicolon
```

### 3.3 表达式左递归消除

```text
Assignment        -> LogicalOr AssignmentTail
AssignmentTail    -> AssignmentOperator Assignment | ε

LogicalOr         -> LogicalAnd LogicalOrTail
LogicalOrTail     -> kw_or LogicalAnd LogicalOrTail | ε

LogicalAnd        -> BitwiseOr LogicalAndTail
LogicalAndTail    -> kw_and BitwiseOr LogicalAndTail | ε

BitwiseOr         -> BitwiseXor BitwiseOrTail
BitwiseOrTail     -> pipe BitwiseXor BitwiseOrTail | ε

BitwiseXor        -> BitwiseAnd BitwiseXorTail
BitwiseXorTail    -> caret BitwiseAnd BitwiseXorTail | ε

BitwiseAnd        -> Equality BitwiseAndTail
BitwiseAndTail    -> amp Equality BitwiseAndTail | ε

Equality          -> Relational EqualityTail
EqualityTail      -> (equal_equal | bang_equal) Relational EqualityTail | ε

Relational        -> Shift RelationalTail
RelationalTail    -> (less | less_equal | greater | greater_equal) Shift RelationalTail | ε

Shift             -> Additive ShiftTail
ShiftTail         -> (less_less | greater_greater) Additive ShiftTail | ε

Additive          -> Multiplicative AdditiveTail
AdditiveTail      -> (plus | minus) Multiplicative AdditiveTail | ε

Multiplicative    -> Cast MultiplicativeTail
MultiplicativeTail-> (star | slash | percent) Cast MultiplicativeTail | ε

Cast              -> Unary CastTail
CastTail          -> kw_as Type CastTail | ε

Unary             -> PrefixOperator Unary | Postfix
Postfix           -> Primary PostfixTail
PostfixTail       -> CallSuffix PostfixTail
                  | PostfixOperator PostfixTail
                  | ε
```

### 3.4 括号表达式 / 元组左公因子

```text
ParenPrimary     -> l_paren Expression ParenPrimaryTail
ParenPrimaryTail -> r_paren
                 | comma Expression TupleTail r_paren
TupleTail        -> comma Expression TupleTail | ε
```

### 3.5 类型文法

```text
Type            -> QualifiedName TypeArgumentOpt TypeConstOpt TypeSuffixList
TypeArgumentOpt -> type_less TypeArgumentList type_greater | ε
TypeArgumentList-> TypeArgument TypeArgumentTail
TypeArgumentTail-> comma TypeArgument TypeArgumentTail | ε
TypeArgument    -> Type | integer_literal
TypeConstOpt    -> kw_const | ε
TypeSuffixList  -> TypeSuffix TypeSuffixList | ε
TypeSuffix      -> type_amp | type_star
```

## 4. SELECT 集合与 LL(1) 判断

完整机器可校验文法与集合计算实现位于：

- [grammar.cppm](/home/kkkzbh/code/cp/parser/core/grammar.cppm)
- [ll1_behavior_test.cpp](/home/kkkzbh/code/cp/test/parser/suites/ll1_behavior_test.cpp)

下面给出按非终结符分组后的 SELECT 结果摘要。

### 4.1 顶层

| 非终结符 | 产生式 | SELECT |
| --- | --- | --- |
| `TranslationUnitPrefix` | `kw_export TranslationUnitAfterExport` | `{ kw_export }` |
| `TranslationUnitPrefix` | `kw_import QualifiedName ; ImportListTail FunctionList` | `{ kw_import }` |
| `TranslationUnitPrefix` | `identifier FunctionAfterName FunctionList` | `{ identifier }` |
| `TranslationUnitPrefix` | `ε` | `{ $ }` |
| `TranslationUnitAfterExport` | `kw_module QualifiedName ; ImportList FunctionList` | `{ kw_module }` |
| `TranslationUnitAfterExport` | `identifier FunctionAfterName FunctionList` | `{ identifier }` |
| `ImportList` | `kw_import QualifiedName ; ImportList` | `{ kw_import }` |
| `ImportList` | `ε` | `{ kw_export, identifier, $ }` |
| `FunctionList` | `FunctionLead FunctionList` | `{ kw_export, identifier }` |
| `FunctionList` | `ε` | `{ $ }` |

### 4.2 参数、块与语句

| 非终结符 | 产生式 | SELECT |
| --- | --- | --- |
| `ParameterListOpt` | `ParameterList` | `{ kw_const, identifier }` |
| `ParameterListOpt` | `ε` | `{ r_paren }` |
| `ReturnTypeOpt` | `arrow Type` | `{ arrow }` |
| `ReturnTypeOpt` | `ε` | `{ l_brace }` |
| `Statement` | `Block` | `{ l_brace }` |
| `Statement` | `DeclarationStmt` | `{ kw_let, kw_const }` |
| `Statement` | `IfStmt` | `{ kw_if }` |
| `Statement` | `WhileStmt` | `{ kw_while }` |
| `Statement` | `DoWhileStmt` | `{ kw_do }` |
| `Statement` | `ForStmt` | `{ kw_for }` |
| `Statement` | `BreakStmt` | `{ kw_break }` |
| `Statement` | `ContinueStmt` | `{ kw_continue }` |
| `Statement` | `ReturnStmt` | `{ kw_return }` |
| `Statement` | `ExprStmt` | `{ identifier, integer_literal, float_literal, char_literal, string_literal, kw_true, kw_false, l_paren, l_bracket, plus, minus, kw_not, tilde, amp, star, plus_plus, minus_minus }` |
| `ElseOpt` | `kw_else ElseBranch` | `{ kw_else }` |
| `ElseOpt` | `ε` | `FOLLOW(ElseOpt)` |

### 4.3 类型

| 非终结符 | 产生式 | SELECT |
| --- | --- | --- |
| `Type` | `QualifiedName TypeArgumentOpt TypeConstOpt TypeSuffixList` | `{ identifier }` |
| `TypeArgumentOpt` | `type_less TypeArgumentList type_greater` | `{ type_less }` |
| `TypeArgumentOpt` | `ε` | `{ kw_const, type_amp, type_star, semicolon, comma, r_paren, kw_as }` |
| `TypeArgument` | `Type` | `{ identifier }` |
| `TypeArgument` | `integer_literal` | `{ integer_literal }` |
| `TypeConstOpt` | `kw_const` | `{ kw_const }` |
| `TypeConstOpt` | `ε` | `{ type_amp, type_star, semicolon, comma, r_paren, kw_as }` |
| `TypeSuffixList` | `TypeSuffix TypeSuffixList` | `{ type_amp, type_star }` |
| `TypeSuffixList` | `ε` | `{ semicolon, comma, r_paren, kw_as }` |

### 4.4 表达式主链

| 非终结符 | 非 ε 产生式的 SELECT | ε 产生式的 SELECT |
| --- | --- | --- |
| `AssignmentTail` | `{ =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>= }` | `{ semicolon, comma, r_paren, r_bracket, r_brace }` |
| `LogicalOrTail` | `{ kw_or }` | `FOLLOW(LogicalOr)` |
| `LogicalAndTail` | `{ kw_and }` | `FOLLOW(LogicalAnd)` |
| `BitwiseOrTail` | `{ pipe }` | `FOLLOW(BitwiseOr)` |
| `BitwiseXorTail` | `{ caret }` | `FOLLOW(BitwiseXor)` |
| `BitwiseAndTail` | `{ amp }` | `FOLLOW(BitwiseAnd)` |
| `EqualityTail` | `{ equal_equal, bang_equal }` | `FOLLOW(Equality)` |
| `RelationalTail` | `{ less, less_equal, greater, greater_equal }` | `FOLLOW(Relational)` |
| `ShiftTail` | `{ less_less, greater_greater }` | `FOLLOW(Shift)` |
| `AdditiveTail` | `{ plus, minus }` | `FOLLOW(Additive)` |
| `MultiplicativeTail` | `{ star, slash, percent }` | `FOLLOW(Multiplicative)` |
| `CastTail` | `{ kw_as }` | `FOLLOW(Cast)` |
| `PostfixTail` | `{ l_paren, plus_plus, minus_minus }` | `FOLLOW(Postfix)` |

### 4.5 Primary

| 非终结符 | 产生式 | SELECT |
| --- | --- | --- |
| `Primary` | `Literal` | `{ integer_literal, float_literal, char_literal, string_literal, kw_true, kw_false }` |
| `Primary` | `QualifiedName` | `{ identifier }` |
| `Primary` | `ArrayLiteral` | `{ l_bracket }` |
| `Primary` | `SequenceLiteral` | `{ l_brace }` |
| `Primary` | `ParenPrimary` | `{ l_paren }` |
| `PrimaryNoBrace` | `Literal` | `{ integer_literal, float_literal, char_literal, string_literal, kw_true, kw_false }` |
| `PrimaryNoBrace` | `QualifiedName` | `{ identifier }` |
| `PrimaryNoBrace` | `ArrayLiteral` | `{ l_bracket }` |
| `PrimaryNoBrace` | `ParenPrimary` | `{ l_paren }` |

### 4.6 LL(1) 判断

`test/parser/suites/ll1_behavior_test.cpp` 对 `cp_ll1_grammar()` 自动计算 FIRST / FOLLOW / SELECT，并检查同一非终结符下任意两条产生式的 SELECT 交集。当前结果是：

- SELECT 集两两不相交
- 该语言子集满足 LL(1) 条件

因此可以使用单符号前瞻的递归下降语法分析器实现。

## 5. 程序实现说明

### 5.1 结构

- `parser/support/ast.cppm`: 最小 AST
- `parser/support/diagnostic.cppm`: 语法诊断
- `parser/support/trace.cppm`: 扁平 trace 事件
- `parser/core/grammar.cppm`: 文法与 FIRST/FOLLOW/SELECT 计算
- `parser/core/recursive_descent.cppm`: 递归下降主体
- `parser/tool/main.cpp`: CLI demo

### 5.2 递归下降策略

- 每个主要非终结符对应一个成员函数
- 表达式严格按 `assignment -> ... -> primary` 层级实现
- 类型解析函数内部支持把 `>>` 拆成两个 `>`，从而正确处理 `array<array<i32,3>,2>`
- `Type(expr)` 在语法上按“限定名 + 调用后缀”处理；是否真为 cast 留到语义阶段

### 5.3 诊断与恢复

- 若 lexer 已报错或出现 `invalid` token，parser 直接拒绝，并追加一条 `lexical_failure`
- 语句层同步点：`;`、`}`、`eof`
- 顶层同步点：`;`、`import`、`export`、`identifier`、`eof`

### 5.4 测试

- `parser_valid_cases_test`: 设计样例与补充正例
- `parser_invalid_cases_test`: 缺分号、缺右括号、错误类型参数、非法保留字等反例
- `parser_api_behavior_test`: AST 结构、trace、词法失败透传
- `parser_ll1_behavior_test`: FIRST/FOLLOW/SELECT 自动计算与冲突检查
