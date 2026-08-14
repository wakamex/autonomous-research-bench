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
5. Compare median end-to-end evaluator wall time, including session setup,
   compilation, cases, judging, and cleanup.
6. Stop after the first worker count whose median wall time is not lower than
   the preceding count.
7. Select the preceding worker count and freeze it for that problem and
   benchmark version.

The procedure uses a median to limit sensitivity to one slow container launch.
The first-decrease rule is intentionally simple. Near-ties are reported rather
than treated as precise estimates. The selected count is not retuned per model,
harness, or submitted solution after system comparisons begin.

Public evaluator concurrency changes when feedback becomes available and must
therefore be identical across compared systems. Private evaluation happens
after the research system stops, so its concurrency may be higher for
operational efficiency as long as it is declared and scores remain identical.

## Current AHC044 result

The [3950X worker sweep](../results/discovery/ahc044-evaluator-workers-3950x/README.md)
tested candidate 13 from the Luna run. Median public wall time fell from 59.771
seconds at one worker to 19.293 seconds at 16 workers, then increased slightly
to 19.314 seconds at 32 workers. AHC044 therefore uses 16 public evaluator
workers for the next discovery protocol.

Rerun the sweep when any causal input changes, including problem, case count,
evaluator image or implementation, CPU topology, container runtime, or whether
sessions and containers persist across submissions.
