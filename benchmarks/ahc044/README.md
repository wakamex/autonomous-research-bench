# AHC044 research workspace

Read the official English statement at
[`../../sources/atcoder/statements/ahc044.html`](../../sources/atcoder/statements/ahc044.html).
Edit only `solution.cpp` unless the run prompt says otherwise.

Submit a candidate for timestamped public evaluation with:

```sh
ars submit \
  --run-dir "$ARS_RUN_DIR" \
  --source benchmarks/ahc044/solution.cpp \
  --workers 16
```

The submission command archives the exact source before evaluation. Its JSON
output contains the 50-case public score plus `elapsed_seconds` and
`remaining_seconds` from the benchmark clock. Private scores are calculated
only after the run and are never shown to the research system.

For official runs, install the native runner and public case bundle outside the
agent-writable workspace. Bubblewrap isolation is enabled by default.

Sixteen public evaluator workers remain the reference 3950X setting. Native
execution makes higher case concurrency cheap, but sixteen matches the 16
physical cores and avoids changing the effective single-case compute available
to time-based solutions. Compared systems must use the same worker count.
