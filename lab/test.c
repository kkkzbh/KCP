/* miniC lexer sample */
int add(int a, int b) {
    return a + b;
}

int main() {
    int value = add(1, 2);
    if (value >= 3 && value != 0) {
        value = value - 1;
    } else {
        value = 0;
    }
    while (value > 0) {
        value = value - 1;
    }
    return value;
}
