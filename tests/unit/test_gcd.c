int printf(char *fmt, ...);

int gcd(int a, int b) {
    while (b != 0) {
        int temp;
        temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int lcm(int a, int b) {
    return a / gcd(a, b) * b;
}

int main() {
    printf("GCD and LCM calculations:\n");
    printf("gcd(48, 18)  = %d\n", gcd(48, 18));
    printf("gcd(100, 75) = %d\n", gcd(100, 75));
    printf("gcd(17, 13)  = %d\n", gcd(17, 13));
    printf("lcm(12, 18)  = %d\n", lcm(12, 18));
    printf("lcm(4, 6)    = %d\n", lcm(4, 6));
    printf("lcm(7, 5)    = %d\n", lcm(7, 5));
    return 0;
}
