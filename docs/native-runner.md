# Native ARS runner

ARS now has one authoritative Rust implementation. ALE-Bench remains useful as
the provenance source and compatibility oracle for older problems, but it is
not a runtime dependency.

## Architecture

The `ars` binary owns:

- official tool builds and deterministic case preparation
- problem, generator, tester, case-bundle, source, compiler, and candidate hashes
- one isolated candidate compilation per evaluation
- concurrent batch-case execution
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

## AHC044 compatibility

The native scorer reproduced the ALE-Bench public and private scores for all 15
candidates archived from the Luna discovery run. Tests cover historical rank
and performance mapping. The Rust human-reference compiler, standings
validation, and replay scorer reproduce the existing AHC044 JSON results
exactly.

At 16 workers on the Ryzen 9 3950X, candidate 13's 50-case public evaluation had
a native median of 3.082 seconds over three trials, including isolated
compilation. The comparable ALE-Bench median was 19.293 seconds. That is 6.26
times faster and reduces wall time by 84.03 percent.

## Intentional scope

The current implementation supports the ARS use case, not every ALE-Bench API:

- Linux hosts
- batch problems with additive integer scores
- maximization objectives
- C++23 candidates
- the standard AtCoder Rust generator and tester interface
- fixed pre-generated public and private case bundles

It omits reactive judges, relative scoring, other languages, visualization,
arbitrary agent-driven case generation, ALE session quotas, and container image
emulation. Add one only when a selected ARS problem or track requires it.

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
- AHC044 is the only native problem package so far. Batch problems using the
  same official tool convention should need packaging rather than runner
  changes, but that needs validation across the rest of Sprint-9.
