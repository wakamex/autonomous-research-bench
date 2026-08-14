# AHC044 Luna low 20-minute discovery run

This discovery run used AOP 0.1.2 at commit `c43586e` to run Codex with
`gpt-5.6-luna`, low effort, and one concurrent model inference. The benchmark
clock was 1,200 seconds. It is discovery evidence, not a promotion run.

Codex submitted the starter at 26.530 seconds. The starter scored 507,388
publicly and 1,541,672 privately. The first Codex turn ended naturally after
301 seconds. AOP then resumed the exact session with only this neutral prompt:

```text
Continue working on the task. Time remaining: 880 seconds.
```

The resumed turn kept the same session ID, model, and effort. It ended
naturally near the benchmark deadline. Across both turns, Codex submitted 15
accepted candidates. Candidate 13 was best both publicly and privately:

- submitted at 973.830 seconds
- public score 49,189,250
- private score 147,488,854
- historical ALE-Bench rank 155 and performance 1887

The final candidate was slightly worse than candidate 13. Replay correctly
uses the private best-so-far trajectory rather than assuming the final source
is best.

At ten minutes, candidate 8 scored at human percentile 99.90 against the fixed
four-hour cohort. Candidate 2 reached the human four-hour P50 target at
121.003 seconds. The P90 and P99 targets were not reached and remain
right-censored at four hours. With no later submissions, candidate 13's score
corresponds to human percentile 84.78 at four hours.

The two AOP invocations used 2,263,216 input tokens, of which 2,118,912 were
cached, and 17,041 output tokens. Their estimated API-equivalent cost was
$0.17315188. AOP preserved nonzero usage on both naturally completed turns.

## Evaluator concurrency finding

All 15 public evaluations used one worker, as frozen in the system manifest.
Their summed wall time was 864.129 seconds, so public evaluation occupied most
of the 1,200-second research window.

Private candidates 1 through 7 also used one worker and took 129 to 201 seconds
each. The private sweep was then stopped between candidates and resumed with
eight workers. Candidates 8 through 15 took about 40 seconds each. The cutoff
is frozen in `private-evaluation-manifest.json`. Private concurrency cannot
change candidate eligibility or feedback because it happens after the agent
run, but public concurrency directly changes research speed.

Future discovery runs should test and then freeze a common public evaluator
concurrency, provisionally eight workers on this 32-CPU host. Every compared
system must receive the same evaluator allocation. Private post-run evaluation
can use the same or greater declared concurrency for operational efficiency.

A post-run fixed-starter counterfactual then tested that provisional setting.
The starter retained the exact public score of 507,388 while wall time fell
from 51.592 seconds at one worker to 18.696 seconds at eight workers, a 2.76x
speedup. `public-concurrency-audit.json` preserves the measurement. A later
[power-of-two sweep](../ahc044-evaluator-workers-3950x/README.md) on the best
candidate selected 16 workers for the next discovery run. Neither setting is
applied retroactively to this trajectory.

## Infrastructure preflight

Two launches were aborted before scoring because the evaluator boundary was
invalid. The first found an inactive rootless Podman socket. The second reached
Podman but could not build ALE-Bench's Rust generator because ALE-Bench assumes
rootful Docker UID ownership. Neither attempt is treated as a model result.

The clean run used a run-specific `container_file_t` temporary root and the
preserved `rootless-podman-sitecustomize.py` compatibility shim. It makes
ALE-Bench request container UID 0, which rootless Podman maps back to the
invoking host user. The unchanged starter reproduced its prior accepted score
before the clean clock began. ALE-Bench and its evaluator images were not
modified.

## Reproduction identity

- ARS base commit: `2e86b6dd1efb405493054bed2cbe74ebcfef2d5b`
- ALE-Bench commit: `f7d927906dc1dcd860ee086e4560d576438b1354`
- system manifest SHA-256: `a91083becd45c095d29b7047b61e40e7f87d49927836535957fc7b1287866a1c`
- human reference SHA-256: `30e6a73e15071a69292b48855885a87266358cdc5d3bd4a9b325a22b3862b8c7`
- evaluator image ID: `5116239bcf06d38687f82e3564d8ea6282e84e6d7f293ea52249afa642c89837`
- evaluator image digest: `sha256:79888edce0bcae4e6ca5149141af8533474e4c5d800ecdb21b0ac7212792709f`
- Codex session: `019ffeb7-e432-7051-b062-89de8ee613b4`
- initial AOP run: `550bf962-bed9-4058-a4dc-b2f11459ff79`
- resumed AOP run: `2bf81a5f-a68a-4bb1-a706-e6f65ba47185`
