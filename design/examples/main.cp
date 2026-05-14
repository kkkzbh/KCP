import math;  // 导入方式
import types_demo;
import flow_demo;

main()
{
    let x: i32 = 1;
    let sum = add(x, 6);
    let avg = average(sum as f64, 10.0);
    let stable = clamp_zero(-3);

    let builtins = literals_and_casts();

    let data: array<i32,4> = [3, -1, 4, 0];
    let total = sum_non_negative(data);
    let prefix = accumulate_until(4);
    let finished = countdown(3);

    let rows: array<array<i32,3>,2> = [[1, 2, 3], [-1, 4, 5]];
    let has_four = contains_target(rows, 4);
}
