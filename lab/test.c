/* miniC score sorting and statistics sample */
void selection_sort(int values[], int n) {
    for (int i = 0; i < n; i = i + 1) {
        int min = i;
        for (int j = i + 1; j < n; j = j + 1) {
            if (values[j] < values[min]) {
                min = j;
            }
        }
        if (min != i) {
            int temp = values[i];
            values[i] = values[min];
            values[min] = temp;
        }
    }
}

int count_passing(int values[], int n) {
    int count = 0;
    for (int i = 0; i < n; i = i + 1) {
        if (values[i] < 0) {
            break;
        }
        if (values[i] < 60) {
            continue;
        }
        count = count + 1;
    }
    return count;
}

int checksum(int values[], int n) {
    int total = 0;
    for (int i = 0; i < n; i = i + 1) {
        total = total + values[i] * (i + 1);
    }
    return total;
}

int main() {
    int scores[7] = {72, 55, 88, 91, 66, 79, -1};
    selection_sort(scores, 6);
    int passing = count_passing(scores, 7);
    int total = checksum(scores, 6);
    return total + passing * 100 + scores[3];
}
