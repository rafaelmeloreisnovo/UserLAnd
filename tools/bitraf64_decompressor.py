#!/usr/bin/env python3
"""
BitRAF64 Decompressor - Prototype
Author: @rafaelmeloreisnovo
RAFCODE-Φ Seal: ⟨Φ⟩-BITRAF64-DECOMPRESS-v1.0
License: MIT

Decompressor for BitRAF64 compressed data.

This is a PROTOTYPE implementation. Production use requires optimization
and extensive testing.
"""

import sys
import zlib
import base64
from pathlib import Path
from typing import Optional


class BitRAF64Decompressor:
    """Decompressor for BitRAF64 format"""
    
    HEADER = "BITRAF64:"
    
    def decompress_data(self, bitraf64_string: str) -> bytes:
        """
        Decompress BitRAF64 encoded data
        
        Args:
            bitraf64_string: BitRAF64 encoded string
            
        Returns:
            Decompressed bytes
        """
        # Check header
        if not bitraf64_string.startswith(self.HEADER):
            raise ValueError("Invalid BitRAF64 format: missing header")
        
        # Remove header
        encoded = bitraf64_string[len(self.HEADER):]
        
        # Decode from base64
        try:
            compressed = base64.b64decode(encoded)
        except Exception as e:
            raise ValueError(f"Invalid base64 encoding: {e}")
        
        # Decompress with zlib
        try:
            data = zlib.decompress(compressed)
        except Exception as e:
            raise ValueError(f"Decompression failed: {e}")
        
        return data
    
    def decompress_file(self, input_path: Path, output_path: Optional[Path] = None) -> Path:
        """
        Decompress a BitRAF64 file
        
        Args:
            input_path: Path to compressed file
            output_path: Path to output file (optional)
            
        Returns:
            Path to decompressed file
        """
        if not input_path.exists():
            raise FileNotFoundError(f"Input file not found: {input_path}")
        
        # Read compressed file
        with open(input_path, 'r', encoding='utf-8') as f:
            compressed_data = f.read()
        
        # Decompress
        data = self.decompress_data(compressed_data)
        
        # Determine output path
        if output_path is None:
            # Try to remove .br64 extension
            if input_path.suffix == '.br64':
                output_path = input_path.with_suffix('')
            else:
                output_path = input_path.with_suffix('.decompressed')
        
        # Write output
        with open(output_path, 'wb') as f:
            f.write(data)
        
        # Print statistics
        compressed_size = len(compressed_data)
        decompressed_size = len(data)
        
        print(f"✓ Decompressed: {input_path.name}")
        print(f"  Compressed size:   {compressed_size:,} bytes")
        print(f"  Decompressed size: {decompressed_size:,} bytes")
        
        return output_path


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("BitRAF64 Decompressor")
        print(f"Usage: {sys.argv[0]} <input_file> [output_file]")
        print()
        print("Arguments:")
        print("  input_file:  BitRAF64 compressed file")
        print("  output_file: Output file (optional)")
        print()
        print("Example:")
        print(f"  {sys.argv[0]} data.txt.br64 data.txt")
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2]) if len(sys.argv) >= 3 else None
    
    try:
        decompressor = BitRAF64Decompressor()
        result = decompressor.decompress_file(input_path, output_path)
        print(f"✓ Output written to: {result}")
        sys.exit(0)
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
