#!/usr/bin/env python3
"""Operational CLI for the Vectras/RAFAELIA knowledge archive.

The tool is deterministic and dependency-free. It turns the archive catalog into
machine-checkable outputs that can be used by CI, Android shell sessions, or
enterprise review workflows without network access.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "knowledge_archives" / "catalog.json"
REPORT = ROOT / "docs" / "knowledge_archives" / "enterprise_report.json"

REQUIRED_FLAGS = {
    "freestanding",
    "nomalloc",
    "noheap",
    "noGC",
    "deterministic",
    "audit_trail",
    "rollback_ready",
    "failover_ready",
    "watchdog_ready",
}
REQUIRED_NAVIGATION = {
    "seed-records",
    "invariant-layer",
    "module-layer",
    "validation-layer",
    "expansion-layer",
    "metaphor-layer",
    "void-layer",
}
REQUIRED_HOOKS = {
    "json-catalog-parse",
    "unique-seed-ids",
    "invariant-present",
    "navigation-targets-present",
    "risk-register-present",
    "best-practices-20-plus",
    "resilience-matrix-present",
    "operational-modes-present",
}
REQUIRED_MODES = {"portable", "arm32", "arm64", "android-shell", "enterprise-ci"}


def load_catalog(path: Path = CATALOG) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def stable_digest(value: Any) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def validate_catalog(catalog: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    seeds = catalog.get("seeds")
    if not isinstance(seeds, list) or not seeds:
        errors.append("catalog.seeds must be a non-empty list")
    else:
        seed_ids = [seed.get("id") for seed in seeds if isinstance(seed, dict)]
        if len(seed_ids) != len(seeds) or any(not seed_id for seed_id in seed_ids):
            errors.append("every seed must have an id")
        if len(seed_ids) != len(set(seed_ids)):
            errors.append("seed ids must be unique")

    invariants = catalog.get("invariants", {})
    invariant = invariants.get("total") if isinstance(invariants, dict) else None
    if not isinstance(invariant, str) or "Phi" not in invariant:
        errors.append("catalog invariants.total must include Phi")

    flags = set(invariants.get("flags", [])) if isinstance(invariants, dict) else set()
    missing_flags = sorted(REQUIRED_FLAGS - flags)
    if missing_flags:
        errors.append("missing invariant flags: " + ", ".join(missing_flags))

    navigation = set(catalog.get("navigation", []))
    missing_navigation = sorted(REQUIRED_NAVIGATION - navigation)
    if missing_navigation:
        errors.append("missing navigation entries: " + ", ".join(missing_navigation))

    hooks = set(catalog.get("validation_hooks", []))
    missing_hooks = sorted(REQUIRED_HOOKS - hooks)
    if missing_hooks:
        errors.append("missing validation hooks: " + ", ".join(missing_hooks))

    modes = catalog.get("operational_modes")
    mode_names = {mode.get("id") for mode in modes if isinstance(mode, dict)} if isinstance(modes, list) else set()
    missing_modes = sorted(REQUIRED_MODES - mode_names)
    if missing_modes:
        errors.append("missing operational modes: " + ", ".join(missing_modes))

    practices = catalog.get("best_practices")
    if not isinstance(practices, list) or len(practices) < 20:
        errors.append("best_practices must contain at least 20 entries")

    resilience = catalog.get("resilience_matrix")
    if not isinstance(resilience, list) or len(resilience) < 5:
        errors.append("resilience_matrix must contain rollback, failover, failtest, watchdog, and mitigation entries")
    else:
        resilience_ids = {item.get("id") for item in resilience if isinstance(item, dict)}
        for required_id in ("rollback", "failover", "failtest", "watchdog", "mitigation"):
            if required_id not in resilience_ids:
                errors.append(f"missing resilience entry: {required_id}")

    risks = catalog.get("risk_register")
    if not isinstance(risks, list) or not risks:
        errors.append("risk_register must be a non-empty list")

    for relative in (
        "docs/knowledge_archives/README.md",
        "docs/knowledge_archives/vectras_vm_android.md",
        "docs/knowledge_archives/rafaelia_seed_manifest.md",
        "docs/knowledge_archives/enterprise_operations.md",
    ):
        if not (ROOT / relative).exists():
            errors.append(f"missing archive document: {relative}")

    return errors


def build_report(catalog: dict[str, Any]) -> dict[str, Any]:
    seeds = catalog.get("seeds", [])
    practices = catalog.get("best_practices", [])
    modes = catalog.get("operational_modes", [])
    resilience = catalog.get("resilience_matrix", [])
    errors = validate_catalog(catalog)
    return {
        "archive_id": catalog.get("archive_id"),
        "version": catalog.get("version"),
        "status": "PASS" if not errors else "FAIL",
        "digest_sha256": stable_digest(catalog),
        "counts": {
            "seeds": len(seeds) if isinstance(seeds, list) else 0,
            "best_practices": len(practices) if isinstance(practices, list) else 0,
            "operational_modes": len(modes) if isinstance(modes, list) else 0,
            "resilience_entries": len(resilience) if isinstance(resilience, list) else 0,
            "risks": len(catalog.get("risk_register", [])) if isinstance(catalog.get("risk_register"), list) else 0,
        },
        "errors": errors,
        "next_actions": catalog.get("next_actions", []),
    }


def run_validate(_args: argparse.Namespace) -> int:
    catalog = load_catalog()
    report = build_report(catalog)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


def run_report(args: argparse.Namespace) -> int:
    catalog = load_catalog()
    report = build_report(catalog)
    if args.write:
        REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


def run_resilience(_args: argparse.Namespace) -> int:
    catalog = load_catalog()
    errors = validate_catalog(catalog)
    payload = {
        "status": "PASS" if not errors else "FAIL",
        "resilience_matrix": catalog.get("resilience_matrix", []),
        "digest_sha256": stable_digest(catalog.get("resilience_matrix", [])),
        "errors": errors,
    }
    print(json.dumps(payload, indent=2, sort_keys=True))
    return 0 if payload["status"] == "PASS" else 1


def parser() -> argparse.ArgumentParser:
    cli = argparse.ArgumentParser(description="Operate the Vectras/RAFAELIA knowledge archive")
    sub = cli.add_subparsers(dest="command", required=True)

    validate = sub.add_parser("validate", help="validate the archive catalog and documents")
    validate.set_defaults(func=run_validate)

    report = sub.add_parser("report", help="emit an enterprise readiness report")
    report.add_argument("--write", action="store_true", help="write docs/knowledge_archives/enterprise_report.json")
    report.set_defaults(func=run_report)

    resilience = sub.add_parser("resilience", help="emit rollback/failover/failtest/watchdog/mitigation matrix")
    resilience.set_defaults(func=run_resilience)

    return cli


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
