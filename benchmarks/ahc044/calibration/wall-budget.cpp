#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

struct Candidate {
    long long error;
    std::vector<int> first;
    std::vector<int> second;
};

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
    const auto started = Clock::now();

    std::vector<long long> need(n);
    std::vector<int> order(n);
    std::vector<int> first(n);
    std::vector<int> second(n);
    std::iota(order.begin(), order.end(), 0);

    auto construct = [&] {
        for (int i = 0; i < n; ++i) {
            need[i] = 2LL * targets[i];
        }
        for (int item : order) {
            auto pick = [&] {
                return static_cast<int>(std::max_element(need.begin(), need.end()) - need.begin());
            };
            first[item] = pick();
            need[first[item]] -= targets[item];
            second[item] = pick();
            need[second[item]] -= targets[item];
        }
    };
    auto error = [&](int steps) {
        std::vector<int> counts(n);
        int position = 0;
        for (int step = 0; step < steps; ++step) {
            ++counts[position];
            position = (counts[position] & 1) ? first[position] : second[position];
        }
        long long total = 0;
        for (int i = 0; i < n; ++i) {
            total += std::llabs(1LL * counts[i] * length - 1LL * targets[i] * steps);
        }
        return total;
    };

    std::sort(order.begin(), order.end(), [&](int left, int right) {
        return targets[left] > targets[right];
    });
    construct();
    Candidate best{error(length), first, second};

    std::mt19937 random(712367821);
    std::vector<Candidate> finalists;
    int trial = 0;
    while (Clock::now() - started < std::chrono::milliseconds(1450)) {
        const int noise = 10 << ((trial / 8) % 8);
        std::iota(order.begin(), order.end(), 0);
        std::vector<int> key(n);
        for (int i = 0; i < n; ++i) {
            key[i] = targets[i] + static_cast<int>(random() % (2 * noise + 1)) - noise;
        }
        std::sort(order.begin(), order.end(), [&](int left, int right) {
            return key[left] > key[right];
        });
        construct();
        if (random() & 1) {
            for (int i = 0; i < n; ++i) {
                std::swap(first[i], second[i]);
            }
        }
        finalists.push_back({error(50000), first, second});
        std::sort(finalists.begin(), finalists.end(), [](const Candidate& left, const Candidate& right) {
            return left.error < right.error;
        });
        if (finalists.size() > 16) {
            finalists.pop_back();
        }
        ++trial;
    }

    for (const Candidate& candidate : finalists) {
        first = candidate.first;
        second = candidate.second;
        const long long full_error = error(length);
        if (full_error < best.error) {
            best = {full_error, first, second};
        }
    }
    for (int i = 0; i < n; ++i) {
        std::cout << best.first[i] << ' ' << best.second[i] << '\n';
    }
}
