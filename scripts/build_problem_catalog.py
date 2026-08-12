"""Build the human-readable ARS problem catalog from sourced AtCoder metadata."""

from __future__ import annotations

import json
from pathlib import Path


SOURCE = Path("sources/atcoder/catalog.json")
OUTPUT = Path("docs/atcoder-heuristic-catalog.md")
PAPER_CUTOFF = "2025-05-01"

FULL = {
    "ahc001", "ahc002", "ahc003", "ahc004", "ahc005", "future-contest-2022-qual",
    "ahc006", "ahc007", "ahc008", "ahc009", "ahc010", "ahc011", "ahc012", "ahc014",
    "ahc015", "ahc016", "ahc017", "ahc019", "ahc020", "ahc021",
    "toyota2023summer-final", "ahc024", "ahc025", "ahc026", "ahc027", "ahc028",
    "ahc030", "ahc031", "ahc032", "ahc033", "ahc034", "ahc035", "ahc038", "ahc039",
    "ahc040", "ahc041", "ahc042", "ahc044", "ahc045", "ahc046",
}

LITE = {
    "ahc008", "ahc011", "ahc015", "ahc016", "ahc024",
    "ahc025", "ahc026", "ahc027", "ahc039", "ahc046",
}

# Categories for FULL entries reproduce Table A1 of the ALE-Bench paper. Categories for every other
# entry are ARS classifications based on the locally snapshotted official English statement.
ANNOTATIONS = {
    "ahc001": ("Packing", "Place non-overlapping ad rectangles near requested points and areas."),
    "ahc002": ("Routing", "Find a high-value walk without revisiting any underlying floor tile."),
    "ahc003": ("Inference", "Learn unknown grid-edge lengths from feedback while answering route queries."),
    "ahc004": ("Packing", "Assemble a toroidal character grid containing as many requested strings as possible."),
    "ahc005": ("Routing", "Find a short closed patrol route from which every road cell is visible."),
    "rcl-contest-2021-long": ("Planning", "Buy and move harvest machines to collect valuable vegetables before they expire."),
    "future-contest-2022-qual": ("Scheduling", "Assign dependent tasks while learning initially unknown worker skills."),
    "ahc006": ("Routing", "Select 50 food orders and route all pickups and deliveries from the depot."),
    "ahc007": ("Network", "Build a low-cost spanning network as uncertain edge lengths are revealed online."),
    "ahc008": ("Game", "Coordinate people placing barriers over turns to separate moving pets."),
    "ahc009": ("Routing", "Encode a route that reaches its target despite randomly skipped commands."),
    "ahc010": ("Network", "Rotate rail tiles to form two large disjoint loops."),
    "ahc011": ("Puzzle", "Slide tiles to create a large connected tree of matching line segments."),
    "ahc012": ("Partitioning", "Cut a strawberry-covered cake into pieces matching requested strawberry counts."),
    "ahc013": ("Network", "Move computers and cable same-type machines into valuable connected clusters."),
    "ahc014": ("Puzzle", "Add weighted grid points by repeatedly drawing valid empty rectangles."),
    "ahc015": ("Game", "Tilt a box after random candy placements to cluster equal flavors."),
    "ahc016": ("Inference", "Design and identify graphs after their edges are perturbed by noise."),
    "ahc017": ("Network", "Schedule road closures across days while limiting network disruption."),
    "ahc018": ("Network", "Probe unknown bedrock and excavate low-cost channels from water sources to houses."),
    "ahc019": ("Puzzle", "Construct matching three-dimensional polycubes from two silhouettes."),
    "ahc020": ("Covering", "Choose broadcast powers and a connecting network that reaches every resident."),
    "ahc021": ("Planning", "Reorder numbered balls in a triangular stack using constrained swaps."),
    "ahc022": ("Inference", "Design temperatures and noisy measurements to match wormholes to exits."),
    "toyota2023summer-final": ("Game", "Store arriving containers online, then retrieve them in numerical order."),
    "ahc023": ("Scheduling", "Schedule crop planting and harvesting while preserving access from the field entrance."),
    "ahc024": ("Network", "Compress a colored map while preserving every region and adjacency relation."),
    "ahc025": ("Partitioning", "Use balance-scale queries to divide unknown weights into balanced groups."),
    "ahc026": ("Planning", "Restack boxes so numbered boxes can be removed in order with little movement."),
    "ahc027": ("Routing", "Create a recurring route that keeps accumulating dirt low across all cells."),
    "ahc029": ("Game", "Play work, cancellation, and investment cards to maximize company money."),
    "ahc028": ("Routing", "Trace a short path through a letter grid that spells all requested words."),
    "ahc030": ("Inference", "Locate hidden polyomino oil fields using noisy aggregate queries."),
    "ahc031": ("Partitioning", "Partition an event hall into changing daily rectangular room layouts."),
    "ahc032": ("Planning", "Apply a limited sequence of modular stamps to match a target matrix."),
    "ahc033": ("Planning", "Coordinate cranes to move containers from incoming rows to exits in order."),
    "ahc034": ("Planning", "Route a dump truck while moving soil to level a grid."),
    "ahc035": ("Game", "Arrange parent seeds over generations to breed a high-value descendant."),
    "ahc036": ("Routing", "Encode signal-array updates that permit a short tour through required cities."),
    "ahc037": ("Network", "Build desired drink compositions through a low-cost network of additive mixtures."),
    "ahc038": ("Planning", "Design and operate a tree-shaped robot arm to relocate balls to targets."),
    "ahc039": ("Partitioning", "Draw a polygon enclosing valuable fish while excluding unwanted fish."),
    "ahc040": ("Packing", "Pack rectangles whose dimensions are uncertain and learned through noisy queries."),
    "ahc041": ("Partitioning", "Select a bounded-height forest that places valuable vertices deep in rooted trees."),
    "ahc042": ("Planning", "Shift rows and columns to eject every demon without ejecting fortune pieces."),
    "ahc043": ("Network", "Build a profitable rail and station network over a sequence of turns."),
    "ahc044": ("Scheduling", "Define a memorable recurring cleaning assignment matching employee target frequencies."),
    "ahc045": ("Network", "Plan a low-cost road network using noisy oracle estimates of settlement connectivity."),
    "ahc046": ("Planning", "Place and remove blocks to skate through ordered targets efficiently."),
    "ahc047": ("Planning", "Design a finite-state probabilistic generator that favors specified strings."),
    "ahc048": ("Planning", "Partition a palette and sequence paint mixtures to reproduce requested colors with little waste."),
    "ahc049": ("Routing", "Carry all boxes to the exit while respecting stack weight and durability."),
    "ahc050": ("Game", "Place rocks and choose a stochastic skating plan to maximize expected prize money."),
    "ahc051": ("Network", "Lay out processors and probabilistic sorters as an accurate waste-routing network."),
    "ahc052": ("Covering", "Configure one shared controller so multiple robots collectively wax every cell."),
    "ahc053": ("Partitioning", "Assign random generated values to piles whose sums should meet target values."),
    "ahc054": ("Game", "Adapt forest obstacles to delay an adventurer while still allowing the flower to be reached."),
    "ahc055": ("Planning", "Order attacks and limited-durability weapons to open every treasure box quickly."),
    "ahc056": ("Planning", "Synthesize a small finite-state, color-changing controller for a grid robot."),
    "ahc057": ("Network", "Bond moving points into required components while minimizing connection energy."),
    "ahc058": ("Planning", "Invest in a hierarchy of production machines to maximize final apple output."),
    "ahc059": ("Routing", "Traverse a card grid with a stack and efficiently remove matching pairs."),
    "ahc060": ("Routing", "Route through a graph to collect flavor stacks and deliver diverse ice creams."),
    "ahc061": ("Game", "Compete against fixed AI players in a simultaneous territory-capturing game."),
    "ahc062": ("Routing", "Construct a weighted Hamiltonian tour of all grid cells."),
    "ahc063": ("Routing", "Control a growing snake to collect food and approximate a target color sequence."),
    "ahc064": ("Planning", "Rearrange railcars through capacity-limited sidings without crossing moves."),
    "ahc065": ("Planning", "Design loop conveyors that send warehouse boxes to the exit in sorted order."),
    "ahc066": ("Planning", "Create reusable robot macros that deliver scattered balls to matching baskets."),
    "ahc067": ("Network", "Install linked doors and switches to delay the shortest route through a castle."),
    "ahc068": ("Puzzle", "Use legal rectangular swaps to transform a walled grid arrangement efficiently."),
    "ahc069": ("Packing", "Accept arriving picnic groups and assign compact connected regions over time."),
}


def duration_label(duration: str) -> str:
    hours, minutes = (int(part) for part in duration.split(":"))
    value = f"{hours:g}h" if minutes == 0 else f"{hours + minutes / 60:g}h"
    return f"Short ({value})" if hours <= 6 else f"Long ({value})"


def main() -> None:
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    contests = source["contests"]
    ids = {contest["contest_id"] for contest in contests}
    if ids != ANNOTATIONS.keys():
        missing = sorted(ids - ANNOTATIONS.keys())
        extra = sorted(ANNOTATIONS.keys() - ids)
        raise RuntimeError(f"annotation mismatch: missing={missing}, extra={extra}")
    if len(contests) != 72 or len(FULL) != 40 or len(LITE) != 10 or not LITE <= FULL:
        raise RuntimeError("catalog or ALE subset counts are inconsistent")

    original_count = sum(contest["start_time"][:10] < PAPER_CUTOFF for contest in contests)
    if original_count != 49:
        raise RuntimeError(f"expected 49 original candidates, found {original_count}")

    lines = [
        "# Rated AtCoder heuristic problem catalog",
        "",
        "This catalog covers the 72 AtCoder heuristic contests with `Rated Range: All` through August 12, 2026. It consists of the 49 contests available at the ALE-Bench cutoff plus 23 later contests.",
        "",
        "## Labels and category provenance",
        "",
        "AtCoder officially labels every row here as `Heuristic` and `Rated Range: All`. Its archive and task pages do not publish semantic tags such as routing, packing, or inference.",
        "",
        "For the 40 ALE-Bench full-set entries, the category reproduces the `Genre` field in Table A1 of the ALE-Bench paper and is marked `Paper`. The remaining 32 categories are ARS classifications based on the official English statement and are marked `ARS`. They are concise research labels, not official AtCoder tags.",
        "",
        "`Full` marks the original 40-problem ALE-Bench set. `Lite` marks its curated 10-problem subset. `Original` means the contest was among the 49 rated candidates available at the paper cutoff; `Added` means it is one of the 23 rated contests held afterward.",
        "",
        "## Local sources",
        "",
        "- [AtCoder source inventory](../sources/atcoder/catalog.json)",
        "- [Source snapshot notes](../sources/atcoder/README.md)",
        "- [ALE-Bench paper](../papers/ale-bench-2506.09050.pdf)",
        "",
        "The source inventory records official URLs and SHA-256 hashes. The complete official English statement HTML for each row is stored under `sources/atcoder/statements`.",
        "",
        "## Catalog",
        "",
        "| ID | Date | Pool | Format | Official task | Category | Category source | Brief description | Full | Lite |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]

    for contest in contests:
        contest_id = contest["contest_id"]
        category, description = ANNOTATIONS[contest_id]
        pool = "Original" if contest["start_time"][:10] < PAPER_CUTOFF else "Added"
        category_source = "Paper" if contest_id in FULL else "ARS"
        task = f"[{contest['task_title']}]({contest['task_url']})"
        lines.append(
            "| "
            + " | ".join(
                [
                    f"`{contest_id}`",
                    contest["start_time"][:10],
                    pool,
                    duration_label(contest["duration"]),
                    task,
                    category,
                    category_source,
                    description,
                    "Yes" if contest_id in FULL else "-",
                    "Yes" if contest_id in LITE else "-",
                ]
            )
            + " |"
        )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(contests)} rows to {OUTPUT}")


if __name__ == "__main__":
    main()
