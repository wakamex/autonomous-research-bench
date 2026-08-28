#include <iostream>
#include <string>

int main() {
    int n, m, k;
    std::cin >> n >> m >> k;
    int x, y;
    for (int i = 0; i < m; ++i) std::cin >> x >> y;
    std::string row;
    for (int i = 0; i < 2 * n - 1; ++i) std::cin >> row;
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < m; ++j) std::cout << "S ";
        std::cout << '\n';
    }
}
