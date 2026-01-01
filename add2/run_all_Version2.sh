#!/usr/bin/env bash
set -euo pipefail
echo "Building dev artifacts..."
make dev
echo "Generating golden vectors..."
python3 generate_golden_vectors.py --outdir golden_vectors --seed 12345 --k 6 --parity 1
echo "Running bitflip harness..."
python3 bitflip_recover.py --dir golden_vectors --out proof_report.json --k 6 --parity 1
echo "Report:"
cat proof_report.json
echo "Done. CLI available: ./ziprafa_cli ; Showroom: ./mega_showroom"