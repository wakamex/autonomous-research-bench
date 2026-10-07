# Rated AtCoder heuristic problem catalog

This catalog covers the 72 AtCoder heuristic contests with `Rated Range: All` through August 12, 2026. It consists of the 49 contests available at the ALE-Bench cutoff plus 23 later contests.

## Labels and category provenance

AtCoder officially labels every row here as `Heuristic` and `Rated Range: All`. Its archive and task pages do not publish semantic tags such as routing, packing, or inference.

For the 40 ALE-Bench full-set entries, the category reproduces the `Genre` field in Table A1 of the ALE-Bench paper and is marked `Paper`. The remaining 32 categories are ARB classifications based on the official English statement and are marked `ARB`. They are concise research labels, not official AtCoder tags.

`Full` marks the original 40-problem ALE-Bench set. `Lite` marks its curated 10-problem subset. `Original` means the contest was among the 49 rated candidates available at the paper cutoff; `Added` means it is one of the 23 rated contests held afterward.

## Local sources

- [AtCoder source inventory](../sources/atcoder/catalog.json)
- [Source snapshot notes](../sources/atcoder/README.md)
- [ALE-Bench paper](../papers/ale-bench-2506.09050.pdf)

The source inventory records official URLs and SHA-256 hashes. `uv run scripts/source_atcoder_catalog.py` writes it and the official English statement HTML for each row under `sources/atcoder/statements`; neither is tracked because AtCoder does not license its pages for redistribution.

## Catalog

| ID | Date | Pool | Format | Official task | Category | Category source | Brief description | Full | Lite |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ahc001` | 2021-03-06 | Original | Long (200h) | [A - AtCoder Ad](https://atcoder.jp/contests/ahc001/tasks/ahc001_a?lang=en) | Packing | Paper | Place non-overlapping ad rectangles near requested points and areas. | Yes | - |
| `ahc002` | 2021-04-25 | Original | Short (4h) | [A - Walking on Tiles](https://atcoder.jp/contests/ahc002/tasks/ahc002_a?lang=en) | Routing | Paper | Find a high-value walk without revisiting any underlying floor tile. | Yes | - |
| `ahc003` | 2021-05-22 | Original | Long (200h) | [A - Shortest Path Queries](https://atcoder.jp/contests/ahc003/tasks/ahc003_a?lang=en) | Inference | Paper | Learn unknown grid-edge lengths from feedback while answering route queries. | Yes | - |
| `ahc004` | 2021-06-26 | Original | Short (6h) | [A - Alien's Genome Assembly](https://atcoder.jp/contests/ahc004/tasks/ahc004_a?lang=en) | Packing | Paper | Assemble a toroidal character grid containing as many requested strings as possible. | Yes | - |
| `ahc005` | 2021-08-07 | Original | Short (4h) | [A - Patrolling](https://atcoder.jp/contests/ahc005/tasks/ahc005_a?lang=en) | Routing | Paper | Find a short closed patrol route from which every road cell is visible. | Yes | - |
| `rcl-contest-2021-long` | 2021-09-05 | Original | Long (168h) | [A - Farm King X](https://atcoder.jp/contests/rcl-contest-2021-long/tasks/rcl_contest_2021_long_a?lang=en) | Planning | ARB | Buy and move harvest machines to collect valuable vegetables before they expire. | - | - |
| `future-contest-2022-qual` | 2021-11-03 | Original | Long (240h) | [A - Project Leader](https://atcoder.jp/contests/future-contest-2022-qual/tasks/future_contest_2022_qual_a?lang=en) | Scheduling | Paper | Assign dependent tasks while learning initially unknown worker skills. | Yes | - |
| `ahc006` | 2021-11-14 | Original | Short (4h) | [A - Food Delivery](https://atcoder.jp/contests/ahc006/tasks/ahc006_a?lang=en) | Routing | Paper | Select 50 food orders and route all pickups and deliveries from the depot. | Yes | - |
| `ahc007` | 2021-12-12 | Original | Short (4h) | [A - Online MST](https://atcoder.jp/contests/ahc007/tasks/ahc007_a?lang=en) | Network | Paper | Build a low-cost spanning network as uncertain edge lengths are revealed online. | Yes | - |
| `ahc008` | 2022-02-12 | Original | Long (343h) | [A - Territory](https://atcoder.jp/contests/ahc008/tasks/ahc008_a?lang=en) | Game | Paper | Coordinate people placing barriers over turns to separate moving pets. | Yes | Yes |
| `ahc009` | 2022-03-26 | Original | Short (4h) | [A - Robust Memory of Commuting Routes](https://atcoder.jp/contests/ahc009/tasks/ahc009_a?lang=en) | Routing | Paper | Encode a route that reaches its target despite randomly skipped commands. | Yes | - |
| `ahc010` | 2022-04-24 | Original | Short (4h) | [A - Loop Lines](https://atcoder.jp/contests/ahc010/tasks/ahc010_a?lang=en) | Network | Paper | Rotate rail tiles to form two large disjoint loops. | Yes | - |
| `ahc011` | 2022-05-28 | Original | Long (199h) | [A - Sliding Tree Puzzle](https://atcoder.jp/contests/ahc011/tasks/ahc011_a?lang=en) | Puzzle | Paper | Slide tiles to create a large connected tree of matching line segments. | Yes | Yes |
| `ahc012` | 2022-07-03 | Original | Short (4h) | [A - AtCoder 10th Anniversary](https://atcoder.jp/contests/ahc012/tasks/ahc012_a?lang=en) | Partitioning | Paper | Cut a strawberry-covered cake into pieces matching requested strawberry counts. | Yes | - |
| `ahc013` | 2022-08-09 | Original | Long (168h) | [A - Server Room](https://atcoder.jp/contests/ahc013/tasks/ahc013_a?lang=en) | Network | ARB | Move computers and cable same-type machines into valuable connected clusters. | - | - |
| `ahc014` | 2022-09-17 | Original | Long (340h) | [A - RectJoin](https://atcoder.jp/contests/ahc014/tasks/ahc014_a?lang=en) | Puzzle | Paper | Add weighted grid points by repeatedly drawing valid empty rectangles. | Yes | - |
| `ahc015` | 2022-10-30 | Original | Short (4h) | [A - Halloween Candy](https://atcoder.jp/contests/ahc015/tasks/ahc015_a?lang=en) | Game | Paper | Tilt a box after random candy placements to cluster equal flavors. | Yes | Yes |
| `ahc016` | 2022-11-11 | Original | Long (216h) | [A - Graphorean](https://atcoder.jp/contests/ahc016/tasks/ahc016_a?lang=en) | Inference | Paper | Design and identify graphs after their edges are perturbed by noise. | Yes | Yes |
| `ahc017` | 2023-01-28 | Original | Long (200h) | [A - Road Repair](https://atcoder.jp/contests/ahc017/tasks/ahc017_a?lang=en) | Network | Paper | Schedule road closures across days while limiting network disruption. | Yes | - |
| `ahc018` | 2023-02-18 | Original | Long (199h) | [A - Excavation](https://atcoder.jp/contests/ahc018/tasks/ahc018_a?lang=en) | Network | ARB | Probe unknown bedrock and excavate low-cost channels from water sources to houses. | - | - |
| `ahc019` | 2023-03-18 | Original | Long (367h) | [A - Silhouette Block Puzzle Creation](https://atcoder.jp/contests/ahc019/tasks/ahc019_a?lang=en) | Puzzle | Paper | Construct matching three-dimensional polycubes from two silhouettes. | Yes | - |
| `ahc020` | 2023-06-11 | Original | Short (4h) | [A - Broadcasting](https://atcoder.jp/contests/ahc020/tasks/ahc020_a?lang=en) | Covering | Paper | Choose broadcast powers and a connecting network that reaches every resident. | Yes | - |
| `ahc021` | 2023-06-25 | Original | Short (4h) | [A - Pyramid Sorting](https://atcoder.jp/contests/ahc021/tasks/ahc021_a?lang=en) | Planning | Paper | Reorder numbered balls in a triangular stack using constrained swaps. | Yes | - |
| `ahc022` | 2023-08-11 | Original | Long (223h) | [A - Exploring Another Space](https://atcoder.jp/contests/ahc022/tasks/ahc022_a?lang=en) | Inference | ARB | Design temperatures and noisy measurements to match wormholes to exits. | - | - |
| `toyota2023summer-final` | 2023-08-27 | Original | Short (3.5h) | [A - Transit Warehouse](https://atcoder.jp/contests/toyota2023summer-final/tasks/toyota2023summer_final_a?lang=en) | Game | Paper | Store arriving containers online, then retrieve them in numerical order. | Yes | - |
| `ahc023` | 2023-09-03 | Original | Long (178h) | [A - Crops on Grid](https://atcoder.jp/contests/ahc023/tasks/asprocon10_a?lang=en) | Scheduling | ARB | Schedule crop planting and harvesting while preserving access from the field entrance. | - | - |
| `ahc024` | 2023-09-24 | Original | Short (4h) | [A - Topological Map](https://atcoder.jp/contests/ahc024/tasks/ahc024_a?lang=en) | Network | Paper | Compress a colored map while preserving every region and adjacency relation. | Yes | Yes |
| `ahc025` | 2023-10-14 | Original | Long (199h) | [A - Balancing by Balance](https://atcoder.jp/contests/ahc025/tasks/ahc025_a?lang=en) | Partitioning | Paper | Use balance-scale queries to divide unknown weights into balanced groups. | Yes | Yes |
| `ahc026` | 2023-11-05 | Original | Short (4h) | [A - Stack of Boxes](https://atcoder.jp/contests/ahc026/tasks/ahc026_a?lang=en) | Planning | Paper | Restack boxes so numbered boxes can be removed in order with little movement. | Yes | Yes |
| `ahc027` | 2023-12-01 | Original | Long (216h) | [A - Recurring Cleaning Route](https://atcoder.jp/contests/ahc027/tasks/ahc027_a?lang=en) | Routing | Paper | Create a recurring route that keeps accumulating dirt low across all cells. | Yes | Yes |
| `ahc029` | 2023-12-22 | Original | Long (105h) | [A - Business Simulation Game](https://atcoder.jp/contests/ahc029/tasks/ahc029_a?lang=en) | Game | ARB | Play work, cancellation, and investment cards to maximize company money. | - | - |
| `ahc028` | 2024-01-13 | Original | Short (4h) | [A - Lucky Words](https://atcoder.jp/contests/ahc028/tasks/ahc028_a?lang=en) | Routing | Paper | Trace a short path through a letter grid that spells all requested words. | Yes | - |
| `ahc030` | 2024-02-09 | Original | Long (240h) | [A - Polyomino Mining](https://atcoder.jp/contests/ahc030/tasks/ahc030_a?lang=en) | Inference | Paper | Locate hidden polyomino oil fields using noisy aggregate queries. | Yes | - |
| `ahc031` | 2024-03-22 | Original | Long (240h) | [A - Event Hall](https://atcoder.jp/contests/ahc031/tasks/ahc031_a?lang=en) | Partitioning | Paper | Partition an event hall into changing daily rectangular room layouts. | Yes | - |
| `ahc032` | 2024-04-07 | Original | Short (4h) | [A - Mod Stamp](https://atcoder.jp/contests/ahc032/tasks/ahc032_a?lang=en) | Planning | Paper | Apply a limited sequence of modular stamps to match a target matrix. | Yes | - |
| `ahc033` | 2024-05-17 | Original | Long (240h) | [A - Container Handling with Cranes](https://atcoder.jp/contests/ahc033/tasks/ahc033_a?lang=en) | Planning | Paper | Coordinate cranes to move containers from incoming rows to exits in order. | Yes | - |
| `ahc034` | 2024-06-16 | Original | Short (4h) | [A - Leveling with a Dump Truck](https://atcoder.jp/contests/ahc034/tasks/ahc034_a?lang=en) | Planning | Paper | Route a dump truck while moving soil to level a grid. | Yes | - |
| `ahc035` | 2024-07-21 | Original | Short (4h) | [A - Breed Improvement](https://atcoder.jp/contests/ahc035/tasks/ahc035_a?lang=en) | Game | Paper | Arrange parent seeds over generations to breed a high-value descendant. | Yes | - |
| `ahc036` | 2024-08-23 | Original | Long (240h) | [A - Efficient Signal Control](https://atcoder.jp/contests/ahc036/tasks/ahc036_a?lang=en) | Routing | ARB | Encode signal-array updates that permit a short tour through required cities. | - | - |
| `ahc037` | 2024-09-15 | Original | Short (4h) | [A - Soda](https://atcoder.jp/contests/ahc037/tasks/ahc037_a?lang=en) | Network | ARB | Build desired drink compositions through a low-cost network of additive mixtures. | - | - |
| `ahc038` | 2024-10-04 | Original | Long (240h) | [A - Tree Robot Arm](https://atcoder.jp/contests/ahc038/tasks/ahc038_a?lang=en) | Planning | Paper | Design and operate a tree-shaped robot arm to relocate balls to targets. | Yes | - |
| `ahc039` | 2024-11-10 | Original | Short (4h) | [A - Purse Seine Fishing](https://atcoder.jp/contests/ahc039/tasks/ahc039_a?lang=en) | Partitioning | Paper | Draw a polygon enclosing valuable fish while excluding unwanted fish. | Yes | Yes |
| `ahc040` | 2024-11-29 | Original | Long (240h) | [A - Packing Uncertain Rectangles](https://atcoder.jp/contests/ahc040/tasks/ahc040_a?lang=en) | Packing | Paper | Pack rectangles whose dimensions are uncertain and learned through noisy queries. | Yes | - |
| `ahc041` | 2025-01-19 | Original | Short (4h) | [A - Christmas Tree Cutting](https://atcoder.jp/contests/ahc041/tasks/ahc041_a?lang=en) | Partitioning | Paper | Select a bounded-height forest that places valuable vertices deep in rooted trees. | Yes | - |
| `ahc042` | 2025-02-02 | Original | Short (4h) | [A - Oni wa Soto, Fuku wa Uchi](https://atcoder.jp/contests/ahc042/tasks/ahc042_a?lang=en) | Planning | Paper | Shift rows and columns to eject every demon without ejecting fortune pieces. | Yes | - |
| `ahc043` | 2025-02-14 | Original | Long (240h) | [A - Railway Company](https://atcoder.jp/contests/ahc043/tasks/ahc043_a?lang=en) | Network | ARB | Build a profitable rail and station network over a sequence of turns. | - | - |
| `ahc044` | 2025-03-16 | Original | Short (4h) | [A - Cleaning Up](https://atcoder.jp/contests/ahc044/tasks/ahc044_a?lang=en) | Scheduling | Paper | Define a memorable recurring cleaning assignment matching employee target frequencies. | Yes | - |
| `ahc045` | 2025-03-28 | Original | Long (240h) | [A - Oracle-Guided Road Network Planning](https://atcoder.jp/contests/ahc045/tasks/ahc045_a?lang=en) | Network | Paper | Plan a low-cost road network using noisy oracle estimates of settlement connectivity. | Yes | - |
| `ahc046` | 2025-04-26 | Original | Short (4h) | [A - Skating with Blocks](https://atcoder.jp/contests/ahc046/tasks/ahc046_a?lang=en) | Planning | Paper | Place and remove blocks to skate through ordered targets efficiently. | Yes | Yes |
| `ahc047` | 2025-05-18 | Added | Short (4h) | [A - Lovely Language Model](https://atcoder.jp/contests/ahc047/tasks/ahc047_a?lang=en) | Planning | ARB | Design a finite-state probabilistic generator that favors specified strings. | - | - |
| `ahc048` | 2025-05-30 | Added | Long (240h) | [A - Mixing on the Palette](https://atcoder.jp/contests/ahc048/tasks/ahc048_a?lang=en) | Planning | ARB | Partition a palette and sequence paint mixtures to reproduce requested colors with little waste. | - | - |
| `ahc049` | 2025-06-21 | Added | Short (4h) | [A - Durability-Constrained Transport](https://atcoder.jp/contests/ahc049/tasks/ahc049_a?lang=en) | Routing | ARB | Carry all boxes to the exit while respecting stack weight and durability. | - | - |
| `ahc050` | 2025-07-06 | Added | Short (4h) | [A - Gamble on Ice](https://atcoder.jp/contests/ahc050/tasks/ahc050_a?lang=en) | Game | ARB | Place rocks and choose a stochastic skating plan to maximize expected prize money. | - | - |
| `ahc051` | 2025-08-01 | Added | Long (240h) | [A - Probabilistic Waste Sorting](https://atcoder.jp/contests/ahc051/tasks/ahc051_a?lang=en) | Network | ARB | Lay out processors and probabilistic sorters as an accurate waste-routing network. | - | - |
| `ahc052` | 2025-08-23 | Added | Short (4h) | [A - Single Controller Multiple Robots](https://atcoder.jp/contests/ahc052/tasks/ahc052_a?lang=en) | Covering | ARB | Configure one shared controller so multiple robots collectively wax every cell. | - | - |
| `ahc053` | 2025-09-13 | Added | Short (4h) | [A - Random Sum Game](https://atcoder.jp/contests/ahc053/tasks/ahc053_a?lang=en) | Partitioning | ARB | Assign random generated values to piles whose sums should meet target values. | - | - |
| `ahc054` | 2025-09-19 | Added | Long (240h) | [A - Treant's Forest](https://atcoder.jp/contests/ahc054/tasks/ahc054_a?lang=en) | Game | ARB | Adapt forest obstacles to delay an adventurer while still allowing the flower to be reached. | - | - |
| `ahc055` | 2025-10-19 | Added | Short (4h) | [A - Weakpoint](https://atcoder.jp/contests/ahc055/tasks/ahc055_a?lang=en) | Planning | ARB | Order attacks and limited-durability weapons to open every treasure box quickly. | - | - |
| `ahc056` | 2025-11-07 | Added | Long (240h) | [A - Grid Turing Robot](https://atcoder.jp/contests/ahc056/tasks/ahc056_a?lang=en) | Planning | ARB | Synthesize a small finite-state, color-changing controller for a grid robot. | - | - |
| `ahc057` | 2025-11-29 | Added | Short (4h) | [A - Molecules](https://atcoder.jp/contests/ahc057/tasks/ahc057_a?lang=en) | Network | ARB | Bond moving points into required components while minimizing connection energy. | - | - |
| `ahc058` | 2025-12-14 | Added | Short (4h) | [A - Apple Incremental Game](https://atcoder.jp/contests/ahc058/tasks/ahc058_a?lang=en) | Planning | ARB | Invest in a hierarchy of production machines to maximize final apple output. | - | - |
| `ahc059` | 2026-01-10 | Added | Short (4h) | [A - Stack to Match Pairs](https://atcoder.jp/contests/ahc059/tasks/ahc059_a?lang=en) | Routing | ARB | Traverse a card grid with a stack and efficiently remove matching pairs. | - | - |
| `ahc060` | 2026-02-01 | Added | Short (4h) | [A - Ice Cream Collection](https://atcoder.jp/contests/ahc060/tasks/ahc060_a?lang=en) | Routing | ARB | Route through a graph to collect flavor stacks and deliver diverse ice creams. | - | - |
| `ahc061` | 2026-02-13 | Added | Long (240h) | [A - Multi-Player Territory Game](https://atcoder.jp/contests/ahc061/tasks/ahc061_a?lang=en) | Game | ARB | Compete against fixed AI players in a simultaneous territory-capturing game. | - | - |
| `ahc062` | 2026-03-14 | Added | Short (4h) | [A - King's Tour](https://atcoder.jp/contests/ahc062/tasks/ahc062_a?lang=en) | Routing | ARB | Construct a weighted Hamiltonian tour of all grid cells. | - | - |
| `ahc063` | 2026-04-03 | Added | Long (240h) | [A - Colorful Ouroboros](https://atcoder.jp/contests/ahc063/tasks/ahc063_a?lang=en) | Routing | ARB | Control a growing snake to collect food and approximate a target color sequence. | - | - |
| `ahc064` | 2026-04-26 | Added | Short (4h) | [A - Non-Crossing Railcar Rearrangement](https://atcoder.jp/contests/ahc064/tasks/ahc064_a?lang=en) | Planning | ARB | Rearrange railcars through capacity-limited sidings without crossing moves. | - | - |
| `ahc065` | 2026-05-16 | Added | Short (4h) | [A - Conveyor Design](https://atcoder.jp/contests/ahc065/tasks/ahc065_a?lang=en) | Planning | ARB | Design loop conveyors that send warehouse boxes to the exit in sorted order. | - | - |
| `ahc066` | 2026-05-29 | Added | Long (239h) | [A - Macro Controller](https://atcoder.jp/contests/ahc066/tasks/ahc066_a?lang=en) | Planning | ARB | Create reusable robot macros that deliver scattered balls to matching baskets. | - | - |
| `ahc067` | 2026-06-21 | Added | Short (4h) | [A - Castle Renovation with Linked Doors](https://atcoder.jp/contests/ahc067/tasks/ahc067_a?lang=en) | Network | ARB | Install linked doors and switches to delay the shortest route through a castle. | - | - |
| `ahc068` | 2026-07-18 | Added | Short (4h) | [A - Rectangle Swap](https://atcoder.jp/contests/ahc068/tasks/ahc068_a?lang=en) | Puzzle | ARB | Use legal rectangular swaps to transform a walled grid arrangement efficiently. | - | - |
| `ahc069` | 2026-07-31 | Added | Long (240h) | [A - Picnic](https://atcoder.jp/contests/ahc069/tasks/ahc069_a?lang=en) | Packing | ARB | Accept arriving picnic groups and assign compact connected regions over time. | - | - |
