// Test variadic function without named parameters (C23 feature)
void foo(...) {  // No named parameter before ...
    // Variadic function
}

int main() {
    foo(1, 2, 3);
    return 0;
}
