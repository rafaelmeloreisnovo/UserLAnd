#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RAFAELIA Workflow Validator - Prototype/Draft
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
License: See repository LICENSE

This tool validates GitHub Actions workflow files for security, correctness,
and compliance with repository policies.

IMPORTANT: This is a prototype/draft. Do not execute any infinite loops.
Ethica[8] blocks prohibited executions.
"""

import os
import sys
import yaml
import re
from pathlib import Path
from typing import Dict, List, Tuple


def validate_workflow_file(workflow_path: Path) -> Tuple[bool, List[str]]:
    """
    Validate a single workflow YAML file.
    
    Args:
        workflow_path: Path to workflow file
        
    Returns:
        Tuple of (is_valid, list_of_errors)
    """
    errors = []
    
    try:
        with open(workflow_path, 'r') as f:
            workflow = yaml.safe_load(f)
    except yaml.YAMLError as e:
        errors.append(f"YAML parsing error: {e}")
        return False, errors
    except Exception as e:
        errors.append(f"File read error: {e}")
        return False, errors
    
    # Check for required fields
    if not workflow:
        errors.append("Empty workflow file")
        return False, errors
    
    if 'name' not in workflow:
        errors.append("Missing 'name' field")
    
    if 'on' not in workflow:
        errors.append("Missing 'on' (trigger) field")
    
    if 'jobs' not in workflow:
        errors.append("Missing 'jobs' field")
    
    # Check for security best practices
    jobs = workflow.get('jobs', {})
    for job_name, job_config in jobs.items():
        if isinstance(job_config, dict):
            steps = job_config.get('steps', [])
            for idx, step in enumerate(steps):
                if isinstance(step, dict):
                    # Check for script injection vulnerabilities
                    run_cmd = step.get('run', '')
                    if '${{' in run_cmd and 'github.event' in run_cmd:
                        errors.append(
                            f"Job '{job_name}' step {idx}: Potential script injection "
                            f"using github.event context"
                        )
                    
                    # Check for actions without version pinning
                    uses = step.get('uses', '')
                    if uses and '@' not in uses:
                        errors.append(
                            f"Job '{job_name}' step {idx}: Action '{uses}' not pinned to version"
                        )
    
    return len(errors) == 0, errors


def validate_all_workflows(workflows_dir: Path) -> int:
    """
    Validate all workflow files in directory.
    
    Args:
        workflows_dir: Path to .github/workflows directory
        
    Returns:
        Exit code (0 for success, 1 for failure)
    """
    if not workflows_dir.exists():
        print(f"❌ Workflows directory not found: {workflows_dir}")
        return 1
    
    workflow_files = list(workflows_dir.glob('*.yml')) + list(workflows_dir.glob('*.yaml'))
    
    if not workflow_files:
        print(f"⚠️  No workflow files found in {workflows_dir}")
        return 0
    
    all_valid = True
    print(f"🔍 Validating {len(workflow_files)} workflow file(s)...")
    
    for workflow_file in sorted(workflow_files):
        is_valid, errors = validate_workflow_file(workflow_file)
        
        if is_valid:
            print(f"✅ {workflow_file.name}: VALID")
        else:
            print(f"❌ {workflow_file.name}: INVALID")
            for error in errors:
                print(f"   - {error}")
            all_valid = False
    
    return 0 if all_valid else 1


def main():
    """Main entry point."""
    # Find repository root
    repo_root = Path(__file__).parent.parent
    workflows_dir = repo_root / '.github' / 'workflows'
    
    print("=" * 70)
    print("RAFAELIA Workflow Validator - Prototype v0.1.0")
    print(f"Author: @rafaelmeloreisnovo | RAFCODE-Φ seal")
    print("=" * 70)
    
    exit_code = validate_all_workflows(workflows_dir)
    
    print("=" * 70)
    if exit_code == 0:
        print("✅ All workflows validated successfully")
    else:
        print("❌ Workflow validation failed")
    print("=" * 70)
    
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
