#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Create the canonical Phase 5 golden hash corpus from reviewed Phase 4 results."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

MAGIC = b"SWP5GLD1"
SCHEMA = 1
MASTER_SEED = 0x534841525057494E
CASES_PER_SHARD = 4096
NON_AUTHORITATIVE_BASELINE_TEMPLATES = frozenset({300, 301})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("results", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--count", type=int, required=True)
    args = parser.parse_args()
    if args.count < 1:
        parser.error("count must be positive")

    hashes: list[int] = []
    with args.results.open(encoding="utf-8") as source:
        for index, line in enumerate(source):
            if index >= args.count:
                break
            row = json.loads(line)
            shard, ordinal = divmod(index, CASES_PER_SHARD)
            if row.get("shard") != shard or row.get("case") != ordinal:
                raise ValueError(f"non-canonical identity at result {index}")
            if row.get("classification") != "pass":
                raise ValueError(f"non-passing result at {shard}/{ordinal}")
            records = [row.get(name, {}).get("record") for name in
                       ("baseline", "interpreter", "jit")]
            if any(not record for record in records):
                raise ValueError(f"missing record at {shard}/{ordinal}")
            baseline, interpreter, jit = records
            template_ids = {record.get("templateId") for record in records}
            if len(template_ids) != 1 or None in template_ids:
                raise ValueError(f"template mismatch at {shard}/{ordinal}")
            template_id = int(template_ids.pop())
            if template_id in NON_AUTHORITATIVE_BASELINE_TEMPLATES:
                if row.get("baselineAuthoritative") is not False or \
                   row.get("comparisonPolicy") != "interpreter-jit-sdm":
                    raise ValueError(f"missing non-authoritative policy at {shard}/{ordinal}")
                if interpreter.get("sdmExpectation") is not True or \
                   jit.get("sdmExpectation") is not True:
                    raise ValueError(f"SDM expectation failed at {shard}/{ordinal}")
                values = {interpreter.get("compatibilityHash"), jit.get("compatibilityHash")}
            else:
                if row.get("baselineAuthoritative") is not True or \
                   row.get("comparisonPolicy") != "three-way-exact":
                    raise ValueError(f"missing authoritative policy at {shard}/{ordinal}")
                values = {record.get("compatibilityHash") for record in records}
            if len(values) != 1 or None in values:
                raise ValueError(f"compatibility mismatch at {shard}/{ordinal}")
            hashes.append(int(values.pop(), 16))
    if len(hashes) != args.count:
        raise ValueError(f"expected {args.count} results, found {len(hashes)}")

    final_shard, final_ordinal = divmod(args.count - 1, CASES_PER_SHARD)
    header = struct.pack("<8sIIIIQ", MAGIC, SCHEMA, args.count, final_shard,
                         final_ordinal, MASTER_SEED)
    args.output.write_bytes(header + struct.pack(f"<{len(hashes)}Q", *hashes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
