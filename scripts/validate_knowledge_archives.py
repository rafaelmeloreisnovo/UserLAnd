#!/usr/bin/env python3
"""Validate RAFAELIA/Vectras knowledge archive metadata.

The validator is intentionally dependency-free so it can run in constrained
Android/Linux shells and CI jobs without network access.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "knowledge_archives" / "catalog.json"
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
}


def fail(message: str) -> int:
    print(f"FAIL: {message}", file=sys.stderr)
    return 1


def main() -> int:
    if not CATALOG.exists():
        return fail(f"missing catalog: {CATALOG}")

    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return fail(f"invalid json: {exc}")

    seeds = catalog.get("seeds")
    if not isinstance(seeds, list) or not seeds:
        return fail("catalog.seeds must be a non-empty list")

    seed_ids = [seed.get("id") for seed in seeds if isinstance(seed, dict)]
    if len(seed_ids) != len(seeds) or any(not seed_id for seed_id in seed_ids):
        return fail("every seed must have an id")
    if len(seed_ids) != len(set(seed_ids)):
        return fail("seed ids must be unique")

    invariant = catalog.get("invariants", {}).get("total")
    if not isinstance(invariant, str) or "Phi" not in invariant:
        return fail("catalog invariants.total must include Phi")

    navigation = set(catalog.get("navigation", []))
    missing_navigation = sorted(REQUIRED_NAVIGATION - navigation)
    if missing_navigation:
        return fail(f"missing navigation entries: {', '.join(missing_navigation)}")

    hooks = set(catalog.get("validation_hooks", []))
    missing_hooks = sorted(REQUIRED_HOOKS - hooks)
    if missing_hooks:
        return fail(f"missing validation hooks: {', '.join(missing_hooks)}")

    risks = catalog.get("risk_register")
    if not isinstance(risks, list) or not risks:
        return fail("risk_register must be a non-empty list")

    for path in (
        ROOT / "docs" / "knowledge_archives" / "README.md",
        ROOT / "docs" / "knowledge_archives" / "vectras_vm_android.md",
        ROOT / "docs" / "knowledge_archives" / "rafaelia_seed_manifest.md",
    ):
        if not path.exists():
            return fail(f"missing archive document: {path.relative_to(ROOT)}")

    print(
        "PASS: knowledge archive catalog valid "
        f"({len(seeds)} seeds, {len(navigation)} navigation entries, {len(risks)} risks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
