# AHC068 package

This package declares the official AHC068 generator and scoring sources and is judged by ARB's shared batch judge in [`judges/batch.rs`](../../judges/batch.rs). `uv run scripts/fetch_problem_packages.py` downloads the official archive recorded in `problem.json`, checks its SHA-256, extracts the official files, and installs the judge as the package tester.

The package defines seeds 0 through 49 for public discovery feedback. Private evaluation uses a separately held seed bundle after the research run. Historical human references remain separate from evaluator packaging.
