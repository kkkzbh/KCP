export module types_demo;
let flag = true;
let other = false;
let ptr = &flag;
let raw = *ptr;
let data = [1, 2];
let seq = {3, 4};
if(not flag or raw == 0) {
    return;
}
