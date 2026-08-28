use std::fs;
use std::path::Path;

use anyhow::{Context, Result, bail};
use serde::{Deserialize, Serialize};

use crate::problem::file_sha256;

pub struct Options<'a> {
    pub worker_sweep: &'a Path,
    pub events: &'a Path,
    pub runs: usize,
    pub physical_cores: usize,
    pub horizon_seconds: f64,
    pub repetitions: usize,
    pub seed: u64,
}

#[derive(Clone, Copy)]
struct Trial {
    compile_seconds: f64,
    case_seconds: f64,
    case_cpu_seconds: f64,
    case_parallelism: f64,
}

#[derive(Deserialize)]
struct WorkerSweep {
    measurements: Vec<Measurement>,
}

#[derive(Deserialize)]
struct Measurement {
    workers: usize,
    trials: Vec<MeasuredTrial>,
}

#[derive(Deserialize)]
struct MeasuredTrial {
    compile_seconds: f64,
    wall_seconds: f64,
    cpu_seconds: f64,
}

#[derive(Clone, Copy)]
enum Policy {
    OneCpuLanes,
    SharedParallelEvaluators,
}

struct Evaluation {
    started: f64,
    phase_started: f64,
    phase: Phase,
    remaining_work: f64,
    maximum_cpus: f64,
    case_work: f64,
    case_parallelism: f64,
    isolated_case_seconds: f64,
    case_overlapped: bool,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Phase {
    Compile,
    Cases,
}

struct Runner {
    wake_at: f64,
    evaluation: Option<Evaluation>,
    completed: usize,
}

#[derive(Clone, Serialize)]
struct Distribution {
    minimum: f64,
    p05: f64,
    median: f64,
    mean: f64,
    p95: f64,
    maximum: f64,
    standard_deviation: f64,
}

struct Repetition {
    completed_evaluations: usize,
    completed_per_run: Vec<usize>,
    feedback_latency: Distribution,
    case_slowdown: Distribution,
    overlap_fraction: f64,
    cpu_utilization: f64,
    peak_concurrent_evaluations: usize,
    peak_concurrent_case_phases: usize,
}

#[derive(Serialize)]
struct Aggregate {
    completed_evaluations: Distribution,
    completed_per_run: Distribution,
    mean_completed_per_run: f64,
    mean_feedback_latency_seconds: Distribution,
    median_feedback_latency_seconds: Distribution,
    p95_feedback_latency_seconds: Distribution,
    maximum_feedback_latency_seconds: Distribution,
    mean_case_slowdown: Distribution,
    p95_case_slowdown: Distribution,
    overlap_fraction: Distribution,
    cpu_utilization: Distribution,
    peak_concurrent_evaluations: Distribution,
    peak_concurrent_case_phases: Distribution,
}

struct SplitMix64 {
    state: u64,
}

impl SplitMix64 {
    fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    fn next_u64(&mut self) -> u64 {
        self.state = self.state.wrapping_add(0x9e3779b97f4a7c15);
        let mut value = self.state;
        value = (value ^ (value >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)).wrapping_mul(0x94d049bb133111eb);
        value ^ (value >> 31)
    }

    fn unit(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 / (1_u64 << 53) as f64
    }

    fn index(&mut self, length: usize) -> usize {
        (self.next_u64() as usize) % length
    }
}

pub fn estimate(options: Options<'_>) -> Result<serde_json::Value> {
    if options.runs == 0
        || options.physical_cores == 0
        || options.repetitions == 0
        || !options.horizon_seconds.is_finite()
        || options.horizon_seconds <= 0.0
    {
        bail!("runs, physical cores, repetitions, and horizon must be positive");
    }
    let one_trials = load_trials(options.worker_sweep, 1)?;
    let parallel_trials = load_trials(options.worker_sweep, 16)?;
    let think_gaps = load_think_gaps(options.events)?;
    let mut one_repetitions = Vec::with_capacity(options.repetitions);
    let mut parallel_repetitions = Vec::with_capacity(options.repetitions);
    for repetition in 0..options.repetitions {
        let seed = options.seed.wrapping_add(repetition as u64);
        one_repetitions.push(simulate(
            &mut SplitMix64::new(seed),
            Policy::OneCpuLanes,
            &one_trials,
            &think_gaps,
            &options,
        ));
        parallel_repetitions.push(simulate(
            &mut SplitMix64::new(seed),
            Policy::SharedParallelEvaluators,
            &parallel_trials,
            &think_gaps,
            &options,
        ));
    }
    let one = aggregate(&one_repetitions, options.runs);
    let parallel = aggregate(&parallel_repetitions, options.runs);
    let speedups = one_repetitions
        .iter()
        .zip(&parallel_repetitions)
        .map(|(one, parallel)| {
            parallel.completed_evaluations as f64 / one.completed_evaluations as f64
        })
        .collect::<Vec<_>>();
    let comparison = serde_json::json!({
        "completed_evaluations_speedup": distribution(speedups),
        "mean_completed_evaluations_speedup": parallel.mean_completed_per_run / one.mean_completed_per_run,
        "mean_feedback_latency_speedup": one.mean_feedback_latency_seconds.mean / parallel.mean_feedback_latency_seconds.mean,
        "mean_evaluations_per_run_one_cpu": one.mean_completed_per_run,
        "mean_evaluations_per_run_shared_16_worker": parallel.mean_completed_per_run,
    });
    Ok(serde_json::json!({
        "schema_version": "ars.concurrent-evaluator-estimate.v2",
        "status": "discovery-model",
        "model": {
            "description": "Closed-loop discrete-event Monte Carlo with max-min fair sharing of physical CPU cores",
            "one_cpu_policy": "Each evaluator phase has maximum parallelism one, so 16 runs can each occupy one core",
            "shared_policy": "Compilers use at most one core and case phases use their isolated effective parallelism while all phases share 16 cores",
            "unmodeled": [
                "scheduler migration and SMT effects",
                "memory-bandwidth, cache, storage, and process-launch contention",
                "changes in model latency when many agent runs share a provider",
                "score sensitivity to reduced compute"
            ]
        },
        "inputs": {
            "worker_sweep": options.worker_sweep,
            "worker_sweep_sha256": file_sha256(options.worker_sweep)?,
            "events": options.events,
            "events_sha256": file_sha256(options.events)?,
            "runs": options.runs,
            "physical_cores": options.physical_cores,
            "horizon_seconds": options.horizon_seconds,
            "repetitions": options.repetitions,
            "seed": options.seed,
            "think_gaps_seconds": think_gaps,
            "think_gap_seconds": distribution(think_gaps.clone())
        },
        "policies": {
            "one_cpu_lanes": one,
            "shared_16_worker_evaluators": parallel
        },
        "comparison": comparison
    }))
}

fn load_trials(path: &Path, workers: usize) -> Result<Vec<Trial>> {
    let sweep: WorkerSweep = serde_json::from_slice(&fs::read(path)?)?;
    let measurement = sweep
        .measurements
        .into_iter()
        .find(|measurement| measurement.workers == workers)
        .with_context(|| format!("worker sweep has no {workers}-worker measurement"))?;
    if measurement.trials.is_empty() {
        bail!("worker sweep has no {workers}-worker trials");
    }
    measurement
        .trials
        .into_iter()
        .map(|trial| {
            let case_seconds = trial.wall_seconds - trial.compile_seconds;
            if case_seconds <= 0.0 || trial.cpu_seconds <= 0.0 {
                bail!("worker sweep contains a non-positive case measurement");
            }
            Ok(Trial {
                compile_seconds: trial.compile_seconds,
                case_seconds,
                case_cpu_seconds: trial.cpu_seconds,
                case_parallelism: (trial.cpu_seconds / case_seconds).min(workers as f64),
            })
        })
        .collect()
}

fn load_think_gaps(path: &Path) -> Result<Vec<f64>> {
    let mut submissions = Vec::new();
    let mut completions = Vec::new();
    for line in fs::read_to_string(path)?
        .lines()
        .filter(|line| !line.is_empty())
    {
        let event: serde_json::Value = serde_json::from_str(line)?;
        match event["event_type"].as_str() {
            Some("candidate.submitted") => submissions.push(event["elapsed_seconds"].as_f64()),
            Some("evaluation.completed")
                if event["payload"]["visibility"].as_str() == Some("public") =>
            {
                completions.push(event["elapsed_seconds"].as_f64())
            }
            _ => {}
        }
    }
    let submissions = submissions
        .into_iter()
        .collect::<Option<Vec<_>>>()
        .context("submission without elapsed_seconds")?;
    let completions = completions
        .into_iter()
        .collect::<Option<Vec<_>>>()
        .context("evaluation without elapsed_seconds")?;
    if submissions.len() < 2 || completions.len() + 1 < submissions.len() {
        bail!("event log has no complete public submission trajectory");
    }
    let gaps = (1..submissions.len())
        .map(|index| submissions[index] - completions[index - 1])
        .collect::<Vec<_>>();
    if gaps.iter().any(|gap| *gap < 0.0) {
        bail!("event log contains a negative post-evaluation think gap");
    }
    Ok(gaps)
}

fn begin_evaluation(runner: &mut Runner, now: f64, trial: Trial, policy: Policy) {
    let (case_work, case_parallelism) = match policy {
        Policy::OneCpuLanes => (trial.case_seconds, 1.0),
        Policy::SharedParallelEvaluators => (trial.case_cpu_seconds, trial.case_parallelism),
    };
    runner.evaluation = Some(Evaluation {
        started: now,
        phase_started: now,
        phase: Phase::Compile,
        remaining_work: trial.compile_seconds,
        maximum_cpus: 1.0,
        case_work,
        case_parallelism,
        isolated_case_seconds: trial.case_seconds,
        case_overlapped: false,
    });
}

fn simulate(
    random: &mut SplitMix64,
    policy: Policy,
    trials: &[Trial],
    think_gaps: &[f64],
    options: &Options<'_>,
) -> Repetition {
    let mean_cycle = mean(think_gaps)
        + trials
            .iter()
            .map(|trial| trial.compile_seconds + trial.case_seconds)
            .sum::<f64>()
            / trials.len() as f64;
    let mut runners = (0..options.runs)
        .map(|_| Runner {
            wake_at: random.unit() * mean_cycle,
            evaluation: None,
            completed: 0,
        })
        .collect::<Vec<_>>();
    let mut now = 0.0;
    let mut latencies = Vec::new();
    let mut slowdowns = Vec::new();
    let mut overlap_count = 0;
    let mut peak_evaluations = 0;
    let mut peak_case_phases = 0;
    let mut used_core_seconds = 0.0;

    while now < options.horizon_seconds {
        for runner in &mut runners {
            if runner.evaluation.is_none() && runner.wake_at <= now + 1e-9 {
                begin_evaluation(runner, now, trials[random.index(trials.len())], policy);
            }
        }
        let active = runners
            .iter()
            .enumerate()
            .filter_map(|(index, runner)| runner.evaluation.as_ref().map(|_| index))
            .collect::<Vec<_>>();
        peak_evaluations = peak_evaluations.max(active.len());
        let case_indices = active
            .iter()
            .copied()
            .filter(|index| {
                runners[*index]
                    .evaluation
                    .as_ref()
                    .is_some_and(|evaluation| evaluation.phase == Phase::Cases)
            })
            .collect::<Vec<_>>();
        peak_case_phases = peak_case_phases.max(case_indices.len());
        if case_indices.len() > 1 {
            for index in case_indices {
                runners[index].evaluation.as_mut().unwrap().case_overlapped = true;
            }
        }
        if active.is_empty() {
            now = runners
                .iter()
                .map(|runner| runner.wake_at)
                .fold(options.horizon_seconds, f64::min)
                .min(options.horizon_seconds);
            continue;
        }
        let maxima = active
            .iter()
            .map(|index| runners[*index].evaluation.as_ref().unwrap().maximum_cpus)
            .collect::<Vec<_>>();
        let allocations = allocate(&maxima, options.physical_cores as f64);
        let finish_after = active
            .iter()
            .zip(&allocations)
            .map(|(index, allocation)| {
                runners[*index].evaluation.as_ref().unwrap().remaining_work / allocation
            })
            .fold(f64::INFINITY, f64::min);
        let wake_after = runners
            .iter()
            .filter(|runner| runner.evaluation.is_none())
            .map(|runner| runner.wake_at - now)
            .fold(f64::INFINITY, f64::min);
        let step = finish_after
            .min(wake_after)
            .min(options.horizon_seconds - now);
        used_core_seconds += allocations.iter().sum::<f64>() * step;
        for (index, allocation) in active.iter().zip(&allocations) {
            let evaluation = runners[*index].evaluation.as_mut().unwrap();
            evaluation.remaining_work = (evaluation.remaining_work - allocation * step).max(0.0);
        }
        now += step;
        for index in active {
            let Some(evaluation) = runners[index].evaluation.as_mut() else {
                continue;
            };
            if evaluation.remaining_work > 1e-8 {
                continue;
            }
            if evaluation.phase == Phase::Compile {
                evaluation.phase = Phase::Cases;
                evaluation.phase_started = now;
                evaluation.remaining_work = evaluation.case_work;
                evaluation.maximum_cpus = evaluation.case_parallelism;
            } else {
                latencies.push(now - evaluation.started);
                slowdowns.push((now - evaluation.phase_started) / evaluation.isolated_case_seconds);
                overlap_count += usize::from(evaluation.case_overlapped);
                runners[index].completed += 1;
                runners[index].evaluation = None;
                runners[index].wake_at = now + think_gaps[random.index(think_gaps.len())];
            }
        }
    }
    let completed_evaluations = runners.iter().map(|runner| runner.completed).sum();
    Repetition {
        completed_evaluations,
        completed_per_run: runners.iter().map(|runner| runner.completed).collect(),
        feedback_latency: distribution(latencies),
        case_slowdown: distribution(slowdowns),
        overlap_fraction: overlap_count as f64 / completed_evaluations as f64,
        cpu_utilization: used_core_seconds
            / (options.physical_cores as f64 * options.horizon_seconds),
        peak_concurrent_evaluations: peak_evaluations,
        peak_concurrent_case_phases: peak_case_phases,
    }
}

fn allocate(maxima: &[f64], cores: f64) -> Vec<f64> {
    let mut order = (0..maxima.len()).collect::<Vec<_>>();
    order.sort_by(|left, right| maxima[*left].total_cmp(&maxima[*right]));
    let mut allocations = vec![0.0; maxima.len()];
    let mut capacity = cores;
    for (position, index) in order.iter().enumerate() {
        let remaining = order.len() - position;
        let share = capacity / remaining as f64;
        if maxima[*index] >= share {
            for remaining_index in &order[position..] {
                allocations[*remaining_index] = share;
            }
            break;
        }
        allocations[*index] = maxima[*index];
        capacity -= maxima[*index];
    }
    allocations
}

fn aggregate(repetitions: &[Repetition], run_count: usize) -> Aggregate {
    let totals = repetitions
        .iter()
        .map(|repetition| repetition.completed_evaluations as f64)
        .collect::<Vec<_>>();
    let mean_total = mean(&totals);
    Aggregate {
        completed_evaluations: distribution(totals),
        completed_per_run: distribution(
            repetitions
                .iter()
                .flat_map(|repetition| {
                    repetition
                        .completed_per_run
                        .iter()
                        .map(|value| *value as f64)
                })
                .collect(),
        ),
        mean_completed_per_run: mean_total / run_count as f64,
        mean_feedback_latency_seconds: distribution(
            repetitions
                .iter()
                .map(|value| value.feedback_latency.mean)
                .collect(),
        ),
        median_feedback_latency_seconds: distribution(
            repetitions
                .iter()
                .map(|value| value.feedback_latency.median)
                .collect(),
        ),
        p95_feedback_latency_seconds: distribution(
            repetitions
                .iter()
                .map(|value| value.feedback_latency.p95)
                .collect(),
        ),
        maximum_feedback_latency_seconds: distribution(
            repetitions
                .iter()
                .map(|value| value.feedback_latency.maximum)
                .collect(),
        ),
        mean_case_slowdown: distribution(
            repetitions
                .iter()
                .map(|value| value.case_slowdown.mean)
                .collect(),
        ),
        p95_case_slowdown: distribution(
            repetitions
                .iter()
                .map(|value| value.case_slowdown.p95)
                .collect(),
        ),
        overlap_fraction: distribution(
            repetitions
                .iter()
                .map(|value| value.overlap_fraction)
                .collect(),
        ),
        cpu_utilization: distribution(
            repetitions
                .iter()
                .map(|value| value.cpu_utilization)
                .collect(),
        ),
        peak_concurrent_evaluations: distribution(
            repetitions
                .iter()
                .map(|value| value.peak_concurrent_evaluations as f64)
                .collect(),
        ),
        peak_concurrent_case_phases: distribution(
            repetitions
                .iter()
                .map(|value| value.peak_concurrent_case_phases as f64)
                .collect(),
        ),
    }
}

fn mean(values: &[f64]) -> f64 {
    values.iter().sum::<f64>() / values.len() as f64
}

fn distribution(mut values: Vec<f64>) -> Distribution {
    values.sort_by(f64::total_cmp);
    let average = mean(&values);
    let variance = if values.len() > 1 {
        values
            .iter()
            .map(|value| (value - average).powi(2))
            .sum::<f64>()
            / (values.len() - 1) as f64
    } else {
        0.0
    };
    Distribution {
        minimum: values[0],
        p05: percentile(&values, 0.05),
        median: percentile(&values, 0.5),
        mean: average,
        p95: percentile(&values, 0.95),
        maximum: values[values.len() - 1],
        standard_deviation: variance.sqrt(),
    }
}

fn percentile(values: &[f64], fraction: f64) -> f64 {
    let position = fraction * (values.len() - 1) as f64;
    let lower = position.floor() as usize;
    let upper = position.ceil() as usize;
    let weight = position - lower as f64;
    values[lower] * (1.0 - weight) + values[upper] * weight
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allocation_respects_capacity_and_phase_caps() {
        let allocations = allocate(&[1.0, 4.0, 16.0], 8.0);
        assert_eq!(allocations, vec![1.0, 3.5, 3.5]);
        assert!((allocations.iter().sum::<f64>() - 8.0).abs() < 1e-12);
    }

    #[test]
    fn splitmix_is_repeatable() {
        let mut left = SplitMix64::new(42);
        let mut right = SplitMix64::new(42);
        assert_eq!(left.next_u64(), right.next_u64());
        assert_eq!(left.next_u64(), right.next_u64());
    }
}
