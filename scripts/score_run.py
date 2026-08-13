"""Replay an ARS event log against a human reference."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from scripts.ars_events import read_events


def validate_events(events: list[dict[str, object]]) -> None:
    if not events:
        raise ValueError("event log is empty")
    run_id = events[0].get("run_id")
    candidates: set[str] = set()
    last_elapsed = -1.0
    stopped = False
    for sequence, event in enumerate(events):
        if event.get("schema_version") != "ars.run-event.v1":
            raise ValueError(f"event {sequence} has an unsupported schema version")
        if event.get("run_id") != run_id or event.get("sequence") != sequence:
            raise ValueError(f"event {sequence} breaks run identity or sequence")
        elapsed = event.get("elapsed_seconds")
        if (
            not isinstance(elapsed, (int, float))
            or isinstance(elapsed, bool)
            or elapsed < last_elapsed
        ):
            raise ValueError(f"event {sequence} has decreasing or invalid elapsed time")
        last_elapsed = float(elapsed)
        event_type = event.get("event_type")
        payload = event.get("payload")
        if not isinstance(payload, dict):
            raise TypeError(f"event {sequence} has invalid payload")
        if sequence == 0 and event_type != "run.started":
            raise ValueError("first event must be run.started")
        if event_type == "candidate.submitted":
            candidate_id = payload.get("candidate_id")
            if not isinstance(candidate_id, str) or candidate_id in candidates:
                raise ValueError(f"event {sequence} has invalid or duplicate candidate")
            candidates.add(candidate_id)
        if (
            event_type == "evaluation.completed"
            and payload.get("candidate_id") not in candidates
        ):
            raise ValueError(f"event {sequence} evaluates an unknown candidate")
        if event_type == "run.stopped":
            if stopped:
                raise ValueError("run has multiple stop events")
            stopped = True
    if not stopped:
        raise ValueError("run has no stop event")


def score_at(user: dict[str, object], elapsed: int) -> int:
    improvements = user.get("improvements")
    if not isinstance(improvements, list):
        raise TypeError("human trajectory has invalid improvements")
    return max(
        (int(item[1]) for item in improvements if int(item[0]) <= elapsed), default=0
    )


def human_scores(reference: dict[str, object], elapsed: int) -> list[int]:
    trajectories = reference.get("trajectories")
    if not isinstance(trajectories, list):
        raise TypeError("human reference has no trajectories")
    return [
        score_at(user, elapsed)
        for user in trajectories
        if int(user["first_submission_seconds"]) <= elapsed
    ]


def percentile_with_ties(scores: list[int], agent_score: int) -> dict[str, float]:
    if not scores:
        raise ValueError("human score population is empty")
    below = sum(score < agent_score for score in scores)
    equal = sum(score == agent_score for score in scores)
    total = len(scores)
    lower = 100 * below / total
    upper = 100 * (below + equal) / total
    return {
        "lower": lower,
        "midrank": (lower + upper) / 2,
        "upper": upper,
    }


def private_candidates(events: list[dict[str, object]]) -> list[dict[str, object]]:
    submissions: dict[str, dict[str, object]] = {}
    evaluations: dict[str, dict[str, object]] = {}
    for event in events:
        payload = event["payload"]
        if event["event_type"] == "candidate.submitted":
            submissions[str(payload["candidate_id"])] = {
                "candidate_id": payload["candidate_id"],
                "submitted_seconds": float(event["elapsed_seconds"]),
            }
        elif (
            event["event_type"] == "evaluation.completed"
            and payload["visibility"] == "private-post-run"
        ):
            evaluations[str(payload["candidate_id"])] = payload
    candidates = []
    for candidate_id, submission in submissions.items():
        evaluation = evaluations.get(candidate_id)
        if evaluation is None:
            continue
        score = evaluation.get("score")
        candidates.append(
            {**submission, "score": score if isinstance(score, int) else None}
        )
    return sorted(
        candidates, key=lambda item: (item["submitted_seconds"], item["candidate_id"])
    )


def replay(
    events: list[dict[str, object]],
    reference: dict[str, object],
    horizon_seconds: int = 14400,
) -> dict[str, object]:
    validate_events(events)
    candidates = private_candidates(events)
    reference_checkpoints = reference.get("checkpoints")
    if not isinstance(reference_checkpoints, list):
        raise TypeError("human reference has no checkpoints")
    checkpoints = []
    for human_checkpoint in reference_checkpoints:
        elapsed = int(human_checkpoint["elapsed_seconds"])
        eligible = [
            candidate
            for candidate in candidates
            if candidate["submitted_seconds"] <= elapsed
            and candidate["score"] is not None
        ]
        best = max(
            eligible,
            key=lambda item: (item["score"], -item["submitted_seconds"]),
            default=None,
        )
        scores = human_scores(reference, elapsed)
        checkpoints.append(
            {
                "elapsed_seconds": elapsed,
                "agent_score": best["score"] if best else None,
                "candidate_id": best["candidate_id"] if best else None,
                "human_count": len(scores),
                "human_percentile": percentile_with_ties(scores, int(best["score"]))
                if best
                else None,
            }
        )

    final_checkpoint = next(
        checkpoint
        for checkpoint in reference_checkpoints
        if int(checkpoint["elapsed_seconds"]) == horizon_seconds
    )
    thresholds = []
    for name, threshold in final_checkpoint["score_percentiles"].items():
        reached = next(
            (
                candidate
                for candidate in candidates
                if candidate["submitted_seconds"] <= horizon_seconds
                and candidate["score"] is not None
                and int(candidate["score"]) >= int(threshold)
            ),
            None,
        )
        thresholds.append(
            {
                "target": name,
                "score": threshold,
                "status": "reached" if reached else "right-censored",
                "elapsed_seconds": reached["submitted_seconds"] if reached else None,
                "candidate_id": reached["candidate_id"] if reached else None,
                "censor_seconds": None if reached else horizon_seconds,
            }
        )
    return {
        "schema_version": "ars.run-score.v1",
        "run_id": events[0]["run_id"],
        "problem_id": reference["problem_id"],
        "score_direction": "maximize",
        "tie_method": "report strict lower bound, inclusive upper bound, and midrank",
        "checkpoints": checkpoints,
        "time_to_human_4h": thresholds,
        "submitted_candidates": len(
            {
                event["payload"].get("candidate_id")
                for event in events
                if event["event_type"] == "candidate.submitted"
            }
        ),
        "privately_scored_candidates": len(candidates),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("events", type=Path)
    parser.add_argument(
        "--reference", type=Path, default=Path("references/human/ahc044.json")
    )
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    result = replay(
        read_events(args.events),
        json.loads(args.reference.read_text(encoding="utf-8")),
    )
    content = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
    else:
        print(content, end="")


if __name__ == "__main__":
    main()
