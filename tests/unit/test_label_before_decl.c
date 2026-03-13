// Test labels before declarations (C23 feature)
int main() {
    goto skip;
skip:
    int x = 42;  // Declaration immediately after label
    return x;
}
