# Native run and trajectory scoring

The `ars` Rust binary is the authoritative runner and scorer. It replaces the
former Python adapter, ALE-Bench session, and Docker or Podman execution path.

## Run lifecycle

Prepare public cases once:

```sh
cargo build --release --locked
target/release/ars prepare \
  --problem problems/ahc044 \
  --output /var/lib/ars/ahc044/public \
  --public
```

Start a run, submit candidates, stop it, and privately evaluate every archived
candidate:

```sh
ars init --run-dir RUN --run-id RUN_ID --duration 14400 \
  --model MODEL --system-manifest-sha256 SHA256 \
  --problem problems/ahc044 --public-cases /var/lib/ars/ahc044/public

ars submit --run-dir RUN --problem problems/ahc044 \
  --source benchmarks/ahc044/solution.cpp --workers 16

ars continuation-prompt --run-dir RUN

ars stop --run-dir RUN --aop-result AOP_RESULT.json

ars private-evaluate --run-dir RUN --problem problems/ahc044 \
  --cases /var/lib/ars-private/ahc044 --workers 16
```

Private seeds and inputs are not part of the public problem package. The
post-run evaluator supplies a separately held case bundle.

## Trajectory score

```sh
ars score RUN/events.jsonl \
  --reference references/human/ahc044.json \
  --output RUN/score.json
```

The scorer validates event identity, sequence, monotonic timestamps, candidate
references, and the single stop event. It reports fixed-time agent scores,
fixed-cohort human percentile tie bounds, and time to the four-hour P50, P90,
and P99 thresholds with right-censoring.

Candidate submission time, not private evaluation time, determines threshold
attainment. Private evaluation happens only after the run stops.
