use tools::*;

fn main() {
    let input =
        std::fs::read_to_string(std::env::args().nth(1).expect("input path")).expect("read input");
    let output = std::fs::read_to_string(std::env::args().nth(2).expect("output path"))
        .unwrap_or_else(|error| fail(&format!("invalid output: {error}")));
    let input = parse_input(&input);
    let output = parse_output(&input, &output).unwrap_or_else(|error| fail(&error));
    let (score, error) = compute_score(&input, &output);
    if !error.is_empty() {
        fail(&error);
    }
    eprintln!("Score = {score}");
}

fn fail(message: &str) -> ! {
    eprintln!("{message}");
    std::process::exit(1)
}
