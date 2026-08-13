# Trajectory replay scorer

[`score_run.py`](../scripts/score_run.py) replays private post-run candidate
scores against a frozen human reference.

For each 10, 30, 60, 120, and 240 minute checkpoint, it selects the highest
private score among candidates submitted by that time. Evaluation completion
time does not affect checkpoint eligibility. It reports the agent score and its
human percentile among one best-so-far score per represented human.

Human ties produce an interval:

- `lower`: percentage of humans with a strictly lower score
- `upper`: percentage with a lower or equal score
- `midrank`: midpoint of the tie interval

The time-to-human-4h metrics use fixed P50, P90, and P99 scores from the
four-hour human distribution. Equality reaches a threshold. A target not
reached by four hours is right-censored at 14,400 seconds and receives no
fabricated completion time.

Run the scorer with:

```sh
uv run --no-project python -m scripts.score_run RUN/events.jsonl \
  --reference references/human/ahc044.json \
  --output RUN/score.json
```

## AHC044 adapter

[`ahc044_runner.py`](../scripts/ahc044_runner.py) provides four commands:

- `init` starts the monotonic run clock and event log.
- `submit` archives a candidate, timestamps it, and returns its public score.
- `stop` imports AOP token, cost, duration, and terminal status data.
- `private-evaluate` scores every archived candidate after the run.

The adapter uses the ALE-Bench 50-case public evaluator during research and
the 150-case private evaluator after the run. AHC044 has no post-contest system
test, so the private score basis matches the official historical score scale.
Private results are never returned to the live research system.

ALE-Bench does not report evaluator CPU-seconds for public or private calls, so
those resource values remain zero in the current adapter. Evaluation wall time,
case count, worker count, image version, and host configuration must be retained
until direct container CPU accounting is added.

The first fixed discovery smoke is retained under
[`results/discovery`](../results/discovery). It also exposed two accounting
limitations to fix before promotion runs: AOP 0.1.1 reports zero token usage
when Codex is killed at the fixed timeout, and adapter stop events currently
record orchestration cleanup after the research deadline. Candidate eligibility
still uses the monotonic submission timestamp, so neither changes replay scores.
