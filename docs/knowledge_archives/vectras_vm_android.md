# Vectras-VM-Android Architecture Archive

## Scope

Vectras-VM-Android is catalogued here as a low-level Android virtual-machine knowledge path. The archive does not claim that every listed subsystem already exists in the repository. Instead, it defines a deterministic map for implementation, audit, and rollback.

## Core invariant

```text
I = Phi(s, S, H, C, G)
```

Meaning in engineering terms:

- `s`: current state vector for data, entropy, hash, and module state.
- `S`: stage/cycle state, including `PSI`, `CHI`, `RHO`, `DELTA`, `SIGMA`, and `OMEGA`.
- `H`: entropy or uncertainty budget.
- `C`: coherence/integrity score.
- `G`: graph/geometry relationship among modules and data.

A module may use metaphorical names, but its implementation contract must still be measurable.

## Freestanding module contract

Every low-level candidate module should state these flags:

| Flag | Required meaning |
| --- | --- |
| `freestanding` | Can be reasoned about without depending on a full hosted runtime for core logic. |
| `nomalloc` | Does not allocate heap memory in the core path. |
| `noheap` | Does not require heap-backed ownership for steady-state execution. |
| `noGC` | Does not rely on garbage collection. |
| `deterministic` | Same input and state produce the same output. |
| `audit_trail` | Emits or updates verifiable evidence of state transitions. |
| `rollback_ready` | Has a previous valid state or checkpoint. |
| `failover_ready` | Has a secondary route when the primary route fails. |
| `watchdog_ready` | Has bounded progress checks or timeout state. |

## Module matrix

| Module | Purpose | Low-level shape | Failsafe route | VOID notes |
| --- | --- | --- | --- | --- |
| VM bootstrap | Select runnable mode for Android host and native helpers | fixed state machine, architecture flags, ABI probe | fall back from accelerated mode to portable mode | exact Vectras runtime ABI must be verified |
| Kernel adapter | Coordinate native helper calls and app-level lifecycle | command table, fixed buffers, explicit status codes | restore last known good state | not a replacement for Android kernel |
| Scheduler adapter | Map work to deterministic bounded cycles | ring slots, static queues, monotonic counters | skip bad slot and reschedule from checkpoint | no real-time guarantee without host support |
| Filesystem archive | Store seeds, manifests, and checkpoints | append-only records, hashes, version tags | previous manifest hash and snapshot | storage durability depends on Android filesystem |
| Identity/audit | Preserve provenance of archive entries | stable IDs, hashes, timestamps, signatures when available | quarantine unverifiable records | identity is not proof of truth by itself |
| Federated data | Keep raw data local and aggregate only summaries | route tags, local query plan, aggregate result | deny query if privacy policy unknown | privacy model requires formal threat analysis |
| GeoLM/TorusFlow | Treat tokens as state transitions | finite states, attractor table, decay counters | return `VOID` when coherence is below threshold | not equivalent to a trained LLM unless implemented and evaluated |
| Rollback ledger | Track reversible actions | journal entries, previous hash, status flags | replay to last valid checkpoint | replay semantics must be tested per module |

## Android/ARM capability map

| Target | Preferred path | Fallback path | Notes |
| --- | --- | --- | --- |
| ARM64 Android | native helper with fixed buffers and NEON-capable optional branch | portable C helper | keep ABI flags explicit |
| ARM32 Android | portable C helper with optional NEON if detected | scalar C path | avoid assuming GPU availability |
| Termux-like shell | shell wrapper around static/native helper | pure shell validation only | shell scripts are not freestanding, but can orchestrate tests |
| PRoot/session layer | bounded startup checks and checkpointed session metadata | restart from last valid rootfs state | host limitations must be recorded |

## Branchless and precomputed strategy

Branchless work is valuable only when it reduces measurable cost or side-channel surface without damaging clarity or correctness. Use this route:

1. Define the state table.
2. Precompute constants and masks.
3. Replace repeated conditionals with table lookup only when all states are covered.
4. Keep a portable readable implementation as the reference oracle.
5. Test accelerated and portable outputs against the same vectors.
6. Log architecture flags used for each run.

## Rollback, failover, failtest, and watchdog plan

| Test class | Minimum check |
| --- | --- |
| Rollback | corrupt current checkpoint and prove previous checkpoint is selected. |
| Failover | disable accelerated path and prove portable path returns identical vectors. |
| Failtest | inject malformed seed, bad hash, and impossible state flags. |
| Watchdog | simulate stalled stage and prove timeout state is emitted. |
| Audit | verify each state transition has ID, timestamp or monotonic counter, previous hash, and status. |

## Parables as engineering lessons

- **The empty cup**: an explicit `VOID` answer is a clean buffer; false certainty contaminates downstream computation.
- **The narrow gate**: a freestanding module has fewer dependencies, so fewer hidden failure paths.
- **The house on rock**: invariants and tests are the foundation; metaphors are useful only when they lead back to measurable contracts.
- **The seed and the tree**: a seed record is not yet a system; it becomes useful through cataloguing, constraints, implementation, and validation.

## Recommended next implementation layer

1. Add a seed ingestion command that normalizes `id`, `title`, `raw_lines`, `validation`, and `hash`.
2. Add deterministic validation for invariant presence, expansion route, line count, and duplicate IDs.
3. Add rollback snapshots for archive updates.
4. Add a native fixed-buffer validator for mobile environments.
5. Add architecture-specific tests only after the portable oracle is stable.
