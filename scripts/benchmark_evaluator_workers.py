"""Find the fastest public evaluator worker count by powers of two."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import statistics
import sys
from datetime import UTC, datetime
from pathlib import Path

from scripts.ahc044_runner import evaluate


def cpu_model() -> str:
    for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
        if line.startswith("model name"):
            return line.split(":", 1)[1].strip()
    return "unknown"


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    root.add_argument("--source", type=Path, required=True)
    root.add_argument("--output", type=Path, required=True)
    root.add_argument("--repeats", type=int, default=3)
    root.add_argument("--max-workers", type=int, default=128)
    root.add_argument("--clear-decrease-percent", type=float, default=5.0)
    root.add_argument("--language", default="cpp23")
    return root


def main() -> None:
    args = parser().parse_args()
    if args.repeats < 1:
        raise ValueError("repeats must be positive")
    if args.max_workers < 1 or args.max_workers & (args.max_workers - 1):
        raise ValueError("max-workers must be a power of two")
    if args.clear_decrease_percent <= 0:
        raise ValueError("clear-decrease-percent must be positive")

    source = args.source.resolve()
    started_at = datetime.now(UTC).isoformat()
    content = source.read_text(encoding="utf-8")
    expected_score: int | None = None
    measurements: list[dict[str, object]] = []
    selected_workers = 1
    best_wall = float("inf")
    stop_reason = "max_workers_reached"
    workers = 1

    while workers <= args.max_workers:
        trials: list[dict[str, object]] = []
        for repeat in range(1, args.repeats + 1):
            result = evaluate(content, args.language, "public", workers)
            if result["judge_result"] != "ACCEPTED":
                raise RuntimeError(
                    f"workers={workers} repeat={repeat}: {result['judge_result']}"
                )
            score = int(result["score"])
            if expected_score is None:
                expected_score = score
            elif score != expected_score:
                raise RuntimeError(
                    f"score changed from {expected_score} to {score} at workers={workers}"
                )
            trial = {
                "repeat": repeat,
                "wall_seconds": result["wall_seconds"],
                "cpu_seconds": result["cpu_seconds"],
                "score": score,
            }
            trials.append(trial)
            print(json.dumps({"workers": workers, **trial}), flush=True)

        median_wall = statistics.median(
            float(trial["wall_seconds"]) for trial in trials
        )
        measurement = {
            "workers": workers,
            "trials": trials,
            "median_wall_seconds": median_wall,
            "public_cases_per_second": 50 / median_wall,
        }
        measurements.append(measurement)
        clear_decrease_wall = best_wall * (1 + args.clear_decrease_percent / 100)
        if median_wall >= clear_decrease_wall:
            stop_reason = "clear_median_performance_decrease"
            break
        if median_wall < best_wall:
            selected_workers = workers
            best_wall = median_wall
        workers *= 2

    report = {
        "schema_version": "ars.evaluator-worker-sweep.v1",
        "status": "discovery",
        "started_at": started_at,
        "completed_at": datetime.now(UTC).isoformat(),
        "problem_id": "ahc044",
        "visibility": "public",
        "public_cases": 50,
        "source": str(source),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "language": args.language,
        "repeats_per_worker_count": args.repeats,
        "worker_sequence": "powers of two",
        "clear_decrease_percent": args.clear_decrease_percent,
        "stop_rule": "continue through near-ties and stop when median wall time is at least clear_decrease_percent slower than the best median seen",
        "selected_workers": selected_workers,
        "stop_reason": stop_reason,
        "cpu_model": cpu_model(),
        "logical_cpus": os.cpu_count(),
        "kernel": platform.release(),
        "python": sys.version.split()[0],
        "measurements": measurements,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"selected_workers": selected_workers, "stop_reason": stop_reason}))


if __name__ == "__main__":
    main()
