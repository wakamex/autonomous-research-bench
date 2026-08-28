# Human trajectory references

These files contain fixed four-hour cohorts built from the timestamped AtCoder submission snapshots. Every person who submitted by 14,400 seconds remains in the cohort at every checkpoint, with score zero before their first accepted scored submission and their best score so far afterward.

Sprint-9 references are available for AHC004, AHC044, AHC050, AHC052, AHC053, AHC062, AHC065, AHC067, and AHC068. Each file records its source snapshot hash, cohort counts, checkpoints, percentile thresholds, and per-human improvement trajectory.

AHC044 has also passed multiset validation against the archived ALE-Bench final standings. The other eight references are derived directly from AtCoder submission histories but have not yet received an independent final-standings distribution check. That validation remains a promotion gate, not a blocker for discovery scoring.
