mod broker;
mod concurrency;
mod cpu;
mod evaluator;
mod events;
mod problem;
mod reference;

use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result, bail};
use clap::{Parser, Subcommand};
use evaluator::Sandbox;
use problem::Package;

#[derive(Parser)]
#[command(
    name = "arb",
    version,
    about = "Autonomous Research Bench runner and scorer"
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    #[command(hide = true, name = "__case-exec")]
    CaseExec {
        #[arg(long)]
        binary: PathBuf,
        #[arg(long)]
        profile: PathBuf,
    },
    Prepare {
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        output: PathBuf,
        #[arg(long, conflicts_with = "public")]
        seeds: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        public: bool,
    },
    Evaluate {
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        cases: PathBuf,
        #[arg(long)]
        source: PathBuf,
        #[arg(long, default_value_t = 1)]
        workers: usize,
        #[arg(long)]
        cpu: Option<usize>,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
        #[arg(long)]
        summary: bool,
    },
    BenchmarkWorkers {
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        cases: PathBuf,
        #[arg(long)]
        source: PathBuf,
        #[arg(long, default_value_t = 3)]
        repeats: usize,
        #[arg(long, default_value_t = 128)]
        max_workers: usize,
        #[arg(long, default_value_t = 5.0)]
        clear_decrease_percent: f64,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
        #[arg(long)]
        output: PathBuf,
    },
    CpuPlan {
        #[arg(long, default_value_t = 10)]
        count: usize,
        #[arg(long, default_value_t = 2.0)]
        sample_seconds: f64,
        #[arg(long)]
        output: Option<PathBuf>,
    },
    EstimateConcurrency {
        #[arg(
            long,
            default_value = "results/discovery/ahc044-native-workers-3950x-v1/worker-sweep.json"
        )]
        worker_sweep: PathBuf,
        #[arg(
            long,
            default_value = "results/discovery/ahc044-aop-luna-low-20m-v3/events.jsonl"
        )]
        events: PathBuf,
        #[arg(long, default_value_t = 16)]
        runs: usize,
        #[arg(long, default_value_t = 16)]
        physical_cores: usize,
        #[arg(long, default_value_t = 14400.0)]
        horizon_seconds: f64,
        #[arg(long, default_value_t = 100)]
        repetitions: usize,
        #[arg(long, default_value_t = 20260828)]
        seed: u64,
        #[arg(long)]
        output: PathBuf,
    },
    Init {
        #[arg(long)]
        run_dir: PathBuf,
        #[arg(long)]
        run_id: String,
        #[arg(long)]
        duration: f64,
        #[arg(long, default_value = "sprint-9-discovery")]
        suite_id: String,
        #[arg(long, default_value = "single-model")]
        track: String,
        #[arg(long)]
        model: String,
        #[arg(long)]
        system_manifest_sha256: String,
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        public_cases: PathBuf,
        #[arg(long)]
        evaluator_cpu: usize,
    },
    Submit {
        #[arg(long)]
        run_dir: Option<PathBuf>,
        #[arg(long)]
        source: PathBuf,
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
    },
    SubmissionBroker {
        #[arg(long)]
        socket: PathBuf,
        #[arg(long)]
        run_dir: PathBuf,
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
        #[arg(long)]
        max_submissions: Option<usize>,
    },
    PreflightSubmit {
        #[arg(long)]
        workspace: PathBuf,
        #[arg(long)]
        source: PathBuf,
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        public_cases: PathBuf,
        #[arg(long)]
        evaluator_cpu: usize,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
    },
    ContinuationPrompt {
        #[arg(long)]
        run_dir: PathBuf,
    },
    Stop {
        #[arg(long)]
        run_dir: PathBuf,
        #[arg(long = "aop-result", required = true)]
        aop_results: Vec<PathBuf>,
    },
    PrivateEvaluate {
        #[arg(long)]
        run_dir: PathBuf,
        #[arg(long, default_value = "problems/ahc044")]
        problem: PathBuf,
        #[arg(long)]
        cases: PathBuf,
        #[arg(long, value_enum, default_value_t = Sandbox::Bwrap)]
        sandbox: Sandbox,
    },
    Score {
        events: PathBuf,
        #[arg(long)]
        reference: PathBuf,
        #[arg(long, default_value_t = 14400)]
        horizon: u64,
        #[arg(long)]
        output: Option<PathBuf>,
    },
    BuildHumanReference {
        #[arg(long)]
        problem_id: String,
        #[arg(long)]
        submissions: PathBuf,
        #[arg(long)]
        output: PathBuf,
        #[arg(long)]
        problem: Option<PathBuf>,
        #[arg(long, default_value_t = 0.99)]
        minimum_match_fraction: f64,
        #[arg(long, default_value_t = 14400)]
        cohort_seconds: u64,
        #[arg(
            long,
            value_delimiter = ',',
            default_value = "600,1800,3600,7200,14400"
        )]
        checkpoints: Vec<u64>,
        #[arg(long, value_delimiter = ',', default_value = "50,90,99")]
        percentiles: Vec<u64>,
        #[arg(long, value_enum, default_value_t = problem::ScoreDirection::Maximize)]
        score_direction: problem::ScoreDirection,
    },
}

fn main() -> Result<()> {
    match Cli::parse().command {
        Commands::CaseExec { binary, profile } => {
            std::process::exit(evaluator::case_exec(&binary, &profile)?);
        }
        Commands::Prepare {
            problem,
            output,
            seeds,
            public: _,
        } => {
            let package = Package::load(&problem)?;
            let seeds = match seeds {
                Some(path) => read_seeds(&path)?,
                None => package.problem.public_seeds.clone(),
            };
            evaluator::prepare_cases(&package, &seeds, &output)?;
            println!("prepared {} cases in {}", seeds.len(), output.display());
        }
        Commands::Evaluate {
            problem,
            cases,
            source,
            workers,
            cpu,
            sandbox,
            summary,
        } => {
            if let Some(cpu) = cpu {
                cpu::enforce(cpu)?;
            }
            let result =
                evaluator::evaluate(&Package::load(&problem)?, &cases, &source, workers, sandbox)?;
            if summary {
                println!(
                    "{}",
                    serde_json::to_string_pretty(&serde_json::json!({
                        "judge_result": result.judge_result, "score": result.score,
                        "wall_seconds": result.wall_seconds, "cpu_seconds": result.cpu_seconds,
                        "compile_seconds": result.compile_seconds, "cases": result.cases,
                        "workers": result.workers, "source_sha256": result.source_sha256,
                        "problem_package_sha256": result.problem_package_sha256,
                    }))?
                );
            } else {
                println!("{}", serde_json::to_string_pretty(&result)?);
            }
        }
        Commands::BenchmarkWorkers {
            problem,
            cases,
            source,
            repeats,
            max_workers,
            clear_decrease_percent,
            sandbox,
            output,
        } => benchmark_workers(BenchmarkOptions {
            problem: &problem,
            cases: &cases,
            source: &source,
            repeats,
            max_workers,
            clear_decrease_percent,
            sandbox,
            output: &output,
        })?,
        Commands::CpuPlan {
            count,
            sample_seconds,
            output,
        } => write_or_print(&cpu::plan(count, sample_seconds)?, output)?,
        Commands::EstimateConcurrency {
            worker_sweep,
            events,
            runs,
            physical_cores,
            horizon_seconds,
            repetitions,
            seed,
            output,
        } => {
            let value = concurrency::estimate(concurrency::Options {
                worker_sweep: &worker_sweep,
                events: &events,
                runs,
                physical_cores,
                horizon_seconds,
                repetitions,
                seed,
            })?;
            write_or_print(&value, Some(output))?;
            println!("{}", serde_json::to_string_pretty(&value["comparison"])?);
        }
        Commands::Init {
            run_dir,
            run_id,
            duration,
            suite_id,
            track,
            model,
            system_manifest_sha256,
            problem,
            public_cases,
            evaluator_cpu,
        } => {
            if !["controlled-model", "single-model", "unlimited"].contains(&track.as_str()) {
                bail!("invalid track {track}");
            }
            let package = Package::load(&problem)?;
            events::init_run(events::InitOptions {
                run_dir: &run_dir,
                run_id: &run_id,
                duration,
                suite_id: &suite_id,
                track: &track,
                model: &model,
                system_manifest_sha256: &system_manifest_sha256,
                package: &package,
                public_cases: &public_cases,
                evaluator_cpu,
            })?;
            println!("{}", run_dir.display());
        }
        Commands::Submit {
            run_dir,
            source,
            problem,
            sandbox,
        } => {
            let default_socket = Path::new("/workspace/.arb-submit.sock");
            let socket = std::env::var_os("ARB_SUBMIT_SOCKET")
                .map(PathBuf::from)
                .or_else(|| default_socket.exists().then(|| default_socket.to_owned()));
            let result = if let Some(socket) = socket {
                broker::submit(&socket, &source)?
            } else {
                let run_dir = run_dir
                    .context("--run-dir is required when no submission broker is available")?;
                cpu::enforce(events::read_state(&run_dir)?.evaluator_cpu)?;
                events::submit(&run_dir, &Package::load(&problem)?, &source, 1, sandbox)?
            };
            println!("{}", serde_json::to_string(&result)?);
        }
        Commands::SubmissionBroker {
            socket,
            run_dir,
            problem,
            sandbox,
            max_submissions,
        } => {
            cpu::enforce(events::read_state(&run_dir)?.evaluator_cpu)?;
            broker::serve(
                &socket,
                &run_dir,
                &Package::load(&problem)?,
                sandbox,
                max_submissions,
            )?;
        }
        Commands::PreflightSubmit {
            workspace,
            source,
            problem,
            public_cases,
            evaluator_cpu,
            sandbox,
        } => {
            let package = Package::load(&problem)?;
            println!(
                "{}",
                serde_json::to_string(&broker::preflight(broker::PreflightOptions {
                    workspace: &workspace,
                    source: &source,
                    package: &package,
                    public_cases: &public_cases,
                    evaluator_cpu,
                    sandbox,
                })?)?
            );
        }
        Commands::ContinuationPrompt { run_dir } => {
            println!("{}", events::continuation_prompt(&run_dir)?)
        }
        Commands::Stop {
            run_dir,
            aop_results,
        } => events::stop_run(&run_dir, &aop_results)?,
        Commands::PrivateEvaluate {
            run_dir,
            problem,
            cases,
            sandbox,
        } => {
            cpu::enforce(events::read_state(&run_dir)?.evaluator_cpu)?;
            for value in
                events::private_evaluate(&run_dir, &Package::load(&problem)?, &cases, 1, sandbox)?
            {
                println!("{}", serde_json::to_string(&value)?);
            }
        }
        Commands::Score {
            events,
            reference,
            horizon,
            output,
        } => {
            let value = reference::replay_score(&events, &reference, horizon)?;
            write_or_print(&value, output)?;
        }
        Commands::BuildHumanReference {
            problem_id,
            submissions,
            output,
            problem,
            minimum_match_fraction,
            cohort_seconds,
            checkpoints,
            percentiles,
            score_direction,
        } => {
            let package = problem.as_deref().map(Package::load).transpose()?;
            reference::build_human_reference(reference::HumanReferenceOptions {
                problem_id: &problem_id,
                submissions: &submissions,
                output: &output,
                cohort_seconds,
                checkpoints: &checkpoints,
                percentiles: &percentiles,
                score_direction,
                package: package.as_ref(),
                minimum_match_fraction,
            })?;
            println!("wrote {}", output.display());
        }
    }
    Ok(())
}

struct BenchmarkOptions<'a> {
    problem: &'a Path,
    cases: &'a Path,
    source: &'a Path,
    repeats: usize,
    max_workers: usize,
    clear_decrease_percent: f64,
    sandbox: Sandbox,
    output: &'a Path,
}

fn benchmark_workers(options: BenchmarkOptions<'_>) -> Result<()> {
    let BenchmarkOptions {
        problem,
        cases,
        source,
        repeats,
        max_workers,
        clear_decrease_percent,
        sandbox,
        output,
    } = options;
    if repeats == 0 || max_workers == 0 || !max_workers.is_power_of_two() {
        bail!("repeats must be positive and max-workers must be a power of two");
    }
    let package = Package::load(problem)?;
    let mut workers = 1;
    let mut best_wall = f64::INFINITY;
    let mut selected_workers = 1;
    let mut expected_score = None;
    let mut measurements = Vec::new();
    let mut stop_reason = "max_workers_reached";
    let case_count = package.problem.public_seeds.len();
    while workers <= max_workers {
        let mut trials = Vec::new();
        for repeat in 1..=repeats {
            let result = evaluator::evaluate(&package, cases, source, workers, sandbox)?;
            if result.judge_result != "ACCEPTED" {
                bail!("workers={workers} repeat={repeat}: {}", result.judge_result);
            }
            if expected_score.is_some_and(|score| Some(score) != result.score) {
                bail!("score changed during worker sweep");
            }
            expected_score = result.score;
            let trial = serde_json::json!({"repeat": repeat, "wall_seconds": result.wall_seconds,
                "cpu_seconds": result.cpu_seconds, "compile_seconds": result.compile_seconds,
                "score": result.score});
            println!(
                "{}",
                serde_json::to_string(&serde_json::json!({"workers": workers, "trial": trial}))?
            );
            trials.push(trial);
        }
        let mut walls = trials
            .iter()
            .map(|trial| trial["wall_seconds"].as_f64().unwrap())
            .collect::<Vec<_>>();
        walls.sort_by(f64::total_cmp);
        let median = walls[walls.len() / 2];
        measurements.push(serde_json::json!({"workers": workers, "trials": trials,
            "median_wall_seconds": median, "cases_per_second": package.problem.public_seeds.len() as f64 / median}));
        if median >= best_wall * (1.0 + clear_decrease_percent / 100.0) {
            stop_reason = "clear_median_performance_decrease";
            break;
        }
        if median < best_wall {
            best_wall = median;
            selected_workers = workers;
        }
        if workers == case_count {
            stop_reason = "all_cases_concurrent";
            break;
        }
        workers = (workers * 2).min(case_count);
    }
    let cpu_model = fs::read_to_string("/proc/cpuinfo")
        .unwrap_or_default()
        .lines()
        .find_map(|line| line.strip_prefix("model name\t: "))
        .unwrap_or("unknown")
        .to_owned();
    let report = serde_json::json!({"schema_version": "arb.evaluator-worker-sweep.v2",
        "status": "discovery", "runner": "native-rust", "problem_id": package.problem.id,
        "problem_package_sha256": package.manifest_hash, "source": source,
        "source_sha256": problem::file_sha256(source)?, "cases": package.problem.public_seeds.len(),
        "repeats_per_worker_count": repeats, "worker_sequence": "powers of two",
        "clear_decrease_percent": clear_decrease_percent,
        "stop_rule": "continue through near-ties and stop when median wall time is at least clear_decrease_percent slower than the best median seen",
        "selected_workers": selected_workers, "stop_reason": stop_reason,
        "cpu_model": cpu_model, "logical_cpus": std::thread::available_parallelism()?.get(),
        "measurements": measurements});
    write_or_print(&report, Some(output.to_path_buf()))?;
    println!("selected {selected_workers} workers");
    Ok(())
}

fn read_seeds(path: &PathBuf) -> Result<Vec<u64>> {
    fs::read_to_string(path)?
        .lines()
        .filter(|line| !line.trim().is_empty())
        .map(|line| {
            line.trim()
                .parse()
                .with_context(|| format!("invalid seed {line}"))
        })
        .collect()
}

fn write_or_print(value: &serde_json::Value, output: Option<PathBuf>) -> Result<()> {
    let mut data = serde_json::to_vec_pretty(value)?;
    data.push(b'\n');
    if let Some(path) = output {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(path, data)?;
    } else {
        print!("{}", String::from_utf8(data)?);
    }
    Ok(())
}
