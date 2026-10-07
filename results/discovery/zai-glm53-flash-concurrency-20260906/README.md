# Z.AI GLM-5.3-Flash concurrency probe

Ten concurrent AOP jobs completed cleanly. The largest clean burst was 14 jobs, while 15 produced 2 request-rate failures and 16 produced 4. Use 10 concurrent ARS lanes on this host: it passed the synchronized burst test, matches the host's ten-lane CPU allocation, and leaves headroom below the observed dynamic provider boundary.

The probe ran on 2026-09-06 from 00:48 to 00:57 America/Toronto, outside the 11:00 to 21:00 local GLM-5.3-Flash campaign window. Z.AI says Coding Plan concurrency is tied to the subscription plan and adjusted dynamically, with higher concurrency during off-peak hours. The measured 14-job clean boundary is therefore an observation for this account and time, not a permanent provider guarantee. See the [Z.AI usage policy](https://docs.z.ai/devpack/usage-policy) and [GLM-5.3-Flash campaign notice](https://docs.z.ai/devpack/notice/event-glm-5.3-flash).

## Results

| AOP jobs | Clean responses | Rate-limit failures | Batch wall time | Median successful task | Slowest successful task |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1/1 | 0 | 7.551 s | 5.730 s | 5.730 s |
| 2 | 2/2 | 0 | 7.062 s | 5.462 s | 5.485 s |
| 4 | 4/4 | 0 | 6.161 s | 4.514 s | 4.649 s |
| 8 | 8/8 | 0 | 19.906 s | 6.919 s | 18.503 s |
| 10 | 10/10 | 0 | 12.129 s | 7.123 s | 10.535 s |
| 12 | 12/12 | 0 | 15.453 s | 9.882 s | 13.935 s |
| 14 | 14/14 | 0 | 25.455 s | 7.467 s | 23.738 s |
| 15 | 13/15 | 2 | 14.709 s | 7.125 s | 13.059 s |
| 16 | 12/16 | 4 | 13.873 s | 7.619 s | 11.941 s |

The ten-job completion times separated into five jobs at 4.29 to 5.01 seconds and five at 9.23 to 10.54 seconds. The twelve-job run similarly separated into five jobs at 4.74 to 5.50 seconds, five at 9.80 to 10.27 seconds, and two at 13.81 to 13.93 seconds. This suggests about five requests received simultaneous generation service while additional accepted requests queued. Higher AOP job counts can still help full ARS runs because their model calls, compilation, and evaluation naturally desynchronize, but they should not be interpreted as the same number of simultaneously generating model calls.

## Method

Each task used installed AOP 0.1.14 with `agent = "codex"`, `provider = "zai-coding-plan"`, `model = "glm-5.3-flash"`, `effort = "low"`, the sealed profile, disabled web access, and the prompt `Reply with exactly OK. Do not call tools.` AOP launched every task in a batch with `--jobs` equal to the tested count. Batches were separated by at least ten seconds. The short synchronized request is intentionally harsher on burst concurrency than a four-hour agent run whose later calls are staggered by tool work.

One initial twelve-job attempt was excluded before reaching Z.AI because `/run/user/1000` was full. AOP reported local state-runtime failures within 0.18 seconds. Cleaning only the probe's sealed runtimes restored capacity, and the repeated twelve-job batch completed 12/12. All model-run records remain in the temporary probe repository at `/tmp/aop-zai-concurrency-u870pmyx/.aop`; AOP cleanup removed only their disposable sealed runtime state.

## Operating choice

Use ten concurrent lanes for the next full suite run. It is the machine's existing CPU-isolation default and passed a fully synchronized provider burst. Recheck the provider boundary before increasing above ten, especially during the campaign's 11:00 to 21:00 local window or after a plan change. A production launcher should treat a request-rate response as retryable with jitter because Z.AI explicitly makes concurrency dynamic.
