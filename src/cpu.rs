use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result, bail};

const PINNED_CPU_ENV: &str = "ARS_EVALUATOR_CPU";

#[derive(Debug)]
struct CpuSample {
    total: u64,
    idle: u64,
}

#[derive(Debug)]
struct Core {
    package: u32,
    core: u32,
    cpus: Vec<usize>,
}

pub fn plan(count: usize, sample_seconds: f64) -> Result<serde_json::Value> {
    if count == 0 || !sample_seconds.is_finite() || sample_seconds <= 0.0 {
        bail!("count and sample seconds must be positive");
    }
    let cores = topology()?;
    if count > cores.len() {
        bail!(
            "requested {count} lanes but only {} physical cores are online",
            cores.len()
        );
    }
    let before = cpu_samples()?;
    thread::sleep(Duration::from_secs_f64(sample_seconds));
    let after = cpu_samples()?;
    let mut usage = BTreeMap::new();
    for (cpu, left) in before {
        let right = after
            .get(&cpu)
            .with_context(|| format!("CPU {cpu} disappeared during sampling"))?;
        let total = right.total.saturating_sub(left.total);
        let idle = right.idle.saturating_sub(left.idle);
        usage.insert(
            cpu,
            if total == 0 {
                0.0
            } else {
                (total - idle) as f64 / total as f64
            },
        );
    }
    let mut ranked = cores
        .into_iter()
        .map(|core| {
            let combined = core
                .cpus
                .iter()
                .map(|cpu| usage.get(cpu).copied().unwrap_or(1.0))
                .sum::<f64>()
                / core.cpus.len() as f64;
            let selected_cpu = *core
                .cpus
                .iter()
                .min_by(|left, right| usage[left].total_cmp(&usage[right]))
                .unwrap();
            (combined, selected_cpu, core)
        })
        .collect::<Vec<_>>();
    ranked.sort_by(|left, right| {
        left.0
            .total_cmp(&right.0)
            .then_with(|| left.2.package.cmp(&right.2.package))
            .then_with(|| left.2.core.cmp(&right.2.core))
    });
    let lanes = ranked
        .into_iter()
        .take(count)
        .enumerate()
        .map(|(lane, (combined, selected_cpu, core))| {
            serde_json::json!({
                "lane": lane,
                "cpu": selected_cpu,
                "physical_package": core.package,
                "physical_core": core.core,
                "thread_siblings": core.cpus,
                "combined_utilization": combined,
                "selected_cpu_utilization": usage[&selected_cpu]
            })
        })
        .collect::<Vec<_>>();
    Ok(serde_json::json!({
        "schema_version": "ars.cpu-plan.v1",
        "default_max_concurrent_runs": 10,
        "requested_lanes": count,
        "effective_lanes": lanes.len(),
        "sample_seconds": sample_seconds,
        "selection": "lowest combined utilization across distinct physical cores",
        "lanes": lanes
    }))
}

pub fn enforce(cpu: usize) -> Result<()> {
    if let Ok(expected) = env::var(PINNED_CPU_ENV) {
        let expected: usize = expected.parse().context("invalid pinned CPU marker")?;
        if expected != cpu {
            bail!("pinned CPU marker {expected} does not match requested CPU {cpu}");
        }
        return verify(cpu);
    }
    if !online_cpus()?.contains(&cpu) {
        bail!("CPU {cpu} is not online");
    }
    let executable = env::current_exe()?.canonicalize()?;
    let working_directory = env::current_dir()?;
    let nonce = SystemTime::now().duration_since(UNIX_EPOCH)?.as_nanos();
    let unit = format!("ars-evaluator-{}-{nonce}", std::process::id());
    let mut command = Command::new("systemd-run");
    command
        .args(["--user", "--wait", "--pipe", "--collect", "--quiet"])
        .arg(format!("--unit={unit}"))
        .arg(format!(
            "--working-directory={}",
            working_directory.display()
        ))
        .arg(format!("--setenv={PINNED_CPU_ENV}={cpu}"))
        .args(["--property", &format!("CPUAffinity={cpu}")])
        .args(["--property", "CPUQuota=100%"])
        .args(["--property", "SystemCallFilter=~sched_setaffinity"])
        .arg(executable)
        .args(env::args_os().skip(1));
    let status = command.status().context("start pinned evaluator service")?;
    std::process::exit(status.code().unwrap_or(1));
}

pub fn current_cpu_contract() -> Result<Option<serde_json::Value>> {
    let Ok(value) = env::var(PINNED_CPU_ENV) else {
        return Ok(None);
    };
    let cpu: usize = value.parse().context("invalid pinned CPU marker")?;
    verify(cpu)?;
    Ok(Some(serde_json::json!({
        "cpu": cpu,
        "affinity": affinity()?,
        "cpu_quota": cpu_quota()?,
        "cgroup": cgroup_path()?
    })))
}

fn verify(cpu: usize) -> Result<()> {
    let affinity = affinity()?;
    if affinity != [cpu] {
        bail!("effective CPU affinity {affinity:?} does not match CPU {cpu}");
    }
    let group = cgroup_path()?;
    if !group
        .file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| name.starts_with("ars-evaluator-") && name.ends_with(".service"))
    {
        bail!(
            "process is not inside an ARS evaluator service: {}",
            group.display()
        );
    }
    let quota = cpu_quota()?;
    let fields = quota.split_whitespace().collect::<Vec<_>>();
    if fields.len() != 2 || fields[0] == "max" || fields[0] != fields[1] {
        bail!("evaluator cgroup does not have an exact one-CPU quota: {quota}");
    }
    Ok(())
}

fn topology() -> Result<Vec<Core>> {
    let mut cores: BTreeMap<(u32, u32), Vec<usize>> = BTreeMap::new();
    for cpu in online_cpus()? {
        let root = PathBuf::from(format!("/sys/devices/system/cpu/cpu{cpu}/topology"));
        let package = read_number(&root.join("physical_package_id"))?;
        let core = read_number(&root.join("core_id"))?;
        cores.entry((package, core)).or_default().push(cpu);
    }
    Ok(cores
        .into_iter()
        .map(|((package, core), mut cpus)| {
            cpus.sort_unstable();
            Core {
                package,
                core,
                cpus,
            }
        })
        .collect())
}

fn online_cpus() -> Result<Vec<usize>> {
    parse_cpu_list(fs::read_to_string("/sys/devices/system/cpu/online")?.trim())
}

fn parse_cpu_list(value: &str) -> Result<Vec<usize>> {
    let mut cpus = Vec::new();
    for part in value.split(',') {
        if let Some((start, end)) = part.split_once('-') {
            let start: usize = start.parse()?;
            let end: usize = end.parse()?;
            if start > end {
                bail!("invalid CPU range {part}");
            }
            cpus.extend(start..=end);
        } else {
            cpus.push(part.parse()?);
        }
    }
    Ok(cpus)
}

fn cpu_samples() -> Result<BTreeMap<usize, CpuSample>> {
    let mut samples = BTreeMap::new();
    for line in fs::read_to_string("/proc/stat")?.lines() {
        let mut fields = line.split_whitespace();
        let Some(name) = fields.next() else {
            continue;
        };
        let Some(number) = name.strip_prefix("cpu") else {
            continue;
        };
        if number.is_empty() || !number.bytes().all(|value| value.is_ascii_digit()) {
            continue;
        }
        let values = fields
            .map(str::parse::<u64>)
            .collect::<std::result::Result<Vec<_>, _>>()?;
        if values.len() < 4 {
            bail!("incomplete CPU counters for {name}");
        }
        samples.insert(
            number.parse()?,
            CpuSample {
                total: values.iter().sum(),
                idle: values[3] + values.get(4).copied().unwrap_or(0),
            },
        );
    }
    Ok(samples)
}

fn affinity() -> Result<Vec<usize>> {
    let mut set: libc::cpu_set_t = unsafe { std::mem::zeroed() };
    if unsafe { libc::sched_getaffinity(0, std::mem::size_of::<libc::cpu_set_t>(), &mut set) } != 0
    {
        return Err(std::io::Error::last_os_error().into());
    }
    Ok((0..libc::CPU_SETSIZE as usize)
        .filter(|cpu| unsafe { libc::CPU_ISSET(*cpu, &set) })
        .collect())
}

fn cgroup_path() -> Result<PathBuf> {
    let cgroup = fs::read_to_string("/proc/self/cgroup")?;
    let relative = cgroup
        .lines()
        .find_map(|line| line.strip_prefix("0::"))
        .context("process has no cgroup-v2 path")?;
    Ok(Path::new("/sys/fs/cgroup").join(relative.trim_start_matches('/')))
}

fn cpu_quota() -> Result<String> {
    Ok(fs::read_to_string(cgroup_path()?.join("cpu.max"))?
        .trim()
        .to_owned())
}

fn read_number(path: &Path) -> Result<u32> {
    Ok(fs::read_to_string(path)?.trim().parse()?)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_cpu_lists() {
        assert_eq!(parse_cpu_list("0-2,5,8-9").unwrap(), vec![0, 1, 2, 5, 8, 9]);
    }
}
