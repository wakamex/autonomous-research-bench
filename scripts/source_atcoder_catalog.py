# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "beautifulsoup4>=4.13,<5",
#   "requests>=2.32,<3",
# ]
# ///

"""Snapshot official AtCoder metadata and statements for the ARS catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from dataclasses import asdict, dataclass
from datetime import date, datetime
from pathlib import Path
from urllib.parse import urljoin

import requests
from bs4 import BeautifulSoup, Tag


BASE_URL = "https://atcoder.jp"
ARCHIVE_URL = f"{BASE_URL}/contests/archive?lang=en&category=0&keyword=&ratedType=4"
USER_AGENT = "ARS research catalog (local source snapshot; contact via repository owner)"


@dataclass(frozen=True)
class Contest:
    start_time: str
    contest_id: str
    contest_name: str
    duration: str
    rated_range: str
    contest_url: str
    task_id: str
    task_title: str
    task_url: str
    story: str
    problem_intro: str
    statement_sha256: str
    statement_file: str
    atcoder_labels: list[str]


def fetch(session: requests.Session, url: str) -> bytes:
    response = session.get(url, timeout=30)
    response.raise_for_status()
    if "text/html" not in response.headers.get("content-type", ""):
        raise RuntimeError(f"expected HTML from {url}, got {response.headers.get('content-type')}")
    if b"<title>" not in response.content or b"AtCoder" not in response.content:
        raise RuntimeError(f"response from {url} does not look like an AtCoder page")
    return response.content


def clean_text(node: Tag | None) -> str:
    if node is None:
        return ""
    return re.sub(r"\s+", " ", node.get_text(" ", strip=True)).strip()


def section_paragraphs(language_node: Tag, heading: str, limit: int) -> str:
    marker = language_node.find(
        lambda tag: isinstance(tag, Tag)
        and tag.name in {"h2", "h3", "h4"}
        and clean_text(tag).casefold() == heading.casefold()
    )
    if marker is None:
        return ""

    paragraphs: list[str] = []
    for sibling in marker.next_siblings:
        if isinstance(sibling, Tag) and sibling.name in {"h2", "h3", "h4", "hr"}:
            break
        if not isinstance(sibling, Tag):
            continue
        candidates = [sibling] if sibling.name == "p" else sibling.find_all("p")
        for paragraph in candidates:
            text = clean_text(paragraph)
            if text:
                paragraphs.append(text)
            if len(paragraphs) >= limit:
                return " ".join(paragraphs)
    return " ".join(paragraphs)


def archive_rows(session: requests.Session, cutoff: date) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    page = 1
    while True:
        url = ARCHIVE_URL if page == 1 else f"{ARCHIVE_URL}&page={page}"
        soup = BeautifulSoup(fetch(session, url), "html.parser")
        page_rows = soup.select("table tbody tr")
        if not page_rows:
            break

        for row in page_rows:
            cells = row.find_all("td")
            if len(cells) != 4:
                continue
            anchor = cells[1].find("a", href=re.compile(r"^/contests/"))
            if anchor is None:
                continue
            start_time = clean_text(cells[0])
            if datetime.strptime(start_time[:10], "%Y-%m-%d").date() > cutoff:
                continue
            rated_range = clean_text(cells[3])
            if rated_range != "All":
                continue
            contest_id = anchor["href"].rstrip("/").split("/")[-1]
            rows.append(
                {
                    "start_time": start_time,
                    "contest_id": contest_id,
                    "contest_name": clean_text(anchor),
                    "duration": clean_text(cells[2]),
                    "rated_range": rated_range,
                    "contest_url": urljoin(BASE_URL, anchor["href"]),
                }
            )
        page += 1

    return sorted(rows, key=lambda item: item["start_time"])


def task_metadata(session: requests.Session, row: dict[str, str], raw_dir: Path) -> Contest:
    tasks_url = f"{row['contest_url']}/tasks?lang=en"
    tasks_soup = BeautifulSoup(fetch(session, tasks_url), "html.parser")
    task_link = tasks_soup.select_one(
        f'table tbody tr a[href^="/contests/{row["contest_id"]}/tasks/"]'
    )
    if task_link is None:
        raise RuntimeError(f"task link not found for {row['contest_id']}")
    task_path = task_link["href"]
    task_url = f"{urljoin(BASE_URL, task_path)}?lang=en"
    statement = fetch(session, task_url)
    statement_soup = BeautifulSoup(statement, "html.parser")
    english = statement_soup.select_one("span.lang-en")
    if english is None:
        raise RuntimeError(f"English statement not found for {row['contest_id']}")

    title_node = statement_soup.find("title")
    task_title = clean_text(title_node).removesuffix(" - AtCoder")
    task_id = task_path.rstrip("/").split("/")[-1]
    output_file = raw_dir / f"{row['contest_id']}.html"
    output_file.write_bytes(statement)

    return Contest(
        **row,
        task_id=task_id,
        task_title=task_title or clean_text(task_link),
        task_url=task_url,
        story=section_paragraphs(english, "Story", 2),
        problem_intro=section_paragraphs(english, "Problem Statement", 3),
        statement_sha256=hashlib.sha256(statement).hexdigest(),
        statement_file=output_file.as_posix(),
        atcoder_labels=["Heuristic", "Rated Range: All"],
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cutoff", type=date.fromisoformat, default=date(2026, 8, 12))
    parser.add_argument("--output", type=Path, default=Path("sources/atcoder"))
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    raw_dir = args.output / "statements"
    raw_dir.mkdir(parents=True, exist_ok=True)

    session = requests.Session()
    session.headers["User-Agent"] = USER_AGENT
    rows = archive_rows(session, args.cutoff)
    contests = [task_metadata(session, row, raw_dir) for row in rows]

    inventory = {
        "source": ARCHIVE_URL,
        "retrieved": date.today().isoformat(),
        "cutoff": args.cutoff.isoformat(),
        "selection": "AtCoder archive AHC Class entries with Rated Range: All",
        "count": len(contests),
        "contests": [asdict(contest) for contest in contests],
    }
    (args.output / "catalog.json").write_text(
        json.dumps(inventory, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"sourced {len(contests)} contests under {args.output}")


if __name__ == "__main__":
    main()
