import java.util.ArrayList;

class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var values = new ArrayList<Integer>(count);
        var text = new StringBuilder(count);

        var index = 0;
        while(index < count) {
            values.add((index * 13 + 5) % 997);
            text.append((char)('a' + (index % 26)));
            index += 1;
        }

        var total = (long)text.length();
        var cursor = 0;
        while(cursor < values.size()) {
            total += values.get(cursor);
            cursor += 1;
        }
        System.exit((int)(total % 251));
    }
}
