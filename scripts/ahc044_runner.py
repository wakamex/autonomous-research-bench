"""Minimal append-only AHC044 runner adapter over ALE-Bench."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import time
import uuid
from pathlib import Path

from scripts.ars_events import append_event, event_lock, read_events, utc_now


def atomic_json(path: Path, value: dict[str, object]) -> None:
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    os.replace(temporary, path)


def load_state(run_dir: Path) -> dict[str, object]:
    return json.loads((run_dir / "state.json").read_text(encoding="utf-8"))


def elapsed(state: dict[str, object]) -> float:
    return time.monotonic() - float(state["started_monotonic"])


def benchmark_timing(state: dict[str, object]) -> dict[str, float]:
    elapsed_seconds = max(elapsed(state), 0.0)
    remaining_seconds = max(
        float(state["duration_seconds"]) - elapsed_seconds, 0.0
    )
    return {
        "elapsed_seconds": round(elapsed_seconds, 6),
        "remaining_seconds": round(remaining_seconds, 6),
    }


def continuation_prompt(state: dict[str, object]) -> str:
    remaining_seconds = math.floor(benchmark_timing(state)["remaining_seconds"])
    return (
        "Continue working on the task. "
        f"Time remaining: {remaining_seconds:,} seconds."
    )


def print_continuation_prompt(args: argparse.Namespace) -> None:
    state = load_state(args.run_dir.resolve())
    print(continuation_prompt(state))


def init_run(args: argparse.Namespace) -> None:
    run_dir = args.run_dir.resolve()
    run_dir.mkdir(parents=True, exist_ok=False)
    (run_dir / "candidates").mkdir()
    state = {
        "run_id": args.run_id,
        "started_at": utc_now(),
        "started_monotonic": time.monotonic(),
        "duration_seconds": args.duration,
        "public_evaluator_cpu_seconds": 0.0,
        "private_evaluator_cpu_seconds": 0.0,
        "model_calls": 0,
        "input_tokens": 0,
        "output_tokens": 0,
        "cached_tokens": 0,
        "cost_usd": 0.0,
    }
    atomic_json(run_dir / "state.json", state)
    with event_lock(run_dir):
        append_event(
            run_dir,
            "run.started",
            0,
            {
                "problem_id": "ahc044",
                "suite_id": args.suite_id,
                "track": args.track,
                "system_manifest_sha256": args.system_manifest_sha256,
            },
        )
        append_event(
            run_dir,
            "model_call.started",
            0,
            {"call_id": "aop-agent", "model": args.model},
        )
    print(run_dir)


def result_payload(result: object) -> tuple[str, int | None, float]:
    judge_result = result.overall_judge_result.value
    score = int(result.overall_absolute_score) if judge_result == "ACCEPTED" else None
    cpu_seconds = sum(float(case.execution_time) for case in result.case_results)
    return judge_result, score, cpu_seconds


def evaluate(
    code: str, language: str, visibility: str, workers: int
) -> dict[str, object]:
    import ale_bench

    started = time.monotonic()
    session = ale_bench.start(
        problem_id="ahc044",
        lite_version=False,
        session_duration=86400,
        num_workers=workers,
    )
    try:
        if visibility == "public":
            result = session.public_eval(
                code, code_language=language, reuse_containers=False
            )
            rank = None
            performance = None
            score_basis = "ALE-Bench 50-case public sum"
        else:
            result, rank, performance = session.private_eval(
                code, code_language=language, reuse_containers=False
            )
            score_basis = (
                "ALE-Bench 150-case private sum comparable to official AHC044 standings"
            )
        judge_result, score, cpu_seconds = result_payload(result)
        return {
            "judge_result": judge_result,
            "score": score,
            "wall_seconds": time.monotonic() - started,
            "cpu_seconds": cpu_seconds,
            "rank": rank,
            "performance": performance,
            "score_basis": score_basis,
        }
    finally:
        session.close()


def evaluation_error(error: Exception) -> dict[str, object]:
    return {
        "judge_result": "INTERNAL_ERROR",
        "score": None,
        "wall_seconds": 0.0,
        "cpu_seconds": 0.0,
        "rank": None,
        "performance": None,
        "score_basis": f"evaluation error: {type(error).__name__}: {error}",
    }


def submit(args: argparse.Namespace) -> None:
    run_dir = args.run_dir.resolve()
    with event_lock(run_dir):
        state = load_state(run_dir)
        submitted_at = elapsed(state)
        if submitted_at > float(state["duration_seconds"]):
            raise RuntimeError("run deadline has passed")
        source = args.source.resolve()
        content = source.read_bytes()
        digest = hashlib.sha256(content).hexdigest()
        candidate_number = (
            sum(
                event["event_type"] == "candidate.submitted"
                for event in read_events(run_dir / "events.jsonl")
            )
            + 1
        )
        candidate_id = f"candidate-{candidate_number:04d}"
        extension = source.suffix or ".txt"
        archived = run_dir / "candidates" / f"{candidate_id}{extension}"
        archived.write_bytes(content)
        append_event(
            run_dir,
            "candidate.submitted",
            submitted_at,
            {
                "candidate_id": candidate_id,
                "source_sha256": digest,
                "language": args.language,
            },
        )
        try:
            evaluation = evaluate(
                content.decode(), args.language, "public", args.workers
            )
        except Exception as error:  # noqa: BLE001 - evaluator failures must become durable events
            evaluation = evaluation_error(error)
        timing = benchmark_timing(state)
        append_event(
            run_dir,
            "evaluation.completed",
            timing["elapsed_seconds"],
            {
                "evaluation_id": f"public-{candidate_id}",
                "candidate_id": candidate_id,
                "visibility": "public",
                **timing,
                **evaluation,
            },
        )
        state["public_evaluator_cpu_seconds"] = float(
            state["public_evaluator_cpu_seconds"]
        ) + float(evaluation["cpu_seconds"])
        atomic_json(run_dir / "state.json", state)
        append_resource_event(run_dir, state)
    print(
        json.dumps(
            {"candidate_id": candidate_id, **timing, **evaluation}, sort_keys=True
        )
    )


def append_resource_event(run_dir: Path, state: dict[str, object]) -> None:
    append_event(
        run_dir,
        "resource.updated",
        elapsed(state),
        {
            "model_calls": state["model_calls"],
            "input_tokens": state["input_tokens"],
            "output_tokens": state["output_tokens"],
            "cached_tokens": state["cached_tokens"],
            "cost_usd": state["cost_usd"],
            "evaluator_cpu_seconds": float(state["public_evaluator_cpu_seconds"])
            + float(state["private_evaluator_cpu_seconds"]),
        },
    )


def stop_run(args: argparse.Namespace) -> None:
    run_dir = args.run_dir.resolve()
    result_paths = (
        args.aop_result
        if isinstance(args.aop_result, list)
        else [args.aop_result]
    )
    aop_results = [
        json.loads(path.read_text(encoding="utf-8")) for path in result_paths
    ]
    final_result = aop_results[-1]

    def usage_total(field: str) -> int:
        return sum(
            int((result.get("usage") or {}).get(field, 0))
            for result in aop_results
        )

    cost_usd = sum(
        float((result.get("api_equivalent_cost") or {}).get("amount_usd", 0) or 0)
        for result in aop_results
    )
    with event_lock(run_dir):
        state = load_state(run_dir)
        state.update(
            {
                "model_calls": len(aop_results),
                "input_tokens": usage_total("input_tokens"),
                "output_tokens": usage_total("output_tokens"),
                "cached_tokens": usage_total("cached_input_tokens"),
                "cost_usd": cost_usd,
            }
        )
        atomic_json(run_dir / "state.json", state)
        timed_out = bool(final_result.get("timed_out"))
        if timed_out:
            model_status = "cancelled"
            stop_reason = "deadline"
        elif final_result.get("succeeded"):
            model_status = "completed"
            stop_reason = "completed"
        else:
            model_status = "failed"
            stop_reason = "failed"
        append_event(
            run_dir,
            "model_call.completed",
            elapsed(state),
            {
                "call_id": "aop-agent",
                "status": model_status,
                "input_tokens": state["input_tokens"],
                "output_tokens": state["output_tokens"],
                "cached_tokens": state["cached_tokens"],
                "cost_usd": state["cost_usd"],
            },
        )
        append_resource_event(run_dir, state)
        append_event(
            run_dir,
            "run.stopped",
            elapsed(state),
            {"reason": stop_reason},
        )


def private_evaluate(args: argparse.Namespace) -> None:
    run_dir = args.run_dir.resolve()
    candidates = sorted((run_dir / "candidates").iterdir())
    existing = {
        event["payload"]["candidate_id"]
        for event in read_events(run_dir / "events.jsonl")
        if event["event_type"] == "evaluation.completed"
        and event["payload"]["visibility"] == "private-post-run"
    }
    for candidate in candidates:
        candidate_id = candidate.stem
        if candidate_id in existing:
            continue
        try:
            evaluation = evaluate(
                candidate.read_text(encoding="utf-8"),
                args.language,
                "private",
                args.workers,
            )
        except Exception as error:  # noqa: BLE001 - continue through every candidate
            evaluation = evaluation_error(error)
        with event_lock(run_dir):
            state = load_state(run_dir)
            append_event(
                run_dir,
                "evaluation.completed",
                elapsed(state),
                {
                    "evaluation_id": f"private-{candidate_id}",
                    "candidate_id": candidate_id,
                    "visibility": "private-post-run",
                    **evaluation,
                },
            )
            state["private_evaluator_cpu_seconds"] = float(
                state["private_evaluator_cpu_seconds"]
            ) + float(evaluation["cpu_seconds"])
            atomic_json(run_dir / "state.json", state)
            append_resource_event(run_dir, state)
        print(json.dumps({"candidate_id": candidate_id, **evaluation}, sort_keys=True))


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    commands = root.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("--run-dir", type=Path, required=True)
    init.add_argument("--run-id", required=True)
    init.add_argument("--duration", type=float, required=True)
    init.add_argument("--suite-id", default="sprint-9-discovery")
    init.add_argument("--track", default="single-model")
    init.add_argument("--model", required=True)
    init.add_argument("--system-manifest-sha256", required=True)
    init.set_defaults(function=init_run)
    submit_command = commands.add_parser("submit")
    submit_command.add_argument("--run-dir", type=Path, required=True)
    submit_command.add_argument("--source", type=Path, required=True)
    submit_command.add_argument("--language", default="cpp23")
    submit_command.add_argument("--workers", type=int, default=1)
    submit_command.set_defaults(function=submit)
    prompt = commands.add_parser("continuation-prompt")
    prompt.add_argument("--run-dir", type=Path, required=True)
    prompt.set_defaults(function=print_continuation_prompt)
    stop = commands.add_parser("stop")
    stop.add_argument("--run-dir", type=Path, required=True)
    stop.add_argument("--aop-result", type=Path, action="append", required=True)
    stop.set_defaults(function=stop_run)
    private = commands.add_parser("private-evaluate")
    private.add_argument("--run-dir", type=Path, required=True)
    private.add_argument("--language", default="cpp23")
    private.add_argument("--workers", type=int, default=1)
    private.set_defaults(function=private_evaluate)
    return root


def main() -> None:
    args = parser().parse_args()
    args.function(args)


if __name__ == "__main__":
    main()
