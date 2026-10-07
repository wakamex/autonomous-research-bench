use std::process::{Command, Stdio};
use tools::exec;

fn main() {
    let mut args = std::env::args().skip(1);
    let program = args
        .next()
        .unwrap_or_else(|| fail("usage: tester <command> [<args>...]"));
    let mut child = Command::new(&program)
        .args(args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap_or_else(|error| fail(&format!("failed to execute {program}: {error}")));
    match exec(&mut child, true) {
        Ok(score) => eprintln!("Score = {score}"),
        Err(error) => {
            let _ = child.kill();
            fail(&error)
        }
    }
}

fn fail(message: &str) -> ! {
    eprintln!("{message}");
    std::process::exit(1)
}
