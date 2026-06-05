#!/usr/bin/env python3
"""Validate RAFAELIA/Vectras knowledge archive metadata.

The validator is intentionally dependency-free so it can run in constrained
Android/Linux shells and CI jobs without network access. It delegates the full
structural checks to ``knowledge_archive_tool`` so the one-shot validator and
enterprise CLI enforce the same contract.
"""

from __future__ import annotations

import sys

from knowledge_archive_tool import build_report, load_catalog


def main() -> int:
    report = build_report(load_catalog())
    if report["status"] != "PASS":
        for error in report["errors"]:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    counts = report["counts"]
    print(
        "PASS: knowledge archive catalog valid "
        f"({counts['seeds']} seeds, "
        f"{counts['best_practices']} best practices, "
        f"{counts['operational_modes']} modes, "
        f"{counts['resilience_entries']} resilience entries, "
        f"{counts['risks']} risks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
