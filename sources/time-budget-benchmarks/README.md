# Time-budget benchmark sources

Retrieved on 2026-08-13 from primary project documentation using the local
web-to-Markdown extraction workflow. These are source snapshots, not ARS
interpretations.

| Local snapshot | Upstream source | Upstream HEAD at retrieval | SHA-256 |
| --- | --- | --- | --- |
| [`paperbench.md`](paperbench.md) | [OpenAI frontier-evals PaperBench README](https://github.com/openai/frontier-evals/blob/main/project/paperbench/README.md) | `51052cede8cc608f95bb00346635e03759013e5a` | `d397188342fcc9ce4ef29547e621fd1cf74eefb6532d7ac0b7857a964a2c9b64` |
| [`mini.md`](mini.md) | [mini-SWE-agent DefaultAgent reference](https://mini-swe-agent.com/latest/reference/agents/default/) | `a83fcae82d2a08f0ee0c688f9d137b3566c097f8` | `c33180205afce25f9436659ec407ad318dfa983a9894a233c73309c7fc94bdae` |
| [`mle.md`](mle.md) | [OpenAI MLE-Bench README](https://github.com/openai/mle-bench/) | `507f92e1138bb6e40dac5c6ee7a6758e6424bf97` | `808751e558fa2721739b93e1ab73a1916407c2a9b4e8cad3803b2b4999e62cb1` |
| [`rebench.md`](rebench.md) | [METR RE-Bench README](https://github.com/METR/RE-Bench/) | `93b98062e55f6945d4a7e213a3226dd419896170` | `d33a2075119bfc70d9f9e916343b8cf5db18b1d7716bdd62a2bf76669c971743` |

[`gpt2-codegolf.md`](gpt2-codegolf.md) is an evidence audit rather than a
verbatim snapshot. It cross-references the task configuration, author account,
full public trajectory, and leaderboard artifacts.

The PaperBench and mini-SWE-agent snapshots contain the most directly relevant
implementation evidence. MLE-Bench supplies the long-runtime, fixed-resource,
valid-submission comparison. RE-Bench supplies the closest research-engineering
task format and contamination controls.
