#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BITRAF64 Decompressor - Prototype/Draft
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
License: See repository LICENSE

BITRAF64 decompression algorithm implementation (prototype).
Extracts data compressed with BITRAF64 format.

IMPORTANT: This is a prototype/draft implementation.
"""

import sys
import base64
import zlib
import json
from pathlib import Path
from typing import Tuple


def decompress_data(bitraf64_str: str) -> Tuple[bytes, dict]:
    """
    Decompress BITRAF64 encoded data.
    
    Args:
        bitraf64_str: BITRAF64 encoded string
        
    Returns:
        Tuple of (decompressed_data, metadata)
    """
    if not bitraf64_str.startswith('BITRAF64:'):
        raise ValueError("Invalid BITRAF64 format - missing header")
    
    # Remove header and decode
    encoded = bitraf64_str[9:]  # Remove "BITRAF64:"
    json_str = base64.b64decode(encoded).decode('utf-8')
    package = json.loads(json_str)
    
    # Validate seal
    if package.get('seal') != 'RAFCODE-Φ':
        raise ValueError("Invalid RAFCODE-Φ seal")
    
    # Decompress data
    compressed = base64.b64decode(package['data'])
    data = zlib.decompress(compressed)
    
    metadata = {
        'version': package.get('version'),
        'author': package.get('author'),
        'original_size': package.get('original_size'),
        'compressed_size': package.get('compressed_size'),
        'metadata': package.get('metadata', {})
    }
    
    return data, metadata


def decompress_file(input_path: Path, output_path: Path) -> int:
    """
    Decompress a BITRAF64 file.
    
    Args:
        input_path: Input BITRAF64 file path
        output_path: Output file path
        
    Returns:
        Exit code (0 for success)
    """
    try:
        with open(input_path, 'r') as f:
            bitraf64_str = f.read()
    except Exception as e:
        print(f"❌ Error reading input file: {e}")
        return 1
    
    try:
        data, metadata = decompress_data(bitraf64_str)
    except Exception as e:
        print(f"❌ Decompression error: {e}")
        return 1
    
    try:
        with open(output_path, 'wb') as f:
            f.write(data)
    except Exception as e:
        print(f"❌ Error writing output file: {e}")
        return 1
    
    print(f"✅ Decompressed {input_path.name}")
    print(f"   Author: {metadata['author']}")
    print(f"   Version: {metadata['version']}")
    print(f"   Original size: {metadata['original_size']} bytes")
    print(f"   Output: {output_path}")
    
    return 0


def main():
    """Main entry point."""
    print("=" * 70)
    print("BITRAF64 Decompressor - Prototype v0.1.0")
    print(f"Author: @rafaelmeloreisnovo | RAFCODE-Φ seal")
    print("=" * 70)
    
    if len(sys.argv) < 3:
        print("Usage: python bitraf64_decompressor.py <input_file> <output_file>")
        print("=" * 70)
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    
    if not input_path.exists():
        print(f"❌ Input file not found: {input_path}")
        sys.exit(1)
    
    exit_code = decompress_file(input_path, output_path)
    
    print("=" * 70)
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
