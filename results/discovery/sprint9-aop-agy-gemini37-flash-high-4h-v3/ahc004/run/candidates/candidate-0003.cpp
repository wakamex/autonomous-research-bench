#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <random>
#include <cmath>

struct TrieNode {
    int child[8];
    std::vector<int> match_ids;
};

struct FastEvaluator {
    int N = 20;
    int M;
    std::vector<TrieNode> trie;
    std::vector<std::string> S;
    
    std::vector<std::string> grid;
    std::vector<int> matched_strings[800];
    std::vector<int> match_count;
    int total_matched = 0;

    void init(int n, int m, const std::vector<std::string>& s_list) {
        N = n;
        M = m;
        S = s_list;
        trie.clear();
        trie.push_back(TrieNode());
        for (int i = 0; i < 8; ++i) trie[0].child[i] = -1;

        for (int i = 0; i < M; ++i) {
            int node = 0;
            for (char c : S[i]) {
                int ch = c - 'A';
                if (trie[node].child[ch] == -1) {
                    trie[node].child[ch] = (int)trie.size();
                    TrieNode next;
                    for (int k = 0; k < 8; ++k) next.child[k] = -1;
                    trie.push_back(next);
                }
                node = trie[node].child[ch];
            }
            trie[node].match_ids.push_back(i);
        }

        match_count.assign(M, 0);
        total_matched = 0;
        grid.assign(N, std::string(N, '.'));
        for (int p = 0; p < 800; ++p) matched_strings[p].clear();
    }

    inline void scan_position(int pos, int dir, int line, int offset) {
        for (int id : matched_strings[pos]) {
            match_count[id]--;
            if (match_count[id] == 0) total_matched--;
        }
        matched_strings[pos].clear();

        int node = 0;
        for (int k = 0; k < 12; ++k) {
            char c;
            if (dir == 0) {
                c = grid[line][(offset + k) % N];
            } else {
                c = grid[(offset + k) % N][line];
            }
            if (c < 'A' || c > 'H') break;
            int ch = c - 'A';
            node = trie[node].child[ch];
            if (node == -1) break;
            for (int id : trie[node].match_ids) {
                matched_strings[pos].push_back(id);
                if (match_count[id] == 0) total_matched++;
                match_count[id]++;
            }
        }
    }

    void set_grid(const std::vector<std::string>& new_grid) {
        grid = new_grid;
        std::fill(match_count.begin(), match_count.end(), 0);
        total_matched = 0;
        for (int p = 0; p < 800; ++p) matched_strings[p].clear();

        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                scan_position(r * 20 + c, 0, r, c);
                scan_position(400 + c * 20 + r, 1, c, r);
            }
        }
    }

    inline void update_cell(int r, int c, char new_val) {
        grid[r][c] = new_val;
        for (int k = 0; k < 12; ++k) {
            int offset = (c - k + N) % N;
            scan_position(r * 20 + offset, 0, r, offset);
        }
        for (int k = 0; k < 12; ++k) {
            int offset = (r - k + N) % N;
            scan_position(400 + c * 20 + offset, 1, c, offset);
        }
    }

    void shift_row(int r, int shift) {
        if (shift == 0) return;
        std::string old_row = grid[r];
        std::string new_row = old_row.substr(shift) + old_row.substr(0, shift);
        grid[r] = new_row;
        for (int c = 0; c < N; ++c) {
            for (int k = 0; k < 12; ++k) {
                int offset = (r - k + N) % N;
                scan_position(400 + c * 20 + offset, 1, c, offset);
            }
        }
    }

    void swap_rows(int r1, int r2) {
        if (r1 == r2) return;
        std::swap(grid[r1], grid[r2]);
        for (int c = 0; c < N; ++c) {
            scan_position(r1 * 20 + c, 0, r1, c);
            scan_position(r2 * 20 + c, 0, r2, c);
        }
        for (int c = 0; c < N; ++c) {
            for (int k = 0; k < 12; ++k) {
                int offset1 = (r1 - k + N) % N;
                scan_position(400 + c * 20 + offset1, 1, c, offset1);
                int offset2 = (r2 - k + N) % N;
                scan_position(400 + c * 20 + offset2, 1, c, offset2);
            }
        }
    }
};

int calc_overlap(const std::string& a, const std::string& b) {
    int max_ov = std::min((int)a.size(), (int)b.size()) - 1;
    for (int k = max_ov; k >= 1; --k) {
        if (a.substr(a.size() - k) == b.substr(0, k)) {
            return k;
        }
    }
    return 0;
}

struct FastMerger {
    struct Node {
        std::string s;
        bool active = true;
    };
    int N;
    std::vector<Node> nodes;
    std::vector<std::vector<int16_t>> ov_table;

    void init(int n, const std::vector<std::string>& init_strings) {
        N = n;
        nodes.clear();
        for (const auto& s : init_strings) {
            nodes.push_back({s, true});
        }
        int K = (int)nodes.size();
        ov_table.assign(K, std::vector<int16_t>(K, 0));
        for (int i = 0; i < K; ++i) {
            for (int j = 0; j < K; ++j) {
                if (i != j) {
                    ov_table[i][j] = (int16_t)calc_overlap(nodes[i].s, nodes[j].s);
                }
            }
        }
    }

    void run_greedy_merge(std::mt19937_64& rng) {
        while (true) {
            int best_ov = 0;
            std::vector<std::pair<int, int>> best_pairs;

            for (size_t i = 0; i < nodes.size(); ++i) {
                if (!nodes[i].active) continue;
                for (size_t j = 0; j < nodes.size(); ++j) {
                    if (i == j || !nodes[j].active) continue;
                    int ov = ov_table[i][j];
                    if (ov < best_ov) continue;
                    int new_len = (int)nodes[i].s.size() + (int)nodes[j].s.size() - ov;
                    if (new_len <= N) {
                        if (ov > best_ov) {
                            best_ov = ov;
                            best_pairs.clear();
                            best_pairs.push_back({(int)i, (int)j});
                        } else if (ov == best_ov && ov > 0) {
                            best_pairs.push_back({(int)i, (int)j});
                        }
                    }
                }
            }

            if (best_ov <= 0 || best_pairs.empty()) break;

            auto p = best_pairs[rng() % best_pairs.size()];
            int u = p.first;
            int v = p.second;

            nodes[u].s = nodes[u].s + nodes[v].s.substr(best_ov);
            nodes[v].active = false;

            for (size_t k = 0; k < nodes.size(); ++k) {
                if (k != (size_t)u && nodes[k].active) {
                    ov_table[u][k] = (int16_t)calc_overlap(nodes[u].s, nodes[k].s);
                    ov_table[k][u] = (int16_t)calc_overlap(nodes[k].s, nodes[u].s);
                }
            }
        }
    }

    std::vector<std::string> get_active_strings() {
        std::vector<std::string> res;
        for (const auto& nd : nodes) {
            if (nd.active) res.push_back(nd.s);
        }
        std::sort(res.begin(), res.end(), [](const std::string& a, const std::string& b) {
            return a.size() > b.size();
        });
        return res;
    }
};

std::vector<std::string> solve_ahc004(int N, int M, const std::vector<std::string>& S) {
    std::mt19937_64 rng(1337);

    // 1. Maximal strings
    std::vector<std::string> unique_S = S;
    std::sort(unique_S.begin(), unique_S.end());
    unique_S.erase(std::unique(unique_S.begin(), unique_S.end()), unique_S.end());

    std::vector<std::string> maximal_S;
    for (size_t i = 0; i < unique_S.size(); ++i) {
        bool is_sub = false;
        for (size_t j = 0; j < unique_S.size(); ++j) {
            if (i != j && unique_S[j].find(unique_S[i]) != std::string::npos) {
                is_sub = true;
                break;
            }
        }
        if (!is_sub) maximal_S.push_back(unique_S[i]);
    }

    FastEvaluator eval;
    eval.init(N, M, S);

    // Multi-restart initial greedy assembly
    std::vector<std::string> best_init_grid(N, std::string(N, '.'));
    int best_init_score = -1;

    int num_restarts = 10;
    for (int r_idx = 0; r_idx < num_restarts; ++r_idx) {
        FastMerger fm;
        fm.init(N, maximal_S);
        fm.run_greedy_merge(rng);
        auto pool = fm.get_active_strings();

        std::vector<std::string> grid(N, std::string(N, '.'));
        std::vector<int> row_len(N, 0);
        for (const auto& s : pool) {
            int best_row = -1;
            int best_ov = -1;
            for (int r = 0; r < N; ++r) {
                if (row_len[r] == 0) {
                    if (best_row == -1) {
                        best_row = r;
                        best_ov = 0;
                    }
                } else {
                    int ov = calc_overlap(grid[r].substr(0, row_len[r]), s);
                    if (row_len[r] + (int)s.size() - ov <= N) {
                        if (ov > best_ov) {
                            best_ov = ov;
                            best_row = r;
                        }
                    }
                }
            }
            if (best_row != -1) {
                if (row_len[best_row] == 0) {
                    for (size_t c = 0; c < s.size(); ++c) {
                        grid[best_row][c] = s[c];
                    }
                    row_len[best_row] = (int)s.size();
                } else {
                    int start = row_len[best_row] - best_ov;
                    for (size_t c = 0; c < s.size(); ++c) {
                        grid[best_row][start + c] = s[c];
                    }
                    row_len[best_row] = start + (int)s.size();
                }
            }
        }

        eval.set_grid(grid);
        if (eval.total_matched > best_init_score) {
            best_init_score = eval.total_matched;
            best_init_grid = grid;
        }
    }

    eval.set_grid(best_init_grid);

    // Stage 2: Simulated Annealing on Row Shifts and Row Swaps
    int best_sa_score = eval.total_matched;
    auto best_sa_grid = eval.grid;
    int sa_iters = 6000;
    for (int iter = 0; iter < sa_iters; ++iter) {
        double T = 3.0 * (1.0 - (double)iter / sa_iters) + 0.01;
        int type = rng() % 2;
        int prev_score = eval.total_matched;
        if (type == 0) {
            int r = rng() % N;
            int shift = 1 + (rng() % (N - 1));
            eval.shift_row(r, shift);
            int delta = eval.total_matched - prev_score;
            if (delta >= 0 || std::exp(delta / T) > (double)(rng() % 1000000) / 1000000.0) {
                if (eval.total_matched > best_sa_score) {
                    best_sa_score = eval.total_matched;
                    best_sa_grid = eval.grid;
                }
            } else {
                eval.shift_row(r, N - shift);
            }
        } else {
            int r1 = rng() % N;
            int r2 = rng() % N;
            if (r1 == r2) continue;
            eval.swap_rows(r1, r2);
            int delta = eval.total_matched - prev_score;
            if (delta >= 0 || std::exp(delta / T) > (double)(rng() % 1000000) / 1000000.0) {
                if (eval.total_matched > best_sa_score) {
                    best_sa_score = eval.total_matched;
                    best_sa_grid = eval.grid;
                }
            } else {
                eval.swap_rows(r1, r2);
            }
        }
    }
    eval.set_grid(best_sa_grid);

    // Stage 3: Dynamic 2D Non-Destructive Placement
    bool improved = true;
    while (improved) {
        improved = false;
        std::vector<int> unmatched;
        for (int i = 0; i < M; ++i) {
            if (eval.match_count[i] == 0) unmatched.push_back(i);
        }
        std::sort(unmatched.begin(), unmatched.end(), [&](int a, int b) {
            return S[a].size() > S[b].size();
        });

        for (int id : unmatched) {
            if (eval.match_count[id] > 0) continue;
            const std::string& str = S[id];
            int len = (int)str.size();

            int best_net_gain = 0;
            int best_dir = -1, best_line = -1, best_offset = -1;

            for (int dir = 0; dir < 2; ++dir) {
                for (int line = 0; line < N; ++line) {
                    for (int offset = 0; offset < N; ++offset) {
                        int conflicts = 0;
                        for (int k = 0; k < len; ++k) {
                            char cur = (dir == 0) ? eval.grid[line][(offset + k) % N] : eval.grid[(offset + k) % N][line];
                            if (cur != '.' && cur != str[k]) conflicts++;
                        }
                        if (conflicts > 3) continue;

                        std::vector<char> old_chars(len);
                        for (int k = 0; k < len; ++k) {
                            if (dir == 0) old_chars[k] = eval.grid[line][(offset + k) % N];
                            else old_chars[k] = eval.grid[(offset + k) % N][line];
                        }
                        int prev_score = eval.total_matched;
                        for (int k = 0; k < len; ++k) {
                            if (dir == 0) eval.update_cell(line, (offset + k) % N, str[k]);
                            else eval.update_cell((offset + k) % N, line, str[k]);
                        }
                        int net_gain = eval.total_matched - prev_score;

                        for (int k = 0; k < len; ++k) {
                            if (dir == 0) eval.update_cell(line, (offset + k) % N, old_chars[k]);
                            else eval.update_cell((offset + k) % N, line, old_chars[k]);
                        }

                        if (net_gain > best_net_gain) {
                            best_net_gain = net_gain;
                            best_dir = dir;
                            best_line = line;
                            best_offset = offset;
                        }
                    }
                }
            }

            if (best_net_gain > 0) {
                for (int k = 0; k < len; ++k) {
                    if (best_dir == 0) eval.update_cell(best_line, (best_offset + k) % N, str[k]);
                    else eval.update_cell((best_offset + k) % N, best_line, str[k]);
                }
                improved = true;
            }
        }
    }

    // Stage 4: Dot filling
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (eval.grid[r][c] == '.') {
                int best_c_score = eval.total_matched;
                char best_char = 'A';
                for (char ch = 'A'; ch <= 'H'; ++ch) {
                    eval.update_cell(r, c, ch);
                    if (eval.total_matched > best_c_score) {
                        best_c_score = eval.total_matched;
                        best_char = ch;
                    }
                    eval.update_cell(r, c, '.');
                }
                eval.update_cell(r, c, best_char);
            }
        }
    }

    // Stage 5: Coordinate descent
    for (int iter = 0; iter < 10; ++iter) {
        bool improved_cd = false;
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                char orig = eval.grid[r][c];
                char best_ch = orig;
                int best_score = eval.total_matched;

                for (char ch = 'A'; ch <= 'H'; ++ch) {
                    if (ch == orig) continue;
                    eval.update_cell(r, c, ch);
                    if (eval.total_matched > best_score) {
                        best_score = eval.total_matched;
                        best_ch = ch;
                    }
                    eval.update_cell(r, c, orig);
                }

                if (best_ch != orig) {
                    eval.update_cell(r, c, best_ch);
                    improved_cd = true;
                }
            }
        }
        if (!improved_cd) break;
    }

    return eval.grid;
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    int N, M;
    if (!(std::cin >> N >> M)) return 0;
    std::vector<std::string> S(M);
    for (int i = 0; i < M; ++i) {
        std::cin >> S[i];
    }

    auto grid = solve_ahc004(N, M, S);
    for (int r = 0; r < N; ++r) {
        std::cout << grid[r] << "\n";
    }
    return 0;
}

