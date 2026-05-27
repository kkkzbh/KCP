import std;
import preprocessor;
import lexer;

// ============================================================
// 实验三：算符优先分析，采用要求中的 (a) 方式。
// ============================================================
//
// 本文件刻意做成单文件普通实验版本，方便按代码顺序阅读。
// (a) 方式的含义是：不让程序自动计算 FIRSTVT / LASTVT，
// 也不自动生成优先关系矩阵，而是人工把矩阵写在 relation_between() 里。
//
// 本实验实现的语言子集只有算术表达式：
//
//    E -> E+E | E-E | E*E | E/E | E%E | (E) | id | integer
//
// 这里的 E 是非终结符，id、integer、+、-、*、/、%、(、)、EOF 是终结符。
// lexer 会把变量名切成 identifier，把整数切成 integer_literal；
// 本实验三不关心名字和值，所以统一把二者看成 operand。
//
// 算符优先分析的核心不是递归下降，也不是 LR 状态机，而是：
//
// 1. 栈里保存已经读入的终结符和规约出的非终结符 E。
// 2. 每一轮只看“栈顶最近终结符 a”和“当前输入终结符 b”。
// 3. 查人工矩阵 relation_between(a, b)，得到三种有效关系：
//
//    a < b：b 还应该进入当前句柄，执行移进 shift。
//           代码动作：stack.push_back(b)，输入下标 index 前进。
//
//    a = b：a 与 b 成对出现，也执行移进 shift。
//           本实验只有 "(" = ")"，用于让 "( E )" 成为一个完整句柄。
//
//    a > b：栈顶已经形成可以规约的句柄，执行规约 reduce。
//           代码动作：从栈顶向左找到句柄，把句柄替换成 E。
//           规约不消耗输入，所以 index 不变，下一轮继续看同一个 b。
//
//    无关系：说明这两个终结符不能相邻，表达式非法。
//
// 人工矩阵编码的优先级和结合性：
//
// 1. * / % 高于 + -。
//    所以 + < *，遇到 a + b * c 时先移进 *，后面先规约 b * c。
//
// 2. + - 和 * / % 都是左结合。
//    所以 + > +、- > -、* > * 等同级关系选择先规约左边。
//
// 3. 括号最高。
//    ( < id、( < ( 让括号内表达式继续移进；
//    ( = ) 让右括号能入栈；
//    ) > 运算符 或 ) > EOF 让 "( E )" 先规约成 E。
//
// 4. EOF 是边界符。
//    初始栈为 EOF，输入末尾也有 EOF。
//    当输入是 EOF 且栈为 EOF E 时，整个表达式接受。
//
// 本实验只做语法接受/拒绝并输出 shift/reduce 过程；
// 不构造 AST，不做类型检查，不计算表达式值。
enum op_terminal : u8 {
    // 不属于本实验三表达式子集的 token 会映射到 invalid 并直接报错。
    invalid = 0;
    // 输入结束符，也是分析栈初始边界符。
    eof = 1;
    // identifier 和 integer_literal 统一映射为 operand，对应文法里的 id / integer。
    operand = 2;
    // 左括号终结符 "("。
    l_paren = 3;
    // 右括号终结符 ")"。
    r_paren = 4;
    // 低优先级加法运算符 "+"。
    plus = 5;
    // 低优先级减法运算符 "-"。
    minus = 6;
    // 高优先级乘法运算符 "*"。
    star = 7;
    // 高优先级除法运算符 "/"。
    slash = 8;
    // 高优先级取模运算符 "%"。
    percent = 9;
}

// 人工算符优先关系矩阵中的单元值。
// 矩阵行是栈顶最近终结符 left，列是当前输入终结符 right。
enum op_relation : u8 {
    // none 表示矩阵中没有关系，两个终结符不能这样相邻。
    none = 0;
    // less 表示 left < right，对应移进。
    less = 1;
    // equal 表示 left = right，本实验只用于括号配对。
    equal = 2;
    // greater 表示 left > right，对应规约。
    greater = 3;
}

// 分析栈元素只有两类：原始终结符，或规约出来的表达式 E。
enum stack_item_kind : u8 {
    // 终结符：来自输入 token，例如 id、+、*、(、)。
    terminal = 0;
    // 非终结符：若干符号按产生式规约得到的 E。
    expression = 1;
}

// 分析栈里的一个元素。
//
// 例如分析 value + 2 时，某一时刻栈可能是：
//
//   EOF E + id
//
// 其中 EOF、+、id 是 terminal，E 是 expression。
// 算符优先矩阵只比较 terminal，E 只参与句柄规约。
struct stack_item {
    // terminal 表示还没规约的 token；expression 表示已经规约出的 E。
    kind: stack_item_kind;
    // 只有 kind 为 terminal 时这个字段才有意义。
    terminal: op_terminal;
}

impl stack_item {
    stack_item()
    {
        return stack_item{ .kind = stack_item_kind::terminal, .terminal = op_terminal::eof };
    }
}

// 单次表达式分析的结果。
//
// accepted 只表示语法层面是否被这个算符优先文法接受。
// steps 用来给演示输出编号，不参与语法判断。
struct parse_result {
    // 是否规约到 EOF E 并读到输入 EOF。
    accepted: bool;
    // 输出演示轨迹时使用的步骤计数。
    steps: usize;
}

impl parse_result {
    parse_result()
    {
        return parse_result{ .accepted = false, .steps = 0 as usize };
    }
}

// 把终结符包装成栈元素。
//
// 输入 token 流被转换成 op_terminal 后，通过这个函数进入分析栈。
terminal_item(kind: op_terminal) -> stack_item
{
    return stack_item{ .kind = stack_item_kind::terminal, .terminal = kind };
}

// 规约成功后压回栈里的统一表达式 E。
//
// 不管句柄是 id、integer、E+E 还是 (E)，规约后都变成同一个非终结符 E。
expression_item() -> stack_item
{
    return stack_item{ .kind = stack_item_kind::expression, .terminal = op_terminal::invalid };
}

// 演示输出用的终结符名称。
//
// 这只影响 println 的可读性，不影响分析逻辑。
terminal_name(kind: op_terminal) -> str
{
    if(kind == op_terminal::eof) { return "EOF"; }
    if(kind == op_terminal::operand) { return "id"; }
    if(kind == op_terminal::l_paren) { return "("; }
    if(kind == op_terminal::r_paren) { return ")"; }
    if(kind == op_terminal::plus) { return "+"; }
    if(kind == op_terminal::minus) { return "-"; }
    if(kind == op_terminal::star) { return "*"; }
    if(kind == op_terminal::slash) { return "/"; }
    if(kind == op_terminal::percent) { return "%"; }
    return "?";
}

// lexer 的 token 比实验三需要的更多，这里只接收最小算术表达式子集。
//
// 支持：
//   identifier、integer_literal、(、)、+、-、*、/、%、EOF
//
// 不支持：
//   关系运算、逻辑运算、赋值、逗号、分号、函数调用等。
//
// 不支持的 token 映射为 invalid，parse_expression() 会直接报错。
to_op_terminal(kind: token_kind) -> op_terminal
{
    if(kind == token_kind::eof) { return op_terminal::eof; }
    if(kind == token_kind::identifier or kind == token_kind::integer_literal) { return op_terminal::operand; }
    if(kind == token_kind::l_paren) { return op_terminal::l_paren; }
    if(kind == token_kind::r_paren) { return op_terminal::r_paren; }
    if(kind == token_kind::plus) { return op_terminal::plus; }
    if(kind == token_kind::minus) { return op_terminal::minus; }
    if(kind == token_kind::star) { return op_terminal::star; }
    if(kind == token_kind::slash) { return op_terminal::slash; }
    if(kind == token_kind::percent) { return op_terminal::percent; }
    return op_terminal::invalid;
}

// 加减是低优先级二元运算符。
//
// 在人工矩阵中，+/- 遇到 */% 时返回 less，表示后者先移进并优先规约。
is_add_op(kind: op_terminal) -> bool
{
    return kind == op_terminal::plus or kind == op_terminal::minus;
}

// 乘除模是高优先级二元运算符。
//
// 在人工矩阵中，*/% 遇到 +/-、*/%、)、EOF 时返回 greater，表示先规约。
is_mul_op(kind: op_terminal) -> bool
{
    return kind == op_terminal::star or kind == op_terminal::slash or kind == op_terminal::percent;
}

// 本实验三只支持这些二元运算符。
//
// reduce_handle() 用它判断 E op E 里的 op 是否合法。
is_binary_op(kind: op_terminal) -> bool
{
    return is_add_op(kind) or is_mul_op(kind);
}

// 复用 lab 预处理器和词法分析器，使实验三从 token 开始。
//
// 编译原理流程中，算符优先分析属于语法分析阶段；
// 它不直接读字符，而是读 lexer 已经切好的 token。
// 所以这里先调用 preprocess() 删除/保留注释空白，再调用 lex() 得到 token 流。
lex_expression(text: str) -> vector<token>
{
    let file = source_file{"<operator-precedence-expression>", text};
    let preprocessed = preprocess(file);
    if(preprocessed.diagnostics.size() != 0) {
        panic("preprocessor diagnostics in operator-precedence expression");
    }
    let lexical = lex(preprocessed);
    if(lexical.diagnostics.size() != 0) {
        panic("lexer diagnostics in operator-precedence expression");
    }
    return move lexical.tokens;
}

// 实验三采用 (a) 方式：直接人工填写算符优先关系矩阵。
//
// 这张矩阵本来可以画成表格。为了保持单文件简单，这里用 if 分支等价表示。
// 横向理解如下：
//
//   left \ right | operand  (    )    + -    * / %    EOF
//   -------------------------------------------------------
//   EOF          |   <     <    -     <       <        =
//   operand      |   -     -    >     >       >        >
//   (            |   <     <    =     <       <        -
//   )            |   -     -    >     >       >        >
//   + -          |   <     <    >     >       <        >
//   * / %        |   <     <    >     >       >        >
//
// 表中 "-" 对应 op_relation::none，表示非法相邻关系。
//
// 为什么 EOF < 运算符、( < 运算符？
// 这个单文件实验为了保持文法最短，使用了 E -> E op E。
// 当左操作数已经规约成 E 后，栈顶最近终结符可能退回 EOF 或 "("：
//
//   EOF E    当前输入是 +
//   ( E      当前输入是 -
//
// 此时必须允许移进二元运算符，否则 value + 2 会在 value 规约成 E 后停止。
// 真正缺左操作数的 + value 也会先移进 +，但最终无法匹配 E+E，
// reduce_handle() 会返回 false，从而拒绝输入。
relation_between(left: op_terminal, right: op_terminal) -> op_relation
{
    // EOF 是输入边界；EOF = EOF 只表示两个结束符相遇。
    if(left == op_terminal::eof and right == op_terminal::eof) { return op_relation::equal; }
    // EOF < 运算符看起来反直觉，但对左递归文法 E -> E+E 是需要的：
    // id 先规约成 E 后，栈变成 EOF E，此时要继续移进后面的二元运算符。
    // 如果表达式真的以 + 开头，后面的 reduce_handle 会因为缺左操作数而拒绝。
    if(left == op_terminal::eof and (right == op_terminal::operand or right == op_terminal::l_paren or is_binary_op(right))) { return op_relation::less; }
    // 操作数后面遇到运算符、右括号或 EOF，说明这个操作数本身可以先规约成 E。
    if(left == op_terminal::operand and (is_binary_op(right) or right == op_terminal::r_paren or right == op_terminal::eof)) { return op_relation::greater; }

    // 左括号后面可以开始一个新的 E；( = ) 表示括号配对。
    if(left == op_terminal::l_paren and (right == op_terminal::operand or right == op_terminal::l_paren or is_binary_op(right))) { return op_relation::less; }
    if(left == op_terminal::l_paren and right == op_terminal::r_paren) { return op_relation::equal; }
    // 右括号结束了一个括号表达式，后面遇到运算符、右括号或 EOF 时先规约。
    if(left == op_terminal::r_paren and (is_binary_op(right) or right == op_terminal::r_paren or right == op_terminal::eof)) { return op_relation::greater; }

    // +、- 遇到操作数、左括号或更高优先级的 *、/、% 时继续移进。
    if(is_add_op(left) and (right == op_terminal::operand or right == op_terminal::l_paren or is_mul_op(right))) { return op_relation::less; }
    // +、- 遇到同级运算符、右括号或 EOF 时规约，体现左结合。
    if(is_add_op(left) and (is_add_op(right) or right == op_terminal::r_paren or right == op_terminal::eof)) { return op_relation::greater; }

    // *、/、% 后面还需要一个右操作数，所以遇到操作数或左括号时移进。
    if(is_mul_op(left) and (right == op_terminal::operand or right == op_terminal::l_paren)) { return op_relation::less; }
    // *、/、% 优先级最高；遇到任意二元运算符、右括号或 EOF 时先规约。
    if(is_mul_op(left) and (is_binary_op(right) or right == op_terminal::r_paren or right == op_terminal::eof)) { return op_relation::greater; }
    return op_relation::none;
}

// 算符优先分析只比较“栈顶最近终结符”和“当前输入终结符”。
// 规约出来的 E 不是终结符，查矩阵时要跳过。
//
// 例如栈为：
//
//   EOF E + E * E
//
// 从右往左看，最近的终结符是 *，不是最后一个 E。
// 下一步如果当前输入是 EOF，就查 * > EOF，然后先规约 E*E。
top_terminal_index(stack: vector<stack_item> const&) -> usize
{
    let index = stack.size();
    while(index > 0 as usize) {
        index -= 1;
        if(stack[index].kind == stack_item_kind::terminal) {
            return index;
        }
    }
    return 0 as usize;
}

// 从某个终结符左侧继续找上一个终结符，用于回溯句柄左边界。
//
// find_handle_start() 会反复调用它。因为句柄内部可能夹着 E，
// 所以不能简单看相邻栈元素，而要找“上一个终结符”。
previous_terminal_index(stack: vector<stack_item> const&, before: usize) -> usize
{
    let index = before;
    while(index > 0 as usize) {
        index -= 1;
        if(stack[index].kind == stack_item_kind::terminal) {
            return index;
        }
    }
    return 0 as usize;
}

// 当 relation_between(previous, current) 是 < 时，
// previous 右边就是当前句柄的起点。
//
// 理论对应：
//   算符优先分析中，句柄左边界由最靠右的 < 关系确定。
//
// 例子：
//   栈：EOF E + E * E
//   当前输入：EOF
//   栈顶最近终结符 current 是 *
//   previous 是 +
//   relation_between(+, *) 是 <
//   所以句柄从 + 右边开始，即 E * E。
//
// 如果找不到 <，说明栈中没有完整句柄，输入非法。
find_handle_start(stack: vector<stack_item> const&) -> usize
{
    let current = top_terminal_index(stack);
    while(current > 0 as usize) {
        let previous = previous_terminal_index(stack, current);
        if(relation_between(stack[previous].terminal, stack[current].terminal) == op_relation::less) {
            return previous + 1;
        }
        current = previous;
    }
    return stack.size();
}

// 把找到的句柄规约成 E。
// 本实验只识别三种句柄：
// 1. id 或 integer
// 2. E op E
// 3. ( E )
//
// 注意这里不区分具体的 E 类型，例如 Additive、Multiplicative。
// 这是因为本实验把优先级全部放在人工矩阵里，而不是放在多层文法里。
// 只要矩阵保证先规约高优先级运算，E op E -> E 就足够演示实验三。
reduce_handle(stack: vector<stack_item>&, start: usize) -> bool
{
    let count = stack.size() - start;
    // id -> E，integer -> E。
    // operand 是 identifier 和 integer_literal 的合并终结符。
    if(count == 1 as usize and stack[start].kind == stack_item_kind::terminal and stack[start].terminal == op_terminal::operand) {
        stack.erase_range(start, stack.size());
        stack.push_back(expression_item());
        return true;
    }
    if(count == 3 as usize) {
        let first = stack[start];
        let second = stack[start + 1];
        let third = stack[start + 2];
        // E op E -> E。
        // 这一步对应所有二元算术产生式：
        //   E -> E+E | E-E | E*E | E/E | E%E
        if(
            first.kind == stack_item_kind::expression
            and second.kind == stack_item_kind::terminal
            and is_binary_op(second.terminal)
            and third.kind == stack_item_kind::expression
        ) {
            stack.erase_range(start, stack.size());
            stack.push_back(expression_item());
            return true;
        }
        // ( E ) -> E。
        // 左右括号能一起规约，是因为矩阵中设置了 ( = )。
        if(
            first.kind == stack_item_kind::terminal
            and first.terminal == op_terminal::l_paren
            and second.kind == stack_item_kind::expression
            and third.kind == stack_item_kind::terminal
            and third.terminal == op_terminal::r_paren
        ) {
            stack.erase_range(start, stack.size());
            stack.push_back(expression_item());
            return true;
        }
    }
    return false;
}

// 输入读到 EOF，栈中只剩 EOF 和一个 E，就说明整个表达式被接受。
accept_stack(stack: vector<stack_item> const&) -> bool
{
    return stack.size() == 2 as usize
        and stack[0 as usize].kind == stack_item_kind::terminal
        and stack[0 as usize].terminal == op_terminal::eof
        and stack[1 as usize].kind == stack_item_kind::expression;
}

// 对一个表达式字符串执行完整实验三流程：
// 源字符串 -> 预处理 -> 词法分析 -> 实验三终结符流 -> 算符优先分析。
//
// 算符优先主循环的标准动作：
//
//   1. a = 栈顶最近终结符
//   2. b = 当前输入终结符
//   3. 查 relation_between(a, b)
//   4. a < b 或 a = b：移进 b
//   5. a > b：规约栈顶句柄
//   6. 无关系：报错
//
// 终止性说明：
// - 每次 shift 都会让 index 前进，输入 token 数有限。
// - 每次 reduce 都会把 id、E op E 或 (E) 替换成 E。
// - 如果既不能 shift 也不能 reduce，就立即返回错误。
// 因此这里不需要再放一个“最大步数”上限。
parse_expression(text: str) -> parse_result
{
    println("input: {}", text);

    // lexer 输出的是完整 token_kind；下面会再收窄成本实验三需要的 op_terminal。
    let tokens = lex_expression(text);

    // input 是算符优先分析器实际读取的终结符流。
    // 这里复用 stack_item 保存输入，是因为本项目里的 vector 元素需要可默认构造；
    // stack_item 已经有默认构造，而裸 enum 不适合作为 vector 元素。
    let input = vector<stack_item>{};
    let token_index: usize = 0;
    while(token_index < tokens.size()) {
        let terminal = to_op_terminal(tokens[token_index].kind);
        if(terminal == op_terminal::invalid) {
            println("error: unsupported token");
            return parse_result{};
        }
        input.push_back(terminal_item(terminal));
        token_index += 1;
    }

    let result = parse_result{};

    // 初始栈只放 EOF。
    // 输入尾部也来自 lexer 的 EOF token。
    // 这是算符优先分析常用的边界符，报告里也常写成 #。
    let stack = vector<stack_item>{};
    stack.push_back(terminal_item(op_terminal::eof));

    // index 指向当前还没有处理的输入终结符。
    // shift 会消耗当前输入，因此 index 前进；
    // reduce 只改栈，不消耗输入，因此 index 不变。
    let index: usize = 0;
    while(true) {
        // 正常 token 流一定带 EOF。如果这里越界，说明输入流本身不完整。
        if(index >= input.size()) {
            println("error: input exhausted");
            return result;
        }

        let lookahead = input[index].terminal;

        // 接受条件必须在查矩阵前判断。
        // 当栈已经是 EOF E，并且当前输入也是 EOF，表示整个表达式已经规约完成。
        if(lookahead == op_terminal::eof and accept_stack(stack)) {
            println("{}: accept", result.steps + 1);
            return parse_result{ .accepted = true, .steps = result.steps };
        }

        // 算符优先矩阵只看终结符，不看栈顶是否是 E。
        // 所以先找到“栈顶最近终结符”。
        let top_index = top_terminal_index(stack);
        let top = stack[top_index].terminal;
        let relation = relation_between(top, lookahead);
        if(relation == op_relation::less or relation == op_relation::equal) {
            // 移进会消耗一个输入终结符，所以 index 前进。
            // EOF 不能被移进；EOF 只用于最后的接受判断。
            if(lookahead == op_terminal::eof) {
                println("{}: error before EOF", result.steps + 1);
                return result;
            }
            stack.push_back(terminal_item(lookahead));
            result.steps += 1;
            println("{}: shift {}", result.steps, terminal_name(lookahead));
            index += 1;
        } else if(relation == op_relation::greater) {
            // 规约不消耗输入；它只把栈顶句柄替换成 E。
            // 下一轮仍然拿同一个 lookahead 继续比较。
            //
            // 先根据终结符之间的 < 关系确定句柄左边界，
            // 再检查这个句柄是否真的是 id、E op E 或 (E)。
            let start = find_handle_start(stack);
            if(start >= stack.size() or not reduce_handle(stack, start)) {
                println("{}: reduce failed", result.steps + 1);
                return result;
            }
            result.steps += 1;
            println("{}: reduce", result.steps);
        } else {
            // none 表示人工矩阵没有给出关系。
            // 例如 value * ) 会在 * 和 ) 之间没有合法句柄，从而走到错误。
            println("{}: error near {}", result.steps + 1, terminal_name(lookahead));
            return result;
        }
    }
    return result;
}

main() -> i32
{
    // 打印本实验使用的文法和矩阵来源，方便截图或写实验报告。
    println("grammar: E -> E+E | E-E | E*E | E/E | E%E | (E) | id | integer");
    println("matrix: handwritten operator precedence table, experiment 3 option (a)");

    // 正确样例：验证括号、加减、乘法优先级都能工作。
    let good = parse_expression("value + 2 * (limit - 3)");
    if(not good.accepted) { return 1; }

    // 错误样例：+ 后面直接跟 *，没有右操作数，最终无法规约成 E op E。
    let bad = parse_expression("value + * 3");
    if(bad.accepted) { return 2; }

    // 错误样例：本实验只支持二元 +，不支持一元 +。
    let unary = parse_expression("+ value");
    if(unary.accepted) { return 3; }

    println("operator-precedence demo ok");
    return 0;
}
