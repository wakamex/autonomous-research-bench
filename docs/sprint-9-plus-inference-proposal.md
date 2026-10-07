# Sprint-9 plus Inference proposal

Status: discovery candidate, frozen from the rated-contest catalog on August 12,
2026. It is not an official benchmark suite.

## Selection rule

Select the latest rated contest lasting at most six hours in each category that
has such a contest. Add the latest rated Inference contest because Inference has
no short contest.

Eligibility still requires comparable historical and agent scoring, a
reproducible evaluator, adequate score resolution, and acceptable evaluation
cost. A failure at one of these gates excludes the problem without replacing it
through subjective hand selection. The rule is rerun only at a declared suite
release, not whenever a newer contest appears.

## Candidate problems

| Category | Problem | Original duration | Pool | Four-hour submissions | Four-hour submitters |
| --- | --- | ---: | --- | ---: | ---: |
| Packing | AHC004 | 6h | ALE full | 2,792 | 537 |
| Routing | AHC062 | 4h | Post-ALE | 4,250 | 682 |
| Network | AHC067 | 4h | Post-ALE | 4,754 | 736 |
| Game | AHC050 | 4h | Post-ALE | 8,421 | 1,120 |
| Partitioning | AHC053 | 4h | Post-ALE | 7,964 | 1,069 |
| Covering | AHC052 | 4h | Post-ALE | 5,247 | 819 |
| Planning | AHC065 | 4h | Post-ALE | 4,600 | 705 |
| Scheduling | AHC044 | 4h | ALE full | 7,843 | 1,012 |
| Puzzle | AHC068 | 4h | Post-ALE | 5,201 | 855 |
| Inference | AHC030 | 240h | ALE full | 358 | 218 |
| Total | 10 problems | 9 short, 1 long | 3 ALE, 7 post-ALE | 51,430 | 7,753 problem-level |

The first nine problems are the `Sprint-9` ablation. They contain 51,072
submissions and 7,535 problem-level submitters. AHC030 contributes only 358
submissions and 218 submitters during the first four hours, so its human curve
requires particular scrutiny.

Submission counts come from the local [four-hour snapshots](../sources/atcoder/submissions-4h/README.md).
Submitters are unique within each problem and cannot be summed into a count of
distinct people across the suite.

## Why test this candidate

The rule reduces curator choice and favors problems less likely to have appeared
in older model training data. Seven of the ten contests occurred after the
ALE-Bench cutoff. It also retains all ten categories without pretending that an
Inference contest exists in the short pool.

Recency does not prove freedom from contamination. Every contest and its public
solutions become possible training material after release. Record each tested
model's knowledge cutoff, disable internet access during official runs, and
freeze the suite version before system results are inspected.

The price of recency is infrastructure work. Only AHC004, AHC030, and AHC044 are
in ALE-Bench, while the seven newer contests need scorer packaging, isolation,
and comparability validation.

## Comparison with ARB-10 breadth

| Property | Sprint-9 plus Inference | ARB-10 breadth |
| --- | ---: | ---: |
| Categories represented | 10 of 10 | 10 of 10 |
| Short contests | 9 | 6 |
| Long contests | 1 | 4 |
| Post-ALE contests | 7 | 0 |
| ALE-Bench packaged problems | 3 | 10 |
| Four-hour submissions | 51,430 | 37,447 |
| Four-hour problem-level submitters | 7,753 | 6,240 |
| Deterministic recency rule | Yes | No |
| Existing breadth-oriented curation | No | Yes |

Sprint-9 plus Inference has denser early human data and lower expected
contamination, but ARB-10 has ready-made infrastructure and deliberately chosen
research-behavior coverage. Contest duration alone is not evidence that the new
suite is cheaper to evaluate.

## Inference ablation

Every discovery system is run on all ten problems. Calculate aggregate rankings
twice from the same frozen results:

1. `Sprint-9+I`, including AHC030.
2. `Sprint-9`, excluding AHC030.

Evaluate AHC030 on:

- score comparability between historical submissions and the agent evaluator
- best-submission-per-human trajectory density and percentile stability
- evaluator CPU, candidate throughput, token use, and operational failures
- unique system failures or strengths not exposed by the other nine problems
- changes in pairwise system ordering and ranking uncertainty
- sensitivity of the aggregate result to its inclusion and score normalization

Retain AHC030 only if its Inference behavior adds useful system discrimination
and its human and agent scores are comparable at acceptable cost. Drop it from
the primary Sprint suite if the nine-problem ranking is stable without it and
AHC030 adds no reproducible capability signal, or if it fails the comparability
or cost gates. Inference can still remain in the broader promotion suite.

Discovery evidence selects the candidate. The inclusion decision, aggregation
rule, and acceptance thresholds must then be frozen before a separately labeled
promotion run.
