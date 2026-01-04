#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BITRAF64 Compressor - Prototype/Draft
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
License: See repository LICENSE

BITRAF64 compression algorithm implementation (prototype).
Provides efficient compression with RAFCODE-Φ metadata embedding.

IMPORTANT: This is a prototype/draft implementation.
"""

import sys
import base64
import zlib
import json
from pathlib import Path
from typing import Dict


def compress_data(data: bytes, metadata: Dict = None) -> str:
    """
    Compress data using BITRAF64 algorithm.
    
    Args:
        data: Raw bytes to compress
        metadata: Optional metadata dictionary
        
    Returns:
        BITRAF64 encoded string
    """
    # Compress using zlib
    compressed = zlib.compress(data, level=9)
    
    # Create BITRAF64 package
    package = {
        'version': '0.1.0',
        'seal': 'RAFCODE-Φ',
        'author': '@rafaelmeloreisnovo',
        'data': base64.b64encode(compressed).decode('ascii'),
        'original_size': len(data),
        'compressed_size': len(compressed),
        'metadata': metadata or {}
    }
    
    # Encode as JSON and base64
    json_str = json.dumps(package, separators=(',', ':'))
    bitraf64 = base64.b64encode(json_str.encode('utf-8')).decode('ascii')
    
    return f"BITRAF64:{bitraf64}"


def compress_file(input_path: Path, output_path: Path) -> int:
    """
    Compress a file using BITRAF64.
    
    Args:
        input_path: Input file path
        output_path: Output file path
        
    Returns:
        Exit code (0 for success)
    """
    try:
        with open(input_path, 'rb') as f:
            data = f.read()
    except Exception as e:
        print(f"❌ Error reading input file: {e}")
        return 1
    
    metadata = {
        'filename': input_path.name,
        'source': str(input_path)
    }
    
    compressed = compress_data(data, metadata)
    
    try:
        with open(output_path, 'w') as f:
            f.write(compressed)
    except Exception as e:
        print(f"❌ Error writing output file: {e}")
        return 1
    
    original_size = len(data)

    # Extract actual compressed byte size from BITRAF64 package
    compressed_size = None
    try:
        # Remove "BITRAF64:" prefix and decode JSON package
        if compressed.startswith("BITRAF64:"):
            payload_b64 = compressed[len("BITRAF64:"):]
        else:
            payload_b64 = compressed
        package_json = base64.b64decode(payload_b64.encode("ascii"))
        package = json.loads(package_json.decode("utf-8"))
        compressed_size = int(package.get("compressed_size", 0))
    except Exception:
        # Fallback: use the encoded string length as a rough estimate
        compressed_size = len(compressed.encode())

    if original_size > 0:
        if compressed_size <= original_size:
            size_change = (1 - compressed_size / original_size) * 100
            change_label = "reduction"
        else:
            size_change = (compressed_size / original_size - 1) * 100
            change_label = "increase"
    else:
        size_change = 0.0
        change_label = "no change"
    
    print(f"✅ Compressed {input_path.name}")
    print(f"   Original size: {original_size} bytes")
    print(f"   Compressed (raw zlib data): {compressed_size} bytes")
    print(f"   Size {change_label}: {size_change:.1f}%")
    
    return 0


def main():
    """Main entry point."""
    print("=" * 70)
    print("BITRAF64 Compressor - Prototype v0.1.0")
    print(f"Author: @rafaelmeloreisnovo | RAFCODE-Φ seal")
    print("=" * 70)
    
    if len(sys.argv) < 3:
        print("Usage: python bitraf64_compressor.py <input_file> <output_file>")
        print("=" * 70)
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    
    if not input_path.exists():
        print(f"❌ Input file not found: {input_path}")
        sys.exit(1)
    
    exit_code = compress_file(input_path, output_path)
    
    print("=" * 70)
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
