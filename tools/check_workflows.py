#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Workflow Validator - Line-by-line YAML Analysis
Author: @rafaelmeloreisnovo
RAFCODE-Φ: WORKFLOW-VALIDATOR-v1.0

This script analyzes GitHub Actions workflow YAML files line-by-line
to detect potential issues, security concerns, and best practice violations.
"""

import os
import sys
import json
import yaml
import glob
from pathlib import Path
from typing import Dict, List, Any

class WorkflowValidator:
    """Validates GitHub Actions workflow files."""
    
    def __init__(self):
        self.findings = []
        self.errors = []
        
    def validate_workflow_file(self, filepath: str) -> Dict[str, Any]:
        """Validate a single workflow file."""
        print(f"🔍 Analyzing: {filepath}")
        
        file_findings = {
            "file": filepath,
            "issues": [],
            "warnings": [],
            "info": []
        }
        
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
                lines = content.split('\n')
                
            # Parse YAML
            try:
                workflow = yaml.safe_load(content)
            except yaml.YAMLError as e:
                file_findings["issues"].append({
                    "line": getattr(e, 'problem_mark', {}).line if hasattr(e, 'problem_mark') else 0,
                    "severity": "error",
                    "message": f"YAML parsing error: {str(e)}"
                })
                return file_findings
            
            # Line-by-line analysis
            for line_num, line in enumerate(lines, 1):
                # Check for secrets exposure
                if 'password' in line.lower() or 'token' in line.lower():
                    if '}}' in line and not '${{' in line:
                        file_findings["warnings"].append({
                            "line": line_num,
                            "severity": "warning",
                            "message": "Potential secret exposure - verify proper secret handling"
                        })
                
                # Check for unsigned container images
                if 'image:' in line and '@sha256:' not in line:
                    file_findings["info"].append({
                        "line": line_num,
                        "severity": "info",
                        "message": "Consider pinning container image to specific SHA256 digest"
                    })
                
                # Check for shell injection risks
                if 'run:' in line:
                    next_lines = lines[line_num:line_num+5]
                    for nl in next_lines:
                        if '${{' in nl and 'github.event' in nl:
                            file_findings["warnings"].append({
                                "line": line_num,
                                "severity": "warning",
                                "message": "Potential shell injection - validate user input before use"
                            })
                            break
            
            # Structural validations
            if workflow:
                # Check for workflow name
                if 'name' not in workflow:
                    file_findings["warnings"].append({
                        "line": 0,
                        "severity": "warning",
                        "message": "Workflow missing 'name' field"
                    })
                
                # Check for jobs
                if 'jobs' not in workflow:
                    file_findings["issues"].append({
                        "line": 0,
                        "severity": "error",
                        "message": "Workflow missing 'jobs' field"
                    })
                elif workflow['jobs']:
                    for job_name, job_data in workflow['jobs'].items():
                        # Check for runner specification
                        if 'runs-on' not in job_data:
                            file_findings["issues"].append({
                                "line": 0,
                                "severity": "error",
                                "message": f"Job '{job_name}' missing 'runs-on' field"
                            })
                        
                        # Check for steps
                        if 'steps' not in job_data or not job_data['steps']:
                            file_findings["warnings"].append({
                                "line": 0,
                                "severity": "warning",
                                "message": f"Job '{job_name}' has no steps defined"
                            })
            
            # Summary
            total_issues = len(file_findings["issues"]) + len(file_findings["warnings"])
            if total_issues == 0:
                file_findings["info"].append({
                    "line": 0,
                    "severity": "info",
                    "message": "✓ No critical issues found"
                })
                
        except Exception as e:
            file_findings["issues"].append({
                "line": 0,
                "severity": "error",
                "message": f"Failed to process file: {str(e)}"
            })
            
        return file_findings
    
    def validate_all_workflows(self, workflows_dir: str = ".github/workflows") -> List[Dict[str, Any]]:
        """Validate all workflow files in the directory."""
        print(f"🔨 Starting workflow validation in: {workflows_dir}")
        
        if not os.path.exists(workflows_dir):
            print(f"❌ Error: Workflows directory not found: {workflows_dir}")
            return []
        
        workflow_files = glob.glob(os.path.join(workflows_dir, "*.yml")) + \
                        glob.glob(os.path.join(workflows_dir, "*.yaml"))
        
        if not workflow_files:
            print(f"⚠️  Warning: No workflow files found in {workflows_dir}")
            return []
        
        print(f"📋 Found {len(workflow_files)} workflow file(s)")
        
        results = []
        for workflow_file in sorted(workflow_files):
            result = self.validate_workflow_file(workflow_file)
            results.append(result)
        
        return results
    
    def generate_report(self, results: List[Dict[str, Any]], output_file: str = "workflow-validation-report.json"):
        """Generate JSON report of findings."""
        report = {
            "timestamp": str(Path(output_file).stat().st_mtime if os.path.exists(output_file) else ""),
            "total_files": len(results),
            "results": results,
            "summary": {
                "total_errors": sum(len(r["issues"]) for r in results),
                "total_warnings": sum(len(r["warnings"]) for r in results),
                "total_info": sum(len(r["info"]) for r in results)
            }
        }
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        
        print(f"\n✓ Report generated: {output_file}")
        print(f"📊 Summary: {report['summary']['total_errors']} errors, "
              f"{report['summary']['total_warnings']} warnings, "
              f"{report['summary']['total_info']} info")
        
        return report

def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Validate GitHub Actions workflow files',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        '--workflows-dir',
        default='.github/workflows',
        help='Path to workflows directory (default: .github/workflows)'
    )
    parser.add_argument(
        '--output',
        default='workflow-validation-report.json',
        help='Output JSON report file (default: workflow-validation-report.json)'
    )
    
    args = parser.parse_args()
    
    validator = WorkflowValidator()
    results = validator.validate_all_workflows(args.workflows_dir)
    
    if results:
        report = validator.generate_report(results, args.output)
        
        # Exit with error if critical issues found
        if report['summary']['total_errors'] > 0:
            print("\n❌ Critical errors found in workflows!")
            sys.exit(1)
        else:
            print("\n✓ Workflow validation completed successfully")
            sys.exit(0)
    else:
        print("\n⚠️  No workflows to validate")
        sys.exit(0)

if __name__ == '__main__':
    main()
