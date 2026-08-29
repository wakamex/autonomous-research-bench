#include <iostream>
#include <string>

int main() {
    int n, value;
    std::cin >> n;
    for (int i = 0; i < n * n; ++i) std::cin >> value;
    std::string row;
    for (int i = 0; i < 2 * n - 1; ++i) std::cin >> row;
}
