# Gemini 3.7 Flash high five-lane discovery run

Gemini produced useful trajectories on all five unused CPUs. It reached the fixed human four-hour P50 target on AHC053 in 174.47 seconds and AHC068 in 232.50 seconds. Four lanes stopped after about 26 minutes when their individual agy quotas were exhausted. AHC044 continued to the four-hour deadline, but its only valid private result remained the starter.

The five lanes started at 2026-08-29 15:09:14 EDT with one pinned CPU each. AHC053 used CPU 18, AHC065 CPU 14, AHC044 CPU 24, AHC068 CPU 15, and AHC030 CPU 25. Each evaluator had `CPUAffinity`, `CPUQuota=100%`, and `SystemCallFilter=~sched_setaffinity` applied by its systemd user service.

## Results

| Problem | Agent stop | Candidates | Best private result | Human four-hour percentile | Time to human four-hour P50 | Endpoint |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| AHC053 | 1,558.52 s | 12 | 89,005,893,381 | 86.72 | 174.47 s | Individual agy quota exhausted |
| AHC065 | 1,540.15 s | 4 | 830,168,336 | 48.65 | Right-censored at 4 h | Individual agy quota exhausted |
| AHC044 | 14,410.29 s | 8 | 1,368,866 | 2.47 | Right-censored at 4 h | Benchmark deadline |
| AHC068 | 1,590.10 s | 9 | 906,198,683 | 69.24 | 232.50 s | Individual agy quota exhausted |
| AHC030 | 1,541.93 s | 42 | 5,184,000,000 cost | Not available | Not available | Individual agy quota exhausted |

AHC053, AHC065, AHC044, and AHC068 maximize score. Their percentiles use the fixed cohort of every human who submitted during the official four-hour contest and each human's best score by the checkpoint. AHC030 minimizes native interaction cost. Historical AtCoder points use a relative maximize transformation, so the current data cannot place its absolute native costs on a valid human percentile scale.

AHC030's best public candidate was candidate 32 at 1,845,000,000 on 50 cases. Its best held-out candidate was candidate 22 at 5,184,000,000 on 100 cases. This public-private difference is exactly why the run records every submitted candidate and privately evaluates the whole trajectory.

AHC044 submitted eight candidates, but only the starter was valid on the held-out cases. Candidate 8 was queued just before the deadline, so its public evaluation was interrupted and completed only in the post-run private pass. Evaluating the pathological AHC044 candidates extended post-processing by about 40 minutes without changing the score.

## Usage

| Problem | Input tokens | Cached input | Output tokens | Reasoning output | API-equivalent cost |
| --- | ---: | ---: | ---: | ---: | ---: |
| AHC053 | 100,816,551 | 97,657,377 | 260,658 | 63,584 | $10.67115127 |
| AHC065 | 15,977,030 | 15,168,833 | 239,986 | 55,851 | $2.64375772 |
| AHC044 | 11,543,696 | 10,878,231 | 125,087 | 28,449 | $1.78404232 |
| AHC068 | 7,585,985 | 7,165,575 | 129,448 | 32,478 | $1.33815562 |
| AHC030 | 26,217,238 | 24,980,781 | 406,891 | 74,135 | $4.32674257 |
| Total | 162,140,500 | 155,850,797 | 1,162,070 | 254,497 | $20.76384950 |

Agy reports conversation-cumulative usage on every resume. The authoritative totals in [`corrected-usage.json`](corrected-usage.json) therefore use the last snapshot from each lane. The run state files sum snapshots and overcount usage. AHC065 completed 29 short continuation invocations before the thirtieth hit quota, which provides direct evidence that unconditional continuation can churn when a harness ends turns frequently.

## Reproduction artifacts

AOP 0.1.13 ran agy 1.1.22 with model `gemini-3.7-flash-high` and high reasoning effort. AOP cannot enforce `--no-web` for agy, so these runs omitted the flag and retained the prompt-level internet prohibition. Each problem directory contains the system manifest, ARS event stream, submitted candidates, all private evaluations, final model workspace source, normalized AOP summaries, and raw AOP requests, results, stderr, final messages, and compressed event streams.

Agy's native host workspace mode edited the controller checkout rather than the requested task worktree. The resulting sources were preserved as each lane's `final-workspace.cpp`, and the authoritative benchmark starters were restored afterward.
