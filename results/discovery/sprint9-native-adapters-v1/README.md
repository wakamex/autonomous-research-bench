# Sprint-9 native adapter validation

All nine Sprint-9 starters were accepted on 50 public cases and on held-out bundles matching each contest's official case count. The native runner now covers eight strict batch-output problems and the interactive AHC053 protocol without Docker or Podman.

## Official-count results

Each starter was compiled once and evaluated serially on one pinned CPU. Scores only establish end-to-end correctness because these are intentionally simple starters.

| Problem | Mode | Cases | Result | Score | Evaluation wall time | Candidate CPU time |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| AHC004 | Batch, legacy generator | 100 | Accepted | 4,624,411 | 1.812 s | 0.125 s |
| AHC044 | Batch | 150 | Accepted | 1,368,866 | 2.449 s | 0.174 s |
| AHC050 | Batch | 150 | Accepted | 37,228,564 | 4.195 s | 0.199 s |
| AHC052 | Batch | 150 | Accepted | 1,500 | 2.187 s | 0.174 s |
| AHC053 | Interactive | 150 | Accepted | 24,757,714,974 | 2.265 s | 0.179 s |
| AHC062 | Batch | 100 | Accepted | 40,002,293,577 | 3.074 s | 1.199 s |
| AHC065 | Batch | 150 | Accepted | 0 | 2.198 s | 0.180 s |
| AHC067 | Batch | 150 | Accepted | 160,505,737 | 2.233 s | 0.172 s |
| AHC068 | Batch | 150 | Accepted | 153 | 2.192 s | 0.177 s |

## Method

The packages contain the official AtCoder Rust tools and record the source zip URL and SHA-256 hash. Public bundles use seeds 0 through 49. Held-out validation used seeds 1,000,000 onward, with 100 cases for AHC004 and AHC062 and 150 for the other contests. The held-out inputs remain outside the repository.

The tester wrappers reject malformed output before reporting a score. An empty candidate was classified as `WRONG_ANSWER` on both AHC050 batch judging and AHC053 interactive judging. `cargo fmt --check`, Clippy with warnings denied, the Rust test suite, and package loading for all nine problems passed.

## Remaining promotion checks

AHC044 already reproduces archived ALE-Bench scores. The eight newer packages have no ALE-Bench counterpart, so their official tools and accepted starters establish native execution compatibility but cannot provide a direct performance comparison. Their fixed-cohort human references still need independent final-standings distribution validation, and held-out seed scores need distribution-level comparability checks against official contest scores before leaderboard promotion.
