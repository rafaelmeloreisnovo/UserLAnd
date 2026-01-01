#!/usr/bin/env python3
"""
RAFAELIA Multi-Architecture Detection and Interoperability
Auto-detects CPU architecture, registers, and optimal compilation flags
Supports: x86_64, ARM, ARM64, RISC-V, MIPS, PowerPC, SPARC, s390x, and more
"""

import platform
import subprocess
import sys
import os
import json
from typing import Dict, List, Optional, Tuple

class ArchitectureDetector:
    """Detects and analyzes system architecture"""
    
    # Architecture mapping
    ARCH_MAP = {
        'x86_64': ['x86_64', 'amd64', 'AMD64'],
        'arm64': ['aarch64', 'arm64', 'ARM64'],
        'arm32': ['armv7l', 'armv6l', 'armhf', 'arm'],
        'riscv': ['riscv64', 'riscv32'],
        'mips': ['mips', 'mips64', 'mips64el', 'mipsel'],
        'powerpc': ['ppc64', 'ppc64le', 'powerpc64', 'ppc'],
        'sparc': ['sparc64', 'sparc'],
        's390x': ['s390x'],
        'alpha': ['alpha'],
        'ia64': ['ia64'],
        'hppa': ['hppa', 'parisc'],
        'm68k': ['m68k'],
    }
    
    # Register sets by architecture
    REGISTERS = {
        'x86_64': {
            'general': ['rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi', 'rbp', 'rsp', 'r8-r15'],
            'syscall': ['rax', 'rdi', 'rsi', 'rdx', 'r10', 'r8', 'r9'],
            'instruction': 'syscall',
        },
        'arm64': {
            'general': ['x0-x30', 'sp', 'pc'],
            'syscall': ['x8', 'x0', 'x1', 'x2', 'x3', 'x4', 'x5'],
            'instruction': 'svc #0',
        },
        'arm32': {
            'general': ['r0-r12', 'sp', 'lr', 'pc'],
            'syscall': ['r7', 'r0', 'r1', 'r2', 'r3', 'r4', 'r5'],
            'instruction': 'svc #0',
        },
        'riscv': {
            'general': ['x0-x31', 'pc'],
            'syscall': ['a7', 'a0', 'a1', 'a2', 'a3', 'a4', 'a5'],
            'instruction': 'ecall',
        },
        'mips': {
            'general': ['$0-$31'],
            'syscall': ['$v0', '$a0', '$a1', '$a2', '$a3'],
            'instruction': 'syscall',
        },
        'powerpc': {
            'general': ['r0-r31'],
            'syscall': ['r0', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8'],
            'instruction': 'sc',
        },
        'sparc': {
            'general': ['g0-g7', 'o0-o7', 'l0-l7', 'i0-i7'],
            'syscall': ['g1', 'o0', 'o1', 'o2', 'o3', 'o4', 'o5'],
            'instruction': 't 0x10',
        },
        's390x': {
            'general': ['r0-r15'],
            'syscall': ['r1', 'r2', 'r3', 'r4', 'r5', 'r6'],
            'instruction': 'svc 0',
        },
    }
    
    # Syscall numbers by architecture
    SYSCALLS = {
        'x86_64': {'read': 0, 'write': 1, 'open': 2, 'close': 3, 'exit': 60},
        'arm64': {'read': 63, 'write': 64, 'open': 5, 'close': 57, 'exit': 93},
        'arm32': {'read': 3, 'write': 4, 'open': 5, 'close': 6, 'exit': 1},
        'riscv': {'read': 63, 'write': 64, 'openat': 56, 'close': 57, 'exit': 93},
        'mips': {'read': 5000, 'write': 5001, 'open': 5002, 'close': 5003, 'exit': 5058},
        'powerpc': {'read': 3, 'write': 4, 'open': 5, 'close': 6, 'exit': 1},
    }
    
    def __init__(self):
        self.machine = platform.machine().lower()
        self.system = platform.system().lower()
        self.arch = self.detect_architecture()
        
    def detect_architecture(self) -> str:
        """Detect the normalized architecture name"""
        for arch, variants in self.ARCH_MAP.items():
            if any(self.machine.startswith(v.lower()) for v in variants):
                return arch
        return 'unknown'
    
    def get_cpu_info(self) -> Dict:
        """Get detailed CPU information"""
        info = {
            'machine': self.machine,
            'system': self.system,
            'arch': self.arch,
            'processor': platform.processor(),
            'bits': platform.architecture()[0],
        }
        
        # Try to get more details from /proc/cpuinfo on Linux
        if self.system == 'linux' and os.path.exists('/proc/cpuinfo'):
            try:
                with open('/proc/cpuinfo', 'r') as f:
                    for line in f:
                        if ':' in line:
                            key, value = line.split(':', 1)
                            key = key.strip().lower()
                            if key in ['model name', 'cpu', 'processor']:
                                info['cpu_model'] = value.strip()
                                break
            except:
                pass
        
        return info
    
    def get_registers(self) -> Dict:
        """Get register information for the architecture"""
        return self.REGISTERS.get(self.arch, {
            'general': ['unknown'],
            'syscall': ['unknown'],
            'instruction': 'unknown',
        })
    
    def get_syscall_numbers(self) -> Dict[str, int]:
        """Get syscall numbers for the architecture"""
        return self.SYSCALLS.get(self.arch, {})
    
    def get_compiler_flags(self) -> List[str]:
        """Get optimal compiler flags for the architecture"""
        base_flags = ['-std=c11', '-Wall', '-Wextra', '-O2']
        
        arch_flags = {
            'x86_64': ['-march=x86-64', '-mtune=generic'],
            'arm64': ['-march=armv8-a'],
            'arm32': ['-march=armv7-a', '-mfpu=neon'],
            'riscv': ['-march=rv64gc'],
            'mips': ['-march=mips64r2'],
            'powerpc': ['-mcpu=power8'],
        }
        
        return base_flags + arch_flags.get(self.arch, [])
    
    def check_features(self) -> Dict[str, bool]:
        """Check available CPU features"""
        features = {
            'sse': False,
            'sse2': False,
            'avx': False,
            'avx2': False,
            'neon': False,
            'sve': False,
        }
        
        if self.system == 'linux':
            try:
                with open('/proc/cpuinfo', 'r') as f:
                    content = f.read().lower()
                    
                    if self.arch == 'x86_64':
                        features['sse'] = 'sse' in content
                        features['sse2'] = 'sse2' in content
                        features['avx'] = 'avx' in content
                        features['avx2'] = 'avx2' in content
                    elif self.arch in ['arm64', 'arm32']:
                        features['neon'] = 'neon' in content or 'asimd' in content
                        features['sve'] = 'sve' in content
            except:
                pass
        
        return features
    
    def get_full_info(self) -> Dict:
        """Get complete architecture information"""
        return {
            'cpu_info': self.get_cpu_info(),
            'registers': self.get_registers(),
            'syscalls': self.get_syscall_numbers(),
            'compiler_flags': self.get_compiler_flags(),
            'features': self.check_features(),
        }
    
    def print_info(self):
        """Print formatted architecture information"""
        info = self.get_full_info()
        
        print("═" * 60)
        print("RAFAELIA Architecture Detection")
        print("═" * 60)
        
        print("\n🖥️  CPU Information:")
        cpu = info['cpu_info']
        print(f"  Architecture: {cpu['arch']}")
        print(f"  Machine:      {cpu['machine']}")
        print(f"  System:       {cpu['system']}")
        print(f"  Bits:         {cpu['bits']}")
        if 'cpu_model' in cpu:
            print(f"  Model:        {cpu['cpu_model']}")
        
        print("\n📋 Register Set:")
        regs = info['registers']
        print(f"  General:      {', '.join(regs['general'])}")
        print(f"  Syscall:      {', '.join(regs['syscall'])}")
        print(f"  Instruction:  {regs['instruction']}")
        
        print("\n🔧 Syscall Numbers:")
        syscalls = info['syscalls']
        if syscalls:
            for name, num in syscalls.items():
                print(f"  {name:10s} = {num}")
        else:
            print("  (Not available for this architecture)")
        
        print("\n⚙️  Compiler Flags:")
        print(f"  {' '.join(info['compiler_flags'])}")
        
        print("\n✨ CPU Features:")
        features = info['features']
        enabled = [k.upper() for k, v in features.items() if v]
        if enabled:
            print(f"  Enabled: {', '.join(enabled)}")
        else:
            print("  (No special features detected)")
        
        print("\n" + "═" * 60)

def generate_c_header(detector: ArchitectureDetector, output_file: str):
    """Generate C header with architecture-specific definitions"""
    info = detector.get_full_info()
    
    with open(output_file, 'w') as f:
        f.write("/* Auto-generated architecture-specific definitions */\n")
        f.write("/* Generated by RAFAELIA Architecture Detector */\n\n")
        
        f.write(f"#define DETECTED_ARCH \"{info['cpu_info']['arch']}\"\n")
        f.write(f"#define DETECTED_MACHINE \"{info['cpu_info']['machine']}\"\n\n")
        
        syscalls = info['syscalls']
        if syscalls:
            f.write("/* Syscall numbers for this architecture */\n")
            for name, num in syscalls.items():
                f.write(f"#define SYS_{name.upper()} {num}\n")
            f.write("\n")
        
        features = info['features']
        enabled = [k for k, v in features.items() if v]
        if enabled:
            f.write("/* Enabled CPU features */\n")
            for feat in enabled:
                f.write(f"#define HAS_{feat.upper()} 1\n")
            f.write("\n")
    
    print(f"✓ Generated C header: {output_file}")

def main():
    """Main entry point"""
    detector = ArchitectureDetector()
    
    if len(sys.argv) > 1:
        if sys.argv[1] == '--json':
            print(json.dumps(detector.get_full_info(), indent=2))
        elif sys.argv[1] == '--header':
            output = sys.argv[2] if len(sys.argv) > 2 else 'arch_detect.h'
            generate_c_header(detector, output)
        elif sys.argv[1] == '--help':
            print("Usage: python3 arch_detect.py [--json|--header [file]|--help]")
            print("\nOptions:")
            print("  --json          Output as JSON")
            print("  --header [file] Generate C header (default: arch_detect.h)")
            print("  --help          Show this help")
        else:
            detector.print_info()
    else:
        detector.print_info()

if __name__ == '__main__':
    main()
