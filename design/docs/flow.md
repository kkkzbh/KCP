基本与C++保持一致

if(一个返回bool的表达式) {

}

if() {

} else if() {

} else {

}

while(一个返回bool的表达式) {

}

do {

} while(一个返回bool的表达式);

仅支持范围for
for(let x : 范围) {

}

可打lable的for
for: l1(const i : 范围) {
    for(const j : 范围) {
        break l1;   // 在l1执行break
        continue l1; // 从l1继续
    }
}