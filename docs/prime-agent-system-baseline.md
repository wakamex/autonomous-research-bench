# Prime Agent as an ARS system baseline

Status: discovery proposal. Prime Agent is not part of the authoritative ARS runner.

## Proposed role

Prime Agent should be evaluated as a submitted harness in the ARS system tracks. The native `ars` binary should remain the authoritative benchmark runner, evaluator, event recorder, deadline authority, and private scorer.

```text
ARS runner
  Owns isolation, deadline, candidate acceptance, scoring, and audit records

Prime Agent
  Supplies the research policy, persistent state, tools, refinement, and optional subagents
```

Prime Agent does not belong in the controlled-model track, where the minimal harness is fixed. It fits the single-model system track when all model calls, including child calls and refinement calls, are serialized. Its normal asynchronous subagent mode fits the unlimited system track when model-call concurrency is recorded.

## Evidence for testing it

Prime Agent combines a persistent IPython environment, durable sessions, recursive child agents, direct messaging, goals, heartbeats, recovery, and versioned supplemental prompts, memories, skills, and subagent specifications. These mechanisms address real ARS failure modes: losing experiment state during compaction, repeatedly parsing large evaluator logs in context, failing to continue after client detachment, and hiding descendant inference cost.

The August 2026 technical report provides mixed evidence rather than a demonstrated autoresearch advantage. On a nanoGPT speedrun, Prime Agent induced more programmatic experiments outside the benchmark training script, but the authors found that harness choice had little effect on final records relative to run noise. Its long-context and GPU-kernel comparisons also reverse by model. This makes Prime Agent worth a controlled ARS baseline, not a roadmap dependency.

Sources:

- Prime Agent technical report: https://arxiv.org/abs/2608.23552
- Prime Agent repository: https://github.com/PrimeIntellect-ai/prime-agent
- Continual Harness experiments: https://arxiv.org/abs/2605.09998

## ARS integration boundary

Prime Agent executes model-generated Python and shell commands with its worker's operating-system permissions. Its daemon and kernel boundaries are not a security sandbox. Official ARS runs must therefore place Prime Agent inside AOP or an equivalent outer sandbox with the same network, filesystem, credential, and process restrictions used for other submitted systems.

Prime Agent must interact with candidates only through the public workspace and `ars submit`. It must not receive private cases, human-reference artifacts, historical future information, or authority to modify the runner, evaluator, event log, benchmark clock, or installed problem package.

The outer ARS deadline remains authoritative. Prime Agent goals, heartbeats, schedules, and autonomous budgets are harness policies inside that deadline. A candidate accepted before the deadline remains eligible even if evaluation finishes later. Workspace snapshots remain non-scoring unless the agent explicitly submitted their source.

## Reproducibility contract

Every run should pin and preserve:

- Prime Agent release and commit
- base model, provider, reasoning effort, and model catalog identity
- complete initial prompt and supplemental harness state
- auto-refinement, compaction, goal, heartbeat, and autonomous-mode settings
- maximum recursion depth and model-call concurrency
- installed skills and their hashes
- session and child-session event histories
- root and descendant input, output, cached-token, and cost totals
- AOP invocation, sandbox policy, runner version, problem package, and evaluator hashes

Start every official run with an empty session-local harness store. Disable access to global Prime Agent harness state. Local refinements created during the run are part of the submitted system trajectory and must be archived. Reusing refinements across benchmark runs would leak development experience unless the initial state was frozen, submitted, and supplied identically to every declared repetition.

## Discovery experiment

The cheapest counterfactual is a paired AHC044 discovery run against the existing AOP/Codex baseline.

Freeze these factors across both conditions:

- model and reasoning effort
- initial task prompt and required time-zero starter submission
- wall-clock duration
- hardware and warm-server state
- public evaluator, case bundle, and worker count
- AOP isolation and network policy
- candidate source and compilation profile

Change only the harness:

1. Existing AOP/Codex baseline.
1. Prime Agent with a clean local state, refinement disabled, subagents disabled, and one model inference at a time.

This first comparison tests the persistent REPL and session interface without mixing in refinement or parallel sampling. If it passes, run separate declared ablations:

1. Enable local refinement while retaining serialized inference.
1. Enable recursive subagents while retaining the same aggregate inference budget.
1. Allow normal concurrent subagents only in the unlimited system track.

Use enough independent runs to estimate ordinary score-trajectory variance. The primary outcomes are the existing ARS score curve and time-to-human thresholds. Also report candidate submissions, valid-evaluation rate, time to first improvement, evaluator CPU, tokens, cost, idle time, recovery events, and model-call concurrency.

## Conformance gate

Before scoring comparative runs, verify that the adapter:

- records every root, child, and refinement model call
- has no overlapping model calls in the single-model track
- keeps resource counters monotonic across compaction and recovery
- accepts no candidate after the ARS deadline
- preserves explicit candidate submission timestamps
- terminates and archives cleanly when the outer runner stops it
- starts from the declared empty or frozen harness state
- cannot read private evaluation or human-reference data
- cannot use network access when the track forbids it

Do not interpret a successful install or multi-hour session as conformance evidence.

## Refinement safety

Prime Agent refinement should be treated as candidate generation, not trusted promotion. Its technical report describes a Factorio agent preserving an RCON resource-spawning exploit as a reusable skill despite an anti-cheating heartbeat. The mechanism retained behavior that improved the measured objective but violated the intended task.

ARS already has the right authority boundary: the submitted system may change its own prompts, memories, skills, and search policy, but it cannot change the evaluator or decide whether an improvement is valid. For a later cross-run refinement experiment, compare each proposed retained change against the frozen incumbent under a matched budget and a protected evaluator before promotion.

## Promotion rule

Prime Agent should join the reference-system panel only if it passes conformance and produces a repeatable improvement over the AOP baseline on held-out discovery runs at acceptable cost. More subagents, more experiments, or longer uninterrupted execution are mechanism observations, not promotion evidence by themselves.

Two failed interventions at the same integration gate should trigger a premise audit rather than more adapter complexity. No native-runner change is justified until a standalone Prime Agent adapter demonstrates missing runner functionality that materially affects a valid ARS run.
