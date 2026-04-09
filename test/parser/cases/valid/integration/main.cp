import math;
import types_demo;
import flow_demo;

main()
{
    let x: i32 = 1;
    let sum = math::add(x, 6);
    let avg = math::average(sum as f64, 10.0);
    let stable = math::clamp_zero(-3);

    let builtins = types_demo::literals_and_casts();

    let data: array<i32,4> = [3, -1, 4, 0];
    let total = flow_demo::sum_non_negative(data);
    let prefix = flow_demo::accumulate_until(4);
    let finished = flow_demo::countdown(3);

    let rows: array<array<i32,3>,2> = [[1, 2, 3], [-1, 4, 5]];
    let has_four = flow_demo::contains_target(rows, 4);
}
