class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var limit = Integer.parseInt(raw);
        var index = 0;
        var total = 0;

        while(index < limit) {
            if(index % 7 == 0) {
                total += 3;
            } else if(index % 5 == 0) {
                total += 2;
            } else {
                total += 1;
            }
            index += 1;
        }

        System.exit(total % 251);
    }
}
