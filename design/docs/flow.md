# 流程控制

本文档记录 cp 流程控制语句。整体语法接近 C++，但逻辑运算使用 `and` / `or` / `not`，范围 `for` 采用当前语言自己的绑定语法。

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

规则：

- 绑定关键字必须是 `let` 或 `const`。
- 范围表达式的目标语义基于 [iteration.md](iteration.md)：表达式必须能产生 `iterator`，也就是实现 `iterable` 或本身实现 `iterator`。
- 循环变量类型为 iterator 的 `iter_item`。
- `const` 循环变量不能被重新赋值。

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
- 标签只能标记 `for`。
- 带标签的 `break` / `continue` 必须解析到外层循环标签。

## break 和 continue

```cp
break;
continue;
break outer;
continue outer;
```

规则：

- `break` 和 `continue` 必须出现在循环内部。
- 不带标签时作用于最近的内层循环。
- 带标签时作用于对应标签的外层 `for`。

## return

```cp
return;
return value;
```

规则：

- `return value;` 的值必须能转换到当前函数返回类型。
- `return;` 只允许用于 `unit` 返回。
- 在块表达式中写 `return` 时，仍然从所在函数返回，不是从块表达式返回。
- 函数声明返回 `!` 时，函数体不能有正常完成路径；`return;` 不合法，`return value;` 只有在 `value` 本身类型为 `!` 时合法。
