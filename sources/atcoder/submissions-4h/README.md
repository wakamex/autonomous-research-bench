# Four-hour submission snapshots

This directory contains public submission metadata from each rated AtCoder
heuristic contest in the local catalog. Each compressed CSV covers the interval
from contest start through four elapsed hours, inclusive, or through contest end
when the contest ended earlier.

The snapshot was retrieved from the AtCoder Problems time-based Submission API
on August 12, 2026. The API is an unofficial index of public AtCoder metadata.
The retrieval script, exact source URL, retrieval timestamp, row counts, and
file hashes are preserved in this repository:

- [`../../../scripts/source_atcoder_submissions.py`](../../../scripts/source_atcoder_submissions.py)
- [`manifest.json`](manifest.json)
- [AtCoder Problems API documentation](https://github.com/kenkoooo/AtCoderProblems/blob/master/doc/api.md)

The source files contain submission IDs, timestamps, elapsed times, contest and
problem IDs, user IDs, language, score, code length, result, and execution time.
They do not contain source code.

The snapshot contains 242,390 submissions from 72 contests. Integrity checks
confirmed the manifest hashes and row counts, unique submission IDs within each
contest, exact contest and problem IDs, and timestamps within every declared
window.

## Proposed ARS-10 at four hours

These are cumulative counts at the four-hour cutoff. A scored submission means
an `AC` result. Other results include failed or invalid submissions such as WA,
TLE, RE, and CE. Submitters are unique within a problem, so the total should not
be interpreted as 6,240 unique people across the whole suite.

| Problem | Format | Submissions | Submitters | Scored (`AC`) | Other results |
| --- | --- | ---: | ---: | ---: | ---: |
| AHC011 | Long | 399 | 226 | 280 | 119 |
| AHC015 | Short | 6,673 | 808 | 5,772 | 901 |
| AHC016 | Long | 596 | 279 | 388 | 208 |
| AHC020 | Short | 7,358 | 994 | 6,437 | 921 |
| AHC024 | Short | 4,318 | 729 | 2,760 | 1,558 |
| AHC027 | Long | 326 | 194 | 218 | 108 |
| AHC039 | Short | 4,436 | 750 | 3,002 | 1,434 |
| AHC040 | Long | 505 | 261 | 381 | 124 |
| AHC044 | Short | 7,843 | 1,012 | 7,007 | 836 |
| AHC046 | Short | 4,993 | 987 | 3,805 | 1,188 |
| Total | 6 short, 4 long | 37,447 | 6,240 problem-level | 30,050 | 7,397 |

The participation split is already useful evidence for the suite experiment.
The four long contests have hundreds of early submissions, but markedly fewer
than the short contests. The next audit should compare first-submission timing,
active-contestant definitions, and score-trajectory resolution rather than use
submission volume alone to accept or reject a problem.

## Completeness and scoring limits

This is a complete snapshot relative to the rows exposed by the AtCoder Problems
time API at retrieval time. It has not yet been independently reconciled row by
row against AtCoder's authenticated all-submissions pages.

The `point` field is the score attached to each public submission in AtCoder
Problems. For long contests in particular, provisional submission scores may
not be directly comparable with final standings or ALE-Bench private-system
scores. The planned trajectory audit must establish the correct score transform
and historical reference population before these files support human-percentile
claims.

To reproduce the snapshot, run:

```sh
uv run scripts/source_atcoder_submissions.py
```
