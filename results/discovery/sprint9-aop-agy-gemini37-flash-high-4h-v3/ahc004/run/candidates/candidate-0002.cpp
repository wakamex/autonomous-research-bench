#include <iostream>
#include <string>

int main() {
    int n, m;
    std::cin >> n >> m;
    std::string ignored;
    for (int i = 0; i < m; ++i) std::cin >> ignored;
    for (int i = 0; i < n; ++i) std::cout << std::string(n, 'A') << '\n';
}
