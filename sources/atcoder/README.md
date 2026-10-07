# AtCoder source snapshot

This directory documents the official source material used to build the ARS catalog of rated AtCoder heuristic contests. The catalog, statements and submission snapshots are generated locally by the sourcing scripts and are not tracked, because AtCoder does not license its pages and data for redistribution.

## Scope

- Archive class: AHC Class
- Rated range: All
- Cutoff: August 12, 2026
- Retrieved: August 12, 2026
- Contests: 72
- Statements: 72 official English task pages

The set contains AHC001 through AHC069 plus three separately named rated heuristic contests:

- `rcl-contest-2021-long`
- `future-contest-2022-qual`
- `toyota2023summer-final`

The machine-readable [`catalog.json`](catalog.json) records the contest metadata, task URL, task title, source hash, story excerpt, problem-statement introduction, and local statement path for every entry. Full source HTML is stored under `statements`.

## AtCoder labels

AtCoder labels these archive entries as `Heuristic` and gives each a rated range of `All`. The official archive and task pages do not expose semantic problem tags such as routing, packing, or inference. Semantic categories in the ARS catalog therefore come from either Table A1 of the ALE-Bench paper or an explicitly identified ARS classification based on the official statement.

## Refreshing the snapshot

Run the sourcing script with uv:

```sh
uv run scripts/source_atcoder_catalog.py
```

The default cutoff is fixed at August 12, 2026 so later contests do not silently change this benchmark-research snapshot.

## Sources

- AtCoder contest archive: https://atcoder.jp/contests/archive?lang=en&category=0&keyword=&ratedType=4
- ALE-Bench paper: https://arxiv.org/abs/2506.09050

Generated statements remain subject to AtCoder's terms and the rights of their respective authors.
