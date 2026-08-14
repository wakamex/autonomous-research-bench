#!/bin/bash
export TMPDIR=/tmp/ars-ahc044-luna-low-20m-v3-eval
export ARS_ROOTLESS_PODMAN=1
export PYTHONPATH=/tmp/ars-rootless-python:.
exec /code/ARS/ALE-Bench/.venv/bin/python "$@"
