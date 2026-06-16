# Custody Chain

`custody/` stores evidence: commands, outputs, hashes, statuses, and runtime
observations.  It turns `TOKEN_VAZIO` into a visible engineering state instead
of an invented claim.

Record format:

```text
date_utc: YYYY-MM-DDTHH:MM:SSZ
status: PASS | FAIL | NOT_RUN | TOKEN_VAZIO | PENDING | AUDIT | RUNTIME | REFERENCE
command: exact command or NOT_RUN
origin: file/function/line when applicable
hash: git/object/output hash when applicable
summary: observed result, never assumed
```

Rules:

- no PASS without command and observed output;
- no runtime claim without binary or test output;
- failed checks stay visible with mitigation or rollback notes;
- generated logs belong here or in a dated subdirectory.

Existing references:

- `docs/knowledge_archives/` for semantic archives;
- `tools/audit_core_integrity.py` for core integrity checks;
- `CORE_CONTRACT.md` for the public invariant.
