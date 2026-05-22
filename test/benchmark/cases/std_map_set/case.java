import java.util.TreeMap;
import java.util.TreeSet;

class Main {
    public static void main(String[] args) {
        var raw = System.getenv("CP_BENCH_INPUT");
        if(raw == null) {
            System.exit(1);
        }

        var count = Integer.parseInt(raw);
        var values = new TreeMap<Integer, Integer>();
        var keys = new TreeSet<Integer>();

        var index = 0;
        while(index < count) {
            var key = (index * 37) % (count * 2 + 1);
            values.putIfAbsent(key, index % 1009);
            keys.add(key);
            index += 1;
        }

        var total = (long)(values.size() + keys.size());
        var probe = 0;
        while(probe < count) {
            var key = (probe * 53) % (count * 2 + 1);
            var value = values.get(key);
            if(value != null) {
                total += value;
            }
            if(keys.contains(key)) {
                total += 3;
            }
            probe += 1;
        }

        System.exit((int)(total % 251));
    }
}
