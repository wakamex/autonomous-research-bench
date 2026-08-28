#include <iostream>
#include <vector>

int main() {
    int n;
    std::cin >> n;
    long long value;
    for (int i = 0; i < n * n; ++i) std::cin >> value;
    for (int i = 0; i < n; ++i) {
        if (i % 2 == 0) {
            for (int j = 0; j < n; ++j) std::cout << i << ' ' << j << '\n';
        } else {
            for (int j = n - 1; j >= 0; --j) std::cout << i << ' ' << j << '\n';
        }
    }
}
