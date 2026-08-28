#include <iostream>
#include <string>
#include <vector>

int main() {
    int n, m;
    std::cin >> n >> m;
    std::vector<std::string> grid(n);
    for (auto& row : grid) std::cin >> row;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (grid[i][j] == '.') std::cout << i << ' ' << j << '\n';
}
