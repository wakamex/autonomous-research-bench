"""Compile an ARS human best-so-far reference from AtCoder submissions."""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import json
import math
import zipfile
from collections import Counter
from collections.abc import Iterable
from itertools import pairwise
from pathlib import Path

DEFAULT_CHECKPOINTS = (600, 1800, 3600, 7200, 14400)
DEFAULT_PERCENTILES = (50, 90, 99)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def nearest_rank(values: list[int], percentile: int) -> int:
    """Return the ascending nearest-rank percentile."""
    if not values:
        raise ValueError("cannot compute a percentile of an empty population")
    if not 0 < percentile <= 100:
        raise ValueError("percentile must be in (0, 100]")
    ordered = sorted(values)
    return ordered[math.ceil(percentile / 100 * len(ordered)) - 1]


def parse_score(value: str) -> int:
    score = float(value)
    if not score.is_integer():
        raise ValueError(f"non-integral AtCoder score: {value}")
    return int(score)


def read_submissions(path: Path) -> list[dict[str, str]]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source))


def compile_reference(
    rows: Iterable[dict[str, str]],
    checkpoints: tuple[int, ...] = DEFAULT_CHECKPOINTS,
    percentiles: tuple[int, ...] = DEFAULT_PERCENTILES,
) -> dict[str, object]:
    ordered = sorted(
        rows, key=lambda row: (int(row["elapsed_seconds"]), int(row["id"]))
    )
    users: dict[str, dict[str, object]] = {}
    for row in ordered:
        user_id = row["user_id"].casefold()
        elapsed = int(row["elapsed_seconds"])
        user = users.setdefault(
            user_id,
            {
                "user_id": row["user_id"],
                "first_submission_seconds": elapsed,
                "improvements": [],
            },
        )
        if row["result"] != "AC":
            continue
        score = parse_score(row["point"])
        improvements = user["improvements"]
        if not isinstance(improvements, list):
            raise TypeError("invalid improvements state")
        previous = improvements[-1][1] if improvements else 0
        if score > previous:
            improvements.append([elapsed, score, int(row["id"])])

    checkpoint_results = []
    for checkpoint in checkpoints:
        represented = [
            user
            for user in users.values()
            if int(user["first_submission_seconds"]) <= checkpoint
        ]
        scores = []
        for user in represented:
            improvements = user["improvements"]
            if not isinstance(improvements, list):
                raise TypeError("invalid improvements state")
            best = max(
                (int(item[1]) for item in improvements if int(item[0]) <= checkpoint),
                default=0,
            )
            scores.append(best)
        checkpoint_results.append(
            {
                "elapsed_seconds": checkpoint,
                "humans": len(scores),
                "humans_with_score": sum(score > 0 for score in scores),
                "submissions": sum(
                    int(row["elapsed_seconds"]) <= checkpoint for row in ordered
                ),
                "score_percentiles": {
                    f"p{percentile}": nearest_rank(scores, percentile)
                    for percentile in percentiles
                },
            }
        )

    trajectories = sorted(
        users.values(), key=lambda user: str(user["user_id"]).casefold()
    )
    return {
        "method": {
            "population": "users with at least one submission by the elapsed-time cutoff",
            "per_human_value": "highest AC score submitted by the cutoff; zero for failed-only humans",
            "score_direction": "maximize",
            "percentile": "ascending nearest-rank: sorted_scores[ceil(p * n) - 1]",
        },
        "checkpoints": checkpoint_results,
        "trajectories": trajectories,
    }


def read_ale_standings(archive: Path, problem_id: str) -> tuple[list[int], str]:
    member = f"{problem_id}//standings_scores.csv"
    with zipfile.ZipFile(archive) as source:
        raw = source.read(member)
    rows = [
        (int(row["rank"]), int(row["score"]))
        for row in csv.DictReader(io.StringIO(raw.decode()))
    ]
    scores: list[int] = []
    for (rank, score), (next_rank, _) in pairwise(rows):
        if score > 0:
            scores.extend([score] * (next_rank - rank))
    return scores, hashlib.sha256(raw).hexdigest()


def validate_standings(
    reference: dict[str, object],
    official_scores: list[int],
    minimum_match_fraction: float = 0.99,
) -> dict[str, object]:
    trajectories = reference["trajectories"]
    if not isinstance(trajectories, list):
        raise TypeError("invalid trajectories")
    public_scores = [
        int(user["improvements"][-1][1])
        for user in trajectories
        if isinstance(user["improvements"], list) and user["improvements"]
    ]
    public = Counter(public_scores)
    official = Counter(official_scores)
    matched = sum((public & official).values())
    matched_fraction = matched / len(public_scores)
    return {
        "status": (
            "passed_with_documented_population_differences"
            if matched_fraction >= minimum_match_fraction
            else "failed"
        ),
        "method": "multiset comparison with ALE-Bench's official final standings score distribution",
        "minimum_match_fraction": minimum_match_fraction,
        "public_scored_humans": len(public_scores),
        "official_scored_positions": len(official_scores),
        "matched_public_scores": matched,
        "matched_public_fraction": matched_fraction,
        "public_only_scores": sorted((public - official).elements(), reverse=True),
        "official_only_scores": sorted((official - public).elements(), reverse=True),
        "official_percentiles": {
            f"p{percentile}": nearest_rank(official_scores, percentile)
            for percentile in DEFAULT_PERCENTILES
        },
        "note": (
            "ALE-Bench standings encode final ranked positions without user IDs, while the ARS reference "
            "includes every public submitter. Exact population equality is neither expected nor assumed."
        ),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--problem", default="ahc044")
    parser.add_argument("--submissions", type=Path)
    parser.add_argument("--ale-bench-zip", type=Path)
    parser.add_argument("--minimum-match-fraction", type=float, default=0.99)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    submissions = args.submissions or Path(
        f"sources/atcoder/submissions-4h/{args.problem}.csv.gz"
    )
    output = args.output or Path(f"references/human/{args.problem}.json")
    rows = read_submissions(submissions)
    reference = compile_reference(rows)
    reference.update(
        {
            "schema_version": "ars.human-reference.v1",
            "problem_id": args.problem,
            "source": {
                "submissions_file": str(submissions),
                "submissions_sha256": sha256(submissions),
            },
        }
    )
    if args.ale_bench_zip:
        scores, standings_hash = read_ale_standings(args.ale_bench_zip, args.problem)
        reference["source"].update(
            {
                "official_standings_url": (
                    f"https://huggingface.co/datasets/SakanaAI/ALE-Bench/resolve/main/{args.problem}.zip"
                ),
                "ale_bench_zip_sha256": sha256(args.ale_bench_zip),
                "standings_csv_sha256": standings_hash,
            }
        )
        reference["validation"] = validate_standings(
            reference, scores, args.minimum_match_fraction
        )
        if reference["validation"]["status"] == "failed":
            fraction = reference["validation"]["matched_public_fraction"]
            raise RuntimeError(
                f"official standings validation matched only {fraction:.2%}; "
                f"minimum is {args.minimum_match_fraction:.2%}"
            )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(reference, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {output} with {len(reference['trajectories'])} human trajectories")


if __name__ == "__main__":
    main()
