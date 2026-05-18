main() -> i32
{
    let flag: bool = true;
    const letter: char = 'x';
    let count = 42;
    let ratio = 0.5;
    let title: str = "";

    let data: array<i32,3> = [4, 5, 6];
    let picked = data[1];
    data[2] = picked + 1;

    let rows: array<array<i32,3>,2> = [[1, 2, 3], [4, 5, 6]];
    let nested = rows[1][0];

    let triple: tuple<i32,f64,char> = (count, ratio, letter);

    let count_ref: i32& = count;
    let count_ptr: i32* = &count;
    let count_ptr_ptr: i32** = &count_ptr;
    let count_ptr_ref: i32*& = count_ptr;
    const fixed_count: i32 = count;
    let readonly_count: i32 const& = fixed_count;

    let widened: f64 = count as f64;
    let narrowed = i32(ratio + widened / 42.0);

    if(not flag or count == 0) {
        return 0;
    }

    return readonly_count + count_ref + narrowed + data[2] + nested + [1, 2, 3][0];
}
