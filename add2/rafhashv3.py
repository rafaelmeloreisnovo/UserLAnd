#!/usr/bin/env python3
# rafhash_tool_improved.py — Refined RAFAELIA hashing, verification and signing utility
# Usage examples:
#   ./rafhash_tool_improved.py compute --file path --output-format json
#   ./rafhash_tool_improved.py compute --dir PATH --output-format yaml --jobs 4
#   ./rafhash_tool_improved.py generate-expected --dir PATH --out expected.json --sign gpg --sign-key you@example.com
#   ./rafhash_tool_improved.py verify --file path --expected expected.json --strict
#
# Goals: clear logging, directory support, parallel hashing, optional blake3 & PyYAML, optional signing (gpg/age).

from __future__ import annotations
import argparse
import hashlib
import json
import logging
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

# Optional deps
try:
    import blake3
    HAVE_BLAKE3 = True
except Exception:
    HAVE_BLAKE3 = False

try:
    import yaml
    HAVE_YAML = True
except Exception:
    HAVE_YAML = False

LOG = logging.getLogger("rafhash")
CHUNK = 8 * 1024 * 1024  # 8MB chunk for streaming

def init_logging(verbose: int = 0):
    level = logging.WARNING
    if verbose >= 2:
        level = logging.DEBUG
    elif verbose == 1:
        level = logging.INFO
    logging.basicConfig(format="%(levelname)s: %(message)s", level=level)

def sha3_256_hex(path: Path) -> str:
    h = hashlib.new("sha3_256")
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()

def sha256_hex(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()

def blake3_hex(path: Path) -> str:
    if not HAVE_BLAKE3:
        raise RuntimeError("blake3 not available")
    h = blake3.blake3()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()

def compute_fragment(path: Path, timestamp: Optional[str] = None) -> Dict:
    ts = timestamp or datetime.utcnow().isoformat() + "Z"
    LOG.debug("Hashing %s", path)
    frag = {
        "file": path.name,
        "path": str(path),
        "timestamp": ts,
        "sha3_256": sha3_256_hex(path),
        "sha256": sha256_hex(path),
        "blake3": ""
    }
    if HAVE_BLAKE3:
        try:
            frag["blake3"] = blake3_hex(path)
            frag["algo_used"] = "blake3"
        except Exception:
            LOG.exception("blake3 calculation failed for %s; falling back", path)
            frag["algo_used"] = "sha3_256"
    else:
        frag["algo_used"] = "sha3_256"
    return frag

def compute_for_dir(dirpath: Path, timestamp: Optional[str] = None, jobs: int = 4) -> List[Dict]:
    files = [p for p in sorted(dirpath.iterdir()) if p.is_file()]
    LOG.info("Found %d files to hash in %s", len(files), dirpath)
    results: List[Dict] = []
    if not files:
        return results
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {ex.submit(compute_fragment, f, timestamp): f for f in files}
        for fut in as_completed(futures):
            f = futures[fut]
            try:
                frag = fut.result()
                results.append(frag)
            except Exception:
                LOG.exception("Failed to hash %s", f)
    results.sort(key=lambda r: r["file"])
    return results

def print_yaml_fragment(frag: Dict):
    print("  - file: \"{}\"".format(frag["file"]))
    print("    path: \"{}\"".format(frag["path"]))
    print("    timestamp: \"{}\"".format(frag["timestamp"]))
    print("    sha3_256: \"{}\"".format(frag["sha3_256"]))
    if frag.get("blake3"):
        print("    blake3: \"{}\"".format(frag["blake3"]))
    print("    sha256: \"{}\"".format(frag["sha256"]))
    print("    algo_used: \"{}\"".format(frag.get("algo_used", "")))

def write_output(data: Dict, outpath: Path, fmt: str = "json"):
    if fmt == "json":
        with outpath.open("w", encoding="utf-8") as fh:
            json.dump(data, fh, ensure_ascii=False, indent=2)
    elif fmt in ("yaml", "yml"):
        if HAVE_YAML:
            with outpath.open("w", encoding="utf-8") as fh:
                yaml.safe_dump(data, fh, sort_keys=False, allow_unicode=True)
        else:
            LOG.warning("PyYAML not installed: writing JSON as fallback")
            with outpath.open("w", encoding="utf-8") as fh:
                json.dump(data, fh, ensure_ascii=False, indent=2)
    else:
        raise ValueError("unsupported format: " + fmt)
    LOG.info("Wrote output to %s", outpath)

def generate_expected(dirpath: Path, outpath: Path, timestamp: Optional[str] = None, jobs: int = 4, fmt: str = "json", sign: Optional[str] = None, sign_key: Optional[str] = None):
    timestamp = timestamp or datetime.utcnow().isoformat() + "Z"
    files = compute_for_dir(dirpath, timestamp=timestamp, jobs=jobs)
    expected = {"generated": timestamp, "files": files}
    write_output(expected, outpath, fmt=fmt)
    if sign:
        try:
            sign_file(outpath, method=sign, key=sign_key)
        except Exception:
            LOG.exception("Signing failed")
            raise
    print(f"[OK] expected hashes written to {outpath}")

def verify_file_against_expected(path: Path, expected_json: Path, strict: bool = False) -> int:
    with expected_json.open("r", encoding="utf-8") as fh:
        try:
            expected = json.load(fh)
        except json.JSONDecodeError:
            if HAVE_YAML:
                fh.seek(0)
                expected = yaml.safe_load(fh)
            else:
                LOG.exception("Failed to parse expected file; install PyYAML for YAML support")
                return 4
    base = path.name
    for ent in expected.get("files", []):
        if ent.get("file") == base:
            frag = compute_fragment(path)
            mismatches = []
            if ent.get("sha3_256") and ent.get("sha3_256") != frag["sha3_256"]:
                mismatches.append("sha3_256")
            if ent.get("blake3") and ent.get("blake3") and ent.get("blake3") != frag.get("blake3"):
                mismatches.append("blake3")
            if ent.get("sha256") and ent.get("sha256") != frag["sha256"]:
                mismatches.append("sha256")
            if mismatches:
                print(json.dumps({"file": base, "status": "MISMATCH", "note": mismatches}, ensure_ascii=False))
                return 2
            else:
                print(json.dumps({"file": base, "status": "OK"}, ensure_ascii=False))
                return 0
    print(json.dumps({"file": base, "status": "NOT_FOUND"}, ensure_ascii=False))
    return 3

def sign_file(path: Path, method: str = "gpg", key: Optional[str] = None) -> None:
    if method == "gpg":
        cmd = ["gpg", "--armor", "--detach-sign", "--output", str(path) + ".asc", str(path)]
        if key:
            cmd.insert(1, "--local-user")
            cmd.insert(2, key)
    elif method == "age":
        if not key:
            raise ValueError("age requires recipient key")
        cmd = ["age", "-o", str(path) + ".age", "-r", key, str(path)]
    else:
        raise ValueError("unsupported signing method: " + method)
    LOG.info("Signing with: %s", " ".join(cmd))
    subprocess.run(cmd, check=True)
    LOG.info("Signature created")

def parse_args(argv: Optional[List[str]] = None):
    ap = argparse.ArgumentParser(prog="rafhash_tool_improved.py")
    ap.add_argument("-v", "--verbose", action="count", default=0, help="Verbose output (repeat for more verbosity)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("compute", help="Compute hash fragment for a file or directory")
    group = c.add_mutually_exclusive_group(required=True)
    group.add_argument("--file", type=Path, help="File to hash")
    group.add_argument("--dir", type=Path, help="Directory to hash (non-recursive)")
    c.add_argument("--timestamp", help="Timestamp to embed (ISO format)")
    c.add_argument("--output-format", choices=["json", "yaml", "yaml-fragment"], default="json")
    c.add_argument("--jobs", type=int, default=4, help="Parallel jobs for directory hashing")

    g = sub.add_parser("generate-expected", help="Generate expected hashes for a directory")
    g.add_argument("--dir", type=Path, required=True)
    g.add_argument("--out", type=Path, required=True)
    g.add_argument("--timestamp", help="Timestamp to embed (ISO format)")
    g.add_argument("--fmt", choices=["json", "yaml"], default="json")
    g.add_argument("--jobs", type=int, default=4)
    g.add_argument("--sign", choices=["gpg", "age"], help="Sign the output using gpg or age (optional)")
    g.add_argument("--sign-key", help="Key or recipient for signing (optional)")

    v = sub.add_parser("verify", help="Verify a file against an expected JSON/YAML")
    v.add_argument("--file", type=Path, required=True)
    v.add_argument("--expected", type=Path, required=True)
    v.add_argument("--strict", action="store_true", help="Fail on extra/missing fields (future extension)")

    return ap.parse_args(argv)

def main(argv: Optional[List[str]] = None):
    args = parse_args(argv)
    init_logging(args.verbose)

    try:
        if args.cmd == "compute":
            if args.file:
                frag = compute_fragment(args.file, timestamp=args.timestamp)
                if args.output_format == "yaml-fragment":
                    print_yaml_fragment(frag)
                elif args.output_format == "json":
                    print(json.dumps(frag, ensure_ascii=False))
                else:
                    if HAVE_YAML:
                        print(yaml.safe_dump(frag, sort_keys=False, allow_unicode=True))
                    else:
                        LOG.warning("PyYAML not installed: printing JSON as fallback")
                        print(json.dumps(frag, ensure_ascii=False))
                return 0
            else:
                files = compute_for_dir(args.dir, timestamp=args.timestamp, jobs=args.jobs)
                out = {"generated": args.timestamp or datetime.utcnow().isoformat() + "Z", "files": files}
                if args.output_format == "yaml-fragment":
                    for f in files:
                        print_yaml_fragment(f)
                elif args.output_format == "json":
                    print(json.dumps(out, ensure_ascii=False, indent=2))
                else:
                    if HAVE_YAML:
                        print(yaml.safe_dump(out, sort_keys=False, allow_unicode=True))
                    else:
                        LOG.warning("PyYAML not installed: printing JSON as fallback")
                        print(json.dumps(out, ensure_ascii=False, indent=2))
                return 0

        elif args.cmd == "generate-expected":
            generate_expected(args.dir, args.out, timestamp=args.timestamp, jobs=args.jobs, fmt=args.fmt, sign=args.sign, sign_key=args.sign_key)
            return 0

        elif args.cmd == "verify":
            return verify_file_against_expected(args.file, args.expected, strict=args.strict)

    except subprocess.CalledProcessError as e:
        LOG.exception("External command failed: %s", e)
        return 10
    except Exception:
        LOG.exception("Unhandled error")
        return 1

if __name__ == "__main__":
    sys.exit(main())
