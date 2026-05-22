class Main {
    static int mixInt(int value, int salt) {
        return (value * 17 + salt) % 1009;
    }

    static long mixLong(long value, long salt) {
        return (value * 17 + salt) % 1009;
    }

    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var index = 0;
        var totalI32 = 0;
        var totalI64 = 0L;
        while(index < count) {
            totalI32 = (totalI32 + mixInt(index, 11)) % 1000003;
            totalI64 = (totalI64 + mixLong(index, 23)) % 1000003;
            index += 1;
        }
        System.exit((int)((totalI32 + totalI64) % 251));
    }
}
