# AHC044 package

This package declares the official AHC044 generator and scoring sources, the 50 public seeds used by ARB, and the public final standings and performance mapping used for historical rank reporting.

`uv run scripts/fetch_problem_packages.py` extracts those files without modification from the pinned ALE-Bench `ahc044.zip` artifact whose SHA-256 is recorded in `problem.json`, and installs ARB's shared batch judge from [`judges/batch.rs`](../../judges/batch.rs) as the package tester. Private
seeds and generated private inputs are deliberately not part of the package.
An official evaluator supplies them separately after the research run ends.
