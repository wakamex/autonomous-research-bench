# AHC044 GLM 5.3 Flash high 20-minute discovery run

GLM 5.3 Flash improved the public 50-case score from the 507,388 starter to 31,905,948 in one 20-minute high-effort Codex turn. Its improved sixth candidate scored 93,979,736 on 150 held-out cases, corresponding to historical AHC044 rank 942 and performance 507. It did not reach the four-hour human P50 target of 136,730,318.

This was a discovery run through AOP 0.1.10 and Codex using Z.AI's `zai-coding-plan` provider. The benchmark clock was 1,200 seconds, model inference concurrency was one, and public evaluations used one worker pinned to logical CPU 2.

## Trajectory

| Candidate | Submitted | Public score | Private score | Historical rank | Public evaluation wall time |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 44.615 s | Evaluation infrastructure failed | 1,368,866 | 997 | - |
| 2 | 147.176 s | Evaluation infrastructure failed | 1,368,866 | 997 | - |
| 3 | 157.148 s | Evaluation infrastructure failed | 1,368,866 | 997 | - |
| 4 | 183.352 s | Evaluation infrastructure failed | 1,368,866 | 997 | - |
| 5 | 235.876 s | 507,388 | 1,368,866 | 997 | 1.295 s |
| 6 | 892.680 s | 31,905,948 | 93,979,736 | 942 | 84.249 s |

Candidates 1 through 5 are the identical starter source. The first four archives were created before evaluation exposed environment and package-path failures. ARS intentionally retained them and privately evaluated every submitted candidate after the run. Candidate 6 spends about 1.65 seconds per case on search, so its serial 50-case public evaluation consumed 84 seconds of the live benchmark clock and left 223 seconds when feedback arrived.

At the 10-minute checkpoint, the starter's private score was at human percentile 86.07 against the fixed four-hour cohort. Candidate 6 was submitted after that checkpoint. If its final score is carried forward while the human trajectory continues, it is at percentile 70.16 at 30 minutes, 47.92 at one hour, 24.70 at two hours, and 7.81 at four hours. P50, P90, and P99 time-to-human-4h are all right-censored.

## Run boundary and accounting

AOP terminated the Codex process at exactly 1,200 seconds and therefore records the invocation as timed out rather than naturally completed. That is the expected terminal condition for this fixed-duration run. AOP's normalized result records zero usage because the terminated Codex turn emitted no completion usage event. The raw event stream proves the session and tool activity but contains no token totals, so token spend and API-equivalent cost are unavailable, not measured zero.

An initial preflight launch using AOP's web-disabled mode was aborted before inference because the installed Codex CLI did not recognize AOP's `--no-tools` flag. The clean clock restarted from a fresh run directory without that flag. The prompt prohibited network use, but this discovery run did not technically enforce network isolation.

## Reproduction identity

- ARS base commit: `848d0ab0edd6921ac4f21cc7510957b823763ec6`
- AOP version: `0.1.10`
- Codex session: `01a048db-fc39-7b43-a9d2-903792167e6c`
- AOP run: `5baeab94-4309-4416-b98b-a38e208c6b64`
- model: `glm-5.3-flash`
- reasoning effort: `high`
- system manifest SHA-256: `4a5e0c7de9deef91efc410bb1801c5604e0e9bd1c61a3c11a742a1b910f9bb0f`
- human reference SHA-256: `30e6a73e15071a69292b48855885a87266358cdc5d3bd4a9b325a22b3862b8c7`
- run problem package SHA-256: `e91ddcd5ef1a5b89b375bf35ba5eba30b52602b4bb28c7146395b266a85f85af`
