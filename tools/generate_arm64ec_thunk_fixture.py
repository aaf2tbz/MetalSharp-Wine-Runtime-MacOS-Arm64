#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate and structurally validate legal LLVM ARM64EC thunk fixtures.

All outputs are build artifacts.  This script deliberately records no source or build path.
"""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def coff_symbols(data):
    if len(data) < 20:
        raise ValueError("truncated COFF header")
    machine = int.from_bytes(data[0:2], "little")
    sections = int.from_bytes(data[2:4], "little")
    symbol_offset = int.from_bytes(data[8:12], "little")
    symbol_count = int.from_bytes(data[12:16], "little")
    if machine != 0xa641:  # IMAGE_FILE_MACHINE_ARM64EC
        raise ValueError("COFF machine is not ARM64EC")
    section_end = 20 + sections * 40
    if section_end > len(data) or symbol_offset + symbol_count * 18 > len(data):
        raise ValueError("truncated COFF section or symbol table")
    relocation_count = 0
    for index in range(sections):
        section = 20 + index * 40
        reloc = int.from_bytes(data[section + 24:section + 28], "little")
        count = int.from_bytes(data[section + 32:section + 34], "little")
        if count and (reloc == 0 or reloc + count * 10 > len(data)):
            raise ValueError("truncated COFF relocation table")
        relocation_count += count
    string_base = symbol_offset + symbol_count * 18
    if string_base + 4 > len(data):
        raise ValueError("missing COFF string table")
    string_size = int.from_bytes(data[string_base:string_base + 4], "little")
    if string_size < 4 or string_base + string_size > len(data):
        raise ValueError("truncated COFF string table")
    strings = data[string_base:string_base + string_size]
    names, index = set(), 0
    while index < symbol_count:
        entry = symbol_offset + index * 18
        raw = data[entry:entry + 8]
        if raw[:4] == b"\0\0\0\0":
            offset = int.from_bytes(raw[4:], "little")
            if offset < 4 or offset >= len(strings):
                raise ValueError("invalid COFF symbol string offset")
            end = strings.find(b"\0", offset)
            if end < 0:
                raise ValueError("unterminated COFF symbol")
            names.add(strings[offset:end].decode("ascii", "strict"))
        else:
            names.add(raw.split(b"\0", 1)[0].decode("ascii", "strict"))
        index += 1 + data[entry + 17]
    return sections, relocation_count, names


def compiler_identity(clang, locked_revision):
    identity = subprocess.run([clang, "--version"], check=True, text=True,
                              stdout=subprocess.PIPE).stdout.splitlines()[0]
    version = re.escape(str(locked_revision))
    if re.search(r"(?<![0-9.])" + version + r"(?![0-9.])", identity) is None:
        raise ValueError("compiler version does not match locked LLVM revision " +
                         str(locked_revision) + ": " + identity)
    return identity


def validate(assembly, obj):
    text = assembly.read_text(encoding="utf-8")
    required = ("__os_arm64x_check_icall", "__os_arm64x_dispatch_call_no_redirect",
                "__os_arm64x_dispatch_ret", "$ientry_thunk", "$iexit_thunk")
    missing = [item for item in required if item not in text]
    if missing:
        raise ValueError("LLVM ARM64EC lowering omitted: " + ", ".join(missing))
    # REG-1: check compiler-generated fixture assembly, not a hand-authored instruction stream.
    forbidden = re.findall(r"\b(?:[xw](?:13|14|23|24|28)|v(?:1[6-9]|2[0-9]|3[0-1]))\b", text)
    if forbidden:
        raise ValueError("generated fixture uses forbidden ARM64EC register(s): " +
                         ", ".join(sorted(set(forbidden))))
    sections, relocations, names = coff_symbols(obj.read_bytes())
    expected_symbols = ("#fixture_integer", "#fixture_float", "#fixture_pair", "#fixture_variadic")
    missing = [item for item in expected_symbols if item not in names]
    if missing or relocations == 0:
        raise ValueError("COFF fixture lacks expected symbols or relocations: " + ", ".join(missing))
    return {"machine": "ARM64EC (0xa641)", "sections": sections, "relocations": relocations,
            "expected_symbols": list(expected_symbols), "helper_references": list(required),
            "forbidden_register_regression": "REG-1 assembly operand scan passed"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--object", required=True)
    parser.add_argument("--llvm-provenance", required=True)
    args = parser.parse_args()
    source, output, obj = pathlib.Path(args.source), pathlib.Path(args.output), pathlib.Path(args.object)
    lock = json.loads(pathlib.Path(args.llvm_provenance).read_text(encoding="utf-8"))
    if lock.get("component") != "llvm" or not lock.get("revision") or not lock.get("license"):
        raise SystemExit("invalid repository-controlled LLVM provenance lock")
    try:
        compiler = compiler_identity(args.clang, lock["revision"])
    except ValueError as error:
        raise SystemExit(str(error)) from error
    output.parent.mkdir(parents=True, exist_ok=True)
    flags = ["--target=arm64ec-windows-msvc", "-O0"]
    subprocess.run([args.clang, *flags, "-S", str(source), "-o", str(output)], check=True)
    subprocess.run([args.clang, *flags, "-c", str(source), "-o", str(obj)], check=True)
    facts = validate(output, obj)
    manifest = {
        "schema": 1,
        "build_tree_only": True,
        "source": source.name,
        "source_sha256": sha256(source),
        "llvm": lock,
        "compiler_identity": compiler,
        "target": "arm64ec-windows-msvc",
        "flags": flags[1:],
        "assembly_sha256": sha256(output),
        "selected_artifact": obj.name,
        "selected_artifact_sha256": sha256(obj),
        "artifact": facts,
    }
    output.with_suffix(".provenance.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                                                        encoding="utf-8")


if __name__ == "__main__":
    main()
