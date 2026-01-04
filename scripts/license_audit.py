#!/usr/bin/env python3
"""
License Audit Script for UserLAnd
Scans Gradle dependencies and generates a license report.
"""

import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Set

# Known licenses and their metadata
APPROVED_LICENSES = {
    'Apache-2.0', 'Apache License 2.0', 'Apache 2.0',
    'MIT', 'MIT License',
    'BSD', 'BSD-2-Clause', 'BSD-3-Clause',
    'GPL-2.0', 'GPL-3.0',
    'LGPL-2.1', 'LGPL-3.0',
    'EPL-1.0', 'EPL-2.0',
    'MPL-2.0',
    'CC0-1.0', 'Public Domain',
}

# Dependencies with known licenses
KNOWN_LICENSES = {
    'androidx.*': 'Apache-2.0',
    'com.google.android.*': 'Apache-2.0',
    'com.google.code.gson:gson': 'Apache-2.0',
    'org.jetbrains.kotlin:.*': 'Apache-2.0',
    'org.jetbrains.kotlinx:.*': 'Apache-2.0',
    'com.squareup.okhttp3:.*': 'Apache-2.0',
    'com.squareup.retrofit2:.*': 'Apache-2.0',
    'io.reactivex.*': 'Apache-2.0',
    'junit:junit': 'EPL-1.0',
    'org.mockito:.*': 'MIT',
    'org.robolectric:.*': 'MIT',
}

class LicenseAuditor:
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir
        self.dependencies: List[Dict] = []
        self.unknown_licenses: Set[str] = set()
        
    def run_gradle_dependencies(self) -> str:
        """Run gradle dependencies task and capture output."""
        print("🔍 Running Gradle dependencies task...")
        try:
            result = subprocess.run(
                ['./gradlew', 'app:dependencies', '--configuration', 'releaseRuntimeClasspath'],
                cwd=self.root_dir,
                capture_output=True,
                text=True,
                timeout=300
            )
            return result.stdout
        except subprocess.TimeoutExpired:
            print("⚠️  Gradle task timed out")
            return ""
        except Exception as e:
            print(f"❌ Error running Gradle: {e}")
            return ""
    
    def parse_dependencies(self, output: str):
        """Parse gradle dependencies output."""
        print("📋 Parsing dependencies...")
        
        # Pattern to match dependency lines like:
        # +--- androidx.appcompat:appcompat:1.6.1
        # |    +--- androidx.core:core:1.9.0 -> 1.13.1
        pattern = r'[\+\\\-\|]\s+[\+\\\-]+\s+([a-zA-Z0-9\.\-_]+:[a-zA-Z0-9\.\-_]+):([0-9\.\-a-zA-Z]+)'
        
        seen = set()
        for line in output.split('\n'):
            match = re.search(pattern, line)
            if match:
                group = match.group(1)
                version = match.group(2).split(' -> ')[-1]  # Handle version conflicts
                dep_id = f"{group}:{version}"
                
                if dep_id not in seen:
                    seen.add(dep_id)
                    license_name = self.detect_license(group)
                    self.dependencies.append({
                        'group': group,
                        'version': version,
                        'license': license_name
                    })
                    
                    if license_name == 'UNKNOWN':
                        self.unknown_licenses.add(group)
        
        print(f"✓ Found {len(self.dependencies)} dependencies")
    
    def detect_license(self, group: str) -> str:
        """Detect license for a dependency group."""
        for pattern, license_name in KNOWN_LICENSES.items():
            if re.match(pattern.replace('*', '.*'), group):
                return license_name
        return 'UNKNOWN'
    
    def generate_markdown_report(self) -> str:
        """Generate markdown report."""
        report = ["# Third-Party Notices", "", "## Dependencies", ""]
        
        # Group by license
        by_license: Dict[str, List] = {}
        for dep in self.dependencies:
            license_name = dep['license']
            if license_name not in by_license:
                by_license[license_name] = []
            by_license[license_name].append(dep)
        
        for license_name in sorted(by_license.keys()):
            deps = by_license[license_name]
            report.append(f"### {license_name}")
            report.append("")
            for dep in sorted(deps, key=lambda x: x['group']):
                report.append(f"- `{dep['group']}:{dep['version']}`")
            report.append("")
        
        return '\n'.join(report)
    
    def generate_json_report(self) -> str:
        """Generate JSON report."""
        return json.dumps({
            'dependencies': self.dependencies,
            'unknown_licenses': list(self.unknown_licenses),
            'stats': {
                'total': len(self.dependencies),
                'unknown': len(self.unknown_licenses)
            }
        }, indent=2)
    
    def check_compliance(self) -> bool:
        """Check if all dependencies have known licenses."""
        if self.unknown_licenses:
            print("\n❌ License Audit FAILED")
            print(f"\nFound {len(self.unknown_licenses)} dependencies with unknown licenses:")
            for dep in sorted(self.unknown_licenses):
                print(f"  - {dep}")
            print("\nPlease add license information to KNOWN_LICENSES in the script.")
            return False
        else:
            print("\n✓ License Audit PASSED")
            print(f"All {len(self.dependencies)} dependencies have known licenses")
            return True

def main():
    root_dir = Path(__file__).parent.parent
    auditor = LicenseAuditor(root_dir)
    
    # Run gradle dependencies
    output = auditor.run_gradle_dependencies()
    if not output:
        print("❌ Failed to get dependencies")
        sys.exit(1)
    
    # Parse dependencies
    auditor.parse_dependencies(output)
    
    # Generate reports
    md_report = auditor.generate_markdown_report()
    json_report = auditor.generate_json_report()
    
    # Save reports
    notices_file = root_dir / 'THIRD_PARTY_NOTICES.md'
    json_file = root_dir / 'build' / 'license_report.json'
    
    notices_file.write_text(md_report)
    print(f"\n📄 Markdown report: {notices_file}")
    
    json_file.parent.mkdir(exist_ok=True)
    json_file.write_text(json_report)
    print(f"📄 JSON report: {json_file}")
    
    # Check compliance
    success = auditor.check_compliance()
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
