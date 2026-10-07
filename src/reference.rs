use std::collections::{BTreeMap, HashMap, HashSet};
use std::fs::{self, File};
use std::path::Path;

use anyhow::{Context, Result, bail};
use flate2::read::GzDecoder;
use serde::Deserialize;
use serde_json::{Value, json};

use crate::events::{Event, read_events};
use crate::problem::{Package, ScoreDirection, file_sha256};

#[derive(Debug, Deserialize)]
struct StandingRow {
    rank: u64,
    score: u64,
}
#[derive(Debug, Deserialize)]
struct PerformanceRow {
    rank: u64,
    performance: i64,
}

pub fn historical_rank(package: &Package, new_score: u64) -> Result<(u64, i64)> {
    let direction = package.problem.score_direction;
    let standings = csv::Reader::from_path(package.standings()?)?
        .deserialize::<StandingRow>()
        .collect::<std::result::Result<Vec<_>, _>>()?;
    let performance = csv::Reader::from_path(package.performance()?)?
        .deserialize::<PerformanceRow>()
        .collect::<std::result::Result<Vec<_>, _>>()?;
    if standings.len() < 2 || performance.len() < 2 {
        bail!("incomplete historical reference");
    }
    let mut groups = Vec::new();
    let mut start = standings[0].rank;
    for pair in standings.windows(2) {
        if pair[0].score == pair[1].score {
            continue;
        }
        groups.push((pair[0].score, start, pair[1].rank - 1));
        start = pair[1].rank;
    }
    let last = standings.last().unwrap();
    groups.push((last.score, last.rank, last.rank));
    let (rank, performance_rank) = if direction == ScoreDirection::Maximize && new_score == 0 {
        (last.rank, last.rank as f64)
    } else {
        groups
            .iter()
            .find_map(|&(score, start, end)| {
                if direction.improves(new_score, score) {
                    Some((start, start as f64))
                } else if new_score == score {
                    Some((start, (start + end) as f64 / 2.0))
                } else {
                    None
                }
            })
            .context("score fell outside standings")?
    };
    let mut map = Vec::<(f64, i64)>::new();
    for pair in performance.windows(2) {
        if pair[0].rank == pair[1].rank {
            continue;
        }
        map.push((
            ((pair[0].rank + pair[1].rank - 1) as f64) / 2.0,
            pair[0].performance,
        ));
    }
    map.push((
        performance.last().unwrap().rank as f64,
        performance.last().unwrap().performance,
    ));
    let value = if let Some((_, value)) = map.iter().find(|(rank, _)| *rank == performance_rank) {
        *value
    } else {
        let upper = map.partition_point(|(rank, _)| *rank < performance_rank);
        let upper = upper.max(1).min(map.len() - 1);
        let (rank_high, perf_high) = map[upper - 1];
        let (rank_low, perf_low) = map[upper];
        (perf_high as f64
            - (perf_high - perf_low) as f64 * (performance_rank - rank_high)
                / (rank_low - rank_high))
            .round_ties_even() as i64
    };
    Ok((rank, value))
}

#[derive(Debug, Deserialize)]
struct Submission {
    id: u64,
    elapsed_seconds: u64,
    user_id: String,
    point: String,
    result: String,
}

#[derive(Debug)]
struct Human {
    display_id: String,
    first_submission: u64,
    improvements: Vec<(u64, u64, u64)>,
}

pub struct HumanReferenceOptions<'a> {
    pub problem_id: &'a str,
    pub submissions: &'a Path,
    pub output: &'a Path,
    pub cohort_seconds: u64,
    pub checkpoints: &'a [u64],
    pub percentiles: &'a [u64],
    pub score_direction: ScoreDirection,
    pub package: Option<&'a Package>,
    pub minimum_match_fraction: f64,
}

pub fn build_human_reference(options: HumanReferenceOptions<'_>) -> Result<Value> {
    let HumanReferenceOptions {
        problem_id,
        submissions,
        output,
        cohort_seconds,
        checkpoints,
        percentiles,
        score_direction,
        package,
        minimum_match_fraction,
    } = options;
    if checkpoints.iter().any(|&value| value > cohort_seconds) {
        bail!("checkpoints cannot exceed the cohort cutoff");
    }
    if percentiles.is_empty() || percentiles.iter().any(|value| !(1..=100).contains(value)) {
        bail!("percentiles must be in the inclusive range 1 through 100");
    }
    let reader = GzDecoder::new(File::open(submissions)?);
    let mut rows = csv::Reader::from_reader(reader)
        .deserialize::<Submission>()
        .collect::<std::result::Result<Vec<_>, _>>()?;
    rows.sort_by_key(|row| (row.elapsed_seconds, row.id));
    let source_submissions = rows.len();
    let source_users = rows
        .iter()
        .map(|row| row.user_id.to_lowercase())
        .collect::<HashSet<_>>()
        .len();
    let source_max_elapsed = rows.iter().map(|row| row.elapsed_seconds).max();
    rows.retain(|row| row.elapsed_seconds <= cohort_seconds);
    let mut users = HashMap::<String, Human>::new();
    for row in &rows {
        let key = row.user_id.to_lowercase();
        let user = users.entry(key).or_insert_with(|| Human {
            display_id: row.user_id.clone(),
            first_submission: row.elapsed_seconds,
            improvements: Vec::new(),
        });
        if row.result != "AC" {
            continue;
        }
        let point = row.point.parse::<f64>()?;
        if point.fract() != 0.0 || point < 0.0 {
            bail!("non-integral score {}", row.point);
        }
        let score = point as u64;
        if user
            .improvements
            .last()
            .is_none_or(|value| score_direction.improves(score, value.1))
        {
            user.improvements.push((row.elapsed_seconds, score, row.id));
        }
    }
    let mut humans = users.into_values().collect::<Vec<_>>();
    if humans.is_empty() {
        bail!("fixed human cohort is empty");
    }
    humans.sort_by(|a, b| {
        a.display_id
            .to_lowercase()
            .cmp(&b.display_id.to_lowercase())
    });
    let checkpoint_values = checkpoints.iter().map(|&time| {
        let scores = humans.iter().map(|human| score_at(&human.improvements, time, score_direction)).collect::<Vec<_>>();
        let submitted = humans.iter().filter(|human| human.first_submission <= time).count();
        let scored = humans.iter().filter(|human| score_at(&human.improvements, time, score_direction).is_some_and(|score| score > 0)).count();
        let submissions_at = rows.iter().filter(|row| row.elapsed_seconds <= time).count();
        let values = percentiles.iter().map(|&percentile|
            (format!("p{percentile}"), json!(nearest_rank(&scores, percentile, score_direction)))).collect::<serde_json::Map<_, _>>();
        json!({"elapsed_seconds": time, "humans": humans.len(), "humans_submitted": submitted,
            "humans_with_score": scored, "submissions": submissions_at, "score_percentiles": values})
    }).collect::<Vec<_>>();
    let trajectories = humans
        .iter()
        .map(|human| {
            json!({
                "user_id": human.display_id, "first_submission_seconds": human.first_submission,
                "improvements": human.improvements,
            })
        })
        .collect::<Vec<_>>();
    let mut value = json!({
        "schema_version": "arb.human-reference.v2", "problem_id": problem_id,
        "source": {"submissions_file": submissions, "submissions_sha256": file_sha256(submissions)?},
        "method": {
            "population": "fixed cohort of users with at least one submission by the configured cohort cutoff",
            "cohort_seconds": cohort_seconds,
            "per_human_value": format!("best AC score in the {} direction submitted by the elapsed-time cutoff", score_direction.as_str()),
            "unscored_value": if score_direction == ScoreDirection::Maximize {json!(0)} else {Value::Null},
            "score_direction": score_direction.as_str(),
            "percentile": "nearest rank over scores ordered from worst to best: ordered_scores[ceil(p * n) - 1]"
        },
        "cohort": {"cutoff_seconds": cohort_seconds,
            "selection": "first submission at or before the cohort cutoff",
            "precomputed_trajectory_count": humans.len(), "source_humans": source_users,
            "cohort_humans": humans.len(), "excluded_humans": source_users - humans.len(),
            "source_submissions": source_submissions, "submissions_by_cutoff": rows.len(),
            "source_max_elapsed_seconds": source_max_elapsed},
        "checkpoints": checkpoint_values, "trajectories": trajectories,
    });
    if let Some(package) = package {
        let (validation, standings_hash) =
            validate_standings(&humans, package, minimum_match_fraction, score_direction)?;
        value["source"]["official_standings_url"] = json!(package.problem.provenance.source_url);
        value["source"]["ale_bench_zip_sha256"] = json!(package.problem.provenance.source_sha256);
        value["source"]["standings_csv_sha256"] = json!(standings_hash);
        value["validation"] = validation;
    }
    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(output, serde_json::to_vec_pretty(&value)?)?;
    Ok(value)
}

fn validate_standings(
    humans: &[Human],
    package: &Package,
    minimum: f64,
    score_direction: ScoreDirection,
) -> Result<(Value, String)> {
    if !(0.0..=1.0).contains(&minimum) {
        bail!("minimum match fraction must be between zero and one");
    }
    let standings_path = package.standings()?;
    let standings = csv::Reader::from_path(&standings_path)?
        .deserialize::<StandingRow>()
        .collect::<std::result::Result<Vec<_>, _>>()?;
    let mut official_scores = Vec::new();
    for pair in standings.windows(2) {
        if pair[0].score > 0 {
            official_scores.extend(std::iter::repeat_n(
                pair[0].score,
                (pair[1].rank - pair[0].rank) as usize,
            ));
        }
    }
    let public_scores = humans
        .iter()
        .filter_map(|human| human.improvements.last().map(|value| value.1))
        .collect::<Vec<_>>();
    let mut public_counts = BTreeMap::<u64, usize>::new();
    let mut official_counts = BTreeMap::<u64, usize>::new();
    for score in &public_scores {
        *public_counts.entry(*score).or_default() += 1;
    }
    for score in &official_scores {
        *official_counts.entry(*score).or_default() += 1;
    }
    let matched: usize = public_counts
        .iter()
        .map(|(score, count)| count.min(official_counts.get(score).unwrap_or(&0)))
        .sum();
    let matched_fraction = matched as f64 / public_scores.len() as f64;
    let difference = |left: &BTreeMap<u64, usize>, right: &BTreeMap<u64, usize>| {
        let mut values = left
            .iter()
            .flat_map(|(score, count)| {
                std::iter::repeat_n(
                    *score,
                    count.saturating_sub(*right.get(score).unwrap_or(&0)),
                )
            })
            .collect::<Vec<_>>();
        values.sort_unstable_by(|a, b| b.cmp(a));
        values
    };
    let validation = json!({
        "status": if matched_fraction >= minimum {"passed_with_documented_population_differences"} else {"failed"},
        "method": "multiset comparison with ALE-Bench's official final standings score distribution",
        "minimum_match_fraction": minimum,
        "public_scored_humans": public_scores.len(),
        "official_scored_positions": official_scores.len(),
        "matched_public_scores": matched,
        "matched_public_fraction": matched_fraction,
        "public_only_scores": difference(&public_counts, &official_counts),
        "official_only_scores": difference(&official_counts, &public_counts),
        "official_percentiles": {
            "p50": nearest_rank(&official_scores.iter().copied().map(Some).collect::<Vec<_>>(), 50, score_direction),
            "p90": nearest_rank(&official_scores.iter().copied().map(Some).collect::<Vec<_>>(), 90, score_direction),
            "p99": nearest_rank(&official_scores.iter().copied().map(Some).collect::<Vec<_>>(), 99, score_direction)
        },
        "note": "ALE-Bench standings encode final ranked positions without user IDs, while the ARB reference includes every public submitter. Exact population equality is neither expected nor assumed."
    });
    if matched_fraction < minimum {
        bail!(
            "official standings matched {:.2}%, below the required {:.2}%",
            matched_fraction * 100.0,
            minimum * 100.0,
        );
    }
    Ok((validation, file_sha256(&standings_path)?))
}

pub fn replay_score(events_path: &Path, reference_path: &Path, horizon: u64) -> Result<Value> {
    let events = read_events(events_path)?;
    validate_events(&events)?;
    let reference: Value = serde_json::from_slice(&fs::read(reference_path)?)?;
    let score_direction: ScoreDirection =
        serde_json::from_value(reference["method"]["score_direction"].clone())
            .context("human reference score direction")?;
    let mut submitted = BTreeMap::<String, f64>::new();
    let mut private = HashMap::<String, Option<u64>>::new();
    for event in &events {
        if event.event_type == "candidate.submitted" {
            submitted.insert(
                event.payload["candidate_id"].as_str().unwrap().into(),
                event.elapsed_seconds,
            );
        } else if event.event_type == "evaluation.completed"
            && event.payload["visibility"] == "private-post-run"
        {
            private.insert(
                event.payload["candidate_id"].as_str().unwrap().into(),
                event.payload["score"].as_u64(),
            );
        }
    }
    let candidates = submitted
        .iter()
        .filter_map(|(id, &time)| private.get(id).map(|score| (id, time, *score)))
        .collect::<Vec<_>>();
    let trajectories = reference["trajectories"]
        .as_array()
        .context("human trajectories")?;
    let checkpoints = reference["checkpoints"]
        .as_array()
        .context("human checkpoints")?;
    let checkpoint_results = checkpoints.iter().map(|checkpoint| {
        let time = checkpoint["elapsed_seconds"].as_u64().unwrap();
        let best = candidates.iter().filter(|(_, submitted, score)| *submitted <= time as f64 && score.is_some())
            .max_by(|a, b| score_direction.compare(a.2.unwrap(), b.2.unwrap()).then_with(|| b.1.total_cmp(&a.1)));
        let scores = human_scores(trajectories, time, score_direction);
        let percentile = best.map(|(_, _, score)| percentile_with_ties(&scores, score.unwrap(), score_direction));
        json!({"elapsed_seconds": time, "agent_score": best.and_then(|value| value.2),
            "candidate_id": best.map(|value| value.0), "human_count": scores.len(), "human_percentile": percentile})
    }).collect::<Vec<_>>();
    let final_checkpoint = checkpoints
        .iter()
        .find(|value| value["elapsed_seconds"] == horizon)
        .context("horizon checkpoint")?;
    let thresholds = final_checkpoint["score_percentiles"].as_object().unwrap().iter().map(|(name, value)| {
        let Some(target) = value.as_u64() else {
            return json!({"target": name, "score": null, "status": "unavailable",
                "elapsed_seconds": null, "candidate_id": null, "censor_seconds": null});
        };
        let reached = candidates.iter().filter(|(_, time, score)| *time <= horizon as f64 && score.is_some_and(|score| score_direction.reaches(score, target)))
            .min_by(|a, b| a.1.total_cmp(&b.1));
        json!({"target": name, "score": target, "status": if reached.is_some() {"reached"} else {"right-censored"},
            "elapsed_seconds": reached.map(|value| value.1), "candidate_id": reached.map(|value| value.0),
            "censor_seconds": if reached.is_none() {Some(horizon)} else {None}})
    }).collect::<Vec<_>>();
    Ok(
        json!({"schema_version": "arb.run-score.v1", "run_id": events[0].run_id,
        "problem_id": reference["problem_id"], "human_cohort": reference["cohort"],
        "score_direction": score_direction.as_str(), "tie_method": "report strict lower bound, inclusive upper bound, and midrank",
        "checkpoints": checkpoint_results, "time_to_human_4h": thresholds,
        "submitted_candidates": submitted.len(), "privately_scored_candidates": candidates.len()}),
    )
}

fn validate_events(events: &[Event]) -> Result<()> {
    if events.is_empty() {
        bail!("event log is empty");
    }
    let run_id = &events[0].run_id;
    let mut candidates = HashSet::new();
    let mut elapsed = -1.0;
    let mut stopped = false;
    for (sequence, event) in events.iter().enumerate() {
        // Runs recorded before the ARB rename used the ars prefix.
        if !matches!(
            event.schema_version.as_str(),
            "arb.run-event.v1" | "ars.run-event.v1"
        ) || &event.run_id != run_id
            || event.sequence != sequence
        {
            bail!("event {sequence} breaks schema, identity, or sequence");
        }
        if event.elapsed_seconds < elapsed {
            bail!("event {sequence} has decreasing time");
        }
        elapsed = event.elapsed_seconds;
        if sequence == 0 && event.event_type != "run.started" {
            bail!("first event must start run");
        }
        if event.event_type == "candidate.submitted" {
            let id = event.payload["candidate_id"]
                .as_str()
                .context("candidate id")?;
            if !candidates.insert(id.to_owned()) {
                bail!("duplicate candidate {id}");
            }
        }
        if event.event_type == "evaluation.completed" {
            let id = event.payload["candidate_id"]
                .as_str()
                .context("evaluation candidate")?;
            if !candidates.contains(id) {
                bail!("evaluation of unknown candidate {id}");
            }
        }
        if event.event_type == "run.stopped" {
            if stopped {
                bail!("multiple stop events");
            }
            stopped = true;
        }
    }
    if !stopped {
        bail!("run has no stop event");
    }
    Ok(())
}

fn score_at(improvements: &[(u64, u64, u64)], time: u64, direction: ScoreDirection) -> Option<u64> {
    improvements
        .iter()
        .filter(|value| value.0 <= time)
        .map(|value| value.1)
        .max_by(|left, right| direction.compare(*left, *right))
        .or((direction == ScoreDirection::Maximize).then_some(0))
}

fn compare_scores(
    left: Option<u64>,
    right: Option<u64>,
    direction: ScoreDirection,
) -> std::cmp::Ordering {
    match (left, right) {
        (None, None) => std::cmp::Ordering::Equal,
        (None, Some(_)) => std::cmp::Ordering::Less,
        (Some(_), None) => std::cmp::Ordering::Greater,
        (Some(left), Some(right)) => direction.compare(left, right),
    }
}

fn nearest_rank(values: &[Option<u64>], percentile: u64, direction: ScoreDirection) -> Option<u64> {
    let mut values = values.to_vec();
    values.sort_unstable_by(|left, right| compare_scores(*left, *right, direction));
    values[(percentile * values.len() as u64).div_ceil(100).max(1) as usize - 1]
}

fn human_scores(trajectories: &[Value], time: u64, direction: ScoreDirection) -> Vec<Option<u64>> {
    trajectories
        .iter()
        .map(|human| {
            human["improvements"]
                .as_array()
                .unwrap()
                .iter()
                .filter(|value| value[0].as_u64().unwrap() <= time)
                .map(|value| value[1].as_u64().unwrap())
                .max_by(|left, right| direction.compare(*left, *right))
                .or((direction == ScoreDirection::Maximize).then_some(0))
        })
        .collect()
}

fn percentile_with_ties(scores: &[Option<u64>], agent: u64, direction: ScoreDirection) -> Value {
    let below = scores
        .iter()
        .filter(|&&score| compare_scores(score, Some(agent), direction).is_lt())
        .count();
    let equal = scores.iter().filter(|&&score| score == Some(agent)).count();
    let total = scores.len() as f64;
    let lower = 100.0 * below as f64 / total;
    let upper = 100.0 * (below + equal) as f64 / total;
    json!({"lower": lower, "midrank": (lower + upper) / 2.0, "upper": upper})
}

#[cfg(test)]
mod tests {
    use super::*;

    fn event(sequence: usize, elapsed_seconds: f64, event_type: &str, payload: Value) -> Event {
        Event {
            schema_version: "arb.run-event.v1".into(),
            run_id: "minimize-run".into(),
            sequence,
            event_type: event_type.into(),
            elapsed_seconds,
            recorded_at: "2026-01-01T00:00:00Z".into(),
            payload,
        }
    }

    #[test]
    fn ahc044_rank_mapping_matches_archived_ale_results() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR"));
        let package = Package::load(&root.join("problems/ahc044")).unwrap();
        for (score, expected) in [
            (1_541_672, (986, 236)),
            (146_704_300, (202, 1774)),
            (147_488_854, (155, 1887)),
        ] {
            assert_eq!(historical_rank(&package, score).unwrap(), expected);
        }
    }

    #[test]
    fn percentile_reports_all_tie_bounds() {
        assert_eq!(
            percentile_with_ties(
                &[Some(0), Some(10), Some(10), Some(20)],
                10,
                ScoreDirection::Maximize,
            ),
            json!({"lower": 25.0, "midrank": 50.0, "upper": 75.0})
        );
        assert_eq!(
            percentile_with_ties(
                &[None, Some(20), Some(10), Some(10)],
                10,
                ScoreDirection::Minimize,
            ),
            json!({"lower": 50.0, "midrank": 75.0, "upper": 100.0})
        );
    }

    #[test]
    fn nearest_rank_uses_the_fixed_cohort_definition() {
        let scores = [Some(0), Some(0), Some(10), Some(20)];
        assert_eq!(nearest_rank(&scores, 50, ScoreDirection::Maximize), Some(0));
        assert_eq!(
            nearest_rank(&scores, 90, ScoreDirection::Maximize),
            Some(20)
        );

        let costs = [None, None, Some(20), Some(10)];
        assert_eq!(nearest_rank(&costs, 50, ScoreDirection::Minimize), None);
        assert_eq!(nearest_rank(&costs, 90, ScoreDirection::Minimize), Some(10));
    }

    #[test]
    fn replay_selects_lower_costs_and_reverses_thresholds() {
        let directory = tempfile::tempdir().unwrap();
        let events_path = directory.path().join("events.jsonl");
        let reference_path = directory.path().join("reference.json");
        let events = [
            event(0, 0.0, "run.started", json!({})),
            event(
                1,
                10.0,
                "candidate.submitted",
                json!({"candidate_id": "candidate-0001"}),
            ),
            event(
                2,
                20.0,
                "candidate.submitted",
                json!({"candidate_id": "candidate-0002"}),
            ),
            event(3, 30.0, "run.stopped", json!({"reason": "deadline"})),
            event(
                4,
                31.0,
                "evaluation.completed",
                json!({"candidate_id": "candidate-0001", "visibility": "private-post-run", "score": 100}),
            ),
            event(
                5,
                32.0,
                "evaluation.completed",
                json!({"candidate_id": "candidate-0002", "visibility": "private-post-run", "score": 50}),
            ),
        ];
        let serialized = events
            .iter()
            .map(|value| serde_json::to_string(value).unwrap())
            .collect::<Vec<_>>()
            .join("\n");
        fs::write(&events_path, format!("{serialized}\n")).unwrap();
        fs::write(
            &reference_path,
            serde_json::to_vec(&json!({
                "problem_id": "minimize-test",
                "method": {"score_direction": "minimize"},
                "cohort": {"cohort_humans": 2},
                "trajectories": [
                    {"improvements": [[5, 80, 1]]},
                    {"improvements": [[5, 40, 2]]}
                ],
                "checkpoints": [
                    {"elapsed_seconds": 15, "score_percentiles": {"p50": 80, "p90": 40}},
                    {"elapsed_seconds": 30, "score_percentiles": {"p50": 80, "p90": 40}}
                ]
            }))
            .unwrap(),
        )
        .unwrap();

        let score = replay_score(&events_path, &reference_path, 30).unwrap();
        assert_eq!(score["checkpoints"][0]["agent_score"], 100);
        assert_eq!(score["checkpoints"][1]["agent_score"], 50);
        assert_eq!(score["checkpoints"][1]["human_percentile"]["midrank"], 50.0);
        assert_eq!(score["time_to_human_4h"][0]["status"], "reached");
        assert_eq!(score["time_to_human_4h"][0]["elapsed_seconds"], 20.0);
        assert_eq!(score["time_to_human_4h"][1]["status"], "right-censored");
    }
}
