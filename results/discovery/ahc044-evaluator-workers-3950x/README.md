# AHC044 evaluator worker sweep on Ryzen 9 3950X

This discovery audit selects public evaluator concurrency for the reference
Ryzen 9 3950X host. It uses candidate 13 from the 20-minute Luna run because it
is the run's strongest public and private candidate and has meaningful per-case
CPU work.

The sweep tests powers of two starting at one worker. Each worker count receives
three complete evaluations of the same 50 public cases. The score must remain
identical. Performance is the reciprocal of median wall time. Near-ties remain
in the sweep. It stops when a worker count is at least 5 percent slower than
the best median seen and selects the fastest measured count.

| Workers | Median wall | Public cases/s | Speedup |
| ---: | ---: | ---: | ---: |
| 1 | 59.771 s | 0.837 | 1.00x |
| 2 | 35.358 s | 1.414 | 1.69x |
| 4 | 25.082 s | 1.993 | 2.38x |
| 8 | 20.971 s | 2.384 | 2.85x |
| 16 | 19.293 s | 2.592 | 3.10x |
| 32 | 19.314 s | 2.589 | 3.09x |
| 64 | 20.954 s | 2.386 | 2.85x |

Every trial produced the same public score of 49,189,250. The 16-worker and
32-worker medians differ by only 0.11 percent, so the sweep continued through
that near-tie. At 64 workers, median wall time was 8.61 percent slower than the
best median at 16 workers. This is a clear decrease under the 5 percent rule,
so the sweep stops at 64 and selects 16. Testing 128 is unnecessary under the
declared rule.

The raw trials and environment identity are in `worker-sweep.json`. Rerun the
sweep when the problem, public case count, evaluator implementation or image,
reference hardware, container runtime, or persistent-session policy changes.
Do not retune concurrency separately for competing systems on the same problem
and benchmark version.
