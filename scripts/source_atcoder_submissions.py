# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "requests>=2.32,<3",
# ]
# ///

"""Snapshot complete AtCoder submission metadata for each contest's first four hours."""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import json
import time
from datetime import datetime, timezone
from pathlib import Path

import requests


API = "https://kenkoooo.com/atcoder/atcoder-api/v3/from/{cursor}"
SOURCE_CATALOG = Path("sources/atcoder/catalog.json")
DEFAULT_OUTPUT = Path("sources/atcoder/submissions-4h")
FIELDS = [
    "id",
    "epoch_second",
    "elapsed_seconds",
    "contest_id",
    "problem_id",
    "user_id",
    "language",
    "point",
    "length",
    "result",
    "execution_time",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--delay", type=float, default=1.1)
    parser.add_argument("--contest", action="append", default=[])
    return parser.parse_args()


def fetch_page(session: requests.Session, cursor: int) -> list[dict[str, object]]:
    response = session.get(API.format(cursor=cursor), timeout=60)
    response.raise_for_status()
    data = response.json()
    if not isinstance(data, list):
        raise RuntimeError(f"unexpected API response at cursor {cursor}")
    return data


def source_window(
    session: requests.Session,
    contest: dict[str, object],
    delay: float,
) -> tuple[list[dict[str, object]], int, int, int]:
    contest_id = str(contest["contest_id"])
    problem_id = str(contest["task_id"])
    start = int(datetime.strptime(str(contest["start_time"]), "%Y-%m-%d %H:%M:%S%z").timestamp())
    hours, minutes = (int(part) for part in str(contest["duration"]).split(":"))
    contest_seconds = hours * 3600 + minutes * 60
    window_seconds = min(4 * 3600, contest_seconds)
    end = start + window_seconds

    cursor = start
    pages = 0
    seen: dict[int, dict[str, object]] = {}
    previous_page_ids: tuple[int, ...] | None = None
    while cursor <= end:
        page = fetch_page(session, cursor)
        pages += 1
        if not page:
            break

        page_ids = tuple(int(row["id"]) for row in page)
        for row in page:
            epoch = int(row["epoch_second"])
            if epoch > end:
                continue
            if row["contest_id"] == contest_id and row["problem_id"] == problem_id:
                item = {field: row.get(field) for field in FIELDS if field != "elapsed_seconds"}
                item["elapsed_seconds"] = epoch - start
                seen[int(row["id"])] = item

        last_epoch = int(page[-1]["epoch_second"])
        if last_epoch > end:
            break
        if page_ids == previous_page_ids:
            cursor = last_epoch + 1
        else:
            cursor = last_epoch
        previous_page_ids = page_ids
        time.sleep(delay)

    rows = sorted(seen.values(), key=lambda row: (int(row["epoch_second"]), int(row["id"])))
    return rows, start, end, pages


def write_csv(path: Path, rows: list[dict[str, object]]) -> str:
    with path.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with io.TextIOWrapper(compressed, encoding="utf-8", newline="") as output:
                writer = csv.DictWriter(output, fieldnames=FIELDS)
                writer.writeheader()
                writer.writerows(rows)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    args = parse_args()
    catalog = json.loads(SOURCE_CATALOG.read_text(encoding="utf-8"))
    contests = catalog["contests"]
    selected = set(args.contest)
    if selected:
        contests = [contest for contest in contests if contest["contest_id"] in selected]
        missing = selected - {contest["contest_id"] for contest in contests}
        if missing:
            raise RuntimeError(f"unknown contests: {sorted(missing)}")

    args.output.mkdir(parents=True, exist_ok=True)
    session = requests.Session()
    session.headers["User-Agent"] = "ARB benchmark research submission metadata snapshot"
    records = []
    for index, contest in enumerate(contests, 1):
        contest_id = contest["contest_id"]
        rows, start, end, pages = source_window(session, contest, args.delay)
        path = args.output / f"{contest_id}.csv.gz"
        checksum = write_csv(path, rows)
        records.append(
            {
                "contest_id": contest_id,
                "problem_id": contest["task_id"],
                "start_epoch_second": start,
                "end_epoch_second": end,
                "window_seconds": end - start,
                "submissions": len(rows),
                "users": len({row["user_id"].casefold() for row in rows}),
                "pages_requested": pages,
                "file": path.name,
                "sha256": checksum,
            }
        )
        print(f"[{index}/{len(contests)}] {contest_id}: {len(rows)} submissions, {pages} pages", flush=True)
        time.sleep(args.delay)

    manifest = {
        "source": "AtCoder Problems time-based Submission API",
        "source_url": "https://kenkoooo.com/atcoder/atcoder-api/v3/from/{unix_time_second}",
        "upstream": "AtCoder public submission metadata",
        "retrieved": datetime.now(timezone.utc).isoformat(),
        "window": "Contest start through four elapsed hours, or contest end when earlier",
        "fields": FIELDS,
        "includes_code": False,
        "contest_count": len(records),
        "submission_count": sum(record["submissions"] for record in records),
        "contests": records,
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {manifest['submission_count']} submissions across {len(records)} contests")


if __name__ == "__main__":
    main()
