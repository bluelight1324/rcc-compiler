int printf(char *fmt, ...);

void bubble_sort(int *data, int n) {
    int i;
    int j;
    int temp;
    i = 0;
    while (i < n - 1) {
        j = 0;
        while (j < n - 1 - i) {
            if (data[j] > data[j + 1]) {
                temp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = temp;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

int main() {
    int data[10];
    int i;
    int n;

    n = 10;

    data[0] = 64;
    data[1] = 34;
    data[2] = 25;
    data[3] = 12;
    data[4] = 22;
    data[5] = 11;
    data[6] = 90;
    data[7] = 1;
    data[8] = 45;
    data[9] = 78;

    printf("Before sorting: ");
    i = 0;
    while (i < n) {
        printf("%d ", data[i]);
        i = i + 1;
    }
    printf("\n");

    bubble_sort(data, n);

    printf("After sorting:  ");
    i = 0;
    while (i < n) {
        printf("%d ", data[i]);
        i = i + 1;
    }
    printf("\n");

    return 0;
}
