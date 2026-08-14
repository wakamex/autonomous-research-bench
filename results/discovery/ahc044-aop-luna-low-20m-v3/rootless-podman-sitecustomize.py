import os


if os.environ.get("ARS_ROOTLESS_PODMAN") == "1":
    os.getuid = lambda: 0
