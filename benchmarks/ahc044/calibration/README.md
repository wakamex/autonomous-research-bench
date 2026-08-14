# Evaluator timing calibration

`cpu-budget.cpp` consumes 1.5 seconds of process CPU and then emits a valid,
deterministic AHC044 answer. It is an evaluator calibration fixture, not a
competitive solver.

Run it once on an otherwise idle reference host to establish its standalone
wall time. Then evaluate the 50 public cases at candidate worker counts. A
worker count is unsafe for public feedback if contention makes any case exceed
the two-second AHC044 limit, even though the same binary is accepted
standalone.

The 0.5-second margin covers process launch and ordinary host jitter without
making the fixture insensitive to substantial CPU oversubscription.

`wall-budget.cpp` runs a deterministic randomized search for about 1.45 seconds
of wall time. Compare its aggregate score at candidate worker counts. A lower
score at higher concurrency reveals reduced search work even when every case
still passes its time limit.
