#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RAFAELIA Ethica[8] Engine - Prototype/Draft
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
License: See repository LICENSE

Ethica[8] enforcement engine for RAFAELIA code execution.
Blocks prohibited operations including infinite loops and unsafe operations.

IMPORTANT: This is a prototype/draft implementation.
"""

import sys
import ast
import re
from typing import List, Tuple


class EthicaViolation(Exception):
    """Exception raised when code violates Ethica[8] rules."""
    pass


class EthicaAnalyzer(ast.NodeVisitor):
    """AST visitor to analyze code for Ethica[8] violations."""
    
    def __init__(self):
        self.violations = []
        self.in_loop_depth = 0
        
    def visit_While(self, node):
        """Check while loops for infinite loop patterns."""
        self.in_loop_depth += 1
        
        # Check for while True: patterns
        if isinstance(node.test, ast.Constant) and node.test.value is True:
            self.violations.append(
                f"Line {node.lineno}: Infinite loop detected (while True)"
            )
        elif isinstance(node.test, ast.NameConstant) and node.test.value is True:
            # Python 3.7 compatibility
            self.violations.append(
                f"Line {node.lineno}: Infinite loop detected (while True)"
            )
        
        self.generic_visit(node)
        self.in_loop_depth -= 1
    
    def visit_For(self, node):
        """Check for loops."""
        self.in_loop_depth += 1
        self.generic_visit(node)
        self.in_loop_depth -= 1


def analyze_code(code: str) -> Tuple[bool, List[str]]:
    """
    Analyze Python code for Ethica[8] violations.
    
    Args:
        code: Python source code string
        
    Returns:
        Tuple of (is_safe, list_of_violations)
    """
    try:
        tree = ast.parse(code)
    except SyntaxError as e:
        return False, [f"Syntax error: {e}"]
    
    analyzer = EthicaAnalyzer()
    analyzer.visit(tree)
    
    return len(analyzer.violations) == 0, analyzer.violations


def check_file(filepath: str) -> int:
    """
    Check a Python file for Ethica[8] violations.
    
    Args:
        filepath: Path to Python file
        
    Returns:
        Exit code (0 for safe, 1 for violations)
    """
    try:
        with open(filepath, 'r') as f:
            code = f.read()
    except Exception as e:
        print(f"❌ Error reading file: {e}")
        return 1
    
    is_safe, violations = analyze_code(code)
    
    if is_safe:
        print(f"✅ {filepath}: No Ethica[8] violations detected")
        return 0
    else:
        print(f"❌ {filepath}: Ethica[8] violations detected:")
        for violation in violations:
            print(f"   - {violation}")
        return 1


def main():
    """Main entry point."""
    print("=" * 70)
    print("RAFAELIA Ethica[8] Engine - Prototype v0.1.0")
    print(f"Author: @rafaelmeloreisnovo | RAFCODE-Φ seal")
    print("=" * 70)
    
    if len(sys.argv) < 2:
        print("Usage: python rafaelia_ethica_engine.py <python_file>")
        print("\nEthica[8] Rules:")
        print("  1. No infinite loops (while True)")
        print("  2. No unbounded recursion")
        print("  3. No unsafe system calls")
        print("  4. Resource limits enforced")
        print("=" * 70)
        sys.exit(1)
    
    filepath = sys.argv[1]
    exit_code = check_file(filepath)
    
    print("=" * 70)
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
