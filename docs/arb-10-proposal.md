# ARB-10 discovery suite proposal

Status: discovery candidate, not an official benchmark suite.

ARB needs a small suite for frequent, affordable comparisons of complete autonomous research systems. The original ALE-Bench lite set is a strong starting point, but it was selected primarily for difficulty and correlation with aggregate ALE-Bench performance. ARB also needs broad coverage of different research loops and reliable human score-versus-time trajectories.

This proposal retains seven ALE-Bench lite problems and replaces three duplicated genres so that all ten genres in the ALE-Bench taxonomy appear once.

## Proposed problems

| Problem | Genre | Format | Judge type | ALE lite | Research behavior |
| --- | --- | --- | --- | --- | --- |
| AHC011 | Puzzle | Long | Standard | Yes | Search over move sequences and structural objectives |
| AHC015 | Game | Short | Reactive | Yes | Online decisions under random placements |
| AHC016 | Inference | Long | Reactive | Yes | Experimental design and noisy identification |
| AHC020 | Covering | Short | Standard | No | Joint coverage, power allocation, and connectivity optimization |
| AHC024 | Network | Short | Standard | Yes | Structural compression while preserving graph relations |
| AHC027 | Routing | Long | Standard | Yes | Recurring-route optimization with time-dependent costs |
| AHC039 | Partitioning | Short | Standard | Yes | Geometric separation of positive and negative targets |
| AHC040 | Packing | Long | Reactive | No | Packing under uncertain dimensions and noisy feedback |
| AHC044 | Scheduling | Short | Standard | No | Long-run frequency matching through compact transition rules |
| AHC046 | Planning | Short | Standard | Yes | Ordered visitation through environment modification |

The resulting balance is:

- 10 genres represented exactly once
- 6 short and 4 long contests
- 7 standard and 3 reactive problems
- 7 retained ALE-Bench lite problems
- 3 additions covering genres absent from ALE-Bench lite

Problem metadata and official sources are available in the [rated AtCoder heuristic problem catalog](atcoder-heuristic-catalog.md).

The [four-hour submission snapshot](../sources/atcoder/submissions-4h/README.md)
lists cumulative submission and participant counts for all ten problems and
preserves the underlying timestamped metadata.

## Changes from ALE-Bench lite

| Remove | Duplicated genre | Add | Previously missing genre |
| --- | --- | --- | --- |
| AHC008 | Game | AHC020 | Covering |
| AHC025 | Partitioning | AHC040 | Packing |
| AHC026 | Planning | AHC044 | Scheduling |

ALE-Bench lite contains two Game problems, two Partitioning problems, and two Planning problems. It contains no Covering, Packing, or Scheduling problem. The replacements trade some of the original difficulty-oriented curation for complete genre coverage.

## Why this may fit ARB better

ARB evaluates the system conducting research rather than only its final program. A compact suite should therefore expose different kinds of research work:

- continuous heuristic and parameter optimization
- structural algorithm changes
- search over action sequences
- experimental design under uncertainty
- online adaptation to feedback
- geometric, graph, routing, packing, and scheduling reasoning
- both fast iteration and long-horizon refinement

Representing each genre once reduces the chance that a system ranks highly because the small suite happens to repeat its strongest problem family.

## What is not yet established

The ALE-Bench paper evaluated its lite set against 73 full-benchmark results and found strong rank correlations. This proposal improves categorical coverage, but it has not yet demonstrated equal or better ranking fidelity.

The proposed ten also remain drawn from the original ALE-Bench 40. They do not test generalization to the 23 rated contests held after the paper cutoff.

No problem should be promoted solely because it fits the desired category table. Each candidate must also have suitable historical data, stable evaluation, useful score resolution, acceptable evaluator cost, and a defensible held-out test protocol.

## Validation before promotion

The proposal should be tested against the cheapest available counterfactual, the existing ALE-Bench lite set.

1. Recover the per-system, per-problem result matrix used in the ALE-Bench evaluation.
2. Compare the proposed suite with ALE-Bench lite using Spearman and Kendall rank correlations against the full 40.
3. Measure pairwise ranking inversions, especially among closely scored systems.
4. Estimate ranking uncertainty under repeated seeds and reduced private evaluation sets.
5. Measure model cost, evaluator CPU time, and end-to-end wall time.
6. Verify that historical human best-so-far trajectories can be reconstructed for every candidate.
7. Verify that private or system-held evaluation instances are available and separate from development instances.
8. Test alternative constrained subsets rather than evaluating only this hand-selected proposal.

Discovery results must not become promotion evidence without a separately declared validation run and fixed acceptance criteria.

## Longer-term suite design

ARB-10 should be an affordable development and screening suite, not the only source of leaderboard evidence. A stronger design would combine:

- a stable core for longitudinal comparisons
- a rotating held-out pool drawn from newer contests
- a broader promotion suite for official results

This structure preserves cheap iteration while limiting specialization to a permanently fixed set of ten problems.
