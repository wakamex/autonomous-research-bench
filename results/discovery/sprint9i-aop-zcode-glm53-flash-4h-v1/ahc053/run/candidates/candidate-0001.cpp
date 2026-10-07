#include <iostream>
#include <vector>

int main() {
    int n, m;
    unsigned long long l, u;
    std::cin >> n >> m >> l >> u;
    for (int i = 0; i < n; ++i) std::cout << 1 + i << (i + 1 == n ? '\n' : ' ');
    std::cout.flush();
    std::vector<unsigned long long> b(m);
    for (auto& value : b) std::cin >> value;
    for (int i = 0; i < n; ++i) std::cout << 0 << (i + 1 == n ? '\n' : ' ');
    std::cout.flush();
}
