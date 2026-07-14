#!/usr/bin/env python3
"""Execute the exact PE32+ oracle natively on Windows x86_64."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import time


MARKER = "MSWR_X64_V1 "


def fail(message: str) -> None:
    raise SystemExit(f"native x86_64 fixture failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()
    if args.timeout < 5 or args.timeout > 120:
        fail("timeout is outside the accepted bound")
    host = platform.machine().lower()
    if os.name != "nt" or host not in {"amd64", "x86_64"}:
        fail(f"native Windows x86_64 is required, observed os={os.name} machine={host}")
    if not args.fixture.is_file():
        fail("fixture is absent")

    env = os.environ.copy()
    env.update({"MSWR_X64_ENV": "oracle-value", "LC_ALL": "C", "LANG": "C"})
    started = time.monotonic()
    try:
        result = subprocess.run(
            [str(args.fixture.resolve()), "mswr-argument"],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=args.timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        fail(f"execution timed out; partial output={error.stdout!r}")
    if len(result.stdout.encode(errors="replace")) > 2 * 1024 * 1024:
        fail("execution exceeded the 2 MiB log bound")
    lines = [line for line in result.stdout.splitlines() if line.startswith(MARKER)]
    if result.returncode or len(lines) != 1:
        fail(f"rc={result.returncode}, semantic result count={len(lines)}\n{result.stdout}")
    semantic = json.loads(lines[0][len(MARKER):])
    if semantic.get("passed") is not True:
        fail(f"semantic fixture failure: {semantic}")

    evidence = {
        "schema": 1,
        "mode": "windows-native",
        "hostArchitecture": "x86_64",
        "fixtureSha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
        "returnCode": result.returncode,
        "durationSeconds": round(time.monotonic() - started, 3),
        "bounded": True,
        "engineTraceObserved": True,
        "semantic": semantic,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(evidence, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
