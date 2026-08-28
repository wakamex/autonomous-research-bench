# Concurrent evaluator estimate

The model estimates whether 16 simultaneous AHC044 research runs should each use one evaluator CPU or allow every evaluator to parallelize across all 16 physical cores. One-CPU lanes sacrifice aggregate feedback throughput but avoid contention-dependent feedback order, so ARS uses them for compared runs.

| Policy | Mean evaluations per four-hour run | Mean feedback latency | P95 feedback latency | Evaluations relative to one CPU |
| --- | ---: | ---: | ---: | ---: |
| One pinned CPU per run | 459.28 | 11.91 s | 12.79 s | 1.000x |
| Shared 16-worker evaluators | 615.73 | 3.97 s | 5.34 s | 1.341x |

The estimate is a 100-repetition, four-hour closed-loop discrete-event simulation. It resamples measured one-worker and 16-worker AHC044 evaluator timings plus the observed gaps between evaluation completion and the next submission in the Luna baseline. Concurrent phases share 16 physical cores through max-min fair allocation.

About 72 percent of shared-policy case evaluations overlapped another evaluation. Their mean slowdown was 1.46x, their P95 slowdown was 2.84x, and a typical four-hour campaign's slowest evaluation was about 8.3 seconds. The shared policy therefore offers about 34 percent more research cycles but exposes individual runs to contention timing.

Run the estimator with:

```sh
cargo run --release --locked -- estimate-concurrency \
  --repetitions 100 \
  --output results/discovery/ahc044-concurrent-evaluator-estimate-v1/estimate.json
```

`estimate.json` records source hashes, the seed, empirical timing inputs, distribution summaries, and modeled limitations. This is discovery evidence, not promotion evidence: scheduler migration, SMT, cache, memory, storage, and provider-side model contention are not simulated.
