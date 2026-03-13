[[nodiscard]] int compute(int x) { return x * 2; }

[[deprecated("use compute() instead")]] int old_compute(int x) { return x + 1; }

int main(void) { compute(5); old_compute(3); return 0; }
