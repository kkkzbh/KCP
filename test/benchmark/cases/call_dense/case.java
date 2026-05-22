class Main {
    static int stepA(int value) {
        return (value * 3 + 1) % 1009;
    }

    static int stepB(int value) {
        return (stepA(value) + stepA(value + 7)) % 1009;
    }

    static int stepC(int value) {
        return (stepB(value) * 5 + stepA(value - 3)) % 1009;
    }

    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var index = 0;
        var total = 0;
        while(index < count) {
            total = (total + stepC(index)) % 1000003;
            index += 1;
        }
        System.exit(total % 251);
    }
}
