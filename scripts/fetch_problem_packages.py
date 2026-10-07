# /// script
# requires-python = ">=3.11"
# ///

"""Assemble problem packages from their recorded official archives and ARB adapters."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import urllib.request
import zipfile
from pathlib import Path


PROBLEMS = Path("problems")
CACHE = Path(".arb-cache/archives")
# ALE-Bench archives name the historical reference tables differently from ARB packages.
REFERENCE_MEMBERS = {"standings": "standings_scores.csv", "performance": "performance.csv"}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def archive_bytes(url: str, expected: str) -> bytes:
    cached = CACHE / f"{expected}.zip"
    if cached.is_file():
        data = cached.read_bytes()
        if sha256(data) == expected:
            return data
    request = urllib.request.Request(url, headers={"User-Agent": "ARB problem package fetch"})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()
    actual = sha256(data)
    if actual != expected:
        raise RuntimeError(f"{url}: SHA-256 {actual} does not match recorded {expected}")
    CACHE.mkdir(parents=True, exist_ok=True)
    cached.write_bytes(data)
    return data


def tools_prefix(names: list[str]) -> str:
    for name in names:
        if name.endswith("tools/Cargo.toml"):
            return name.removesuffix("tools/Cargo.toml")
    raise RuntimeError("archive has no tools/Cargo.toml")


def archive_member(problem: dict, relative: str, prefix: str) -> str:
    for key, member in REFERENCE_MEMBERS.items():
        if relative == (problem.get("reference") or {}).get(key):
            return prefix + member
    return prefix + relative


def fetch(package: Path) -> int:
    problem = json.loads((package / "problem.json").read_text(encoding="utf-8"))
    provenance = problem["provenance"]
    archive = zipfile.ZipFile(io.BytesIO(archive_bytes(provenance["source_url"], provenance["source_sha256"])))
    prefix = tools_prefix(archive.namelist())
    written = 0
    for relative in problem["package_files"]:
        adapter = package / "adapters" / relative
        data = adapter.read_bytes() if adapter.is_file() else archive.read(archive_member(problem, relative, prefix))
        target = package / relative
        if not target.is_file() or target.read_bytes() != data:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            written += 1
    return written


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ids", nargs="*", help="problem IDs to fetch; defaults to every package")
    args = parser.parse_args()
    packages = [PROBLEMS / name for name in args.ids] or sorted(p.parent for p in PROBLEMS.glob("*/problem.json"))
    for package in packages:
        print(f"{package.name}: {fetch(package)} files written")


if __name__ == "__main__":
    main()
