# AHC044 native evaluator concurrency discovery

This discovery run tests the completed native runner on the Ryzen 9 3950X.
The fixed-work sweep used candidate 13, five trials per worker count, the 50
public cases, bubblewrap isolation, and the exact problem package recorded in
`worker-sweep.json`.

| Workers | Median total | Median evaluation | Result |
| ---: | ---: | ---: | --- |
| 1 | 11.444 s | 9.171 s | accepted |
| 2 | 6.877 s | 4.580 s | accepted |
| 4 | 5.061 s | 2.436 s | accepted |
| 8 | 3.842 s | 1.325 s | accepted |
| 16 | 3.331 s | 0.788 s | accepted |
| 32 | 2.824 s | 0.502 s | accepted |
| 50 | 2.829 s | 0.482 s | accepted |

The raw end-to-end optimum is 32 workers. Fifty workers only improves the
evaluation phase by about 20 ms and loses that gain to ordinary compile-time
variation.

That raw result is not sufficient for the public benchmark. The fixed CPU
fixture is accepted standalone in 1.51 seconds and remains accepted at 16 and
32 workers. At 32, however, its slowest measured case rises to 1.93 seconds.
At 50, 33 and then 42 of 50 cases time out in the two fully recorded repeats.

The wall-budgeted search fixture also shows the quality effect. Its median
aggregate score is 49,231,880 at 16 workers, 49,230,832 at 32, and 49,220,698
at 50. Median candidate CPU falls from 73.93 seconds at 16 to 71.04 at 32 and
45.51 at 50. The score effect at 32 is small for this solver, but it confirms
that logical-thread concurrency changes the search rather than merely making
the same evaluation finish sooner.

The official Rust tester was also invoked as a standalone process 1,000 times
per repeat. Its median was 3.44 ms per invocation. Even deleting that entire
cost would save only a few milliseconds of batch wall time at 32-way case
parallelism, so linking the tester into the runner is not justified.

The public default remains 16 workers. It is the fastest tested setting that
gives each case a physical core and preserves standalone timing closely. The
32-worker result is useful as the raw throughput number, but not as a safe
replacement for public feedback. `contaminated-worker-sweep.json` preserves an
excluded first attempt that overlapped a separate high-CPU Rust release build.
