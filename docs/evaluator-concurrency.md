# Evaluator concurrency

ARS treats evaluator throughput as part of its reference hardware definition.
The primary reference category targets maximum stable performance on one Ryzen
9 3950X with 16 physical cores and 32 hardware threads. Faster or different
machines may publish hardware-declared results, but they are not assumed to be
identical to the reference merely because their CPU frequency is throttled.

## Selection procedure

For each problem and evaluator configuration:

1. Choose one accepted candidate with representative nontrivial execution cost.
2. Evaluate the same fixed public cases at 1, 2, 4, 8, and subsequent powers of
   two.
3. Run three complete repetitions at each worker count.
4. Require every trial to return the identical score.
5. Compare median end-to-end evaluator wall time, including compilation, case
   execution, judging, and cleanup.
6. Continue through near-ties. Stop when a worker count's median wall time is
   at least 5 percent slower than the best median seen.
7. Check the fastest counts with a fixed CPU-budget fixture and a
   wall-budgeted search fixture. Reject counts that change a standalone
   candidate's verdict or materially reduce the work available to each case.
8. Select the fastest eligible worker count and freeze it for that problem and
   benchmark version.

The procedure uses a median to limit sensitivity to one slow process launch.
The 5 percent boundary prevents ordinary timing noise from ending the sweep.
Near-ties are reported rather than treated as precise estimates. The selected
count is not retuned per model, harness, or submitted solution after system
comparisons begin.

Public evaluator concurrency changes when feedback becomes available and must
therefore be identical across compared systems. Private evaluation happens
after the research system stops, so its concurrency may be higher for
operational efficiency as long as it is declared and scores remain identical.

## Current AHC044 result

The [3950X worker sweep](../results/discovery/ahc044-evaluator-workers-3950x/README.md)
tested candidate 13 through the former ALE-Bench evaluator. Median public wall time fell from 59.771
seconds at one worker to 19.293 seconds at 16 workers and was effectively tied
at 32. At 64 workers it increased to 20.954 seconds, an 8.61 percent regression.
AHC044 therefore uses 16 public evaluator workers for the next discovery
protocol.

The native runner reproduces candidate 13's score and reduces its 16-worker
median from 19.293 seconds to about 3.1 seconds. The completed-runner
[discovery sweep](../results/discovery/ahc044-native-workers-3950x-v1/README.md)
found a raw end-to-end optimum of 2.824 seconds at 32 workers. Fifty workers
was effectively tied at 2.829 seconds.

The calibration audit does not support raising the public default. A candidate
that consumes 1.5 seconds of process CPU took up to 1.93 seconds wall at 32
workers and timed out at 50, despite being accepted in 1.51 seconds standalone.
A wall-budgeted search also received less CPU and a lower score as concurrency
increased. ARS therefore retains 16 public workers, matching the 3950X physical
core count. Thirty-two is reported separately as the raw throughput optimum.

Rerun the sweep when any causal input changes, including problem, case count,
evaluator implementation, compiler, sandbox, CPU topology, or case caching.
