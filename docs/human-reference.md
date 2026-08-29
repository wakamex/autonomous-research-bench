# Human reference compiler

The human reference compiler reconstructs one best-so-far score trajectory per
public submitter in a fixed cohort: everyone who submitted at least once during
the official four-hour contest. At every elapsed-time cutoff, every cohort member
is included. A user's value is their highest `AC` score by the cutoff, or zero
before their first scored submission and while all submissions have failed.

Percentiles use the ascending nearest-rank definition:

```text
sorted_scores[ceil(percentile / 100 * population_size) - 1]
```

This makes P90 the score at or above 90 percent of the fixed cohort. Because
every individual trajectory is nondecreasing and the denominator never changes,
each percentile score threshold is also nondecreasing. The definition is frozen
in each generated reference artifact.

The same compiler and replay scorer support minimize objectives with `--score-direction minimize`. Scores are always ordered from worst to best before applying nearest rank, so P90 retains the same meaning. Best-so-far costs and their thresholds are nonincreasing, and an agent reaches a target when its cost is at or below it. Before a human's first scored submission, maximize references use the established score of zero while minimize references use `null`, which remains worse than every numeric cost.

Each artifact precomputes the selected cohort as its `trajectories` list. Its
`cohort` summary separately records total users and submissions present in the
source, users admitted or excluded by the desired cutoff, submissions by that
cutoff, and the source's maximum elapsed time. This makes shorter cutoff audits
cheap and makes it clear whether the source actually covers the full comparison
window. Use `--cohort-seconds` to compile a different fixed cohort.

## AHC044 result

The initial artifact is [`ahc044.json`](../references/human/ahc044.json). It was
compiled from 7,843 public submissions by 1,012 humans. At four hours, 987 had a
scored submission.

| Elapsed | Cohort | Submitted | Scored | P50 | P90 | P99 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 10m | 1,012 | 151 | 141 | 0 | 81,253,586 | 112,722,738 |
| 30m | 1,012 | 402 | 392 | 0 | 112,722,738 | 141,071,162 |
| 1h | 1,012 | 637 | 618 | 101,430,556 | 135,007,154 | 147,080,008 |
| 2h | 1,012 | 842 | 825 | 118,826,980 | 145,595,608 | 149,467,772 |
| 4h | 1,012 | 1,012 | 987 | 136,730,318 | 148,472,162 | 149,730,504 |

The four-hour row defines the provisional time-to-human-4h targets.

## Official standings validation

The compiler compared the 987 public best-score values with the expanded final
standings score distribution packaged unchanged in the ALE-Bench AHC044 source
archive. It matched 984 public scores, or 99.70 percent. The public and official
P99 values are identical. Public P50 differs from the official distribution by
0.21 percent and P90 by 0.06 percent.

The validation is not an assertion that the populations are identical. The
ALE-Bench standings file contains ranks and scores but no user IDs, has 996
positive ranked positions, and does not contain the highest public submitter's
score. ARS intentionally uses every public submitter represented by the
timestamped source. The artifact records both unmatched multisets and source
hashes so this choice remains auditable.

Reproduce the fixed-cohort portion with the native runner:

```sh
ars build-human-reference \
  --problem-id ahc044 \
  --submissions sources/atcoder/submissions-4h/ahc044.csv.gz \
  --problem problems/ahc044 \
  --output references/human/ahc044.json
```

The AHC044 problem package preserves the final standings and rank-performance
tables. The benchmark no longer loads an ALE-Bench archive at runtime.
