# AHC044 native runner validation

This discovery gate compares the native Rust ARS runner with the archived
ALE-Bench results used by the Luna baseline.

- All 15 private scores match exactly.
- Candidate 13's 50-case public score matches at 49,189,250.
- Native median wall time is 3.082 seconds at 16 workers, including isolated
  compilation and case execution.
- The comparable ALE-Bench median is 19.293 seconds.
- Native execution is 6.26 times faster and uses 84.03 percent less wall time.
- The Rust human-reference compiler, standings validation, and replay scorer
  reproduce their existing outputs exactly.

The raw compatibility values are in `compatibility.json`. This is discovery
evidence, not a promotion run. The runner, problem package, compiler profile,
and concurrency policy still need validation across the rest of Sprint-9.
