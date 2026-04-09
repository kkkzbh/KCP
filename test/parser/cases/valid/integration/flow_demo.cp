export module flow_demo;

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

export accumulate_until(limit: i32) -> i32
{
    let total: i32 = 0;
    let current: i32 = 0;

    while(current < limit) {
        total = total + current;
        current = current + 1;
    }

    return total;
}

export countdown(start: i32) -> i32
{
    let current = start;

    do {
        current = current - 1;
    } while(current > 0);

    return current;
}

export contains_target(rows: array<array<i32,3>,2>, target: i32) -> bool
{
    let found: bool = false;

    for: outer(let row : rows) {
        for(const value : row) {
            if(value < 0) {
                continue outer;
            }

            if(value == target) {
                found = true;
                break outer;
            }
        }
    }

    return found;
}
