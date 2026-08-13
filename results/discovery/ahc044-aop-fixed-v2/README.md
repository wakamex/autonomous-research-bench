# AHC044 fixed baseline v2

This is the first complete ARS AHC044 discovery smoke. AOP ran Codex with
`gpt-5.4-mini`, medium effort, one concurrent model call, and a five-minute
research deadline. The prompt required an immediate starter submission, but the
agent first inspected local documentation and then submitted the unchanged
starter. No later candidate was submitted before AOP terminated the active model
at the deadline.

The candidate was archived at 65.988 seconds. It scored 507,388 on the 50-case
public evaluator and 1,541,672 on the 150-case private evaluator, corresponding
to ALE-Bench historical rank 986 and performance 236. Its human percentile
midrank is 18.21 at 10 minutes and 3.11 at four hours. All fixed human four-hour
P50, P90, and P99 targets are right-censored at 14,400 seconds.

The run is discovery evidence only. AOP 0.1.1 returned zero token counts because
the model process was killed at timeout, so the zero-token and zero-cost fields
are missing telemetry, not free inference. ALE-Bench returned no per-case CPU
times, so evaluator CPU-seconds are also zero. Measured evaluator wall times are
63.929 seconds public and 173.616 seconds private.

The frozen event log labels the expected timeout as a failed run because that was
the adapter behavior used for this smoke. The live adapter now maps a fixed AOP
timeout to model cancellation and run reason `deadline`.

Reproduction identity:

- repository commit: `e5182e29c373b6b5750f0cbb12edf887cfa01452`
- ALE-Bench commit: `f7d927906dc1dcd860ee086e4560d576438b1354`
- frozen manifest SHA-256: `adca49cf32452d53d5929ac26fc8a8c0a3a67253f0bf5aa05ea90b5868568932`
- human reference SHA-256: `7b0f56c5ee7ff0ab214f4ef476c71c8897cd931864cecb472f6c2fc2ce677381`
- candidate SHA-256: `ee07a996f930d393349e4491d4a1adaf432680edd2791d8b1531e717b030ab57`
- evaluator image ID: `5116239bcf06d38687f82e3564d8ea6282e84e6d7f293ea52249afa642c89837`
- evaluator image digest: `sha256:79888edce0bcae4e6ca5149141af8533474e4c5d800ecdb21b0ac7212792709f`
- host: Fedora Linux kernel `6.17.9-300.fc43.x86_64`, Podman 5.7.1
- evaluator workers: 1

The frozen manifest incorrectly declared AOP 0.1.2. The observed executable was
AOP 0.1.1. The committed live manifest is corrected for subsequent runs, while
the exact run-time manifest snapshot remains here and matches the event hash.
