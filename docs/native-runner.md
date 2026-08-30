# Native ARS runner

ARS now has one authoritative Rust implementation. ALE-Bench remains useful as
the provenance source and compatibility oracle for older problems, but it is
not a runtime dependency.

## Architecture

The `ars` binary owns:

- official tool builds and deterministic case preparation
- problem, generator, tester, case-bundle, source, compiler, and candidate hashes
- one isolated candidate compilation per evaluation
- batch-output and interactive candidate execution through strict official-tool adapters
- concurrent case execution
- wall, CPU, address-space, output, file-descriptor, and process limits
- bubblewrap filesystem and network isolation
- append-only run events and AOP usage import
- hidden post-run evaluation of every submitted candidate
- historical rank and performance mapping
- fixed-cohort human-reference compilation and trajectory replay
- evaluator worker sweeps

Public and private evaluation use the same engine. Public seeds are part of the
problem package. Private seeds and generated inputs are a separate evaluator
artifact that is not present while the agent is running.

Official deployment installs the release binary, problem package, and public
case bundle outside the agent-writable workspace. Repository-local execution is
for development only.

## Agent submission broker

Agent runs must receive no write access to the authoritative run directory. The host starts `ars submission-broker` with the run directory, problem package, public cases already recorded in run state, and evaluator sandbox. The broker listens on a mode-0600 Unix socket placed inside the task worktree. In the standard AOP edit profile, `ars submit --source benchmarks/PROBLEM/solution.cpp` finds that socket at `/workspace/.ars-submit.sock`. The client sends the source contents to the broker; the broker owns deadline checks, candidate numbering, timestamps, source archival, evaluation, and event writes. `ARS_SUBMIT_SOCKET` remains available as an override for other workspace layouts.

This boundary prevents the failure seen in the ten-lane GLM run, where `$ARS_RUN_DIR` was read-only inside AOP and agents diverted evaluations into writable `/tmp` copies. It also avoids the unsafe alternative of making the event log agent-writable.

Before starting a scored clock, run the starter through the same read-only outer-filesystem boundary:

```sh
ars preflight-submit \
  --workspace "$AOP_WORKTREE" \
  --problem problems/ahc044 \
  --public-cases "$PUBLIC_CASES" \
  --source benchmarks/ahc044/solution.cpp \
  --evaluator-cpu 14
```

The preflight creates a temporary unscored run, starts the real broker, invokes the real `ars submit` client inside a bubblewrap view where only the workspace is writable, and requires exactly one accepted authoritative evaluation. A failed preflight prevents the scored run from starting.

For the scored run, keep the run directory outside the AOP worktree and place only the socket inside it:

```sh
ars submission-broker \
  --socket "$AOP_WORKTREE/.ars-submit.sock" \
  --run-dir "$RUN_DIR" \
  --problem problems/ahc044 &
```

The broker is an ARS controller process, not part of the submitted harness. Stop it after AOP exits and before private evaluation.

## AHC044 compatibility and Sprint-9 coverage

The native scorer reproduced the ALE-Bench public and private scores for all 15
candidates archived from the Luna discovery run. Tests cover historical rank
and performance mapping. The Rust human-reference compiler, standings
validation, and replay scorer reproduce the existing AHC044 JSON results
exactly.

At 16 workers on the Ryzen 9 3950X, candidate 13's 50-case public evaluation had
a native median of 3.082 seconds over three trials, including isolated
compilation. The comparable ALE-Bench median was 19.293 seconds. That is 6.26
times faster and reduces wall time by 84.03 percent.

Sprint-9 now has native packages for AHC004, AHC044, AHC050, AHC052, AHC053, AHC062, AHC065, AHC067, and AHC068. The package format declares the two observed generator interfaces and whether judging is batch-output or interactive. AHC053's tester launches the isolated candidate and preserves its required two-stage conversation. Strict tester adapters turn malformed output into `WRONG_ANSWER` instead of accepting the visualizers' zero-score fallback.

The [Sprint-9 adapter smoke test](../results/discovery/sprint9-native-adapters-v1/README.md) generated and evaluated 50 cases for every problem. All nine starters were accepted under the one-CPU policy.

## Intentional scope

The current implementation supports the ARS use case, not every ALE-Bench API:

- Linux hosts
- additive nonnegative integer scores
- maximization objectives
- C++23 candidates
- the observed AtCoder Rust generator interfaces and batch or interactive testers
- fixed pre-generated public and private case bundles

It omits relative scoring, other languages, visualization, arbitrary agent-driven case generation, ALE session quotas, and container image emulation. Add one only when a selected ARS problem or track requires it.

## Remaining gaps

- Bubblewrap and Linux `wait4` and rlimit facilities are host dependencies, so
  portability currently means one ARS binary and problem package across Linux
  hosts, not cross-platform execution.
- RLIMIT_AS is stricter and less precise than a cgroup memory controller. The
  reference candidates do not approach the memory limit, so memory-limit parity
  needs a targeted fixture before promotion.
- The local GCC 15.2 toolchain differs from AHC044's original GCC 12.2 image.
  All archived scores match, but toolchain identity is recorded and belongs in
  the hardware and software execution profile.
- Case bundle hashes detect corruption. Adversarial integrity still depends on
  installing the runner and bundles outside the agent's writable boundary.
- The official tools are source-compatible but not interface-identical. A new problem still requires a package-level generator and judge-mode audit before inclusion.
- Human-score comparison requires a private bundle with the official case count. ARS now rejects a mismatched private bundle, but the held-out seed policy and score-distribution comparability still need promotion evidence for every new Sprint problem.
