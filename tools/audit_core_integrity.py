#!/usr/bin/env python3
"""
Audit Core Integrity Tool

This tool scans the Rafaelia core directories to ensure there is no
telemetry, analytics, or sensitive logging code in the core library.

The core should be minimal and free from:
- Telemetry collection
- Analytics tracking
- Network communication
- Sensitive data logging
- Personal information collection

Exit codes:
  0: Audit passed - no violations found
  1: Audit failed - violations detected
  2: Error during audit (e.g., directory not found)
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Tuple


# Patterns that indicate potential security/privacy violations
VIOLATION_PATTERNS = [
    # Telemetry and analytics
    (r'telemetry|analytics|tracking|metrics', 'Potential telemetry/analytics code'),
    
    # Network operations (core should not do network I/O)
    (r'http|https|socket|connect|send|recv|curl|wget', 'Network operation detected'),
    
    # Sensitive logging patterns
    (r'log.*password|log.*secret|log.*token|log.*key|log.*credential', 
     'Sensitive data logging'),
    
    # Personal information
    (r'email|username|user_id|device_id|imei|phone|address', 
     'Personal information detected'),
    
    # Suspicious data collection
    (r'collect.*data|gather.*info|send.*stats|report.*usage', 
     'Data collection pattern'),
]

# Files and directories to exclude from audit
EXCLUDE_PATTERNS = [
    'test',
    'tests',
    '__pycache__',
    '.git',
    'raf_kerneld.py',  # Daemon is explicitly for telemetry - excluded from core audit
]

# Whitelist patterns that are acceptable
WHITELIST_PATTERNS = [
    r'#include.*socket',  # Just includes, not usage
    r'//.*telemetry',     # Comments about telemetry
    r'/\*.*telemetry',    # Comments about telemetry
    r'memcpy',            # Memory copy, not data collection
    r'user["\s]',         # CPU field name "user", not user tracking
    r'raf_compute_cpu_metrics',  # Local CPU utilization math, not telemetry reporting
    r'RafCpuMetrics',            # Local CPU metric struct, not telemetry reporting
]


class Violation:
    def __init__(self, file_path: Path, line_num: int, line: str, reason: str):
        self.file_path = file_path
        self.line_num = line_num
        self.line = line.strip()
        self.reason = reason
    
    def __str__(self) -> str:
        return f"{self.file_path}:{self.line_num}: {self.reason}\n  > {self.line}"


def is_excluded(path: Path) -> bool:
    """Check if path should be excluded from audit."""
    path_str = str(path)
    for pattern in EXCLUDE_PATTERNS:
        if pattern in path_str:
            return True
    return False


def is_whitelisted(line: str) -> bool:
    """Check if line matches whitelist patterns."""
    line_lower = line.lower()
    for pattern in WHITELIST_PATTERNS:
        if re.search(pattern, line_lower, re.IGNORECASE):
            return True
    return False


def scan_file(file_path: Path) -> List[Violation]:
    """Scan a single file for violations."""
    violations = []
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, start=1):
                # Skip whitelisted lines
                if is_whitelisted(line):
                    continue
                
                # Check for violation patterns
                line_lower = line.lower()
                for pattern, reason in VIOLATION_PATTERNS:
                    if re.search(pattern, line_lower, re.IGNORECASE):
                        violations.append(Violation(file_path, line_num, line, reason))
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
    
    return violations


def scan_directory(directory: Path) -> List[Violation]:
    """Recursively scan directory for violations."""
    violations = []
    
    if not directory.exists():
        print(f"Error: Directory does not exist: {directory}", file=sys.stderr)
        sys.exit(2)
    
    # Scan C/C++ source files, headers, and Python files
    extensions = ['*.c', '*.cpp', '*.h', '*.hpp', '*.py']
    
    for ext in extensions:
        for file_path in directory.rglob(ext):
            if is_excluded(file_path):
                continue
            
            file_violations = scan_file(file_path)
            violations.extend(file_violations)
    
    return violations


def main():
    parser = argparse.ArgumentParser(
        description='Audit Rafaelia core for telemetry and sensitive code'
    )
    parser.add_argument(
        '--root',
        type=Path,
        default=Path(__file__).parent.parent,
        help='Root directory of the repository'
    )
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Print verbose output'
    )
    
    args = parser.parse_args()
    root = args.root.resolve()
    
    print("🔍 Auditing Rafaelia Core Integrity...")
    print(f"Root directory: {root}")
    print()
    
    # Directories to audit
    audit_dirs = [
        root / 'core' / 'librafaelia',
        root / 'rafaelia',
    ]
    
    all_violations = []
    files_scanned = 0
    
    for audit_dir in audit_dirs:
        if not audit_dir.exists():
            if args.verbose:
                print(f"Skipping non-existent directory: {audit_dir}")
            continue
        
        print(f"Scanning: {audit_dir}")
        violations = scan_directory(audit_dir)
        all_violations.extend(violations)
        
        # Count files scanned
        for ext in ['*.c', '*.cpp', '*.h', '*.hpp', '*.py']:
            files_scanned += sum(1 for f in audit_dir.rglob(ext) if not is_excluded(f))
    
    print(f"Files scanned: {files_scanned}")
    print()
    
    if all_violations:
        print("❌ AUDIT FAILED - Violations detected:")
        print()
        
        # Group violations by file
        violations_by_file = {}
        for v in all_violations:
            if v.file_path not in violations_by_file:
                violations_by_file[v.file_path] = []
            violations_by_file[v.file_path].append(v)
        
        for file_path, violations in sorted(violations_by_file.items()):
            print(f"\n{file_path}:")
            for v in violations:
                print(f"  Line {v.line_num}: {v.reason}")
                print(f"    > {v.line}")
        
        print()
        print(f"Total violations: {len(all_violations)}")
        print()
        print("Please review the violations and ensure the core is free from")
        print("telemetry, analytics, and sensitive logging code.")
        sys.exit(1)
    else:
        print("✅ AUDIT PASSED - No violations detected")
        print()
        print("The core library is clean and free from:")
        print("  - Telemetry collection")
        print("  - Analytics tracking")
        print("  - Network communication")
        print("  - Sensitive data logging")
        sys.exit(0)


if __name__ == '__main__':
    main()
