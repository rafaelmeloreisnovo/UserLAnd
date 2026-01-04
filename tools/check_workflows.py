#!/usr/bin/env python3
"""
RAFAELIA Workflow Validator
Author: @rafaelmeloreisnovo
RAFCODE-Φ Seal: ⟨Φ⟩-WORKFLOW-VALIDATOR-v1.0
License: MIT

This script analyzes GitHub Actions workflow files line-by-line to detect
potential issues, security concerns, and best practices violations.
"""

import sys
import json
import yaml
import re
from pathlib import Path
from typing import List, Dict, Any


class WorkflowValidator:
    """Validates GitHub Actions workflow files"""
    
    def __init__(self, workflows_dir: str):
        self.workflows_dir = Path(workflows_dir)
        self.findings = []
        
    def validate_all(self) -> List[Dict[str, Any]]:
        """Validate all workflow files in the directory"""
        if not self.workflows_dir.exists():
            self.findings.append({
                "severity": "error",
                "file": str(self.workflows_dir),
                "line": 0,
                "message": f"Workflows directory not found: {self.workflows_dir}"
            })
            return self.findings
            
        workflow_files = list(self.workflows_dir.glob("*.yml")) + list(self.workflows_dir.glob("*.yaml"))
        
        if not workflow_files:
            self.findings.append({
                "severity": "info",
                "file": str(self.workflows_dir),
                "line": 0,
                "message": "No workflow files found"
            })
            return self.findings
            
        for workflow_file in workflow_files:
            self.validate_workflow(workflow_file)
            
        return self.findings
    
    def validate_workflow(self, workflow_path: Path):
        """Validate a single workflow file"""
        try:
            with open(workflow_path, 'r', encoding='utf-8') as f:
                content = f.read()
                lines = content.split('\n')
                
            # Parse YAML
            try:
                workflow = yaml.safe_load(content)
            except yaml.YAMLError as e:
                self.findings.append({
                    "severity": "error",
                    "file": str(workflow_path.name),
                    "line": e.problem_mark.line if hasattr(e, "problem_mark") and e.problem_mark is not None else 0,
                    "message": f"YAML parsing error: {str(e)}"
                })
                return
                
            # Line-by-line checks
            for line_num, line in enumerate(lines, start=1):
                self.check_line(workflow_path.name, line_num, line)
                
            # Structural checks
            self.check_structure(workflow_path.name, workflow)
            
        except Exception as e:
            self.findings.append({
                "severity": "error",
                "file": str(workflow_path.name),
                "line": 0,
                "message": f"Failed to validate workflow: {str(e)}"
            })
    
    def check_line(self, filename: str, line_num: int, line: str):
        """Check a single line for issues"""
        # Check for hardcoded secrets
        secret_patterns = [
            r'password\s*:\s*["\'](?!.*\$\{\{)',
            r'token\s*:\s*["\'](?!.*\$\{\{)',
            r'api[_-]?key\s*:\s*["\'](?!.*\$\{\{)',
        ]
        
        for pattern in secret_patterns:
            if re.search(pattern, line, re.IGNORECASE):
                self.findings.append({
                    "severity": "critical",
                    "file": filename,
                    "line": line_num,
                    "message": "Potential hardcoded secret detected"
                })
        
        # Check for unsafe shell execution
        if 'run:' in line:
            if '${{' in line and 'github.event' in line:
                self.findings.append({
                    "severity": "warning",
                    "file": filename,
                    "line": line_num,
                    "message": "Potential command injection via github.event - ensure proper input sanitization"
                })
        
        # Check for deprecated actions
        if 'actions/checkout@v1' in line or 'actions/checkout@v2' in line:
            self.findings.append({
                "severity": "info",
                "file": filename,
                "line": line_num,
                "message": "Consider updating to actions/checkout@v4 for latest features"
            })
    
    def check_structure(self, filename: str, workflow: Dict[str, Any]):
        """Check workflow structure for best practices"""
        if not workflow:
            return
            
        # Check if workflow has a name
        if 'name' not in workflow:
            self.findings.append({
                "severity": "info",
                "file": filename,
                "line": 0,
                "message": "Workflow should have a descriptive 'name' field"
            })
        
        # Check for jobs
        if 'jobs' not in workflow:
            self.findings.append({
                "severity": "warning",
                "file": filename,
                "line": 0,
                "message": "Workflow has no jobs defined"
            })
            return
            
        jobs = workflow.get('jobs', {})
        for job_name, job_config in jobs.items():
            if not isinstance(job_config, dict):
                continue
                
            # Check for timeout
            if 'timeout-minutes' not in job_config:
                self.findings.append({
                    "severity": "info",
                    "file": filename,
                    "line": 0,
                    "message": f"Job '{job_name}' should define timeout-minutes to prevent hanging"
                })


def main():
    """Main entry point"""
    # Determine workflows directory
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    workflows_dir = repo_root / ".github" / "workflows"
    
    print(f"🔍 Validating workflows in: {workflows_dir}")
    
    validator = WorkflowValidator(str(workflows_dir))
    findings = validator.validate_all()
    
    # Generate output
    output_file = repo_root / "workflow_findings.json"
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(findings, f, indent=2)
    
    print(f"✓ Validation complete. Findings written to: {output_file}")
    print(f"📊 Summary: {len(findings)} finding(s)")
    
    # Print summary by severity
    severity_counts = {}
    for finding in findings:
        severity = finding.get('severity', 'unknown')
        severity_counts[severity] = severity_counts.get(severity, 0) + 1
    
    for severity, count in sorted(severity_counts.items()):
        emoji = {"critical": "🔴", "error": "❌", "warning": "⚠️", "info": "ℹ️"}.get(severity, "❓")
        print(f"  {emoji} {severity}: {count}")
    
    # Exit with error if critical or error findings
    if any(f.get('severity') in ['critical', 'error'] for f in findings):
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
