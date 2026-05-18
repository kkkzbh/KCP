main() -> i32
{
    const answer: i32 = 42;
    let empty: i32 = 0;
    let ratio = 0.5;
    let widened: f64 = answer as f64;
    let rounded = i32(widened * ratio);

    if(answer > 0 and not false) {
        return rounded + empty;
    }

    return 0;
}
