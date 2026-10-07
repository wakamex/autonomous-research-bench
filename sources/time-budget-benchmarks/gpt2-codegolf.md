# GPT-2 code-golf buzzer-beater

Retrieved and checked 2026-08-13.

## Identification

This is the example described in the question. Terminal-Bench 2.0's
`gpt2-codegolf` task gives the agent a raw binary dump of the GPT-2 124M
weights and `vocab.bpe`. The agent must write a dependency-free C inference
program smaller than 5,000 bytes. The official task configuration allows 900
seconds for the agent and estimates 2,400 minutes for an expert human.

Primary sources:

- [Terminal-Bench task page](https://www.tbench.ai/benchmarks/terminal-bench-2/gpt2-codegolf)
- [Task configuration at the evaluated commit](https://github.com/laude-institute/terminal-bench-2/blob/69671fbaac6d67a7ef0dfec016cc38a64ef7a77c/gpt2-codegolf/task.toml)
- [Hanson Wang's run account](https://x.com/hansonwng/status/2030000810894184808)
- [Published Codex trajectory viewer](https://gist.github.com/hansonw/db53a79e266310585024ab774f6a3845)
- [Terminal-Bench 2 leaderboard artifacts](https://huggingface.co/datasets/harborframework/terminal-bench-2-leaderboard/tree/main/submissions/terminal-bench/2.0/Forge__GPT-5.4/2026-03-07__04-54-03)

## What happened

Hanson Wang describes GPT-5.4 and GPT-5.3-Codex as the first models to solve
the task consistently without verifier exploits. In the showcased GPT-5.4
Codex trajectory, the model:

1. Inspected the raw checkpoint and inferred its tensor order.
2. Downloaded a known Hugging Face GPT-2 checkpoint and matched tensor summary
   statistics to identify the unknown layout.
3. Prototyped inference and BPE tokenization in Python.
4. Wrote and compiled a readable C implementation around minute 12.
5. Minified it to 4,694 bytes, introduced an undefined-behavior crash, and
   used AddressSanitizer to diagnose it.
6. Fixed the bad `k++` argument evaluation and had a working program with
   roughly 35 seconds left in the 15-minute window.

The public session viewer lasts 17 minutes 46 seconds because the model was
allowed to keep testing after the scoring cutoff. The qualifying artifact was
already working before 15 minutes. Wang reports a roughly 20 to 25 percent
success rate for GPT-5.4 and GPT-5.3-Codex on this task at the time, with the
deadline responsible for many failures.

## Time-awareness audit

The buzzer-beater was not produced by an explicit countdown prompt.

- The task prompt shown in the full trajectory does not state the 15-minute
  limit.
- The public trajectory contains no later user or harness reminder about time
  remaining.
- Tool results report the duration of individual commands, but no overall
  remaining-time value.
- Relative `+MM:SS` labels appear in the generated HTML viewer. They are
  presentation metadata, not messages in the conversation.
- The model continued stress testing past minute 15 when allowed, which is
  further evidence that it was not managing against a visible hard deadline.

There was still time-related machinery outside the model:

- Harbor enforced the task's 900-second agent timeout.
- The scoring object was the durable `/app/gpt2.c` file, so a correct artifact
  present at cutoff could survive termination without a special final answer.
- Related official Forge leaderboard trajectories show wall-clock timestamps
  on tool events and the model often wrapped expensive inference checks in
  `timeout 120`. Those per-command guards were chosen during execution. They
  were not an overall deadline countdown or a fixed checkpoint-submission
  policy.

The main lesson for ARB is that this run demonstrates artifact-at-deadline
scoring, not successful deadline-aware prompting. Its last-second success was
largely accidental with respect to the global clock. A rerun could spend the
last two minutes on another long verification and lose despite having a useful
candidate nearby.

