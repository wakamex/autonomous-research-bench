# AHC044 research workspace

Read the official English statement at
[`../../sources/atcoder/statements/ahc044.html`](../../sources/atcoder/statements/ahc044.html).
Edit only `solution.cpp` unless the run prompt says otherwise.

Submit a candidate for timestamped public evaluation with:

```sh
"$ARS_ALE_PYTHON" -m scripts.ahc044_runner submit \
  --run-dir "$ARS_RUN_DIR" \
  --source benchmarks/ahc044/solution.cpp \
  --workers 16
```

The submission command archives the exact source before evaluation. Its JSON
output contains the 50-case public score plus `elapsed_seconds` and
`remaining_seconds` from the benchmark clock. Private scores are calculated
only after the run and are never shown to the research system.

Sixteen public evaluator workers are the current discovery setting. A
power-of-two sweep on the reference 3950X reduced median evaluation time for a
representative candidate from 59.8 seconds at one worker to 19.3 seconds at 16.
Performance decreased slightly at 32 workers. Compared systems must use the
same worker count.
