#!/bin/bash
set -euo pipefail
set -a
source /code/aop/.env
set +a
export ARS_RUN_DIR=/code/ARS/.aop/worktrees/ars-ahc044-glm53-flash-high-20m-v5/results/discovery/ahc044-aop-glm53-flash-high-20m-v5/run
export AOP_CODEX_BIN=/tmp/ars-codex-official.hEAlbN/bin/codex
export PATH=/code/ARS/.aop/worktrees/ars-ahc044-glm53-flash-high-20m-v5/bin:$PATH
exec aop run ars-ahc044-glm53-flash-high-20m-v5 --agent codex --model glm-5.3-flash --provider zai-coding-plan --effort high --profile host --no-web --timeout 1200 --json --prompt-file benchmarks/ahc044/baseline-prompt.md
