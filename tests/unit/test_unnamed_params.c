// Test unnamed function parameters
void foo(int x, int) {  // Second parameter is unnamed
    // Only x is accessible
}

int main() {
    foo(1, 2);
    return 0;
}
