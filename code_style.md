# Code Style

1. 函数统一使用 trailing return type，形式为 `auto f(...) -> T`；

2. 构造返回对象和临时配置对象时，优先使用 designated initializer；局部常量写作 `auto const`，参数和成员保持 `type const&` 风格。

3. 统一使用struct

    成员变量放在底部

    需要private则写一个private

    不写多余的public和private 只写有必要的

    struct 左大括号换行

4. 循环使用基于范围的for循环，搭配ranges+views使用
    如果范围比较短，用范围for消耗 如 for(auto const i : std::views::iota(0,n))
    范围比较长用std::ranges::for_each消耗，如 (走)
    std::ranges::for_each (

    ​	std::views::iota(0,n)
    ​	| std::views::take(k)
    ​	| std::views::filter(\[\](auto const i) {
    ​		return i & 1;	

    ​	}),
    ​	\[\](auto const i) {
    ​		std::println("{}",i);

    ​	}

    );
    lambda不要压行

5. 局部变量修饰顺序 auto constexpr static inline name = type{ exp };

6. 优先使用字面量，例如uz(size_t)和sv(string_view)
