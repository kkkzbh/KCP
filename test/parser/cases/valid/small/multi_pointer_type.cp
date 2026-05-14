main()
{
    let value: i32 = 1;
    let pointer: i32* = &value;
    let pointer_pointer: i32** = &pointer;
    let pointer_reference: i32*& = pointer;
    let pointer_pointer_reference: i32**& = pointer_pointer;
    let const_pointer_pointer: i32 const** = pointer_pointer;
}
