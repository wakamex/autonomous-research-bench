#include <ctime>
#include <iostream>
#include <vector>

namespace {

double process_seconds() {
    timespec now{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) / 1e9;
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n;
    int length;
    if (!(std::cin >> n >> length)) {
        return 0;
    }
    std::vector<int> targets(n);
    for (int& target : targets) {
        std::cin >> target;
    }

    const double started = process_seconds();
    volatile unsigned long long state = 0x9e3779b97f4a7c15ULL;
    while (process_seconds() - started < 1.5) {
        for (int i = 0; i < 10000; ++i) {
            state ^= state << 7;
            state ^= state >> 9;
            state *= 0xbf58476d1ce4e5b9ULL;
        }
    }

    for (int i = 0; i < n; ++i) {
        std::cout << i << ' ' << i << '\n';
    }
    return state == 0 ? 1 : 0;
}
