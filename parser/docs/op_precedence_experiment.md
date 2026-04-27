# cp 实验三 2.2 算符优先语法分析设计

## 1. 目标与定位

本次实验三选择输入方式 (c) ——「输入文法，由程序自动生成该文法的算符优先关系矩阵」——并把它做成两件事：

1. 教学用的算符优先分析器（本文档主线），完整覆盖实验要求 (1)(c)、(2)、(3)。
2. 主管线表达式分析器的"事实校准器"。cp 真编译器的表达式分析走的是 clang / rust 形态：前缀 / 调用 / 后缀仍由递归下降处理，二元算符由 Pratt（绑定力 binding power）解析。算符优先模块自动生成的优先矩阵被一致性测试拿来与 Pratt 绑定力表交叉对照，未来调整任何算符的优先级只需改一处 (`parser/core/expression_precedence.cppm`)，CI 自动检查不漂移。

> 这一节同时说明：实验三模块不会进入"token → AST"主路径（那条路径是 Pratt），但它和主路径共享算符优先级的"事实之源"，因此并不是一次性代码。

## 2. 文法选择

文法不能直接套教材里的 `G[E]: E → E+T | T; T → T*F | F; F → (E) | i`，因为我们要让"自动构表 → 自动驱动"对实际 cp 语言的二元算符（19 个，含 `as`）生效。所以做了一层小改造：

- 把 cp 中所有真正的二元算符按"绑定力（左结合）"分层。
- 以 `kw_as`（`X as Type`）为例外：它的右手边是类型而不是表达式，没法塞进二元算符上下文，因此**不进入算符优先文法**，但仍存在于 Pratt 表中，由主管线递归下降单独 dispatch。
- 剩余 18 个算符按 cp design/docs 的优先级（与 C++ 一致，去掉 `<=>`）形成 10 个层级。每一层 `E_i` 写成一对教科书形式产生式：`E_i → E_i op E_{i+1}` 与 `E_i → E_{i+1}`。最底层 `E_{10}` 落到 primary：括号包裹的表达式与若干字面量 / `identifier`。

也就是说自动构表的输入文法形式上是：

```text
E0  → E0 or  E1 | E1
E1  → E1 and E2 | E2
E2  → E2 |   E3 | E3
E3  → E3 ^   E4 | E4
E4  → E4 &   E5 | E5
E5  → E5 ==  E6 | E5 != E6 | E6
E6  → E6 <   E7 | E6 <= E7 | E6 > E7 | E6 >= E7 | E7
E7  → E7 <<  E8 | E7 >> E8 | E8
E8  → E8 +   E9 | E8 -  E9 | E9
E9  → E9 *  E10 | E9 / E10 | E9 % E10 | E10
E10 → ( E0 )
     | identifier | integer_literal | float_literal
     | char_literal | string_literal | true | false
```

终结符均使用 lexer `token_kind` 的字符串名（`plus`、`star`、`l_paren`、`identifier` …），这样能直接复用 `to_string(token_kind)`，无需另维护映射表。`build_op_precedence_table` 的输入是这套通用 `grammar_definition`，不绑死 cp，任何文法都能跑。

文法生成器代码在 `parser/core/operator_precedence.cppm:cp_expression_op_grammar()`，**它直接读取共享的 `cp_binary_operator_table` 并按 `left_bp` 分层**——这是实现"两块共享同一份算符优先级"的关键。

## 3. 算法

### 3.1 算符文法判定

对每条产生式 `A → X1 X2 ... Xn`：

- 若存在 `Xi`、`Xi+1` 都是非终结符 ⇒ 不是算符文法。
- 若 RHS 为 ε ⇒ 不是算符文法。

任一条违反，`is_operator_grammar=false`，并向 `conflicts` 写入一条解释性诊断。

### 3.2 FIRSTVT(P) 与 LASTVT(P)

按教材定义反复迭代到收敛：

- `P → a...` 或 `P → Q a...`（`a` 终结符） ⇒ `a ∈ FIRSTVT(P)`
- `P → Q ...` ⇒ `FIRSTVT(Q) ⊆ FIRSTVT(P)`
- LASTVT 取对称形式

实现位于 `compute_firstvt_lastvt`，使用 `changed` 标志的固定点循环。

### 3.3 优先关系矩阵构造

遍历所有产生式 `A → X1...Xn`，对每个相邻 / 间隔位置：

- `Xi`、`Xi+1` 都是终结符 ⇒ `Xi =· Xi+1`
- `Xi`、`Xi+2` 都是终结符且 `Xi+1` 是非终结符 ⇒ `Xi =· Xi+2`
- `Xi` 终结符、`Xi+1` 非终结符 ⇒ ∀ `b ∈ FIRSTVT(Xi+1)`：`Xi <· b`
- `Xi` 非终结符、`Xi+1` 终结符 ⇒ ∀ `a ∈ LASTVT(Xi)`：`a ·> Xi+1`

边界 `$`：

- ∀ `a ∈ FIRSTVT(start)`：`$ <· a`
- ∀ `a ∈ LASTVT(start)`：`a ·> $`
- `$ =· $`（接受）

写表时若同一格已存在并与新关系冲突，置 `is_operator_precedence_grammar=false` 并追加一条 `conflicts` 描述（含两侧的关系符号），便于排错。

### 3.4 表驱动 shift-reduce 驱动器

栈 `S = [$]`，输入串结尾追加 `$`：

1. 取 `S` 中最靠右的终结符位置 `j`，比较 `S[j]` 与当前输入符号 `a`。
2. `<·` 或 `=·` ⇒ 移进 `a`。
3. `·>` ⇒ 从右向左找最近的 `<·`，把这之后的整段（含中间的占位非终结符）弹出，压入占位符 `N`——只识别"最左素短语"，不还原到具体产生式（教科书做法）。
4. 找不到关系 / 关系为 none ⇒ 写一条诊断并失败。
5. 当栈为 `[$, N]` 且当前输入为 `$` 时接受。

可选 `trace_enabled` 时每一步都写一个 `trace_event`，含栈快照、剩余输入、动作字符串，便于教学演示和单元测试比对。

> 实现细节：占位符 `N` 不在文法的非终结符集合里，会被 `last_terminal_index` 误判成终结符。为此驱动器**先把 `N` 注入临时非终结符集合**再开始驱动。这一点很容易踩坑。

## 4. cp 表达式子集的构表结果（节选）

对 `cp_expression_op_grammar()` 自动生成的 18×18 矩阵（连同 `$`，共 28×28，含 `(` `)` 和 7 种 primary 终结符），关键观察：

- `+ <· *`、`* ·> +`：与 C/C++ 一致，乘法绑定力高于加法。
- `+ ·> +`、`* ·> *`：左结合体现为同优先级算符之间为 `·>`。
- `( =· )`：括号匹配。
- `$ <· identifier`、`identifier ·> $`：起始 / 接受边界。
- 算符之间总是存在确定的关系（`·>` / `<·`），没有 `none`。
- 任意字面量与字面量之间是 `none`，任意字面量与 `(` 之间也是 `none`：这正好是"两个相邻 atom 缺少算符"这一类语法错的捕获机制。

完整矩阵和 FIRSTVT / LASTVT 由 `op_precedence_demo` 在每次运行时自动打印（见下一节）。

## 5. 运行实例

构建：

```bash
cmake --build build --target op_precedence_demo
```

例 1：合法表达式（接受）

```bash
echo 'a + b * c' | ./build/parser/op_precedence_demo - --trace
```

输出（截取）：

```text
Terminal stream: identifier plus identifier star identifier $
accepted
Trace:
  [$] | [identifier plus identifier star identifier $] | init
  [$ identifier] | [plus identifier star identifier $] | shift identifier
  [$ N] | [plus identifier star identifier $] | reduce identifier
  [$ N plus] | [identifier star identifier $] | shift plus
  [$ N plus identifier] | [star identifier $] | shift identifier
  [$ N plus N] | [star identifier $] | reduce identifier
  [$ N plus N star] | [identifier $] | shift star
  [$ N plus N star identifier] | [$] | shift identifier
  [$ N plus N star N] | [$] | reduce identifier
  [$ N plus N] | [$] | reduce N star N
  [$ N] | [$] | reduce N plus N
  [$ N] | [$] | accept
```

trace 中两次 `reduce N star N` 与 `reduce N plus N` 的相对顺序正好对应"乘法先于加法"的语义。

例 2：非法（缺算符，相邻 atom）

```bash
echo 'a b' | ./build/parser/op_precedence_demo -
```

输出：

```text
rejected
diag: no precedence relation between identifier and identifier
```

例 3：括号 / 不同优先级混合 `(a + b) * c` 同样被接受，可参考 `test/parser/suites/op_precedence_test.cpp` 的对应用例。

## 6. Pratt 与算符优先矩阵的一致性

主管线表达式分析器是 Pratt（`parser/core/recursive_descent.cppm:parse_expression_pratt`）。它和算符优先实验共用 `cp_binary_operator_table`：

- A 块（Pratt）：`find_binary_operator(token_kind)` 查表得到 `(left_bp, right_bp)`，配合 `min_bp` 决定移进/收回。
- B 块（实验三）：`cp_expression_op_grammar()` 按 `left_bp` 分层生成产生式，再由 `build_op_precedence_table` 构造矩阵。

两边一致性由 `op_precedence_test.cpp:check_pratt_consistency()` 强制：

```text
prev.right_bp >  next.left_bp  ⇒  prev ·> next   (左结合，prev 先归约)
prev.right_bp <= next.left_bp  ⇒  prev <· next   (移进 next)
```

测试遍历 cp 所有非 `as` 二元算符两两组合，把 Pratt 蕴含的关系与自动生成的矩阵格子逐一比对。任何一格不一致 ⇒ CI 红。今后修改任何算符的优先级，只需改 `cp_binary_operator_table` 那张表，A 块 Pratt 与 B 块矩阵会一起跟着变，且这条断言会兜底确认。

`kw_as` 在两边的处理方式不同：Pratt 表里它有绑定力（130/131），但其右侧是 `Type`，因此 Pratt 在循环里专门 dispatch 到 `parse_type()`；它**不进入** `cp_expression_op_grammar()` 的产生式集合，因此一致性测试也跳过它。

## 7. 与实验要求的对应

- (1) 选择 (c)：见 `cp_expression_op_grammar()` 与 `build_op_precedence_table`。
- (2) 自动构造算符优先关系矩阵：见 `op_precedence_demo` 的输出，以及 `op_precedence_test.cpp` 的 `check_construction` 用例（断言 `is_operator_grammar` / `is_operator_precedence_grammar` 为真且 `conflicts` 为空）。
- (3) 输出分析过程：见 `op_precedence_demo --trace` 与 `op_parse_result::trace`，每一步包含栈快照、剩余串、动作。
- 拒识与诊断：相邻 atom（缺算符）以及非算符文法 / 矩阵冲突文法的负例分别由 `check_negative_cases` / `check_non_operator_grammar` / `check_matrix_conflict` 覆盖。

## 8. 关键文件清单

- `parser/core/expression_precedence.cppm` — 共享的 cp 二元算符表 `cp_binary_operator_table`，A/B 共用的事实之源。
- `parser/core/operator_precedence.cppm` — 实验三主体：FIRSTVT/LASTVT/优先矩阵构造与表驱动 shift-reduce 驱动器；导出 `cp_expression_op_grammar()`、`build_op_precedence_table`、`parse_with_op_precedence`、`tokens_to_terminal_names`。
- `parser/core/recursive_descent.cppm` — A 块 Pratt 表达式分析器（`parse_expression_pratt`），由 `parse_assignment` 入口直接调用。
- `parser/core/grammar.cppm` — LL(1) 文法已经把二元表达式长链路折叠为 `Expression → Assignment → Unary AssignmentTail`；二元算符不再出现在 LL(1) 文法里。
- `parser/tool/op_precedence_demo.cpp` — 命令行 demo：打印产生式、FIRSTVT、LASTVT、矩阵以及解析 trace。
- `test/parser/suites/op_precedence_test.cpp` — 实验三测试套件，含 Pratt↔矩阵一致性断言。
