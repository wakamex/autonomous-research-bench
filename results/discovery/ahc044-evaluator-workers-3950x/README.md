# AHC044 evaluator worker sweep on Ryzen 9 3950X

This discovery audit selects public evaluator concurrency for the reference
Ryzen 9 3950X host. It uses candidate 13 from the 20-minute Luna run because it
is the run's strongest public and private candidate and has meaningful per-case
CPU work.

The sweep tests powers of two starting at one worker. Each worker count receives
three complete evaluations of the same 50 public cases. The score must remain
identical. Performance is the reciprocal of median wall time. The sweep stops
after the first worker count whose median is not faster than its predecessor
and selects the preceding count.

| Workers | Median wall | Public cases/s | Speedup |
| ---: | ---: | ---: | ---: |
| 1 | 59.771 s | 0.837 | 1.00x |
| 2 | 35.358 s | 1.414 | 1.69x |
| 4 | 25.082 s | 1.993 | 2.38x |
| 8 | 20.971 s | 2.384 | 2.85x |
| 16 | 19.293 s | 2.592 | 3.10x |
| 32 | 19.314 s | 2.589 | 3.09x |

Every trial produced the same public score of 49,189,250. Median performance
first decreased at 32 workers, so the rule selects 16 workers. The 16-worker
and 32-worker medians differ by only 0.11 percent, which is below the precision
that three repetitions can establish. Sixteen remains the correct protocol
choice under the declared first-decrease rule and uses fewer concurrent
containers.

The raw trials and environment identity are in `worker-sweep.json`. Rerun the
sweep when the problem, public case count, evaluator implementation or image,
reference hardware, container runtime, or persistent-session policy changes.
Do not retune concurrency separately for competing systems on the same problem
and benchmark version.
