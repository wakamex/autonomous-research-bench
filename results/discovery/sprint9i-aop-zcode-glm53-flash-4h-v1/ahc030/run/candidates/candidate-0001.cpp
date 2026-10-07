#include <iostream>
#include <utility>
#include <vector>

int main() {
    int n, m;
    double epsilon;
    std::cin >> n >> m >> epsilon;

    for (int field = 0; field < m; ++field) {
        int area;
        std::cin >> area;
        for (int cell = 0, row, column; cell < area; ++cell) {
            std::cin >> row >> column;
        }
    }

    std::vector<std::pair<int, int>> oil;
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            std::cout << "q 1 " << row << ' ' << column << std::endl;
            int amount;
            std::cin >> amount;
            if (amount > 0) oil.emplace_back(row, column);
        }
    }

    std::cout << "a " << oil.size();
    for (auto [row, column] : oil) std::cout << ' ' << row << ' ' << column;
    std::cout << std::endl;

    int accepted;
    std::cin >> accepted;
}
