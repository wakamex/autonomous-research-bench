# Evaluator CPU policy

ARS gives every active benchmark run one evaluator CPU. Each evaluator is pinned to one logical CPU on a distinct physical core, limited to one CPU of cgroup quota, and prevented from changing its own affinity. This makes feedback latency substantially more repeatable than allowing parallel evaluations to contend for all cores.

The host selects the number of simultaneous benchmark lanes. The default is 10 on the reference Ryzen 9 3950X, leaving six physical cores for model runners, services, and the operating system. A dedicated 16-core host may choose up to 16 lanes after verifying that unrelated work does not share the selected physical cores.

## Lane selection

Generate a plan before starting a wave:

```sh
ars cpu-plan --count 10 --output cpu-plan.json
```

The planner samples utilization, groups SMT siblings by physical core, ranks physical cores by combined utilization, and chooses the less-used logical CPU from each selected core. It never assigns two lanes to the same physical core. Keep the resulting assignment fixed for the wave rather than choosing a new CPU for each submission.

Pass one lane CPU when initializing each run:

```sh
ars init --run-dir RUN --run-id RUN_ID --duration 14400 \
  --model MODEL --system-manifest-sha256 SHA256 \
  --problem problems/ahc044 --public-cases /var/lib/ars/ahc044/public \
  --evaluator-cpu 3
```

`ars submit` and `ars private-evaluate` then enter a transient systemd user service with `CPUAffinity=`, `CPUQuota=100%`, and `SystemCallFilter=~sched_setaffinity`. The evaluator verifies the effective affinity, quota, and cgroup before running and records that contract in the event log. Compilation and case execution use the same CPU contract.

This protects the evaluator from migration and prevents candidate code from widening its affinity. It does not reserve the core from unrelated host processes. Selecting quiet physical cores and leaving host capacity unused reduces that remaining interference.

## Dedicated-host isolation

Tuna is not part of the benchmark path. It mutates process and IRQ affinity across the host, needs privileged setup and cleanup, and cannot reliably restore every pre-existing custom affinity after an interrupted run.

A dedicated cloud or benchmark machine may later use cgroup v2 isolated CPU partitions to reserve evaluator cores and separate housekeeping work. That is a host deployment option, not a requirement of the current portable runner. Promotion of such a configuration requires measuring evaluator latency and score equivalence against the current policy and recording the loaded cgroup configuration.

## Concurrency estimate

The [AHC044 concurrency estimate](../results/discovery/ahc044-concurrent-evaluator-estimate-v1/README.md) compares one-CPU lanes with 16-worker evaluators sharing a 16-core host. Shared parallel evaluation produced about 34 percent more research cycles in the model, but overlapping evaluations had enough latency variation to affect which run receives feedback first. ARS therefore favors deterministic one-CPU lanes over maximum aggregate throughput for compared runs.

The earlier worker sweeps remain historical measurements of raw evaluator throughput. They no longer define the official public-evaluation policy.
