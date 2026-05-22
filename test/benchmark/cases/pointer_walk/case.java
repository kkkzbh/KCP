class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var values = new int[count];
        var index = 0;
        while(index < count) {
            values[index] = (index * 17 + 3) % 1009;
            index += 1;
        }

        var total = 0L;
        var cursor = 0;
        while(cursor < values.length) {
            total += values[cursor];
            cursor += 1;
        }

        System.exit((int)(total % 251));
    }
}
