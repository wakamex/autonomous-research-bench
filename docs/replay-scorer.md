# Native run and trajectory scoring

The `ars` Rust binary is the authoritative runner and scorer. It replaces the former Python adapter, ALE-Bench session, and Docker or Podman execution path.

## Run lifecycle

Prepare public cases once:

```sh
cargo build --release --locked
target/release/ars prepare \
  --problem problems/ahc044 \
  --output /var/lib/ars/ahc044/public \
  --public
```

Generate a CPU plan, start a run, submit candidates, stop it, and privately evaluate every archived candidate:

```sh
ars cpu-plan --count 10 --output cpu-plan.json

ars init --run-dir RUN --run-id RUN_ID --duration 14400 \
  --model MODEL --system-manifest-sha256 SHA256 \
  --problem problems/ahc044 --public-cases /var/lib/ars/ahc044/public \
  --evaluator-cpu 3

ars submit --run-dir RUN --problem problems/ahc044 \
  --source benchmarks/ahc044/solution.cpp

ars continuation-prompt --run-dir RUN

ars stop --run-dir RUN --aop-result AOP_RESULT.json

ars private-evaluate --run-dir RUN --problem problems/ahc044 \
  --cases /var/lib/ars-private/ahc044
```

Private seeds and inputs are not part of the public problem package. The post-run evaluator supplies a separately held case bundle. Public and private evaluation use the run's recorded one-CPU contract.

## Trajectory score

```sh
ars score RUN/events.jsonl \
  --reference references/human/ahc044.json \
  --output RUN/score.json
```

The scorer validates event identity, sequence, monotonic timestamps, candidate references, and the single stop event. It reports fixed-time agent scores, fixed-cohort human percentile tie bounds, and time to the four-hour P50, P90, and P99 thresholds with right-censoring.

Candidate submission time, not private evaluation time, determines threshold attainment. Private evaluation happens only after the run stops.
