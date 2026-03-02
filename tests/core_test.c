#include <stdio.h>
#include <stdlib.h>

void test_true() {
    if (1 != 1) {
        fprintf(stderr, "FAIL: test_true\n");
        exit(1);
    }
    printf("PASS: test_true\n");
}

int main() {
    test_true();
    printf("All tests passed!\n");
    return 0;
}
