#!/bin/sh
export ARS_EVALUATOR_CPU=14
if [ "${1:-}" = "submit" ]; then
  shift
  exec /code/ARS/target/release/ars submit --problem /code/ARS/problems/ahc044 --sandbox none "$@"
fi
exec /code/ARS/target/release/ars "$@"
