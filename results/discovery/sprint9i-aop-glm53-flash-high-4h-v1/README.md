# GLM 5.3 Flash high ten-lane discovery run

This ten-lane launch produced no submitted benchmark trajectory. Five requests were rejected after about 12 seconds by Z.AI request-rate limits. The other five ran for about 55 minutes, performed extensive direct local work, never called `ars submit`, and then exhausted the shared five-hour Z.AI usage allowance. Their modified final workspace programs were later recovered and evaluated as endpoint snapshots.

## Recovered endpoint results

All five connected lanes left modified, non-starter `solution.cpp` files in their AOP worktrees. Each recovered source compiled and passed every case in a regenerated held-out bundle using the original seeds from 1,000,000 onward and the official case count. Evaluation used one worker per problem on five distinct physical cores.

| Problem | Recovered held-out score | Cases | Human four-hour percentile | Human four-hour P50 | Interpretation |
| --- | ---: | ---: | ---: | ---: | --- |
| AHC004 | 5,053,124,394 | 100 | 85.29 | 2,678,107,190 | Above P50, below P90 |
| AHC050 | 68,972,255 | 150 | 9.02 | 137,493,188 | Below P50 |
| AHC052 | 268,139 | 150 | 39.19 | 332,951 | Below P50 |
| AHC062 | 40,122,769,222 | 100 | 32.55 | 40,686,455,725 | Below P50 |
| AHC067 | 417,252,222 | 150 | 21.33 | 849,429,310 | Below P50 |

Each problem directory preserves `recovered-final-workspace.cpp`, the complete `recovered-private-evaluation.json`, and evaluator stderr. These are final-state datapoints at the lane endpoint, not reconstructed submission trajectories. The sources existed by the recorded run stop near 55 minutes, but no evidence identifies when an intermediate version first attained the final score.

| Problem | CPU | Agent duration | Submitted candidates | Endpoint |
| --- | ---: | ---: | ---: | --- |
| AHC004 | 10 | 3,342.01 s | 0 | Usage allowance exhausted |
| AHC062 | 27 | 3,333.49 s | 0 | Usage allowance exhausted |
| AHC067 | 19 | 3,332.79 s | 0 | Usage allowance exhausted |
| AHC050 | 28 | 3,329.68 s | 0 | Usage allowance exhausted after one completed turn and continuation |
| AHC052 | 13 | 3,343.83 s | 0 | Usage allowance exhausted |
| AHC053 | 18 | 12.26 s | 0 | Request-rate limit |
| AHC065 | 14 | 11.98 s | 0 | Request-rate limit |
| AHC044 | 24 | 12.06 s | 0 | Request-rate limit |
| AHC068 | 15 | 12.72 s | 0 | Request-rate limit |
| AHC030 | 25 | 12.71 s | 0 | Request-rate limit |

The five connected lanes used 68,586,364 input tokens, including 67,919,360 cached tokens, 336,408 output tokens, and 156,302 reasoning-output tokens. Their calculated API-equivalent cost was $1.15291770. Codex resume usage is incremental, so [`corrected-usage.json`](corrected-usage.json) sums invocation records. Rejected lanes reported no usage.

All lanes used AOP 0.1.13, Codex 0.150.1, model `glm-5.3-flash`, high reasoning effort, and pinned one-CPU evaluators. Each problem directory retains its manifest, ARS state and events, normalized AOP result, and raw compressed AOP record. [`cpu-plan.json`](cpu-plan.json) records the complete allocation.
