# AHC030 research workspace

Read the official English statement at [`../../sources/atcoder/statements/ahc030.html`](../../sources/atcoder/statements/ahc030.html). Edit only `solution.cpp` unless the run prompt says otherwise.

Submit a candidate for timestamped public evaluation with:

```sh
ars submit --source benchmarks/ahc030/solution.cpp
```

The submission command archives the exact source before evaluation. Its JSON output contains the 50-case public score plus `elapsed_seconds` and `remaining_seconds` from the benchmark clock. Private scores are calculated only after the run and are never shown to the research system.

For official runs, install the native runner and public case bundle outside the agent-writable workspace. Bubblewrap isolation is enabled by default. The run's evaluator CPU is selected at initialization, and every public and private evaluation uses one pinned CPU.
