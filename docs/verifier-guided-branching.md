# Verifier-guided research branching proposal

Status: discovery proposal. It is not part of the authoritative ARB runner or any promotion protocol.

## Decision to make

ARB should test whether an LLM verifier can improve autonomous research by choosing which proposed experiment to implement next.

This is different from using an LLM to select among completed candidate programs. ARB already has an objective public evaluator for that job. On the existing AHC044 Luna run, the public and private scores of the 15 candidates have Spearman rank correlation 0.9991, and candidate 13 is best under both. On the reference Ryzen 9 3950X, the selected 16-worker public evaluator takes a median 19.293 seconds for the 50 cases. A model judge applied after implementation would therefore compete with a fast and nearly perfect task-specific judge.

The plausible opportunity occurs earlier:

```text
current research state
        |
generate N possible next experiments
        |
rank proposals before implementation
        |
implement and evaluate only the selected branch
        |
continue from the resulting state
```

The experiment should determine whether better branch selection saves more research time than proposal generation and verification consume.

## Sources and motivation

[LLM-as-a-Verifier](https://github.com/llm-as-a-verifier/llm-as-a-verifier) uses repeated, criteria-decomposed, probability-weighted model scores to rank candidate trajectories. Its [paper](https://arxiv.org/abs/2607.05391) reports improved best-of-N selection on agentic benchmarks. The related [self-verification result](https://x.com/jackyk02/status/2089421448784023553) uses one model to generate several completed trajectories and rank them with the same model.

ARB is a useful environment for testing the underlying decision policy because it records objective score-versus-time trajectories. It can measure whether a verifier selected a productive idea, how much opportunity it missed relative to the best available branch, and whether the extra inference improved the final result under a fixed wall-clock budget.

## Desired end state

A successful verifier-guided policy would:

- generate a small set of meaningfully different research proposals at selected checkpoints;
- rank proposals without seeing their eventual evaluator results;
- select branches with lower regret than ordinary model self-selection;
- improve ARB score trajectories under the same research deadline;
- preserve explicit candidate submission, evaluator isolation, and auditability;
- report all extra calls, tokens, cost, latency, and model concurrency; and
- remain a harness policy rather than a hidden source of benchmark assistance.

The smallest likely live policy uses three proposals and implements one. Larger N is not justified until three-way selection shows positive value.

## Non-goals

This proposal does not:

- replace the public or private evaluator with an LLM;
- treat a model score as proof that a candidate is correct or competitive;
- add model-generated candidates to the score trajectory without explicit submission;
- change the benchmark deadline, evaluator allocation, or private-test policy;
- claim model self-improvement or update model weights;
- promote the LLM-as-a-Verifier package as an ARB dependency before the method passes the discovery gates; or
- use discovery results as promotion evidence.

## Hypotheses

### H1: Continuous verification predicts productive branches

A probability-weighted, criteria-decomposed verifier ranks proposed experiments closer to their objective downstream value than random choice, the generator's ordinary self-choice, or a discrete LLM judge.

Falsifier: continuous verification does not reduce paired selection regret on blinded proposal sets.

### H2: Selection quality survives its inference overhead

Implementing the verifier-selected branch produces a better ARB score trajectory under the same wall-clock deadline than the current single-path research loop.

Falsifier: verification improves offline ranking but its extra latency, tokens, or context growth leaves the end-to-end trajectory unchanged or worse.

### H3: Same-model verification is viable in the single-model track

One model can generate proposals and rank them sequentially without concurrent inference or an auxiliary model.

Falsifier: self-preference bias, backend limitations, or added latency erase the selection advantage. A result from an auxiliary DeepSeek or Gemini verifier does not establish this hypothesis for local Qwen.

## Experimental ladder

Each stage answers a separate question. Do not proceed merely because the previous stage was operationally successful.

### Stage 0: backend conformance

Before evaluating selection quality, verify that the scorer actually implements the intended continuous signal:

1. Constrain each score position to the declared A-T scale.
1. Retrieve token-level log probabilities for the score position.
1. Confirm that probabilities cover the allowed scale and are normalized only as explicitly documented.
1. Detect missing log probabilities, truncated answers, discrete-text fallback, and all-0.5 ties as distinct outcomes.
1. Repeat fixed fixture comparisons with both candidate orders to measure slot bias.
1. Persist the model identifier, endpoint implementation and version, sampling parameters, prompt hash, and response-usage fields.

This gate matters for the planned Qwen3.8-27B baseline. A llama.cpp-compatible chat endpoint does not by itself prove support for the prefill and constrained logprob behavior used by the upstream implementation. Backend compatibility is a separate hypothesis and must be tested rather than inferred.

Acceptance gate:

- every accepted fixture exposes the required score-token distribution;
- failures are explicit rather than silently converted to neutral scores;
- replay reproduces scores from the persisted raw responses; and
- candidate-order sensitivity is measured before selecting a mitigation.

### Stage 1: retrospective plumbing test

Use the 15 archived candidates from `results/discovery/ahc044-aop-luna-low-20m-v3/` as a local fixture.

For each comparison:

- expose the problem, the common predecessor state where one exists, candidate source or diff, and contemporaneous reasoning and tool output;
- hide public and private scores, historical ranks, later messages, and all future evaluator feedback;
- randomize candidate order while preserving a paired reverse-order check; and
- ask each ranker which candidate the objective evaluator will prefer.

Compare:

1. deterministic random choice;
1. generator self-choice with an ordinary prompt;
1. a discrete pairwise judge using the same verifier model;
1. continuous criteria-decomposed verification; and
1. the public evaluator as the explicit oracle, not as an eligible learned ranker.

Report pairwise accuracy, ties, calibration, position sensitivity, latency, tokens, and cost. Public and private labels should be reported separately.

This stage is only a plumbing and weak predictive sanity check. Later AHC044 candidates inherit information from earlier evaluations, and comparing programs after implementation does not prove that proposal ranking saves research time. Failure is still useful: a verifier that cannot rank completed local candidates does not justify a more expensive branching study.

### Stage 2: blinded proposal-selection discovery

This is the first material test.

Create at least 20 proposal sets across frozen research checkpoints. Begin with AHC044 for infrastructure reuse, then add a second packaged problem before making a general ARB claim.

At each checkpoint:

1. Restore the same source, research context, known public results, remaining time, and model state.
1. Generate N=3 concrete next-experiment proposals. Require the expected code change, hypothesis, predicted effect, evaluation plan, time estimate, and rollback condition.
1. Freeze the proposal set before invoking any ranker or implementation agent.
1. Rank the same blinded set with every learned baseline. Random-choice outcomes are computed over the frozen set rather than consuming additional model calls.
1. Independently implement every proposal from the same checkpoint in an isolated worktree with an equal implementation deadline and the same agent configuration.
1. Submit and evaluate every resulting candidate through the authoritative ARB runner.
1. Treat invalid or unsubmitted branches as observed outcomes, not missing data.

Implementing all branches is an offline discovery expense needed to reveal the oracle branch. It is not the proposed live policy.

For proposal set s with outcome value V and ranker choice r, define regret as:

```text
regret_s(r) = max_i V_s(i) - V_s(r)
```

Use score improvement over the checkpoint candidate as the primary outcome. Also report whether a branch produced diagnostic knowledge despite failing to improve the immediate score, but do not fold a subjective knowledge label into the primary metric.

Advance to Stage 3 only if all of the following hold:

- at least 20 complete blinded proposal sets are available;
- continuous verification has lower mean paired regret than generator self-choice;
- a paired bootstrap 95 percent confidence interval for that regret difference excludes zero in the favorable direction;
- the conclusion is not caused by one checkpoint or one proposal family; and
- the estimated implementation time saved exceeds verifier latency under the proposed live configuration.

If the sample is too small or heterogeneous for the interval to stabilize, collect more discovery sets rather than weakening the gate after inspecting the result.

### Stage 3: end-to-end ARB ablation

Freeze the most promising Stage 2 policy before launching complete runs.

Compare:

- control: the existing single-path research loop;
- self-choice: generate three proposals and let the generator choose one with an ordinary prompt; and
- verifier-guided: generate the same number of proposals, rank them with the frozen continuous verifier, and implement one.

Every run receives the same ARB wall-clock deadline, evaluator worker allocation, tool access, initial candidate, and model-call concurrency rule. Verifier time is inside the research deadline. Do not compensate the treatment with extra wall time. Report model calls, input, cached and output tokens, cost, evaluator CPU, candidate count, idle time, and time spent generating and ranking proposals.

Use repeated seeds on at least two problems. Primary outcomes are the complete private score-versus-time curve and time-to-human thresholds. Fixed-checkpoint scores, final best score, and attainment rate are summaries. Analyze whether any improvement comes from better experiment selection, fewer regressions, or simply more generated ideas.

Stage 3 remains discovery. Define numeric promotion thresholds and a held-out promotion design only after these results exist.

## Ranker input and rubric

The verifier must not receive evaluator results unavailable at the decision time. Its minimal input is:

- task statement and scoring direction;
- current candidate and known public evaluation history;
- remaining research time and observed evaluator latency;
- the frozen proposal set; and
- criteria defined before observing branch outcomes.

Start with the smallest criteria decomposition that reflects the decision:

1. Expected score gain - Is the hypothesis likely to improve the benchmark objective from the current state?
1. Evidence and diagnosis - Does the proposal test a concrete uncertainty and produce interpretable feedback?
1. Time feasibility - Can it be implemented, evaluated, and safely submitted within the stated reserve?
1. Regression risk - Does it preserve or recover the known-good candidate if the experiment fails?

Do not add criteria because they sound generally desirable. A new criterion needs evidence that the current rubric misses a recurring selection failure.

## Qwen3.8-27B and track classification

Test model topology separately from verifier methodology:

| Configuration | ARB track implication | What it establishes | | --- | --- | --- | | Qwen3.8-27B generates and verifies sequentially | Single-model system | Same-model local viability | | Qwen3.8-27B generator plus hosted DeepSeek verifier | Unlimited system | Multi-model practical value | | One frozen model and branching policy supplied to every entrant | Potential controlled-model policy after promotion | Harness effect under a common model |

Do not compare a same-model local treatment with an auxiliary hosted verifier and attribute the result only to continuous verification. Model identity, backend behavior, and verifier policy are separate causal factors.

The local Qwen experiment must record weight and KV-cache quantization, context limit, MTP setting, prompt-ingestion speed, generation speed, peak VRAM, and warm-server state as already required by the planned local baseline.

## Artifact and audit requirements

Discovery may use a standalone local harness without changing the run-event schema. Preserve, at minimum:

- checkpoint identity and source hash;
- proposal-set text and hash;
- randomization and candidate-order seed;
- ranker configuration and prompt hash;
- raw verifier responses and extracted score distributions;
- selected proposal and selection latency;
- independent implementation worktree identity and result;
- submitted candidate identity and public and private scores; and
- complete model and evaluator resource accounting.

If Stage 3 demonstrates value and the policy is promoted, then extend the event schema with explicit proposal-generation and branch-selection events. Do not add new authoritative event types, queues, or recovery machinery during the offline discovery stages.

Hosted verifiers may receive public problem text and candidate proposals during declared discovery runs. Never send private evaluation inputs, hidden seeds, confidential system strategy, credentials, or unpublished competition material to an external verifier. Keep all ARB and ABC26 artifacts local unless the user explicitly approves a specific disclosure.

## Stop limits and premise audit

- Allow at most two failed backend interventions before auditing whether the required constrained logprob interface is actually supported.
- Stop Stage 1 if blinded payload construction cannot exclude future evaluator leakage with auditable provenance.
- Stop Stage 2 after its declared proposal-set and cost ceiling. Extend it only under a new written hypothesis, not because the result is disappointing.
- Do not proceed to live integration if continuous verification fails the paired regret gate or has negative estimated wall-clock value.
- If objective public evaluation becomes cheap enough to test proposals directly, prefer the objective evaluator and retire the verifier hypothesis for that decision point.
- Treat a benefit confined to one model, problem, proposal family, or external verifier as scoped evidence rather than a general ARB result.

## Proposed deliverables

The discovery should produce:

1. a backend-conformance report with replay fixtures;
1. a blinded AHC044 retrospective ranker report;
1. a versioned set of frozen proposal-selection checkpoints;
1. independently implemented branch outcomes and regret analysis;
1. an end-to-end equal-deadline ablation if the offline gate passes; and
1. a decision record to integrate, narrow, or reject verifier-guided branching.

The first implementation deliverable should be the standalone conformance and retrospective harness. The authoritative Rust runner should remain unchanged until the offline selection premise survives falsification.
