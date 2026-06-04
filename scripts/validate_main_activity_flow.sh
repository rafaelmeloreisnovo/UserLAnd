#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_ACTIVITY="$ROOT_DIR/app/src/main/java/tech/ula/MainActivity.kt"
ACTION_MAPPER="$ROOT_DIR/app/src/main/java/tech/ula/MainActivityStateAction.kt"
PROGUARD="$ROOT_DIR/app/proguard-rules.pro"

require_text() {
  local file="$1"
  local pattern="$2"
  local description="$3"
  if ! grep -Fq -- "$pattern" "$file"; then
    echo "FAIL: $description" >&2
    echo "  missing pattern: $pattern" >&2
    echo "  file: $file" >&2
    exit 1
  fi
  echo "PASS: $description"
}

reject_text() {
  local file="$1"
  local pattern="$2"
  local description="$3"
  if grep -Fq -- "$pattern" "$file"; then
    echo "FAIL: $description" >&2
    echo "  forbidden pattern: $pattern" >&2
    echo "  file: $file" >&2
    exit 1
  fi
  echo "PASS: $description"
}

require_text "$MAIN_ACTIVITY" "viewModel.submitAppSelection(app, autoStart)" "app selections enter the ViewModel startup FSM"
require_text "$MAIN_ACTIVITY" "viewModel.submitSessionSelection(session)" "session selections enter the ViewModel startup FSM"
require_text "$MAIN_ACTIVITY" "MainActivityStateActionMapper.from(state)" "activity delegates state mapping to the deterministic mapper"
require_text "$MAIN_ACTIVITY" "IllegalStateHandler.getLocalizationData(action.state).getString(this)" "illegal state messages use the shared localizer"
require_text "$ACTION_MAPPER" "is SessionCanBeStarted -> MainActivityStateAction.StartSession" "mapper covers session start state"
require_text "$ACTION_MAPPER" "is SessionCanBeRestarted -> MainActivityStateAction.StartSession" "mapper covers session restart state"
require_text "$ACTION_MAPPER" "is IllegalState -> MainActivityStateAction.ShowIllegalState(state)" "mapper delegates all illegal states without duplicate branches"
require_text "$PROGUARD" "-dontwarn org.tukaani.xz.**" "R8 keeps optional XZ backend warnings non-fatal"
reject_text "$MAIN_ACTIVITY" "Minimal stub MainActivity" "old CI-only MainActivity stub text is absent"
reject_text "$MAIN_ACTIVITY" "For now, this minimal stub" "old app-selection placeholder text is absent"

python3 - <<'PY' "$ACTION_MAPPER"
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text()
required_actions = [
    "RequestFilesystemCredentials",
    "RequestAppServiceType",
    "ConfirmLargeDownload",
    "ConfirmLowStorage",
    "ShowDownloadProgress",
    "ShowExtractionProgress",
    "CompleteProgress",
]
missing = [item for item in required_actions if item not in text]
if missing:
    print(f"FAIL: mapper missing actions: {', '.join(missing)}", file=sys.stderr)
    sys.exit(1)
print("PASS: mapper contains the expected user-input/progress action surface")
PY
