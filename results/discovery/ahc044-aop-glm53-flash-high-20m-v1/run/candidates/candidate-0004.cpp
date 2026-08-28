#include <iostream>

int main() {
    int n, l;
    std::cin >> n >> l;
    for (int i = 0, target; i < n; ++i) std::cin >> target;
    for (int i = 0; i < n; ++i) std::cout << i << ' ' << i << '\n';
}
