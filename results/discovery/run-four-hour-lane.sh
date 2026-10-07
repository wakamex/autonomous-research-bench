#!/bin/bash
set -uo pipefail

task=$1
agent=$2
model=$3
provider=$4
problem=$5
run_dir=$6
prompt=$7
broker_unit=$8
no_web=${9:-false}
aop_bin=${AOP_BIN:-aop}
effort=${AOP_EFFORT-high}
aop_dir="$run_dir/../aop"
mkdir -p "$aop_dir"
set -a
source /code/aop/.env
set +a

cleanup() {
  systemctl --user stop "$broker_unit" >/dev/null 2>&1 || true
}
trap cleanup EXIT

started_at=$(jq -r .started_at "$run_dir/state.json")
duration=$(jq -r '.duration_seconds | floor' "$run_dir/state.json")
deadline=$(( $(date --date "$started_at" +%s) + duration ))
invocation=${AOP_INVOCATION:-1}
resume_id=${AOP_RESUME_ID:-}
retry_failures=${AOP_RETRY_FAILURES:-false}
retry_mode=${AOP_RETRY_MODE:-resume}
results=()
while (( $(date +%s) < deadline )); do
  remaining=$(( deadline - $(date +%s) ))
  result=$(printf '%s/invocation-%03d.json' "$aop_dir" "$invocation")
  stderr=$(printf '%s/invocation-%03d.stderr.log' "$aop_dir" "$invocation")
  if [[ -z "$resume_id" ]]; then
    command=("$aop_bin" run "$task" --agent "$agent" --model "$model" --profile edit --timeout "$remaining" --json --prompt-file "$prompt")
    if [[ -n "$effort" ]]; then
      command+=(--effort "$effort")
    fi
    if [[ -n "$provider" ]]; then
      command+=(--provider "$provider")
    fi
    if [[ "$no_web" == true ]]; then
      command+=(--no-web)
    fi
  else
    continuation=$(/code/ARS/target/release/arb continuation-prompt --run-dir "$run_dir")
    command=("$aop_bin" resume "$resume_id" --timeout "$remaining" --json --prompt "$continuation")
  fi
  "${command[@]}" >"$result" 2>"$stderr"
  status=$?
  if ! jq -e . "$result" >/dev/null 2>&1; then
    break
  fi
  results+=(--aop-result "$result")
  resume_id=$(jq -r '.run_id // empty' "$result")
  timed_out=$(jq -r '.timed_out // false' "$result")
  succeeded=$(jq -r '.succeeded // false' "$result")
  if [[ "$timed_out" == true || -z "$resume_id" ]]; then
    break
  fi
  if [[ "$succeeded" != true || $status -ne 0 ]]; then
    if [[ "$retry_failures" != true ]]; then
      break
    fi
    if [[ "$retry_mode" == fresh ]]; then
      resume_id=
    fi
    sleep 30
  fi
  invocation=$((invocation + 1))
done

if (( ${#results[@]} > 0 )); then
  /code/ARS/target/release/arb stop --run-dir "$run_dir" "${results[@]}"
fi
