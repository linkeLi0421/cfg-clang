#include <stdio.h>

int calculate_sum(int a, int b) {
    int result = a + b;
    int multiplier = 2;
    result = result * multiplier;
    
    if (result > 10) {
        result = result + 5;
    } else {
        result = result - 3;
    }
    
    return result;
}

int main() {
    int x = 5;
    int y = 3;
    int sum = calculate_sum(x, y);
    printf("Result: %d\n", sum);
    return 0;
}