#!/usr/bin/env python3
"""
RAFAELIA Ethica[8] Engine - Prototype
Author: @rafaelmeloreisnovo
RAFCODE-Φ Seal: ⟨Φ⟩-ETHICA-ENGINE-v1.0
License: MIT

Ethica[8] is an ethical constraint engine that prevents infinite loops,
resource exhaustion, and other potentially harmful code patterns from
executing automatically in CI/CD pipelines.

This is a PROTOTYPE/SKELETON implementation. Production use requires
extensive testing and refinement.
"""

import ast
import sys
from pathlib import Path
from typing import List, Dict, Any, Tuple


class EthicaEngine:
    """
    Ethica[8] - Ethical Constraint Engine
    
    Eight core principles:
    1. No infinite loops
    2. Bounded recursion
    3. Resource limits
    4. Time constraints
    5. Memory bounds
    6. I/O throttling
    7. Safe concurrency
    8. Graceful degradation
    """
    
    def __init__(self):
        self.violations = []
        self.max_loop_iterations = 10000  # Configurable limit
        self.max_recursion_depth = 1000
        
    def analyze_python_file(self, filepath: Path) -> Tuple[bool, List[Dict[str, Any]]]:
        """
        Analyze a Python file for ethical violations
        
        Returns:
            (is_safe, violations_list)
        """
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                source = f.read()
                
            tree = ast.parse(source, filename=str(filepath))
            self.violations = []
            
            # Visit all nodes in the AST
            self._visit_node(tree, str(filepath))
            
            is_safe = len(self.violations) == 0
            return is_safe, self.violations
            
        except SyntaxError as e:
            return False, [{
                "type": "syntax_error",
                "file": str(filepath),
                "line": e.lineno,
                "message": str(e)
            }]
        except Exception as e:
            return False, [{
                "type": "analysis_error",
                "file": str(filepath),
                "line": 0,
                "message": f"Failed to analyze: {str(e)}"
            }]
    
    def _visit_node(self, node: ast.AST, filepath: str):
        """Visit an AST node and check for violations"""
        
        # Check for infinite while loops
        if isinstance(node, ast.While):
            if self._is_infinite_while(node):
                self.violations.append({
                    "type": "infinite_loop",
                    "severity": "critical",
                    "file": filepath,
                    "line": node.lineno,
                    "message": "Potential infinite while loop detected (while True without break)"
                })
        
        # Check for unbounded recursion
        if isinstance(node, ast.FunctionDef):
            if self._has_unbounded_recursion(node):
                self.violations.append({
                    "type": "unbounded_recursion",
                    "severity": "high",
                    "file": filepath,
                    "line": node.lineno,
                    "message": f"Function '{node.name}' may have unbounded recursion"
                })
        
        # Check for suspicious eval/exec usage
        if isinstance(node, ast.Call):
            if isinstance(node.func, ast.Name):
                if node.func.id in ['eval', 'exec']:
                    self.violations.append({
                        "type": "dangerous_call",
                        "severity": "critical",
                        "file": filepath,
                        "line": node.lineno,
                        "message": f"Dangerous function '{node.func.id}' detected"
                    })
        
        # Recursively visit child nodes
        for child in ast.iter_child_nodes(node):
            self._visit_node(child, filepath)
    
    def _is_infinite_while(self, node: ast.While) -> bool:
        """Check if a while loop might be infinite"""
        # Simple heuristic: while True without a break statement
        if isinstance(node.test, ast.Constant) and node.test.value is True:
            # Check if there's a break statement in the body
            has_break = self._has_break_statement(node.body)
            return not has_break
        return False
    
    def _has_break_statement(self, body: List[ast.stmt]) -> bool:
        """Check if a code block contains a break statement"""
        for stmt in body:
            if isinstance(stmt, ast.Break):
                return True
            # Check nested structures
            if isinstance(stmt, (ast.If, ast.For, ast.While, ast.With)):
                if hasattr(stmt, 'body') and self._has_break_statement(stmt.body):
                    return True
                if hasattr(stmt, 'orelse') and self._has_break_statement(stmt.orelse):
                    return True
        return False
    
    def _has_unbounded_recursion(self, node: ast.FunctionDef) -> bool:
        """Check if a function has potentially unbounded recursion"""
        func_name = node.name
        
        # Look for calls to itself without clear base case
        for stmt in ast.walk(node):
            if isinstance(stmt, ast.Call):
                if isinstance(stmt.func, ast.Name) and stmt.func.id == func_name:
                    # Found recursive call - check for base case
                    # Simple heuristic: look for if statements that might be base cases
                    has_base_case = self._has_apparent_base_case(node)
                    if not has_base_case:
                        return True
        
        return False
    
    def _has_apparent_base_case(self, node: ast.FunctionDef) -> bool:
        """Check if function has an apparent base case for recursion"""
        # Look for early returns or if statements that might be base cases
        for stmt in node.body:
            if isinstance(stmt, ast.Return):
                return True
            if isinstance(stmt, ast.If):
                # If has a return statement, might be a base case
                for if_stmt in ast.walk(stmt):
                    if isinstance(if_stmt, ast.Return):
                        return True
        return False


def main():
    """Main entry point for Ethica[8] engine"""
    
    if len(sys.argv) < 2:
        print("Usage: rafaelia_ethica_engine.py <file_or_directory>")
        print("\nEthica[8] - Ethical Constraint Engine")
        print("Analyzes code for potentially harmful patterns")
        sys.exit(1)
    
    target = Path(sys.argv[1])
    engine = EthicaEngine()
    
    all_violations = []
    
    if target.is_file():
        files = [target]
    elif target.is_dir():
        files = list(target.rglob("*.py"))
    else:
        print(f"❌ Error: {target} not found")
        sys.exit(1)
    
    print(f"🔍 Ethica[8] Analysis")
    print(f"📁 Scanning: {target}")
    print(f"📄 Files to analyze: {len(files)}")
    print()
    
    for filepath in files:
        is_safe, violations = engine.analyze_python_file(filepath)
        
        if not is_safe:
            all_violations.extend(violations)
            print(f"⚠️  {filepath.name}: {len(violations)} violation(s)")
        else:
            print(f"✓  {filepath.name}: Safe")
    
    print()
    print("=" * 60)
    
    if all_violations:
        print(f"🔴 Found {len(all_violations)} ethical violation(s)")
        print()
        
        for v in all_violations:
            severity_emoji = {"critical": "🔴", "high": "🟠", "medium": "🟡", "low": "🟢"}.get(v.get('severity', 'medium'), "⚠️")
            print(f"{severity_emoji} {v['type'].upper()}")
            print(f"   File: {v['file']}")
            print(f"   Line: {v['line']}")
            print(f"   {v['message']}")
            print()
        
        sys.exit(1)
    else:
        print("✅ All files passed Ethica[8] analysis")
        print("🛡️  No infinite loops or dangerous patterns detected")
        sys.exit(0)


if __name__ == "__main__":
    main()
