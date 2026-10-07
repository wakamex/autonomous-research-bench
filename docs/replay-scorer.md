# Native run and trajectory scoring

The `arb` Rust binary is the authoritative runner and scorer. It replaces the former Python adapter, ALE-Bench session, and Docker or Podman execution path.

## Run lifecycle

Prepare public cases once:

```sh
cargo build --release --locked
target/release/arb prepare \
  --problem problems/ahc044 \
  --output /var/lib/arb/ahc044/public \
  --public
```

Generate a CPU plan, start a run, submit candidates, stop it, and privately evaluate every archived candidate:

```sh
arb cpu-plan --count 10 --output cpu-plan.json

arb init --run-dir RUN --run-id RUN_ID --duration 14400 \
  --model MODEL --system-manifest-sha256 SHA256 \
  --problem problems/ahc044 --public-cases /var/lib/arb/ahc044/public \
  --evaluator-cpu 3

arb submit --run-dir RUN --problem problems/ahc044 \
  --source benchmarks/ahc044/solution.cpp

arb continuation-prompt --run-dir RUN

arb stop --run-dir RUN --aop-result AOP_RESULT.json

arb private-evaluate --run-dir RUN --problem problems/ahc044 \
  --cases /var/lib/arb-private/ahc044
```

Private seeds and inputs are not part of the public problem package. The post-run evaluator supplies a separately held case bundle. Public and private evaluation use the run's recorded one-CPU contract.

## Trajectory score

```sh
arb score RUN/events.jsonl \
  --reference references/human/ahc044.json \
  --output RUN/score.json
```

The scorer validates event identity, sequence, monotonic timestamps, candidate references, and the single stop event. It reports fixed-time agent scores, fixed-cohort human percentile tie bounds, and time to the four-hour P50, P90, and P99 thresholds with right-censoring.

The reference artifact supplies the score direction. One shared ordering path selects best-so-far candidates, ranks scores from worst to best, computes tie bounds, and tests threshold attainment for both maximize and minimize objectives.

Candidate submission time, not private evaluation time, determines threshold attainment. Private evaluation happens only after the run stops.
