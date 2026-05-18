sum<T...>(values: T...) -> i32
requires T...: display
{
    let total = 0;
    template for (let value : values...) {
        total = total + value;
    }
    template for (type U : T...) {
        type current = U;
    }
    return total;
}
