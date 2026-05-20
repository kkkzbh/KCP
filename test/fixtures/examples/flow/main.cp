sum_non_negative(values: [i32; 4]) -> i32
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

countdown(start: i32) -> i32
{
    let current = start;

    do {
        current = current - 1;
    } while(current > 0);

    return current;
}

contains_target(rows: [[i32; 3]; 2], target: i32) -> bool
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

main() -> i32
{
    let values: [i32; 4] = [3, -1, 4, 0];
    let rows: [[i32; 3]; 2] = [[1, 2, 3], [-1, 4, 5]];

    if(contains_target(rows, 4)) {
        return sum_non_negative(values) + countdown(3);
    }

    return 0;
}
