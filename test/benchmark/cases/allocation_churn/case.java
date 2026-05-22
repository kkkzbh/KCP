class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var outer = 0;
        var total = 0;
        while(outer < count) {
            var values = new int[32];
            var index = 0;
            while(index < 32) {
                values[index] = (outer + index * 13) % 997;
                index += 1;
            }
            total = (total + values[outer % 32]) % 1000003;
            outer += 1;
        }
        System.exit(total % 251);
    }
}
