"""Append-only ARS run event helpers."""

from __future__ import annotations

import fcntl
import json
import os
from collections.abc import Iterator
from contextlib import contextmanager
from datetime import UTC, datetime
from pathlib import Path

SCHEMA_VERSION = "ars.run-event.v1"


def utc_now() -> str:
    return datetime.now(UTC).isoformat()


def read_events(path: Path) -> list[dict[str, object]]:
    if not path.exists():
        return []
    events = []
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        try:
            event = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(
                f"invalid JSON at {path}:{line_number}: {error}"
            ) from error
        if not isinstance(event, dict):
            raise TypeError(f"event at {path}:{line_number} is not an object")
        events.append(event)
    return events


@contextmanager
def event_lock(run_dir: Path) -> Iterator[None]:
    run_dir.mkdir(parents=True, exist_ok=True)
    lock_path = run_dir / "events.lock"
    with lock_path.open("a", encoding="utf-8") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def append_event(
    run_dir: Path,
    event_type: str,
    elapsed_seconds: float,
    payload: dict[str, object],
) -> dict[str, object]:
    events_path = run_dir / "events.jsonl"
    events = read_events(events_path)
    run_id = _run_id(run_dir, events)
    event = {
        "schema_version": SCHEMA_VERSION,
        "run_id": run_id,
        "sequence": len(events),
        "event_type": event_type,
        "elapsed_seconds": round(max(elapsed_seconds, 0.0), 6),
        "recorded_at": utc_now(),
        "payload": payload,
    }
    line = json.dumps(event, separators=(",", ":"), sort_keys=True) + "\n"
    descriptor = os.open(events_path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.write(descriptor, line.encode())
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    return event


def _run_id(run_dir: Path, events: list[dict[str, object]]) -> str:
    if events:
        run_id = events[0].get("run_id")
        if not isinstance(run_id, str):
            raise ValueError("first event has no run_id")
        return run_id
    state_path = run_dir / "state.json"
    state = json.loads(state_path.read_text(encoding="utf-8"))
    run_id = state.get("run_id")
    if not isinstance(run_id, str):
        raise TypeError("run state has no run_id")
    return run_id
