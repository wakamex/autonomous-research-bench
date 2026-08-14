# Trajectory replay scorer

[`score_run.py`](../scripts/score_run.py) replays private post-run candidate
scores against a frozen human reference.

For each 10, 30, 60, 120, and 240 minute checkpoint, it selects the highest
private score among candidates submitted by that time. Evaluation completion
time does not affect checkpoint eligibility. It reports the agent score and its
human percentile among one best-so-far score per member of the fixed four-hour
human cohort. Humans who have not submitted by the checkpoint have score zero.

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

[`ahc044_runner.py`](../scripts/ahc044_runner.py) provides five commands:

- `init` starts the monotonic run clock and event log.
- `submit` archives a candidate, timestamps it, and returns its public score and
  benchmark clock.
- `continuation-prompt` prints a neutral prompt containing the current time left.
- `stop` imports AOP token, cost, duration, and terminal status data.
- `private-evaluate` scores every archived candidate after the run.

The adapter uses the ALE-Bench 50-case public evaluator during research and
the 150-case private evaluator after the run. AHC044 has no post-contest system
test, so the private score basis matches the official historical score scale.
Private results are never returned to the live research system.

Every public evaluation response includes `elapsed_seconds` and
`remaining_seconds`, measured after evaluation finishes. If the model ends a
turn before the benchmark deadline, the supervisor may resume the exact session
with the output of `continuation-prompt`:

```text
Continue working on the task. Time remaining: 7,088 seconds.
```

This contract is independent of the agent harness. It supplies the clock but
does not prescribe an experiment policy, add warning phases, or force a final
submission.

The adapter sums ALE-Bench's reported case execution times as evaluator
CPU-seconds. Very fast cases can be reported as zero, so evaluation wall time,
case count, worker count, image version, and host configuration must also be
retained.

Evaluator concurrency has two different roles. Public evaluation happens on
the benchmark clock, so its worker count changes research throughput and must
be frozen identically across compared systems. Private evaluation happens only
after the agent stops and cannot affect its trajectory, so it may use higher
declared concurrency for operational efficiency. The first 20-minute AHC044
discovery run reduced private candidate wall time from 129 to 201 seconds at
one worker to about 40 seconds at eight workers. It also found that serial
public evaluations occupied most of the live research window, making public
worker count an important protocol parameter rather than an implementation
detail.

A fixed-starter public audit first confirmed the effect directly. A subsequent
[power-of-two sweep](evaluator-concurrency.md) on a representative candidate
selected 16 public workers for the next AHC044 discovery protocol. Median wall
time fell from 59.8 seconds at one worker to 19.3 seconds at 16. Thirty-two was
effectively tied, while 64 was 8.61 percent slower. The setting must be
reevaluated when the host, problem, case count, evaluator, or session lifecycle
changes.

The first fixed discovery smoke is retained under
[`results/discovery`](../results/discovery). It also exposed two accounting
limitations to fix before promotion runs: AOP 0.1.1 reports zero token usage
when Codex is killed at the fixed timeout, though usage can be recovered from
the isolated Codex rollout with clanker-analytics. Adapter stop events currently
record orchestration cleanup after the research deadline. Candidate eligibility
still uses the monotonic submission timestamp, so neither changes replay scores.
