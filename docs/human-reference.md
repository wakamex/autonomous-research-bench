# Human reference compiler

The human reference compiler reconstructs one best-so-far score trajectory per
public submitter. At an elapsed-time cutoff, it includes users with at least one
submission by then. A user's value is their highest `AC` score by the cutoff, or
the no-score value zero when all of their submissions have failed.

Percentiles use the ascending nearest-rank definition:

```text
sorted_scores[ceil(percentile / 100 * population_size) - 1]
```

This makes P90 the score at or above 90 percent of represented humans. The
definition is frozen in each generated reference artifact.

## AHC044 result

The initial artifact is [`ahc044.json`](../references/human/ahc044.json). It was
compiled from 7,843 public submissions by 1,012 humans. At four hours, 987 had a
scored submission.

| Elapsed | Humans | Scored humans | P50 | P90 | P99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 10m | 151 | 141 | 112,722,734 | 112,722,738 | 144,701,614 |
| 30m | 402 | 392 | 112,722,734 | 125,741,594 | 146,699,040 |
| 1h | 637 | 618 | 112,722,738 | 138,332,064 | 148,358,722 |
| 2h | 842 | 825 | 127,460,686 | 146,498,184 | 149,527,164 |
| 4h | 1,012 | 987 | 136,730,318 | 148,472,162 | 149,730,504 |

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

Reproduce the artifact after downloading the cited ALE-Bench archive:

```sh
uv run --no-project scripts/build_human_reference.py \
  --ale-bench-zip /path/to/ahc044.zip
```
