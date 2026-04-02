export module types_demo;

export literals_and_casts() -> i32
{
    let flag: bool = true;
    const letter: char = 'x';
    let count = 42;
    let ratio = 0.5;
    let title: str = "cp";

    let seq: sequence<i32,3> = {1, 2, 3};
    let data: array<i32,3> = [4, 5, 6];
    let triple: tuple<i32,f64,char> = (count, ratio, letter);

    let count_ref: i32& = count;
    let count_ptr: i32* = &count;

    let widened: f64 = count as f64;
    let narrowed = i32(ratio + widened / 42.0);

    if(not flag or count == 0) {
        return 0;
    }

    return count_ref + narrowed;
}
