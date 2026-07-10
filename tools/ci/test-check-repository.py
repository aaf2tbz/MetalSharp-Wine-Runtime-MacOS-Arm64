#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for repository policy classification."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
POLICY_PATH = ROOT / "tools" / "ci" / "check-repository.py"

spec = importlib.util.spec_from_file_location("check_repository", POLICY_PATH)
assert spec is not None
check_repository = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(check_repository)


class RepositoryPolicyTests(unittest.TestCase):
    def classify(self, relative: str, data: bytes) -> list[str]:
        return check_repository.classify_tracked_file(Path(relative), data)

    def assert_has_error(self, errors: list[str], fragment: str) -> None:
        self.assertTrue(any(fragment in error for error in errors), errors)

    def test_allows_normal_source(self) -> None:
        errors = self.classify("src/example.c", b"int example(void) { return 0; }\n")
        self.assertEqual(errors, [])

    def test_rejects_executable_suffix(self) -> None:
        errors = self.classify("fixtures/sample.exe", b"not executable bytes\n")
        self.assert_has_error(errors, "tracked executable/debug artifact suffix")

    def test_rejects_nested_mixed_case_dsym_component(self) -> None:
        errors = self.classify("fixtures/Sample.DsYm/Contents/Info.plist", b"metadata\n")
        self.assert_has_error(errors, "tracked debug-symbol bundle path")

    def test_rejects_zip_suffix(self) -> None:
        errors = self.classify("fixtures/archive.zip", b"not a zip payload\n")
        self.assert_has_error(errors, "tracked opaque archive/container suffix")

    def test_rejects_zip_magic_under_neutral_suffix(self) -> None:
        errors = self.classify("fixtures/archive.data", b"PK\x03\x04payload")
        self.assert_has_error(errors, "tracked opaque archive/container magic (ZIP)")

    def test_rejects_private_path_marker(self) -> None:
        private_path = ("/Users/" + "averyfelts/private/input").encode("utf-8")
        errors = self.classify("fixtures/metadata.txt", b"source=" + private_path + b"\n")
        self.assert_has_error(errors, "forbidden local/secret marker")


if __name__ == "__main__":
    unittest.main()
