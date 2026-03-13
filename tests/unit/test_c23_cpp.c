// test_c23_cpp.c - Test C23 preprocessor features
// Tests: #elifdef, #elifndef, #warning

// Test #elifdef
#define FEATURE_A
#undef FEATURE_B
#define FEATURE_C

#ifdef FEATURE_X
    int test_elifdef = 1;
#elifdef FEATURE_A
    int test_elifdef = 2;  // This should be selected
#elifdef FEATURE_B
    int test_elifdef = 3;
#else
    int test_elifdef = 4;
#endif

// Test #elifndef
#ifdef FEATURE_Y
    int test_elifndef = 1;
#elifndef FEATURE_B
    int test_elifndef = 2;  // This should be selected (FEATURE_B is not defined)
#elifndef FEATURE_C
    int test_elifndef = 3;
#else
    int test_elifndef = 4;
#endif

// Test #warning (should emit warning during preprocessing)
#ifdef FEATURE_A
    #warning "FEATURE_A is defined - this is expected"
#endif

int main() {
    // Verify the preprocessor selections
    int result = 0;

    if (test_elifdef != 2) {
        result = 1;  // #elifdef failed
    }

    if (test_elifndef != 2) {
        result = 1;  // #elifndef failed
    }

    return result;
}
