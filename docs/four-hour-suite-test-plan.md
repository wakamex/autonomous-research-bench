# Four-hour suite test plan

Status: planned discovery experiment. No suite has been selected.

## Decision to make

ARS will test whether its primary speedrun benchmark should use:

1. the rule-selected Sprint-9 plus Inference suite, with a Sprint-9 ablation, or
2. the mixed short and long ARS-10 breadth proposal.

Both candidates will receive the same four-hour agent wall-clock cap per problem. We will select one only after auditing human trajectories, evaluator cost, ranking stability, and coverage. A third outcome, using Sprint as the primary leaderboard and ARS-10 as a separate promotion suite, remains explicitly allowed.

## Metric under test

The primary comparison is equal elapsed wall-clock from problem release:

```text
t = 0: the human contestant and autonomous system receive the problem
t > 0: compare their best-so-far scores at equal elapsed time
```

Each agent run stops after four hours. Historical human trajectories are truncated at the same point even when the original contest lasted several days.

Required checkpoints are:

- 10 minutes
- 30 minutes
- 1 hour
- 2 hours
- 4 hours

The full best-so-far score curve remains the canonical result. Checkpoints are summaries, not substitutes for the curve.

The provisional time-to-threshold metrics use the human score distribution at
four elapsed hours. For each problem, compute the P50, P90, and P99 score from
one best submission per human at four hours. Then report when the agent first
submitted a candidate whose comparable benchmark score reached each fixed
threshold:

```text
Time to human 4h P50
Time to human 4h P90
Time to human 4h P99
```

A threshold not reached within four hours is right-censored, not assigned a
fabricated completion time. Same-time human percentiles at the required
checkpoints remain a separate view of progress.

## Why four hours

Four hours matches the dominant AtCoder short-contest format and keeps repeated agent runs practical. It also gives the benchmark a consistent speedrun horizon instead of assigning agents the original 4 to 367 hour contest duration.

Contest length is not treated as a direct measure of evaluator cost, model-token demand, or problem difficulty. Those quantities will be measured separately.

## Candidate suites

### Candidate A: Sprint-9 plus Inference

The [Sprint-9 plus Inference proposal](sprint-9-plus-inference-proposal.md)
selects the latest rated contest lasting at most six hours in each of the nine
categories represented by short contests, then adds the latest Inference
contest. Inference has no short contest in the current catalog.

The candidate is frozen from the August 12, 2026 catalog. Eligibility still
requires:

- reliable timestamped human submissions through four hours
- comparable agent and human scoring instances
- useful score resolution throughout the four-hour window
- low and stable evaluator CPU cost
- varied research behaviors and problem structures
- limited overlap with development or tuning problems

The same discovery results will be aggregated both with and without AHC030.
This tests whether Inference contributes enough distinct capability signal to
justify the tenth problem and the only long-contest exception.

### Candidate B: ARS-10 breadth

The current [ARS-10 proposal](ars-10-proposal.md) contains six short and four long contests and covers all ten ALE-Bench genres once.

Every problem still receives only four hours of agent time. The original long-contest duration is not reproduced.

### Candidate C: split roles

If neither suite dominates, ARS may use:

- Sprint for frequent development and the primary human speedrun leaderboard
- ARS-10 breadth for less frequent promotion and generalization runs

This is a first-class outcome, not a fallback to conceal an inconclusive comparison.

## Hypotheses

### H1: Sprint has cleaner human comparability

Short-contest participants are more likely to work continuously over the measured four hours. Their timestamped score trajectories may therefore be a better approximation of active human research time than the first four hours of a multi-day contest.

Falsifier: the audited long problems have equally usable early trajectories after accounting for participation, delayed starts, and scoring differences.

### H2: Sprint is cheaper at equal agent wall-clock

The nine short problems may have lower evaluator cost or simpler operational requirements, but contest duration alone does not establish this. AHC030 is measured separately so its incremental cost is visible.

Falsifier: measured evaluator CPU, candidate throughput, model tokens, or operational failure rates are not materially lower than for the mixed suite.

### H3: ARS-10 has better capability coverage

The mixed suite may reveal system strengths and weaknesses hidden by a short-only suite, especially for inference under feedback and long-contest problem structures.

Falsifier: a constrained short-only subset achieves comparable coverage and ranking stability.

### H4: Four hours is long enough to rank systems

Most useful separation between systems and against humans should emerge within the common four-hour window.

Falsifier: rankings remain unstable at four hours or mixed-suite long problems show material, system-dependent improvements only after four hours.

## Phase 1: source and comparability audit

Audit every candidate problem before running agents.

For each problem, record:

- official start time and duration
- judge type and interaction protocol
- provisional and final scoring rules
- whether system tests differ from in-contest tests
- availability of timestamped submissions and scores
- fixed cohort size, plus humans submitted and scored at each checkpoint
- delayed-entry and first-submission distributions
- whether best-so-far trajectories can be reconstructed without future information
- availability of separate development and official evaluation instances
- scorer determinism and seed policy
- expected evaluator CPU per candidate program

Exclude a problem from human speedrun claims when the agent score cannot be made comparable with the historical human score. It may remain eligible for a system-only breadth track.

## Phase 2: human trajectory reconstruction

For each surviving problem:

1. Reconstruct each contestant's chronological best-so-far score curve.
2. Normalize timestamps relative to contest start.
3. At each elapsed time, include every human who has submitted by that time and
   retain exactly that human's best scored submission so far.
4. Treat a human with only non-scored results by that time as a bottom-ranked
   no-score state rather than inventing a numeric score.
5. Do not add contestants using submissions made after the elapsed-time cutoff.
6. Validate representative trajectories against the official standings and submission history.
7. Freeze the transformation code and source hashes before agent promotion runs.

This best-submission-per-human construction is the only human reference
population. ARS will not publish separate all-entrant and active-contestant
percentiles.

## Phase 3: evaluator cost audit

Run fixed baseline programs through each evaluator using declared discovery seeds.

Measure:

- evaluator CPU-seconds per case and per candidate
- wall time and peak memory
- public and private case counts
- candidate evaluations per hour
- score variance across seed subsets
- setup, container, and failure overhead

Use reduced seeds only if rank and score stability remain adequate. Seed reduction is a separate causal factor from problem-subset selection and should be tested separately.

## Phase 4: system discovery runs

Run a small, diverse panel of frozen systems on the union of both candidate suites. Use the same system configuration, model-call concurrency rule, hardware allocation, evaluator policy, and four-hour cap for every problem.

At minimum, include systems expected to differ in:

- base model capability
- memory and context management
- experiment selection
- code-search strategy
- use of evaluator feedback

Record:

- score-versus-wall-clock curve
- time to human 4h P50, P90, and P99
- model input, output, and cached tokens
- inference cost
- evaluator CPU-seconds
- candidate programs evaluated
- model and evaluator concurrency
- failures, restarts, and idle time

Discovery runs may tune the experimental protocol, but they cannot serve as promotion evidence.

## Phase 5: suite comparison

Compare Sprint-9, Sprint-9 plus Inference, and ARS-10 breadth using:

- Spearman and Kendall system-rank correlation with the broader audited pool
- pairwise ranking inversion rate, especially for close systems
- bootstrap confidence intervals over problems and seeds
- rank stability at 1, 2, and 4 hours
- genre and research-behavior coverage
- human-trajectory completeness and comparability
- evaluator CPU, model tokens, inference cost, and end-to-end wall time
- operational failure rate
- sensitivity to any single problem

Also test constrained alternative short subsets. Do not compare only one hand-selected Sprint suite against one hand-selected mixed suite.

For the Inference ablation, report pairwise ranking changes, uncertainty, unique
system failures, human-score comparability, and incremental cost. Decide whether
to retain AHC030 during discovery, then freeze that decision before promotion.

Discovery should initially retain the full score curve, five fixed checkpoints,
and all three four-hour human thresholds. Before promotion, remove redundant or
unstable summaries and freeze the smallest metric set that still explains
meaningful system differences. Discovery outputs cannot be silently promoted
after the metric set is narrowed.

## Promotion run

Before the promotion run:

1. Freeze the candidate suites and exclusions.
2. Freeze the best-submission-per-human trajectory transformation.
3. Freeze seeds, evaluator configuration, checkpoints, and resource policy.
4. Declare acceptance thresholds from discovery evidence.
5. Use held-out systems, repetitions, seeds, or problems not used to choose the suite.
6. Label the run as promotion before execution.

The smallest acceptable promotion design should include repeated runs for enough systems to estimate pairwise rank uncertainty. A single run per system is not sufficient evidence for close rankings.

## Proposed acceptance criteria

Numeric thresholds will be fixed after Phase 4, before promotion. The chosen primary suite should at minimum:

- preserve broad-pool system ordering with high rank correlation
- keep pairwise inversions low enough for the intended leaderboard resolution
- produce stable rankings by four hours
- have usable and comparable human trajectories for every scored problem
- meet the declared evaluator and inference budget
- avoid domination by one problem or one narrow research behavior

If Sprint meets these criteria, it becomes the leading candidate for the primary speedrun suite. If ARS-10 materially improves ranking or capability coverage at acceptable cost, it remains a primary-suite candidate. If the tradeoff is real, use Candidate C and keep the roles separate.

## Stop limits and premise audit

- Allow at most two failed interventions at any data-access, trajectory, or evaluator gate before auditing the premise and forming a new hypothesis.
- Set a per-problem source-audit limit before starting the audit.
- Set a per-system run-cost ceiling before discovery runs.
- Do not extend the four-hour cap during this experiment. A longer-horizon study is a separate declared experiment.
- Evidence that human and agent scores cannot be made comparable is scope-escape evidence for removing that problem from the human speedrun track.

## Outputs

The experiment should produce:

- a versioned problem-audit table
- reproducible human trajectory data and transformation code
- evaluator cost and variance measurements
- frozen discovery and promotion manifests
- suite-ranking and pairwise-stability analysis
- a written decision selecting Sprint, ARS-10 breadth, split roles, or neither

Until those outputs exist, the four-hour cutoff and both suites remain hypotheses under test.
