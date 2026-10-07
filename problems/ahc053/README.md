# AHC053 package

This package declares the official AHC053 generator and scoring sources and keeps the strict ARB tester adapter needed by the native runner in `adapters/`. `uv run scripts/fetch_problem_packages.py` downloads the official archive recorded in `problem.json`, checks its SHA-256, extracts the official files, and adds the adapter.

The package defines seeds 0 through 49 for public discovery feedback. Private evaluation uses a separately held seed bundle after the research run. Historical human references remain separate from evaluator packaging.
