# GLM 5.3 Flash high ten-lane discovery run

This ten-lane launch produced no benchmark trajectory. Five requests were rejected after about 12 seconds by Z.AI request-rate limits. The other five ran for about 55 minutes, performed extensive direct local work, never called `ars submit`, and then exhausted the shared five-hour Z.AI usage allowance.

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
