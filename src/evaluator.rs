use std::fs::{self, File};
use std::os::unix::process::{CommandExt, ExitStatusExt};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use anyhow::{Context, Result, bail};
use serde::Serialize;
use tempfile::TempDir;

use crate::problem::{Package, file_sha256};

#[derive(Debug, Clone, Copy, clap::ValueEnum)]
pub enum Sandbox {
    Bwrap,
    None,
}

impl Sandbox {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Bwrap => "bwrap",
            Self::None => "none",
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct CaseResult {
    pub case: String,
    pub judge_result: String,
    pub score: Option<u64>,
    pub wall_seconds: f64,
    pub cpu_seconds: f64,
    pub peak_memory_bytes: u64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub message: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Evaluation {
    pub judge_result: String,
    pub score: Option<u64>,
    pub wall_seconds: f64,
    pub cpu_seconds: f64,
    pub compile_seconds: f64,
    pub cases: usize,
    pub workers: usize,
    pub sandbox: String,
    pub source_sha256: String,
    pub candidate_binary_sha256: String,
    pub tester_sha256: String,
    pub compiler: String,
    pub problem_package_sha256: String,
    pub case_results: Vec<CaseResult>,
}

#[derive(Debug, serde::Deserialize)]
struct CasesMetadata {
    schema_version: String,
    problem_id: String,
    problem_package_sha256: String,
    tester_sha256: String,
    inputs_sha256: String,
    cases: usize,
}

#[derive(Debug)]
struct RunOutcome {
    status: std::process::ExitStatus,
    wall: Duration,
    cpu: Duration,
    max_rss_bytes: u64,
    timed_out: bool,
}

pub fn prepare_cases(package: &Package, seeds: &[u64], output: &Path) -> Result<()> {
    package.build_tools()?;
    fs::create_dir_all(output)?;
    if fs::read_dir(output)?.next().is_some() {
        bail!("case directory is not empty: {}", output.display());
    }
    let seeds_path = output.join("seeds.txt");
    let seed_text = seeds
        .iter()
        .map(u64::to_string)
        .collect::<Vec<_>>()
        .join("\n")
        + "\n";
    fs::write(&seeds_path, seed_text)?;
    let inputs = output.join("inputs");
    let status = Command::new(package.generator())
        .arg("--dir")
        .arg(&inputs)
        .arg(&seeds_path)
        .status()
        .context("run official generator")?;
    if !status.success() {
        bail!("official generator failed with {status}");
    }
    let generated = case_paths(&inputs)?;
    if generated.len() != seeds.len() {
        bail!(
            "generated {} cases for {} seeds",
            generated.len(),
            seeds.len()
        );
    }
    let metadata = serde_json::json!({
        "schema_version": "ars.cases.v1",
        "problem_id": package.problem.id,
        "problem_package_sha256": package.manifest_hash,
        "generator_sha256": file_sha256(&package.generator())?,
        "tester_sha256": file_sha256(&package.tester())?,
        "inputs_sha256": hash_case_files(&generated)?,
        "seeds_sha256": crate::problem::hex_sha256(fs::read(&seeds_path)?.as_slice()),
        "cases": generated.len(),
    });
    fs::write(
        output.join("cases.json"),
        serde_json::to_vec_pretty(&metadata)?,
    )?;
    Ok(())
}

pub fn evaluate(
    package: &Package,
    cases_root: &Path,
    source: &Path,
    workers: usize,
    sandbox: Sandbox,
) -> Result<Evaluation> {
    if workers == 0 {
        bail!("workers must be positive");
    }
    package.build_tools()?;
    let metadata: CasesMetadata = serde_json::from_slice(
        &fs::read(cases_root.join("cases.json"))
            .with_context(|| format!("case bundle {} has no cases.json", cases_root.display()))?,
    )?;
    if metadata.schema_version != "ars.cases.v1"
        || metadata.problem_id != package.problem.id
        || metadata.problem_package_sha256 != package.manifest_hash
        || metadata.tester_sha256 != file_sha256(&package.tester())?
    {
        bail!("case bundle identity does not match the problem package");
    }
    let inputs = cases_root.join("inputs");
    let cases = case_paths(&inputs)?;
    if cases.is_empty() {
        bail!("no numbered .txt cases in {}", inputs.display());
    }
    if cases.len() != metadata.cases {
        bail!(
            "case bundle declares {} cases but contains {}",
            metadata.cases,
            cases.len()
        );
    }
    if hash_case_files(&cases)? != metadata.inputs_sha256 {
        bail!("case bundle content hash does not match cases.json");
    }
    let work = TempDir::new().context("create evaluation directory")?;
    let binary = work.path().join("candidate");
    File::create(&binary)?;
    let (compile_outcome, compile_stderr) = compile_candidate(package, source, &binary, sandbox)?;
    let compile_seconds = compile_outcome.wall.as_secs_f64();
    if !compile_outcome.status.success() || compile_outcome.timed_out {
        let message = String::from_utf8_lossy(&compile_stderr);
        return Ok(Evaluation {
            judge_result: "COMPILATION_ERROR".into(),
            score: None,
            wall_seconds: compile_seconds,
            cpu_seconds: 0.0,
            compile_seconds,
            cases: cases.len(),
            workers,
            sandbox: sandbox.as_str().into(),
            source_sha256: file_sha256(source)?,
            candidate_binary_sha256: String::new(),
            tester_sha256: file_sha256(&package.tester())?,
            compiler: compiler_identity(&package.problem.compile.program),
            problem_package_sha256: package.manifest_hash.clone(),
            case_results: vec![CaseResult {
                case: "compile".into(),
                judge_result: "COMPILATION_ERROR".into(),
                score: None,
                wall_seconds: compile_seconds,
                cpu_seconds: 0.0,
                peak_memory_bytes: 0,
                message: Some(truncate(&message, 4096)),
            }],
        });
    }

    let started = Instant::now();
    let cases = Arc::new(cases);
    let next = Arc::new(AtomicUsize::new(0));
    let results = Arc::new(Mutex::new(vec![None; cases.len()]));
    let worker_count = workers.min(cases.len());
    thread::scope(|scope| {
        for _ in 0..worker_count {
            let cases = Arc::clone(&cases);
            let next = Arc::clone(&next);
            let results = Arc::clone(&results);
            let binary = binary.clone();
            let tester = package.tester();
            let problem = package.problem.clone();
            scope.spawn(move || {
                loop {
                    let index = next.fetch_add(1, Ordering::Relaxed);
                    if index >= cases.len() {
                        break;
                    }
                    let result = run_case(&binary, &tester, &cases[index], &problem, sandbox)
                        .unwrap_or_else(|error| CaseResult {
                            case: case_name(&cases[index]),
                            judge_result: "INTERNAL_ERROR".into(),
                            score: None,
                            wall_seconds: 0.0,
                            cpu_seconds: 0.0,
                            peak_memory_bytes: 0,
                            message: Some(format!("{error:#}")),
                        });
                    results.lock().expect("result lock")[index] = Some(result);
                }
            });
        }
    });
    let wall_seconds = started.elapsed().as_secs_f64() + compile_seconds;
    let case_results: Vec<CaseResult> = Arc::try_unwrap(results)
        .unwrap()
        .into_inner()
        .unwrap()
        .into_iter()
        .map(|result| result.expect("worker filled result"))
        .collect();
    let accepted = case_results
        .iter()
        .all(|result| result.judge_result == "ACCEPTED");
    let score = accepted.then(|| {
        case_results
            .iter()
            .map(|result| result.score.unwrap())
            .sum()
    });
    let cpu_seconds = case_results.iter().map(|result| result.cpu_seconds).sum();
    Ok(Evaluation {
        judge_result: if accepted {
            "ACCEPTED"
        } else {
            overall_failure(&case_results)
        }
        .into(),
        score,
        wall_seconds,
        cpu_seconds,
        compile_seconds,
        cases: case_results.len(),
        workers,
        sandbox: sandbox.as_str().into(),
        source_sha256: file_sha256(source)?,
        candidate_binary_sha256: file_sha256(&binary)?,
        tester_sha256: file_sha256(&package.tester())?,
        compiler: compiler_identity(&package.problem.compile.program),
        problem_package_sha256: package.manifest_hash.clone(),
        case_results,
    })
}

fn compile_candidate(
    package: &Package,
    source: &Path,
    binary: &Path,
    sandbox: Sandbox,
) -> Result<(RunOutcome, Vec<u8>)> {
    let scratch = TempDir::new()?;
    let stderr_path = scratch.path().join("compiler-stderr.txt");
    let stderr = File::create(&stderr_path)?;
    let mut command = match sandbox {
        Sandbox::None => {
            let mut command = Command::new(&package.problem.compile.program);
            command
                .args(&package.problem.compile.args)
                .arg(source)
                .arg("-o")
                .arg(binary);
            command
        }
        Sandbox::Bwrap => {
            let source = source.canonicalize()?;
            let binary = binary.canonicalize()?;
            let compiler = PathBuf::from("/usr/bin").join(&package.problem.compile.program);
            if !compiler.is_file() {
                bail!("sandbox compiler does not exist: {}", compiler.display());
            }
            let mut command = Command::new("bwrap");
            command
                .args([
                    "--die-with-parent",
                    "--unshare-net",
                    "--unshare-pid",
                    "--unshare-ipc",
                    "--unshare-uts",
                    "--new-session",
                    "--clearenv",
                    "--setenv",
                    "PATH",
                    "/usr/bin:/bin",
                    "--ro-bind",
                    "/usr",
                    "/usr",
                    "--ro-bind",
                    "/lib64",
                    "/lib64",
                    "--proc",
                    "/proc",
                    "--dev",
                    "/dev",
                    "--tmpfs",
                    "/tmp",
                    "--ro-bind",
                ])
                .arg(source)
                .arg("/source.cpp")
                .arg("--bind")
                .arg(binary)
                .arg("/candidate")
                .arg("--")
                .arg(compiler)
                .args(&package.problem.compile.args)
                .arg("/source.cpp")
                .arg("-o")
                .arg("/candidate");
            command
        }
    };
    command.stdout(std::process::Stdio::null()).stderr(stderr);
    let memory = package.problem.compile_memory_limit_bytes;
    let cpu = package.problem.compile_time_limit_seconds.ceil() as u64;
    unsafe {
        command.pre_exec(move || {
            libc::setpgid(0, 0);
            set_limit(libc::RLIMIT_AS, memory, memory)?;
            set_limit(libc::RLIMIT_CPU, cpu, cpu)?;
            set_limit(libc::RLIMIT_FSIZE, 256 * 1024 * 1024, 256 * 1024 * 1024)?;
            set_limit(libc::RLIMIT_NOFILE, 128, 128)?;
            Ok(())
        });
    }
    let child = command.spawn().context("start candidate compiler")?;
    let outcome = wait_child(
        child,
        Duration::from_secs_f64(package.problem.compile_time_limit_seconds + 0.5),
    )?;
    Ok((outcome, fs::read(stderr_path)?))
}

fn compiler_identity(program: &str) -> String {
    Command::new(program)
        .arg("--version")
        .output()
        .ok()
        .and_then(|output| String::from_utf8(output.stdout).ok())
        .and_then(|output| output.lines().next().map(str::to_owned))
        .unwrap_or_else(|| program.to_owned())
}

fn case_paths(inputs: &Path) -> Result<Vec<PathBuf>> {
    let mut paths = fs::read_dir(inputs)
        .with_context(|| format!("read cases from {}", inputs.display()))?
        .filter_map(|entry| entry.ok().map(|entry| entry.path()))
        .filter(|path| path.extension().is_some_and(|ext| ext == "txt"))
        .collect::<Vec<_>>();
    paths.sort();
    for (index, path) in paths.iter().enumerate() {
        let expected = format!("{index:04}.txt");
        if path.file_name().and_then(|name| name.to_str()) != Some(&expected) {
            bail!("expected case {expected}, found {}", path.display());
        }
    }
    Ok(paths)
}

fn run_case(
    binary: &Path,
    tester: &Path,
    input: &Path,
    problem: &crate::problem::Problem,
    sandbox: Sandbox,
) -> Result<CaseResult> {
    let scratch = TempDir::new()?;
    let output_path = scratch.path().join("output.txt");
    let stderr_path = scratch.path().join("stderr.txt");
    let profile_path = scratch.path().join("profile.txt");
    let stdin = File::open(input)?;
    let stdout = File::create(&output_path)?;
    let stderr = File::create(&stderr_path)?;
    File::create(&profile_path)?;
    let mut command = candidate_command(binary, &profile_path, sandbox)?;
    command.stdin(stdin).stdout(stdout).stderr(stderr);
    let memory = problem.memory_limit_bytes;
    let output_limit = problem.output_limit_bytes;
    let cpu_limit = problem.time_limit_seconds.ceil() as u64 + 1;
    unsafe {
        command.pre_exec(move || {
            libc::setpgid(0, 0);
            set_limit(libc::RLIMIT_AS, memory, memory)?;
            set_limit(libc::RLIMIT_FSIZE, output_limit, output_limit)?;
            set_limit(libc::RLIMIT_CPU, cpu_limit, cpu_limit)?;
            set_limit(libc::RLIMIT_NOFILE, 64, 64)?;
            Ok(())
        });
    }
    let child = command.spawn().context("start candidate")?;
    let mut outcome = wait_child(
        child,
        Duration::from_secs_f64(problem.time_limit_seconds + 0.35),
    )?;
    apply_case_profile(&mut outcome, &profile_path);
    let stderr_text = fs::read_to_string(&stderr_path).unwrap_or_default();
    let mut result = CaseResult {
        case: case_name(input),
        judge_result: "ACCEPTED".into(),
        score: None,
        wall_seconds: outcome.wall.as_secs_f64(),
        cpu_seconds: outcome.cpu.as_secs_f64(),
        peak_memory_bytes: outcome.max_rss_bytes,
        message: None,
    };
    if outcome.timed_out
        || outcome.wall.as_secs_f64() > problem.time_limit_seconds
        || outcome.cpu.as_secs_f64() > problem.time_limit_seconds
    {
        result.judge_result = "TIME_LIMIT_EXCEEDED".into();
        return Ok(result);
    }
    if !outcome.status.success() {
        result.judge_result = if outcome.status.signal() == Some(libc::SIGXFSZ) {
            "OUTPUT_LIMIT_EXCEEDED".into()
        } else if outcome.status.signal() == Some(libc::SIGKILL)
            && outcome.max_rss_bytes >= problem.memory_limit_bytes.saturating_sub(1024 * 1024)
        {
            "MEMORY_LIMIT_EXCEEDED".into()
        } else {
            "RUNTIME_ERROR".into()
        };
        result.message = (!stderr_text.is_empty()).then(|| truncate(&stderr_text, 4096));
        return Ok(result);
    }
    let judged = Command::new(tester)
        .arg(input)
        .arg(&output_path)
        .output()
        .context("run tester")?;
    let judge_stderr = String::from_utf8_lossy(&judged.stderr);
    if !judged.status.success() {
        result.judge_result = "WRONG_ANSWER".into();
        result.message = Some(truncate(&judge_stderr, 4096));
        return Ok(result);
    }
    let score = judge_stderr
        .lines()
        .rev()
        .find_map(|line| line.strip_prefix("Score = "))
        .and_then(|value| value.parse::<u64>().ok());
    match score {
        Some(score) => result.score = Some(score),
        None => {
            result.judge_result = "INTERNAL_ERROR".into();
            result.message = Some(format!(
                "tester did not emit a score: {}",
                truncate(&judge_stderr, 4096)
            ));
        }
    }
    Ok(result)
}

fn candidate_command(binary: &Path, profile: &Path, sandbox: Sandbox) -> Result<Command> {
    let runner = std::env::current_exe()?.canonicalize()?;
    match sandbox {
        Sandbox::None => {
            let mut command = Command::new(&runner);
            command
                .arg("__case-exec")
                .arg("--binary")
                .arg(binary)
                .arg("--profile")
                .arg(profile);
            Ok(command)
        }
        Sandbox::Bwrap => {
            let binary = binary.canonicalize()?;
            let profile = profile.canonicalize()?;
            let mut command = Command::new("bwrap");
            command
                .args([
                    "--die-with-parent",
                    "--unshare-net",
                    "--unshare-pid",
                    "--unshare-ipc",
                    "--unshare-uts",
                    "--new-session",
                    "--clearenv",
                    "--ro-bind",
                    "/usr",
                    "/usr",
                    "--ro-bind",
                    "/lib64",
                    "/lib64",
                    "--proc",
                    "/proc",
                    "--dev",
                    "/dev",
                    "--tmpfs",
                    "/tmp",
                    "--ro-bind",
                ])
                .arg(binary)
                .arg("/ars-candidate")
                .arg("--ro-bind")
                .arg(runner)
                .arg("/ars-runner")
                .arg("--bind")
                .arg(profile)
                .arg("/ars-profile")
                .arg("--")
                .arg("/ars-runner")
                .arg("__case-exec")
                .arg("--binary")
                .arg("/ars-candidate")
                .arg("--profile")
                .arg("/ars-profile");
            Ok(command)
        }
    }
}

fn apply_case_profile(outcome: &mut RunOutcome, path: &Path) {
    let Ok(content) = fs::read_to_string(path) else {
        return;
    };
    let Some(line) = content.lines().rev().find(|line| line.starts_with('{')) else {
        return;
    };
    let Ok(profile) = serde_json::from_str::<serde_json::Value>(line) else {
        return;
    };
    let user = profile["user_seconds"].as_f64().unwrap_or(0.0);
    let system = profile["system_seconds"].as_f64().unwrap_or(0.0);
    let rss = profile["peak_memory_bytes"].as_u64().unwrap_or(0);
    let wall = profile["wall_seconds"].as_f64();
    outcome.cpu = Duration::from_secs_f64(user + system);
    outcome.max_rss_bytes = rss;
    if let Some(wall) = wall {
        outcome.wall = Duration::from_secs_f64(wall);
    }
}

pub fn case_exec(binary: &Path, profile: &Path) -> Result<i32> {
    let mut command = Command::new(binary);
    command.stdin(std::process::Stdio::inherit());
    command.stdout(std::process::Stdio::inherit());
    command.stderr(std::process::Stdio::inherit());
    unsafe {
        command.pre_exec(|| {
            set_limit(libc::RLIMIT_NPROC, 64, 64)?;
            Ok(())
        });
    }
    let started = Instant::now();
    let child = command
        .spawn()
        .context("case executor could not start candidate")?;
    let pid = child.id() as libc::pid_t;
    let mut status = 0;
    let mut usage: libc::rusage = unsafe { std::mem::zeroed() };
    if unsafe { libc::wait4(pid, &mut status, 0, &mut usage) } != pid {
        return Err(std::io::Error::last_os_error().into());
    }
    let data = serde_json::json!({
        "user_seconds": timeval_duration(usage.ru_utime).as_secs_f64(),
        "system_seconds": timeval_duration(usage.ru_stime).as_secs_f64(),
        "peak_memory_bytes": (usage.ru_maxrss as u64) * 1024,
        "wall_seconds": started.elapsed().as_secs_f64(),
    });
    fs::write(profile, serde_json::to_vec(&data)?)?;
    let exit = std::process::ExitStatus::from_raw(status);
    Ok(exit
        .code()
        .unwrap_or_else(|| 128 + exit.signal().unwrap_or(1)))
}

fn hash_case_files(paths: &[PathBuf]) -> Result<String> {
    use sha2::{Digest, Sha256};
    let mut digest = Sha256::new();
    for path in paths {
        let name = path.file_name().context("case filename")?.to_string_lossy();
        let data = fs::read(path)?;
        digest.update(name.as_bytes());
        digest.update(b"\0");
        digest.update((data.len() as u64).to_le_bytes());
        digest.update(data);
    }
    Ok(format!("{:x}", digest.finalize()))
}

unsafe fn set_limit(
    resource: libc::__rlimit_resource_t,
    soft: u64,
    hard: u64,
) -> std::io::Result<()> {
    let limit = libc::rlimit {
        rlim_cur: soft,
        rlim_max: hard,
    };
    if unsafe { libc::setrlimit(resource, &limit) } == 0 {
        Ok(())
    } else {
        Err(std::io::Error::last_os_error())
    }
}

fn wait_child(child: std::process::Child, timeout: Duration) -> Result<RunOutcome> {
    let pid = child.id() as libc::pid_t;
    let started = Instant::now();
    loop {
        let mut status = 0;
        let mut usage: libc::rusage = unsafe { std::mem::zeroed() };
        let result = unsafe { libc::wait4(pid, &mut status, libc::WNOHANG, &mut usage) };
        if result == pid {
            let cpu = timeval_duration(usage.ru_utime) + timeval_duration(usage.ru_stime);
            return Ok(RunOutcome {
                status: std::process::ExitStatus::from_raw(status),
                wall: started.elapsed(),
                cpu,
                max_rss_bytes: (usage.ru_maxrss as u64) * 1024,
                timed_out: false,
            });
        }
        if result < 0 {
            return Err(std::io::Error::last_os_error().into());
        }
        if started.elapsed() >= timeout {
            unsafe {
                libc::kill(-pid, libc::SIGKILL);
            }
            let mut usage: libc::rusage = unsafe { std::mem::zeroed() };
            let mut status = 0;
            unsafe {
                libc::wait4(pid, &mut status, 0, &mut usage);
            }
            return Ok(RunOutcome {
                status: std::process::ExitStatus::from_raw(status),
                wall: started.elapsed(),
                cpu: timeval_duration(usage.ru_utime) + timeval_duration(usage.ru_stime),
                max_rss_bytes: (usage.ru_maxrss as u64) * 1024,
                timed_out: true,
            });
        }
        thread::sleep(Duration::from_millis(2));
    }
}

fn timeval_duration(value: libc::timeval) -> Duration {
    Duration::from_secs(value.tv_sec as u64) + Duration::from_micros(value.tv_usec as u64)
}

fn case_name(path: &Path) -> String {
    path.file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .into_owned()
}

fn truncate(value: &str, bytes: usize) -> String {
    value.chars().take(bytes).collect()
}

fn overall_failure(results: &[CaseResult]) -> &'static str {
    for status in [
        "INTERNAL_ERROR",
        "COMPILATION_ERROR",
        "MEMORY_LIMIT_EXCEEDED",
        "TIME_LIMIT_EXCEEDED",
        "OUTPUT_LIMIT_EXCEEDED",
        "RUNTIME_ERROR",
        "WRONG_ANSWER",
    ] {
        if results.iter().any(|result| result.judge_result == status) {
            return status;
        }
    }
    "INTERNAL_ERROR"
}
