#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RAFCODE-Φ Encoder - Prototype/Draft
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
License: See repository LICENSE

RAFCODE-Φ encoding for metadata and authorship seals.
Embeds cryptographic signatures and provenance information.

IMPORTANT: This is a prototype/draft implementation.
"""

import sys
import json
import hashlib
from datetime import datetime
from pathlib import Path
from typing import Dict


def generate_rafcode_seal(data: Dict) -> str:
    """
    Generate RAFCODE-Φ seal for data.
    
    Args:
        data: Dictionary containing data to seal
        
    Returns:
        RAFCODE-Φ seal string
    """
    # Create canonical JSON representation
    canonical = json.dumps(data, sort_keys=True, separators=(',', ':'))
    
    # Generate hash
    hash_obj = hashlib.sha256(canonical.encode('utf-8'))
    signature = hash_obj.hexdigest()[:16]
    
    # Create seal
    seal = {
        'type': 'RAFCODE-Φ',
        'version': '0.1.0',
        'author': '@rafaelmeloreisnovo',
        'timestamp': datetime.utcnow().isoformat() + 'Z',
        'signature': signature,
        'status': 'PROTOTYPE-DRAFT'
    }
    
    return f"RAFCODE-Φ:{json.dumps(seal, separators=(',', ':'))}"


def encode_file_metadata(filepath: Path) -> Dict:
    """
    Create RAFCODE-Φ metadata for a file.
    
    Args:
        filepath: Path to file
        
    Returns:
        Metadata dictionary with RAFCODE-Φ seal
    """
    try:
        stat = filepath.stat()
        with open(filepath, 'rb') as f:
            content_hash = hashlib.sha256(f.read()).hexdigest()
    except Exception as e:
        print(f"❌ Error reading file: {e}")
        return {}
    
    metadata = {
        'filename': filepath.name,
        'path': str(filepath),
        'size': stat.st_size,
        'content_hash': content_hash,
        'author': '@rafaelmeloreisnovo',
        'seal': generate_rafcode_seal({'file': filepath.name, 'hash': content_hash})
    }
    
    return metadata


def main():
    """Main entry point."""
    print("=" * 70)
    print("RAFCODE-Φ Encoder - Prototype v0.1.0")
    print(f"Author: @rafaelmeloreisnovo | RAFCODE-Φ seal")
    print("=" * 70)
    
    if len(sys.argv) < 2:
        print("Usage: python rafacode_encoder.py <file_or_directory>")
        print("\nGenerates RAFCODE-Φ seals and metadata for files.")
        print("=" * 70)
        sys.exit(1)
    
    path = Path(sys.argv[1])
    
    if not path.exists():
        print(f"❌ Path not found: {path}")
        sys.exit(1)
    
    if path.is_file():
        metadata = encode_file_metadata(path)
        print("\n📝 File Metadata:")
        print(json.dumps(metadata, indent=2))
    elif path.is_dir():
        print(f"\n📁 Encoding directory: {path}")
        for file_path in path.rglob('*'):
            if file_path.is_file():
                metadata = encode_file_metadata(file_path)
                print(f"\n✅ {file_path.relative_to(path)}")
                print(f"   Seal: {metadata.get('seal', 'N/A')[:60]}...")
    
    print("=" * 70)


if __name__ == '__main__':
    main()
