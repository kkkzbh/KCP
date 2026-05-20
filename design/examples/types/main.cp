main() -> i32
{
    let flag: bool = true;
    const letter: char = 'x';
    let count = 42;
    let ratio = 0.5;
    let title: str = "";

    let data: [i32; 3] = [4, 5, 6];
    let picked = data[1];
    data[2] = picked + 1;

    let rows: [[i32; 3]; 2] = [[1, 2, 3], [4, 5, 6]];
    let nested = rows[1][0];

    let triple: (i32, f64, char) = (count, ratio, letter);
    let first_field = triple.0;
    let pair = (10, 20);
    let ref
        (left, right) = pair;
    left = 21;
    type pair_item = decltype(right);
    let copied_right: pair_item = right;
    let (copy_left, copy_right) = pair;

    let count_ref: i32& = count;
    let count_ptr: i32* = &count;
    let count_ptr_ptr: i32** = &count_ptr;
    let count_ptr_ref: i32*& = count_ptr;
    const fixed_count: i32 = count;
    let readonly_count: i32 const& = fixed_count;

    let widened: f64 = count as f64;
    let narrowed = (ratio + widened / 42.0) as i32;

    if(not flag or count == 0) {
        return 0;
    }

    return readonly_count + count_ref + narrowed + data[2] + nested + [1, 2, 3][0]
        + first_field + copied_right + copy_left + copy_right - 103;
}
