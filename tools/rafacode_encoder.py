#!/usr/bin/env python3
"""
RAFCODE-Φ Encoder - Prototype
Author: @rafaelmeloreisnovo
RAFCODE-Φ Seal: ⟨Φ⟩-RAFCODE-ENCODER-v1.0
License: MIT

RAFCODE-Φ (RAFCODE-Phi) is a symbolic encoding system that combines
cryptographic hashing with semantic markers for code authentication
and provenance tracking.

This is a PROTOTYPE/SKELETON implementation. Production use requires
cryptographic review and extensive testing.
"""

import sys
import hashlib
import json
from pathlib import Path
from datetime import datetime
from typing import Dict, Any, Optional


class RAFCODEEncoder:
    """
    RAFCODE-Φ Encoder
    
    Generates unique seals for files using:
    - SHA-256 hash
    - Timestamp
    - Author information
    - Semantic tags
    """
    
    def __init__(self, author: str = "@rafaelmeloreisnovo"):
        self.author = author
        self.version = "1.0"
    
    def encode_file(self, filepath: Path, component_name: Optional[str] = None) -> Dict[str, Any]:
        """
        Generate RAFCODE-Φ seal for a file
        
        Args:
            filepath: Path to file
            component_name: Optional semantic name for component
            
        Returns:
            Dictionary with seal information
        """
        if not filepath.exists():
            raise FileNotFoundError(f"File not found: {filepath}")
        
        # Read file content
        with open(filepath, 'rb') as f:
            content = f.read()
        
        # Calculate hash
        sha256_hash = hashlib.sha256(content).hexdigest()
        
        # Generate seal
        timestamp = datetime.utcnow().isoformat() + 'Z'
        
        if component_name is None:
            component_name = filepath.stem.upper()
        
        seal = {
            "seal": f"⟨Φ⟩-{component_name}-v{self.version}",
            "author": self.author,
            "file": str(filepath.name),
            "hash": {
                "algorithm": "SHA-256",
                "value": sha256_hash
            },
            "timestamp": timestamp,
            "version": self.version
        }
        
        return seal
    
    def generate_seal_string(self, filepath: Path, component_name: Optional[str] = None) -> str:
        """
        Generate a compact seal string for embedding in source code
        
        Args:
            filepath: Path to file
            component_name: Optional semantic name
            
        Returns:
            Compact seal string
        """
        seal = self.encode_file(filepath, component_name)
        return seal["seal"]
    
    def generate_header_comment(self, 
                                filepath: Path, 
                                component_name: Optional[str] = None,
                                comment_style: str = "#") -> str:
        """
        Generate a header comment with RAFCODE-Φ seal
        
        Args:
            filepath: Path to file
            component_name: Optional semantic name
            comment_style: Comment character(s) to use
            
        Returns:
            Multi-line header comment
        """
        seal = self.encode_file(filepath, component_name)
        
        lines = [
            f"{comment_style} Author: {seal['author']}",
            f"{comment_style} RAFCODE-Φ Seal: {seal['seal']}",
            f"{comment_style} Hash (SHA-256): {seal['hash']['value'][:16]}...",
            f"{comment_style} Generated: {seal['timestamp']}"
        ]
        
        return "\n".join(lines)


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("RAFCODE-Φ Encoder")
        print(f"Usage: {sys.argv[0]} <file> [component_name]")
        print()
        print("Arguments:")
        print("  file:           File to encode")
        print("  component_name: Optional semantic name (default: filename)")
        print()
        print("Example:")
        print(f"  {sys.argv[0]} script.py MY-SCRIPT")
        sys.exit(1)
    
    filepath = Path(sys.argv[1])
    component_name = sys.argv[2] if len(sys.argv) >= 3 else None
    
    try:
        encoder = RAFCODEEncoder()
        
        # Generate full seal
        seal = encoder.encode_file(filepath, component_name)
        
        print("=" * 60)
        print("RAFCODE-Φ SEAL GENERATED")
        print("=" * 60)
        print()
        print(json.dumps(seal, indent=2))
        print()
        print("Compact seal string:")
        print(f"  {seal['seal']}")
        print()
        print("Header comment (Python/Shell):")
        print("-" * 60)
        header = encoder.generate_header_comment(filepath, component_name)
        print(header)
        print("-" * 60)
        print()
        print("✓ RAFCODE-Φ seal generated successfully")
        
        # Optionally write seal to JSON file
        seal_file = filepath.with_suffix('.rafcode.json')
        with open(seal_file, 'w', encoding='utf-8') as f:
            json.dump(seal, f, indent=2)
        print(f"✓ Seal metadata written to: {seal_file}")
        
        sys.exit(0)
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
