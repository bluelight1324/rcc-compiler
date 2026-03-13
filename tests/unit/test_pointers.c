int printf(char *fmt, ...);

int sum_array(int *arr, int n) {
    int total;
    int i;
    total = 0;
    i = 0;
    while (i < n) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}

int max_array(int *arr, int n) {
    int max;
    int i;
    max = arr[0];
    i = 1;
    while (i < n) {
        if (arr[i] > max) {
            max = arr[i];
        }
        i = i + 1;
    }
    return max;
}

int min_array(int *arr, int n) {
    int min;
    int i;
    min = arr[0];
    i = 1;
    while (i < n) {
        if (arr[i] < min) {
            min = arr[i];
        }
        i = i + 1;
    }
    return min;
}

int main() {
    int data[8];

    data[0] = 42;
    data[1] = 17;
    data[2] = 93;
    data[3] = 5;
    data[4] = 68;
    data[5] = 31;
    data[6] = 84;
    data[7] = 12;

    printf("Array: 42 17 93 5 68 31 84 12\n");
    printf("Sum: %d\n", sum_array(data, 8));
    printf("Max: %d\n", max_array(data, 8));
    printf("Min: %d\n", min_array(data, 8));
    printf("Average: %d\n", sum_array(data, 8) / 8);
    return 0;
}
