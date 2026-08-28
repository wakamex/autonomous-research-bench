use std::fs::{self, File, OpenOptions};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};

use anyhow::{Context, Result, bail};
use chrono::Utc;
use fs2::FileExt;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};

use crate::evaluator::{Sandbox, evaluate};
use crate::problem::{Package, file_sha256};
use crate::reference::historical_rank;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Event {
    pub schema_version: String,
    pub run_id: String,
    pub sequence: usize,
    pub event_type: String,
    pub elapsed_seconds: f64,
    pub recorded_at: String,
    pub payload: Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct State {
    pub run_id: String,
    pub started_at: String,
    pub started_monotonic: f64,
    pub duration_seconds: f64,
    pub problem_id: String,
    pub problem_package_sha256: String,
    pub public_cases: PathBuf,
    pub evaluator_cpu: usize,
    pub public_evaluator_cpu_seconds: f64,
    pub private_evaluator_cpu_seconds: f64,
    pub model_calls: u64,
    pub input_tokens: u64,
    pub output_tokens: u64,
    pub cached_tokens: u64,
    pub cost_usd: f64,
}

pub struct RunLock {
    _file: File,
}

impl RunLock {
    pub fn acquire(run_dir: &Path) -> Result<Self> {
        fs::create_dir_all(run_dir)?;
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(run_dir.join("events.lock"))?;
        file.lock_exclusive()?;
        Ok(Self { _file: file })
    }
}

pub struct InitOptions<'a> {
    pub run_dir: &'a Path,
    pub run_id: &'a str,
    pub duration: f64,
    pub suite_id: &'a str,
    pub track: &'a str,
    pub model: &'a str,
    pub system_manifest_sha256: &'a str,
    pub package: &'a Package,
    pub public_cases: &'a Path,
    pub evaluator_cpu: usize,
}

pub fn init_run(options: InitOptions<'_>) -> Result<()> {
    if options.run_dir.exists() {
        bail!(
            "run directory already exists: {}",
            options.run_dir.display()
        );
    }
    fs::create_dir_all(options.run_dir.join("candidates"))?;
    let state = State {
        run_id: options.run_id.into(),
        started_at: Utc::now().to_rfc3339(),
        started_monotonic: monotonic(),
        duration_seconds: options.duration,
        problem_id: options.package.problem.id.clone(),
        problem_package_sha256: options.package.manifest_hash.clone(),
        public_cases: options.public_cases.canonicalize()?,
        evaluator_cpu: options.evaluator_cpu,
        public_evaluator_cpu_seconds: 0.0,
        private_evaluator_cpu_seconds: 0.0,
        model_calls: 0,
        input_tokens: 0,
        output_tokens: 0,
        cached_tokens: 0,
        cost_usd: 0.0,
    };
    write_state(options.run_dir, &state)?;
    let _lock = RunLock::acquire(options.run_dir)?;
    append_event(
        options.run_dir,
        &state,
        0.0,
        "run.started",
        json!({
            "problem_id": options.package.problem.id, "suite_id": options.suite_id,
            "track": options.track, "system_manifest_sha256": options.system_manifest_sha256,
            "evaluator_cpu": options.evaluator_cpu, "evaluator_workers": 1,
        }),
    )?;
    append_event(
        options.run_dir,
        &state,
        0.0,
        "model_call.started",
        json!({
            "call_id": "aop-agent", "model": options.model,
        }),
    )?;
    Ok(())
}

pub fn submit(
    run_dir: &Path,
    package: &Package,
    source: &Path,
    workers: usize,
    sandbox: Sandbox,
) -> Result<Value> {
    let _lock = RunLock::acquire(run_dir)?;
    let mut state = read_state(run_dir)?;
    verify_package(&state, package)?;
    let submitted_at = elapsed(&state);
    if submitted_at > state.duration_seconds {
        bail!("run deadline has passed");
    }
    let events = read_events(&run_dir.join("events.jsonl"))?;
    if events.iter().any(|event| event.event_type == "run.stopped") {
        bail!("run has already stopped");
    }
    let number = events
        .iter()
        .filter(|event| event.event_type == "candidate.submitted")
        .count()
        + 1;
    let candidate_id = format!("candidate-{number:04}");
    let extension = source
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("txt");
    let archived = run_dir
        .join("candidates")
        .join(format!("{candidate_id}.{extension}"));
    fs::copy(source, &archived)?;
    append_event(
        run_dir,
        &state,
        submitted_at,
        "candidate.submitted",
        json!({
            "candidate_id": candidate_id, "source_sha256": file_sha256(&archived)?,
            "language": package.problem.compile.language,
        }),
    )?;
    let evaluation = evaluate(package, &state.public_cases, &archived, workers, sandbox)?;
    let timing = timing(&state);
    let payload = evaluation_payload(
        &candidate_id,
        "public",
        &evaluation,
        timing.0,
        timing.1,
        None,
    )?;
    append_event(
        run_dir,
        &state,
        timing.0,
        "evaluation.completed",
        payload.clone(),
    )?;
    state.public_evaluator_cpu_seconds += evaluation.cpu_seconds;
    write_state(run_dir, &state)?;
    append_resource(run_dir, &state)?;
    Ok(json!({
        "candidate_id": candidate_id,
        "elapsed_seconds": timing.0,
        "remaining_seconds": timing.1,
        "judge_result": evaluation.judge_result,
        "score": evaluation.score,
        "wall_seconds": evaluation.wall_seconds,
        "cpu_seconds": evaluation.cpu_seconds,
        "compile_seconds": evaluation.compile_seconds,
        "score_basis": format!("native ARS {}-case sum", evaluation.cases),
    }))
}

pub fn private_evaluate(
    run_dir: &Path,
    package: &Package,
    cases: &Path,
    workers: usize,
    sandbox: Sandbox,
) -> Result<Vec<Value>> {
    let case_metadata: Value = serde_json::from_slice(&fs::read(cases.join("cases.json"))?)?;
    let case_count = case_metadata["cases"]
        .as_u64()
        .context("private case bundle has no case count")? as usize;
    if case_count != package.problem.official_case_count {
        bail!(
            "private case bundle contains {case_count} cases, but {} requires {} for human-score comparability",
            package.problem.id,
            package.problem.official_case_count
        );
    }
    let events = read_events(&run_dir.join("events.jsonl"))?;
    if !events.iter().any(|event| event.event_type == "run.stopped") {
        bail!("private evaluation is only allowed after the run stops");
    }
    let existing = events
        .iter()
        .filter(|event| event.event_type == "evaluation.completed")
        .filter_map(|event| {
            (event.payload.get("visibility")?.as_str()? == "private-post-run")
                .then(|| {
                    event
                        .payload
                        .get("candidate_id")?
                        .as_str()
                        .map(str::to_owned)
                })
                .flatten()
        })
        .collect::<std::collections::HashSet<_>>();
    let mut candidates = fs::read_dir(run_dir.join("candidates"))?
        .filter_map(|entry| entry.ok().map(|entry| entry.path()))
        .collect::<Vec<_>>();
    candidates.sort();
    let mut outputs = Vec::new();
    for candidate in candidates {
        let candidate_id = candidate
            .file_stem()
            .unwrap()
            .to_string_lossy()
            .into_owned();
        if existing.contains(&candidate_id) {
            continue;
        }
        let evaluation = evaluate(package, cases, &candidate, workers, sandbox)?;
        let rank = if package.problem.reference.is_some() {
            evaluation
                .score
                .map(|score| historical_rank(package, score))
                .transpose()?
        } else {
            None
        };
        let _lock = RunLock::acquire(run_dir)?;
        let mut state = read_state(run_dir)?;
        verify_package(&state, package)?;
        let payload = evaluation_payload(
            &candidate_id,
            "private-post-run",
            &evaluation,
            0.0,
            0.0,
            rank,
        )?;
        append_event(
            run_dir,
            &state,
            elapsed(&state),
            "evaluation.completed",
            payload.clone(),
        )?;
        state.private_evaluator_cpu_seconds += evaluation.cpu_seconds;
        write_state(run_dir, &state)?;
        append_resource(run_dir, &state)?;
        outputs.push(payload);
    }
    Ok(outputs)
}

pub fn stop_run(run_dir: &Path, aop_results: &[PathBuf]) -> Result<()> {
    if aop_results.is_empty() {
        bail!("at least one AOP result is required");
    }
    let values = aop_results
        .iter()
        .map(|path| -> Result<Value> { Ok(serde_json::from_slice(&fs::read(path)?)?) })
        .collect::<Result<Vec<_>>>()?;
    let final_result = values.last().unwrap();
    let total = |field: &str| {
        values
            .iter()
            .map(|value| {
                value
                    .get("usage")
                    .and_then(|usage| usage.get(field))
                    .and_then(Value::as_u64)
                    .unwrap_or(0)
            })
            .sum()
    };
    let cost = values
        .iter()
        .map(|value| {
            value
                .get("api_equivalent_cost")
                .and_then(|cost| cost.get("amount_usd"))
                .and_then(Value::as_f64)
                .unwrap_or(0.0)
        })
        .sum();
    let (status, reason) = if final_result
        .get("timed_out")
        .and_then(Value::as_bool)
        .unwrap_or(false)
    {
        ("cancelled", "deadline")
    } else if final_result
        .get("succeeded")
        .and_then(Value::as_bool)
        .unwrap_or(false)
    {
        ("completed", "completed")
    } else {
        ("failed", "failed")
    };
    let _lock = RunLock::acquire(run_dir)?;
    let mut state = read_state(run_dir)?;
    if read_events(&run_dir.join("events.jsonl"))?
        .iter()
        .any(|event| event.event_type == "run.stopped")
    {
        bail!("run has already stopped");
    }
    state.model_calls = values.len() as u64;
    state.input_tokens = total("input_tokens");
    state.output_tokens = total("output_tokens");
    state.cached_tokens = total("cached_input_tokens");
    state.cost_usd = cost;
    write_state(run_dir, &state)?;
    append_event(
        run_dir,
        &state,
        elapsed(&state),
        "model_call.completed",
        json!({
            "call_id": "aop-agent", "status": status, "input_tokens": state.input_tokens,
            "output_tokens": state.output_tokens, "cached_tokens": state.cached_tokens, "cost_usd": state.cost_usd,
        }),
    )?;
    append_resource(run_dir, &state)?;
    append_event(
        run_dir,
        &state,
        elapsed(&state),
        "run.stopped",
        json!({"reason": reason}),
    )?;
    Ok(())
}

pub fn continuation_prompt(run_dir: &Path) -> Result<String> {
    let state = read_state(run_dir)?;
    Ok(format!(
        "Continue working on the task. Time remaining: {} seconds.",
        with_commas(timing(&state).1.floor() as u64)
    ))
}

pub fn read_events(path: &Path) -> Result<Vec<Event>> {
    if !path.exists() {
        return Ok(Vec::new());
    }
    BufReader::new(File::open(path)?)
        .lines()
        .enumerate()
        .map(|(index, line)| {
            serde_json::from_str(&line?)
                .with_context(|| format!("invalid event line {}", index + 1))
        })
        .collect()
}

pub fn read_state(run_dir: &Path) -> Result<State> {
    Ok(serde_json::from_slice(&fs::read(
        run_dir.join("state.json"),
    )?)?)
}

fn write_state(run_dir: &Path, state: &State) -> Result<()> {
    let temporary = run_dir.join(format!(".state.{}.tmp", std::process::id()));
    fs::write(&temporary, serde_json::to_vec_pretty(state)?)?;
    fs::rename(temporary, run_dir.join("state.json"))?;
    Ok(())
}

fn append_event(
    run_dir: &Path,
    state: &State,
    elapsed: f64,
    event_type: &str,
    payload: Value,
) -> Result<()> {
    let path = run_dir.join("events.jsonl");
    let sequence = read_events(&path)?.len();
    let event = Event {
        schema_version: "ars.run-event.v1".into(),
        run_id: state.run_id.clone(),
        sequence,
        event_type: event_type.into(),
        elapsed_seconds: elapsed.max(0.0),
        recorded_at: Utc::now().to_rfc3339(),
        payload,
    };
    let mut file = OpenOptions::new().create(true).append(true).open(path)?;
    serde_json::to_writer(&mut file, &event)?;
    file.write_all(b"\n")?;
    file.sync_data()?;
    Ok(())
}

fn append_resource(run_dir: &Path, state: &State) -> Result<()> {
    append_event(
        run_dir,
        state,
        elapsed(state),
        "resource.updated",
        json!({
            "model_calls": state.model_calls, "input_tokens": state.input_tokens,
            "output_tokens": state.output_tokens, "cached_tokens": state.cached_tokens,
            "cost_usd": state.cost_usd,
            "evaluator_cpu_seconds": state.public_evaluator_cpu_seconds + state.private_evaluator_cpu_seconds,
        }),
    )
}

fn evaluation_payload(
    candidate_id: &str,
    visibility: &str,
    evaluation: &crate::evaluator::Evaluation,
    elapsed_seconds: f64,
    remaining_seconds: f64,
    rank: Option<(u64, i64)>,
) -> Result<Value> {
    Ok(json!({
        "evaluation_id": format!("{}-{candidate_id}", if visibility == "public" { "public" } else { "private" }),
        "candidate_id": candidate_id, "visibility": visibility,
        "judge_result": evaluation.judge_result, "score": evaluation.score,
        "wall_seconds": evaluation.wall_seconds, "cpu_seconds": evaluation.cpu_seconds,
        "compile_seconds": evaluation.compile_seconds,
        "cases": evaluation.cases,
        "workers": evaluation.workers,
        "sandbox": evaluation.sandbox,
        "elapsed_seconds": elapsed_seconds, "remaining_seconds": remaining_seconds,
        "rank": rank.map(|value| value.0), "performance": rank.map(|value| value.1),
        "score_basis": format!("native ARS {}-case sum", evaluation.cases),
        "source_sha256": evaluation.source_sha256,
        "candidate_binary_sha256": evaluation.candidate_binary_sha256,
        "tester_sha256": evaluation.tester_sha256,
        "problem_package_sha256": evaluation.problem_package_sha256,
        "compiler": evaluation.compiler,
        "cpu_contract": crate::cpu::current_cpu_contract()?,
    }))
}

fn verify_package(state: &State, package: &Package) -> Result<()> {
    if state.problem_id != package.problem.id
        || state.problem_package_sha256 != package.manifest_hash
    {
        bail!("run and problem package identity differ");
    }
    Ok(())
}

fn elapsed(state: &State) -> f64 {
    (monotonic() - state.started_monotonic).max(0.0)
}
fn timing(state: &State) -> (f64, f64) {
    let elapsed = elapsed(state);
    (elapsed, (state.duration_seconds - elapsed).max(0.0))
}

fn monotonic() -> f64 {
    let mut value: libc::timespec = unsafe { std::mem::zeroed() };
    unsafe {
        libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut value);
    }
    value.tv_sec as f64 + value.tv_nsec as f64 / 1e9
}

fn with_commas(value: u64) -> String {
    let digits = value.to_string();
    let mut output = String::new();
    for (index, character) in digits.chars().enumerate() {
        if index > 0 && (digits.len() - index).is_multiple_of(3) {
            output.push(',');
        }
        output.push(character);
    }
    output
}

#[cfg(test)]
mod tests {
    use super::with_commas;

    #[test]
    fn formats_deadline_seconds() {
        assert_eq!(with_commas(999), "999");
        assert_eq!(with_commas(14_400), "14,400");
    }
}
