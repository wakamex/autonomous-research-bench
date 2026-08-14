# Autonomous Research Speedrun

Submit the researcher, not the answer.

Autonomous Research Speedrun, or ARS, is a proposed living benchmark and leaderboard for autonomous research systems. It measures how quickly a complete system can discover competitive solutions to optimization problems.

Most optimization benchmarks evaluate only the final submitted solution. The model, harness, prompts, memory, experiment loop, and search strategy that produced it are ignored. ARS reverses that relationship:

```text
Current:  submit the solution, ignore the harness and model
ARS:      submit the harness and model, ignore the final solution
```

The goal is to evaluate the researcher itself.

## Motivation

Keller Jordan's modded-nanoGPT work demonstrates a compelling training and optimization speedrun format. An autonomous agent can repeatedly modify code, run experiments, inspect results, and search for improvements. The benchmark supplies the optimization environment, while the autoresearch system supplies the search policy.

That environment is deep and highly optimized. ARS aims to complement it with a broader suite of algorithm-engineering problems and a leaderboard centered on the capabilities of the autonomous research system.

## Initial benchmark suite

The first proposed suite is ARS-AHC, built from AtCoder Heuristic Contest problems.

These problems are a strong fit because they:

- reward iterative algorithm and implementation improvements
- have objective, automatically evaluated scores
- include historical human submissions and timestamps
- span a variety of optimization structures
- support repeated experiments within practical compute budgets

[ALE-Bench](https://github.com/SakanaAI/ALE-Bench) already packages 40 historical AtCoder Heuristic Contest problems for autonomous algorithm-engineering evaluation. ARS-AHC could build on that infrastructure, initially using a carefully selected subset of roughly 8 to 12 problems to make frequent benchmark runs affordable.

ALE-Bench is included as a pinned Git submodule under `ALE-Bench`. After cloning ARS, initialize it with:

```sh
git submodule update --init --recursive
```

## What is measured

The main result is wall-clock efficiency against historical human performance.

For each problem, historical AtCoder submissions can be used to reconstruct contestants' best-so-far score trajectories. An ARS run can then be compared with human percentile curves and reported in terms such as:

```text
Time to human 4h P50:  8 minutes
Time to human 4h P90: 47 minutes
Time to human 4h P99: 2 hours 16 minutes
```

Each target is fixed from the distribution of one best submission per human at four hours. The complete score-versus-wall-clock curve and same-time human percentiles at fixed checkpoints remain first-class results. A possible aggregate metric is the geometric mean time-to-threshold across problems that reached the target, reported with the target attainment rate.

Every run should also report the resources used:

- model and harness configuration
- inference cost and token counts
- evaluation CPU-seconds
- number of candidate programs evaluated
- model-call and evaluation concurrency
- total wall-clock time

These measurements make systems with different architectures easier to understand without collapsing every tradeoff into one number.

## Tracks

### Controlled model track

All entrants use the same minimal harness and resource limits. Only the model varies.

This track isolates model capability as much as practical.

### Single-model system track

Entrants may submit any harness and model combination, but only one model inference may run at a time.

This track measures agent architecture, prompting, memory, experiment selection, and other system design choices without allowing massive parallel sampling to dominate. It is the clearest track for comparing time to human performance.

### Unlimited system track

Entrants submit the complete harness and model system with unrestricted concurrent model calls.

This track measures practical wall-clock performance under normal deployment, including parallel research agents, ensembles, branching strategies, and other scaling techniques.

## Evaluation integrity

The agent should iterate on development instances that are separate from the fixed official evaluation set. Official runs should use hidden or system-held test instances where the underlying problem format permits it.

This separation reduces benchmark-instance hill climbing and rewards research strategies that generalize beyond the examples visible during development.

Reproducible runs should preserve the effective configuration, model identifiers, prompts, timing data, optimizer or search parameters, source hashes, and generated artifacts needed for audit.

## Design principles

- Evaluate the autonomous research system rather than a hand-selected final artifact.
- Preserve score trajectories instead of reporting only the final score.
- Compare machine progress with historical human progress over equal wall-clock time.
- Make inference, evaluation, and concurrency costs visible.
- Support both controlled scientific comparisons and practical unrestricted systems.
- Keep the suite affordable enough for frequent reruns as models and harnesses improve.
- Prefer held-out evaluation and reproducible execution over benchmark-specific tuning.

## Relationship to existing work

```text
Keller-style speedruns
  Deep, highly optimized autoresearch environments

ARS-AHC
  A broad autonomous-research speedrun suite
```

ARS is not intended to replace solution leaderboards or static algorithm benchmarks. It asks a different question:

> Given a problem, a budget, and access to experiments, how quickly can an autonomous research system make progress?

The ALE-Bench paper and its provenance record are preserved under [`papers`](papers/README.md).

The [`rated AtCoder heuristic problem catalog`](docs/atcoder-heuristic-catalog.md) documents all 72 rated contests through August 12, 2026, including the original ALE-Bench full and lite selections.

The [`ARS-10 discovery suite proposal`](docs/ars-10-proposal.md) defines a provisional ten-problem subset with complete genre coverage and the validation required before promotion.

The [`four-hour suite test plan`](docs/four-hour-suite-test-plan.md) compares Sprint-9, Sprint-9 plus Inference, and the mixed ARS-10 proposal under an equal four-hour wall-clock cap.

The [`Sprint-9 plus Inference proposal`](docs/sprint-9-plus-inference-proposal.md) uses a deterministic latest-per-category rule and tests whether its one long Inference problem should remain after discovery.

The [`four-hour submission snapshots`](sources/atcoder/submissions-4h/README.md) preserve timestamped public submission metadata for all 72 rated contests and summarize the proposed ARS-10 at the cutoff.

The [`human reference compiler`](docs/human-reference.md) reconstructs fixed-time percentiles and four-hour human thresholds. The [`run event schema`](docs/run-event-schema.md) defines the append-only contract for future runners.

The [`trajectory replay scorer and AHC044 adapter`](docs/replay-scorer.md) turn timestamped candidate submissions into checkpoint percentiles and time-to-human-4h results.

The [`first fixed AHC044 discovery baseline`](results/discovery/ahc044-aop-fixed-v2/README.md) preserves its candidate, public and private evaluations, event trajectory, replay score, and reproduction identity.

The [`20-minute Luna continuation run`](results/discovery/ahc044-aop-luna-low-20m-v3/README.md) validates exact-session continuation, resumed-turn usage accounting, 15-candidate trajectory replay, and the evaluator-concurrency policy.

The [`3950X evaluator concurrency procedure`](docs/evaluator-concurrency.md) tests powers of two until median performance decreases. Its first AHC044 sweep selects 16 public workers for the next discovery protocol.

The [`time-budget management research`](docs/time-budget-management.md) compares long-running agent harnesses and proposes deadline, snapshot, submission, and recovery rules for ARS.

## Status

ARS is currently a design-stage project. The first milestone is to validate the benchmark methodology on a small set of AtCoder Heuristic Contest problems before fixing the initial suite, thresholds, resource policy, and leaderboard protocol.

Likely early work includes:

1. Audit ALE-Bench and the available historical AtCoder data.
2. Select representative candidate problems with reliable scoring and timelines.
3. Reconstruct and validate human best-so-far percentile curves.
4. Define timing, concurrency, compute, and inference accounting rules.
5. Build a minimal reference harness and end-to-end runner.
6. Run baseline systems and test whether the proposed metrics are stable.
7. Publish a versioned benchmark specification when the methodology is validated.

## Short description

Autonomous Research Speedrun is a living benchmark for measuring how quickly AI research systems can improve solutions to optimization problems. Its first proposed suite, ARS-AHC, compares complete harness-and-model systems against historical human progress on AtCoder Heuristic Contests.
