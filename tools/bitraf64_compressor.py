#!/usr/bin/env python3
"""
BitRAF64 Compressor - Prototype
Author: @rafaelmeloreisnovo
RAFCODE-Φ Seal: ⟨Φ⟩-BITRAF64-COMPRESS-v1.0
License: MIT

BitRAF64 is a simple compression scheme that combines basic compression
with base64 encoding for safe transmission.

This is a PROTOTYPE implementation. Production use requires optimization
and extensive testing.
"""

import sys
import zlib
import base64
from pathlib import Path
from typing import Optional


class BitRAF64Compressor:
    """Simple compressor using zlib + base64"""
    
    def __init__(self, compression_level: int = 6):
        """
        Initialize compressor
        
        Args:
            compression_level: zlib compression level (0-9, default 6)
        """
        if not 0 <= compression_level <= 9:
            raise ValueError("Compression level must be between 0 and 9")
        self.compression_level = compression_level
    
    def compress_data(self, data: bytes) -> str:
        """
        Compress data and encode as BitRAF64
        
        Args:
            data: Raw bytes to compress
            
        Returns:
            BitRAF64 encoded string
        """
        # Compress with zlib
        compressed = zlib.compress(data, level=self.compression_level)
        
        # Encode to base64
        encoded = base64.b64encode(compressed).decode('ascii')
        
        # Add BitRAF64 header
        return f"BITRAF64:{encoded}"
    
    def compress_file(self, input_path: Path, output_path: Optional[Path] = None) -> Path:
        """
        Compress a file
        
        Args:
            input_path: Path to input file
            output_path: Path to output file (optional, defaults to input + .br64)
            
        Returns:
            Path to compressed file
        """
        if not input_path.exists():
            raise FileNotFoundError(f"Input file not found: {input_path}")
        
        # Read input file
        with open(input_path, 'rb') as f:
            data = f.read()
        
        # Compress
        compressed = self.compress_data(data)
        
        # Determine output path
        if output_path is None:
            output_path = input_path.with_suffix(input_path.suffix + '.br64')
        
        # Write output
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(compressed)
        
        # Print statistics
        original_size = len(data)
        compressed_size = len(compressed)
        ratio = (1 - compressed_size / original_size) * 100 if original_size > 0 else 0
        
        print(f"✓ Compressed: {input_path.name}")
        print(f"  Original size:   {original_size:,} bytes")
        print(f"  Compressed size: {compressed_size:,} bytes")
        print(f"  Compression:     {ratio:.1f}%")
        
        return output_path


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("BitRAF64 Compressor")
        print(f"Usage: {sys.argv[0]} <input_file> [output_file] [compression_level]")
        print()
        print("Arguments:")
        print("  input_file:        File to compress")
        print("  output_file:       Output file (optional, defaults to input.br64)")
        print("  compression_level: 0-9 (optional, default 6)")
        print()
        print("Example:")
        print(f"  {sys.argv[0]} data.txt data.txt.br64 9")
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2]) if len(sys.argv) >= 3 else None
    compression_level = int(sys.argv[3]) if len(sys.argv) >= 4 else 6
    
    try:
        compressor = BitRAF64Compressor(compression_level=compression_level)
        result = compressor.compress_file(input_path, output_path)
        print(f"✓ Output written to: {result}")
        sys.exit(0)
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
