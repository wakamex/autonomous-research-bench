use std::fs;
use std::io::{Read, Write};
use std::os::unix::fs::{FileTypeExt, PermissionsExt};
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::thread;
use std::time::Duration;

use anyhow::{Context, Result, bail};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use tempfile::Builder;

use crate::evaluator::Sandbox;
use crate::events;
use crate::events::InitOptions;
use crate::problem::Package;

const MAX_REQUEST_BYTES: u64 = 16 * 1024 * 1024;

#[derive(Serialize, Deserialize)]
struct SubmitRequest {
    source_name: String,
    source: String,
}

#[derive(Serialize, Deserialize)]
struct SubmitResponse {
    result: Option<Value>,
    error: Option<String>,
}

pub fn submit(socket: &Path, source: &Path) -> Result<Value> {
    let source_name = source
        .file_name()
        .and_then(|value| value.to_str())
        .context("submission source has no UTF-8 filename")?;
    let request = SubmitRequest {
        source_name: source_name.into(),
        source: fs::read_to_string(source).context("submission source is not UTF-8")?,
    };
    let encoded = serde_json::to_vec(&request)?;
    if encoded.len() as u64 > MAX_REQUEST_BYTES {
        bail!("submission request exceeds {MAX_REQUEST_BYTES} bytes");
    }
    let mut stream = UnixStream::connect(socket)
        .with_context(|| format!("connect to submission broker {}", socket.display()))?;
    stream.write_all(&encoded)?;
    stream.shutdown(std::net::Shutdown::Write)?;
    let mut response = Vec::new();
    stream.take(MAX_REQUEST_BYTES).read_to_end(&mut response)?;
    let response: SubmitResponse =
        serde_json::from_slice(&response).context("invalid response from submission broker")?;
    match (response.result, response.error) {
        (Some(result), None) => Ok(result),
        (None, Some(error)) => bail!("submission broker: {error}"),
        _ => bail!("submission broker returned an inconsistent response"),
    }
}

pub fn serve(
    socket: &Path,
    run_dir: &Path,
    package: &Package,
    sandbox: Sandbox,
    max_submissions: Option<usize>,
) -> Result<()> {
    if max_submissions == Some(0) {
        bail!("max submissions must be positive");
    }
    if socket.exists() {
        let metadata = fs::symlink_metadata(socket)?;
        if !metadata.file_type().is_socket() || UnixStream::connect(socket).is_ok() {
            bail!("submission socket already exists: {}", socket.display());
        }
        fs::remove_file(socket)
            .with_context(|| format!("remove stale submission socket {}", socket.display()))?;
    }
    if let Some(parent) = socket.parent() {
        fs::create_dir_all(parent)?;
    }
    let listener = UnixListener::bind(socket)
        .with_context(|| format!("bind submission socket {}", socket.display()))?;
    fs::set_permissions(socket, fs::Permissions::from_mode(0o600))?;
    let _socket = SocketGuard(socket.to_owned());
    for (handled, stream) in listener.incoming().enumerate() {
        let mut stream = stream?;
        let response = match handle(&mut stream, run_dir, package, sandbox) {
            Ok(result) => SubmitResponse {
                result: Some(result),
                error: None,
            },
            Err(error) => SubmitResponse {
                result: None,
                error: Some(format!("{error:#}")),
            },
        };
        serde_json::to_writer(&mut stream, &response)?;
        stream.flush()?;
        if max_submissions.is_some_and(|maximum| handled + 1 >= maximum) {
            break;
        }
    }
    Ok(())
}

pub struct PreflightOptions<'a> {
    pub workspace: &'a Path,
    pub source: &'a Path,
    pub package: &'a Package,
    pub public_cases: &'a Path,
    pub evaluator_cpu: usize,
    pub sandbox: Sandbox,
}

pub fn preflight(options: PreflightOptions<'_>) -> Result<Value> {
    if options.source.is_absolute()
        || options
            .source
            .components()
            .any(|part| matches!(part, std::path::Component::ParentDir))
    {
        bail!("preflight source must be relative to the workspace");
    }
    let workspace = options.workspace.canonicalize()?;
    let source = workspace.join(options.source);
    if !source.is_file() {
        bail!("preflight source does not exist: {}", source.display());
    }
    let temporary = tempfile::tempdir()?;
    let run_dir = temporary.path().join("run");
    events::init_run(InitOptions {
        run_dir: &run_dir,
        run_id: "submission-preflight",
        duration: 3600.0,
        suite_id: "submission-preflight",
        track: "single-model",
        model: "preflight",
        system_manifest_sha256: "preflight",
        package: options.package,
        public_cases: options.public_cases,
        evaluator_cpu: options.evaluator_cpu,
    })?;

    let socket_name = format!(".arb-preflight-{}.sock", std::process::id());
    let socket = workspace.join(&socket_name);
    let executable = std::env::current_exe()?;
    let mut broker = Command::new(&executable)
        .args(["submission-broker", "--socket"])
        .arg(&socket)
        .arg("--run-dir")
        .arg(&run_dir)
        .arg("--problem")
        .arg(&options.package.root)
        .arg("--sandbox")
        .arg(options.sandbox.as_str())
        .args(["--max-submissions", "1"])
        .spawn()?;
    for _ in 0..200 {
        if socket.exists() {
            break;
        }
        if broker.try_wait()?.is_some() {
            bail!("submission broker exited before creating its socket");
        }
        thread::sleep(Duration::from_millis(10));
    }
    if !socket.exists() {
        let _ = broker.kill();
        let _ = broker.wait();
        bail!("submission broker did not create its socket");
    }

    let guest_source = workspace.join(options.source);
    let guest_socket = socket.clone();
    let output = Command::new("bwrap")
        .args([
            "--die-with-parent",
            "--unshare-net",
            "--ro-bind",
            "/",
            "/",
            "--bind",
        ])
        .arg(&workspace)
        .arg(&workspace)
        .arg("--chdir")
        .arg(&workspace)
        .args(["--setenv", "ARB_SUBMIT_SOCKET"])
        .arg(&guest_socket)
        .arg(&executable)
        .arg("submit")
        .arg("--source")
        .arg(&guest_source)
        .output()?;
    if !output.status.success() {
        let _ = broker.kill();
        let _ = broker.wait();
        bail!(
            "sandboxed starter submission failed with {}\n{}",
            output.status,
            String::from_utf8_lossy(&output.stderr)
        );
    }
    let status = broker.wait()?;
    if !status.success() {
        bail!("submission broker exited with {status}");
    }
    let result: Value = serde_json::from_slice(&output.stdout)?;
    let recorded = events::read_events(&run_dir.join("events.jsonl"))?;
    let submitted = recorded
        .iter()
        .filter(|event| event.event_type == "candidate.submitted")
        .count();
    let evaluated = recorded
        .iter()
        .filter(|event| event.event_type == "evaluation.completed")
        .count();
    if submitted != 1 || evaluated != 1 || result["judge_result"] != "ACCEPTED" {
        bail!("starter preflight did not record one accepted evaluation");
    }
    Ok(result)
}

fn handle(
    stream: &mut UnixStream,
    run_dir: &Path,
    package: &Package,
    sandbox: Sandbox,
) -> Result<Value> {
    let mut encoded = Vec::new();
    stream
        .take(MAX_REQUEST_BYTES + 1)
        .read_to_end(&mut encoded)?;
    if encoded.len() as u64 > MAX_REQUEST_BYTES {
        bail!("submission request exceeds {MAX_REQUEST_BYTES} bytes");
    }
    let request: SubmitRequest = serde_json::from_slice(&encoded)?;
    let extension = Path::new(&request.source_name)
        .extension()
        .and_then(|value| value.to_str())
        .filter(|value| {
            value
                .chars()
                .all(|character| character.is_ascii_alphanumeric())
        })
        .unwrap_or("txt");
    let mut source = Builder::new().suffix(&format!(".{extension}")).tempfile()?;
    source.write_all(request.source.as_bytes())?;
    source.flush()?;
    events::submit(run_dir, package, source.path(), 1, sandbox)
}

struct SocketGuard(PathBuf);

impl Drop for SocketGuard {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}
