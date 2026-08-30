use std::fs;
use std::path::Path;
use std::process::Command;

use serde_json::Value;

#[test]
fn evaluates_and_classifies_native_candidates() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let binary = env!("CARGO_BIN_EXE_ars");
    let temp = tempfile::tempdir().unwrap();
    let seeds = temp.path().join("seeds.txt");
    fs::write(&seeds, "0\n").unwrap();
    let cases = temp.path().join("cases");
    let status = Command::new(binary)
        .current_dir(root)
        .args(["prepare", "--problem", "problems/ahc044", "--output"])
        .arg(&cases)
        .arg("--seeds")
        .arg(&seeds)
        .status()
        .unwrap();
    assert!(status.success());

    let accepted = write_source(
        temp.path(),
        "accepted.cpp",
        r#"
        #include <iostream>
        int main() { int n, l, x; std::cin >> n >> l; for(int i=0;i<n;i++) std::cin >> x;
        for(int i=0;i<n;i++) std::cout << i << ' ' << i << '\n'; }
    "#,
    );
    assert_eq!(
        evaluate(binary, root, &cases, &accepted, "bwrap"),
        "ACCEPTED"
    );

    let isolated = write_source(
        temp.path(),
        "isolated.cpp",
        r#"
        #include <fstream>
        #include <iostream>
        int main() { if (std::ifstream("/code/ARS/README.md")) return 1;
        int n, l, x; std::cin >> n >> l; for(int i=0;i<n;i++) std::cin >> x;
        for(int i=0;i<n;i++) std::cout << i << ' ' << i << '\n'; }
    "#,
    );
    assert_eq!(
        evaluate(binary, root, &cases, &isolated, "bwrap"),
        "ACCEPTED"
    );

    let wrong = write_source(temp.path(), "wrong.cpp", "int main() {}");
    assert_eq!(
        evaluate(binary, root, &cases, &wrong, "none"),
        "WRONG_ANSWER"
    );

    let runtime = write_source(temp.path(), "runtime.cpp", "int main() { return 1; }");
    assert_eq!(
        evaluate(binary, root, &cases, &runtime, "none"),
        "RUNTIME_ERROR"
    );

    let invalid = write_source(temp.path(), "invalid.cpp", "this is not C++");
    assert_eq!(
        evaluate(binary, root, &cases, &invalid, "none"),
        "COMPILATION_ERROR"
    );

    let timeout = write_source(temp.path(), "timeout.cpp", "int main() { for (;;) {} }");
    assert_eq!(
        evaluate(binary, root, &cases, &timeout, "none"),
        "TIME_LIMIT_EXCEEDED"
    );
}

#[test]
fn broker_records_a_submission_from_a_read_only_outer_filesystem() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let binary = env!("CARGO_BIN_EXE_ars");
    let temp = tempfile::tempdir().unwrap();
    let seeds = temp.path().join("seeds.txt");
    fs::write(&seeds, "0\n").unwrap();
    let cases = temp.path().join("cases");
    let prepared = Command::new(binary)
        .current_dir(root)
        .args(["prepare", "--problem", "problems/ahc044", "--output"])
        .arg(&cases)
        .arg("--seeds")
        .arg(&seeds)
        .status()
        .unwrap();
    assert!(prepared.success());

    let workspace = temp.path().join("workspace");
    fs::create_dir(&workspace).unwrap();
    write_source(
        &workspace,
        "solution.cpp",
        r#"
        #include <iostream>
        int main() { int n, l, x; std::cin >> n >> l; for(int i=0;i<n;i++) std::cin >> x;
        for(int i=0;i<n;i++) std::cout << i << ' ' << i << '\n'; }
    "#,
    );
    let cpu = unsafe { libc::sched_getcpu() };
    assert!(cpu >= 0);
    let output = Command::new(binary)
        .current_dir(root)
        .args(["preflight-submit", "--workspace"])
        .arg(&workspace)
        .args(["--source", "solution.cpp", "--problem"])
        .arg(root.join("problems/ahc044"))
        .arg("--public-cases")
        .arg(&cases)
        .args(["--evaluator-cpu", &cpu.to_string(), "--sandbox", "none"])
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let result: Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(result["candidate_id"], "candidate-0001");
    assert_eq!(result["judge_result"], "ACCEPTED");
    assert!(fs::read_dir(&workspace).unwrap().all(|entry| {
        !entry
            .unwrap()
            .file_name()
            .to_string_lossy()
            .starts_with(".ars-preflight-")
    }));
}

fn evaluate(binary: &str, root: &Path, cases: &Path, source: &Path, sandbox: &str) -> String {
    let output = Command::new(binary)
        .current_dir(root)
        .args(["evaluate", "--problem", "problems/ahc044", "--cases"])
        .arg(cases)
        .arg("--source")
        .arg(source)
        .args(["--workers", "1", "--sandbox", sandbox, "--summary"])
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let result: Value = serde_json::from_slice(&output.stdout).unwrap();
    result["judge_result"].as_str().unwrap().to_owned()
}

fn write_source(root: &Path, name: &str, source: &str) -> std::path::PathBuf {
    let path = root.join(name);
    fs::write(&path, source).unwrap();
    path
}
