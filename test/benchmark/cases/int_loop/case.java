class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var limit = Long.parseLong(raw);
        var index = 0L;
        var total = 7L;

        while(index < limit) {
            total = (total + ((index * 31 + 17) % 1009)) % 1000003;
            index += 1;
        }

        System.exit((int)(total % 251));
    }
}
