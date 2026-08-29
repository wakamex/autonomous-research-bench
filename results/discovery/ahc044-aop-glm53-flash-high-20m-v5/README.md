# AHC044 GLM 5.3 Flash high 20-minute AOP 0.1.13 discovery run

GLM 5.3 Flash submitted the starter at 30.45 seconds but did not produce an improved candidate before the 20-minute deadline. The starter scored 507,388 on 50 public cases and 1,368,866 on 150 held-out cases, corresponding to historical rank 997 and performance -80. This run verifies AOP 0.1.13's app-server interruption accounting and `--no-web` policy under the pinned native evaluator, but its research result is worse than the earlier AOP 0.1.10 run that reached a 93,979,736 held-out score.

## Trajectory

| Candidate | Submitted | Public result | Private score | Historical rank | Performance | Public evaluation wall time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 30.452 s | 507,388 | 1,368,866 | 997 | -80 | 0.948 s |

The 10-minute fixed checkpoint carries candidate 1 forward with a held-out score of 1,368,866 and human percentile 86.07 against the fixed four-hour cohort at that time. P50, P90, and P99 time-to-human-4h are all right-censored. [`score.json`](score.json) contains the complete fixed-checkpoint and threshold results.

The agent read the local statement and then spent most of the run designing and testing algorithms in temporary files. Its first local optimization experiment started around 772 seconds, a second approach started around 1,122 seconds, and its last local test started around 1,189 seconds. It never edited `benchmarks/ahc044/solution.cpp` or submitted a second candidate. The run therefore exposes a time-management failure rather than an evaluator bottleneck.

## Accounting

| Measurement | Value |
| --- | ---: |
| AOP wall time | 1,200.079 s |
| Input tokens | 195,698 |
| Cached input tokens | 143,232 |
| Output tokens | 37,561 |
| Reasoning output tokens | 34,410 |
| Calculated API-equivalent cost | $0.01547368 |
| Public evaluator CPU time | 0.055 s |
| Private evaluator wall time | 2.390 s |

AOP interrupted the active Codex turn at the deadline and reported provider status `interrupted`. Accounting is `partial` because the turn did not complete, but the retained usage is measured through the native interruption handshake rather than zero or null. The Z.AI Coding Plan route is subscription-backed; the dollar amount is AOP's API-equivalent calculation.

## Isolation and evaluator boundary

AOP 0.1.13 started Codex 0.150.1 through app-server with `--no-web`. A preceding real-tool smoke confirmed local shell and file access while `curl` failed because external DNS and network access were denied.

The public evaluator ran on logical CPU 14 inside `ars-evaluator-aop-glm53-flash-high-20m-v5.service` with affinity `[14]`, `CPUQuota=100%`, and `SystemCallFilter=~sched_setaffinity`. The service wrapped AOP so agent-triggered evaluations inherited the verified CPU contract without giving the network-denied Codex sandbox access to the user systemd bus.

Public evaluation used the controller-installed ARS binary, AHC044 package, tester, and case bundle. The task-local launcher added the fixed CPU marker and selected `--sandbox none` for `ars submit` because Codex's workspace sandbox does not permit a second bubblewrap namespace. Candidate compilation and execution still inherited Codex's network-denied workspace sandbox, the evaluator service cgroup, ARS resource limits, and the syscall filter. Post-run private evaluation ran outside Codex with ARS's normal bubblewrap sandbox.

The preflight also found that rebuilding the Rust tester under a different AOP worktree path changes its binary hash. Using the controller-installed package and case bundle keeps the evaluator identity authoritative and avoids coupling a run to path-dependent worktree build output.

## Reproduction identity

- ARS base commit: `88d414e2ddddca3b044374758a4d2d2f80d82c3a`
- AOP release: `0.1.13`, release commit `631e075a16b56be155d98bb389a0cded4f5dfb81`
- Codex version: `0.150.1`
- Codex session: `01a04ba2-4fe0-7e23-88bf-91528ee4fa83`
- AOP run: `f18419e1-3c9b-4d61-bcb5-e9d510c14497`
- model: `glm-5.3-flash`
- reasoning effort: `high`
- system manifest SHA-256: `d86f06a2b0e77870e3366fcfb4e92b2ce2c95f49140d6a0489ebb673d2fa9642`
- human reference SHA-256: `30e6a73e15071a69292b48855885a87266358cdc5d3bd4a9b325a22b3862b8c7`
- problem package SHA-256: `28552e8804f5c8accc5b1cfb23a4b6b9a0b31815c353b5cc302666c90f52c705`
- public case metadata SHA-256: `d0ba8eca311d21b5a890f91c501cbeab909055e2ad34e76b2accfaeb18d1f378`
- held-out input bundle SHA-256: `5dc9c368f2e032c84b87c0d04f3463a65c1dc6a290f6a60629000df00e616e48`

The normalized AOP request and result are under [`aop/`](aop/), with the 9.1 MB raw event stream compressed to `events.jsonl.gz`. [`cpu-plan.json`](cpu-plan.json), [`launcher.sh`](launcher.sh), and [`evaluator-wrapper.sh`](evaluator-wrapper.sh) preserve the selected lane and exact deployment boundary. The ARS trajectory, state, and archived candidate are under [`run/`](run/).
