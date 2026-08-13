# AHC044 fixed baseline v2

This is the first complete ARS AHC044 discovery smoke. AOP ran Codex with
`gpt-5.4-mini`, medium effort, one concurrent model call, and a five-minute
research deadline. The prompt required an immediate starter submission, but the
agent first inspected local documentation and then submitted the unchanged
starter. No later candidate was submitted before AOP terminated the active model
at the deadline.

The candidate was archived at 65.988 seconds. It scored 507,388 on the 50-case
public evaluator and 1,541,672 on the 150-case private evaluator, corresponding
to ALE-Bench historical rank 986 and performance 236. Against the fixed
four-hour human cohort, its percentile midrank is 87.80 at 10 minutes and 3.11
at four hours. All fixed human four-hour
P50, P90, and P99 targets are right-censored at 14,400 seconds.

The run is discovery evidence only. AOP 0.1.1 returned zero token counts because
the model process was killed at timeout. Those raw zero fields are superseded by
[`recovered-usage.json`](recovered-usage.json), reconstructed from the isolated
Codex rollout with clanker-analytics. The run processed 513,957 tokens: 500,417
input tokens, of which 466,176 were cached, and 13,540 output tokens. This is
94,399 cache-adjusted tokens under clanker-analytics' 0.1x cache-read convention.
ALE-Bench returned no per-case CPU times, so evaluator CPU-seconds remain zero.
Measured evaluator wall times are 63.929 seconds public and 173.616 seconds
private.

The frozen event log labels the expected timeout as a failed run because that was
the adapter behavior used for this smoke. The live adapter now maps a fixed AOP
timeout to model cancellation and run reason `deadline`.

Reproduction identity:

- repository commit: `e5182e29c373b6b5750f0cbb12edf887cfa01452`
- ALE-Bench commit: `f7d927906dc1dcd860ee086e4560d576438b1354`
- frozen manifest SHA-256: `adca49cf32452d53d5929ac26fc8a8c0a3a67253f0bf5aa05ea90b5868568932`
- current replay reference SHA-256: `30e6a73e15071a69292b48855885a87266358cdc5d3bd4a9b325a22b3862b8c7`
- candidate SHA-256: `ee07a996f930d393349e4491d4a1adaf432680edd2791d8b1531e717b030ab57`
- evaluator image ID: `5116239bcf06d38687f82e3564d8ea6282e84e6d7f293ea52249afa642c89837`
- evaluator image digest: `sha256:79888edce0bcae4e6ca5149141af8533474e4c5d800ecdb21b0ac7212792709f`
- host: Fedora Linux kernel `6.17.9-300.fc43.x86_64`, Podman 5.7.1
- evaluator workers: 1
- Codex session: `019ffa3a-7ff7-73c0-9f68-599ab9b24b3e`

The frozen manifest incorrectly declared AOP 0.1.2. The observed executable was
AOP 0.1.1. The committed live manifest is corrected for subsequent runs, while
the exact run-time manifest snapshot remains here and matches the event hash.
