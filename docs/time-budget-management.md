# Managing time-limited agent runs

## What other harnesses actually guarantee

Time-limited benchmarks generally guarantee that the harness stops and produces
an auditable outcome. They do not guarantee that the model completes the task or
even emits a valid final submission. Time management remains part of the system
being evaluated.

PaperBench is the closest useful precedent for long-running research work. Its
canonical configuration gives the solver an explicit `time_limit`, separates
agent rollout from reproduction and grading, and stores timestamped submission
directories. It creates initial and final workspace snapshots and can create
intermediate snapshots by message count or elapsed seconds. Missing direct
submissions are graded as zero. See the locally preserved
[`PaperBench README`](../sources/time-budget-benchmarks/paperbench.md).

mini-SWE-agent layers wall-clock, step, and cost limits. It checks limits before
another model query, emits typed `TimeExceeded` or `LimitsExceeded` outcomes,
and saves the trajectory from a `finally` block. Its timeout outcome contains an
empty submission, so durable logging is guaranteed but useful task completion is
not. See the locally preserved
[`DefaultAgent reference`](../sources/time-budget-benchmarks/mini.md).

MLE-Bench standardizes a 24-hour runtime and fixed compute resources, requires a
CSV artifact, provides structural submission validation, and grades submission
files independently of the agent implementation. Its current leaderboard also
separates runs with materially different runtime or test-set access, which is a
warning that time and feedback policies are part of result comparability. See
the locally preserved [`MLE-Bench README`](../sources/time-budget-benchmarks/mle.md).

RE-Bench gives agents open-ended research environments with objective scoring
tools and compares performance under fixed time budgets. It standardizes the
task environment but leaves iteration and stopping policy to the agent scaffold.
See the locally preserved [`RE-Bench README`](../sources/time-budget-benchmarks/rebench.md)
and the [official paper](https://arxiv.org/abs/2411.15114).

OpenHands adds a complementary harness technique rather than a benchmark rule:
its default stuck detector recognizes repeated action-observation cycles,
repeated errors, monologues, alternating loops, and repeated context failures.
It can stop or redirect an unproductive run. The thresholds and patterns are
documented in the [official OpenHands guide](https://docs.openhands.dev/sdk/guides/agent-stuck-detector).

## Useful design pattern

The common pattern is layered:

1. An external monotonic deadline ends the agent process.
2. The research artifact is separate from the model's final prose response.
3. State, trajectories, and resource counters are persisted throughout the run.
4. Evaluation has its own timeout and runs outside the research budget.
5. Timeout, invalid submission, infrastructure failure, and ordinary low score
   are distinct outcomes.
6. Step, token, or cost limits may supplement wall time, but they must be frozen
   and disclosed because they change the capability being measured.

Periodic snapshotting protects against process loss, but automatically scoring a
snapshot changes an explicit-submission benchmark into a workspace-state
benchmark. PaperBench can use workspace snapshots because the workspace is its
submission object. ARS is modeled on human contest submissions, so only explicit
candidate submissions should affect the score trajectory.

## Recommended ARS behavior

ARS should make the run reliable without rescuing poor time management:

- Archive and privately score a starter candidate at time zero. This guarantees
  a valid trajectory without spending model time on submission plumbing.
- Count only explicit post-start candidate submissions for improvements. Do not
  auto-promote unsubmitted work at timeout.
- Persist periodic worktree snapshots for audit and recovery, clearly marked
  non-scoring.
- Put remaining research time and observed evaluator latency in every public
  evaluation response. Standardized warnings at fixed fractions of the budget
  are reasonable in the controlled track; custom scheduling remains part of the
  submitted harness in system tracks.
- Archive a candidate before public evaluation, as the current adapter does. A
  candidate accepted before the deadline remains eligible even if evaluation
  finishes afterward or the live agent is terminated.
- Freeze candidate acceptance exactly at the research deadline, then allow a
  separate teardown and private-evaluation grace period. The outer process
  timeout must not be mistaken for the benchmark clock.
- Record `deadline_with_candidate` and `deadline_without_candidate` separately,
  along with the best candidate timestamp. A timeout is an expected terminal
  condition, not an infrastructure failure.
- Add optional stuck detection and progress nudges, but treat their policy as
  harness configuration. The controlled-model track needs one frozen policy.
- Keep wall clock as the official budget. Cost, tokens, calls, steps, evaluator
  time, and concurrency remain reported secondary resource measures rather than
  substitute deadlines.

For the five-minute AHC044 smoke, the most important immediate changes are the
time-zero starter, explicit remaining-time feedback, and separating the
five-minute research cutoff from AOP teardown. The run already had the other
critical property: the candidate source was archived before evaluation and was
therefore recoverable despite the model being killed at the deadline.
