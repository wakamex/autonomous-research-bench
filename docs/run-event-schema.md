# Run event schema

ARS runners write append-only JSON Lines using
[`run-event-v1.schema.json`](../schemas/run-event-v1.schema.json). Each line is
one event and carries `ars.run-event.v1` as its schema version.
An individual [`example event`](../examples/run-event-v1.json) is included for
schema validation.

Sequences start at zero and increase by one. `elapsed_seconds` comes from the
runner's monotonic clock and cannot decrease. `recorded_at` is an audit timestamp
and is not used for benchmark timing.

The benchmark submission time is the `elapsed_seconds` on
`candidate.submitted`, when the runner accepts an explicit candidate from the
research system. Evaluation completion time does not move that submission to a
later checkpoint.

Public evaluations may be shown to the research system. Private evaluations of
submitted candidates happen after the run and use `private-post-run`; their
scores must not be returned to the live system. This permits private
best-so-far reconstruction without leaking benchmark feedback.

Native evaluation events also retain the source, compiled candidate, tester,
and problem-package hashes plus the effective compiler identity. Case bundles
separately hash the generator, tester, seeds, and generated inputs.

Each native evaluation records its verified CPU contract: the selected CPU, effective affinity, cgroup path, and one-CPU quota. `run.started` records the run's assigned evaluator CPU and fixed worker count of one.

The schema defines these events:

- `run.started`
- `model_call.started`
- `model_call.completed`
- `candidate.submitted`
- `evaluation.completed`
- `resource.updated`
- `run.stopped`

Semantic validation beyond JSON Schema must reject duplicate or skipped
sequences, decreasing elapsed time, unknown candidate references, concurrent
model calls in the single-model track, post-deadline submissions, and resource
counters that decrease.
