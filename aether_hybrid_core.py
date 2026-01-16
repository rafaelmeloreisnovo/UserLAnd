#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
AETHER HYBRID CORE (CLI)
Motores:
  - AETHER : hash não-criptográfico (FNV+rotate) em Python para cargas pequenas
             + fallback turbo via BLAKE2b para arquivos grandes.
  - IRON   : SHA-256 (hashlib).
  - BLAKE  : BLAKE2b/256 (hashlib).
Uso:
  python aether_hybrid_core.py alvo [-e aether|iron|blake] [-w WIDTH] [-f]
"""

import argparse
import hashlib
import os
import sys
import time
from io import BytesIO
from typing import BinaryIO, Dict

# -----------------------
# 1) Núcleo AETHER (Python puro, para payload pequeno)
# -----------------------

OFFSET = 0xCBF29CE484222325
PRIME  = 0x100000001B3
MASK64 = (1 << 64) - 1

def rotl64(x, r):
    r &= 63
    return ((x << r) & MASK64) | (x >> (64 - r))

def aether_update(state: int, chunk: bytes) -> int:
    """Versão FNV+rotate, byte/word-wise, para payload pequeno."""
    h = state
    data = chunk
    n_blocks = len(data) // 8
    for i in range(n_blocks):
        k = int.from_bytes(data[i*8:(i+1)*8], "little", signed=False)
        h ^= k
        h = (h * PRIME) & MASK64
        h = rotl64(h, 31)
        h ^= (h >> 33)
    tail = data[n_blocks*8:]
    for b in tail:
        h ^= b
        h = (h * PRIME) & MASK64
    return h

def aether_small_stream(stream: BinaryIO, width_bits: int = 256, chunk_size: int = 4 * 1024 * 1024) -> Dict:
    """
    AETHER puro em Python – usar apenas para payload até alguns MB.
    width_bits <= 1024 (saída concatenando lanes de 64 bits).
    """
    passes = max(1, min(16, width_bits // 64))
    states = [OFFSET ^ (i * 0x9E3779B97F4A7C15 & MASK64) for i in range(passes)]

    total_bytes = 0
    t0 = time.time()
    while True:
        chunk = stream.read(chunk_size)
        if not chunk:
            break
        total_bytes += len(chunk)
        for i in range(passes):
            states[i] = aether_update(states[i], chunk)
    t1 = time.time()

    digest_hex = "".join(f"{h:016x}" for h in states)
    elapsed = max(1e-9, t1 - t0)
    mb = total_bytes / (1024.0 * 1024.0)
    speed = mb / elapsed
    return {
        "engine": "AETHER_PY",
        "width_bits": passes * 64,
        "hex": digest_hex,
        "bytes": total_bytes,
        "mb": mb,
        "seconds": elapsed,
        "mb_per_s": speed,
    }

# -----------------------
# 2) Fallback AETHER TURBO (BLAKE2b por baixo)
# -----------------------

def aether_big_stream(stream: BinaryIO, width_bits: int = 256, chunk_size: int = 4 * 1024 * 1024) -> Dict:
    """
    AETHER_TURBO: implementado em cima de BLAKE2b (hashlib),
    para payload grande (dezenas/centenas de MB).
    width_bits: 64..1024.
    - Até 256 bits: 1 digest BLAKE2b/256, truncado.
    - >256 bits   : múltiplos digests 256-bit concatenados.
    """
    if width_bits < 64:
        width_bits = 64
    if width_bits > 1024:
        width_bits = 1024

    if width_bits <= 256:
        h = hashlib.blake2b(digest_size=32, person=b"AETHER_TURBO")
        total_bytes = 0
        t0 = time.time()
        while True:
            chunk = stream.read(chunk_size)
            if not chunk:
                break
            total_bytes += len(chunk)
            h.update(chunk)
        t1 = time.time()
        full_hex = h.hexdigest()
        hex_len = width_bits // 4
        digest_hex = full_hex[:hex_len]
    else:
        # 512 ou 1024 bits: múltiplos BLAKE2b/256
        passes = width_bits // 256
        hashers = [
            hashlib.blake2b(digest_size=32, person=b"AETHER_TURBO_" + bytes([i]))
            for i in range(passes)
        ]
        total_bytes = 0
        t0 = time.time()
        while True:
            chunk = stream.read(chunk_size)
            if not chunk:
                break
            total_bytes += len(chunk)
            for h in hashers:
                h.update(chunk)
        t1 = time.time()
        digest_hex = "".join(h.hexdigest() for h in hashers)

    elapsed = max(1e-9, t1 - t0)
    mb = total_bytes / (1024.0 * 1024.0)
    speed = mb / elapsed
    return {
        "engine": "AETHER_TURBO_BLAKE2B",
        "width_bits": width_bits,
        "hex": digest_hex,
        "bytes": total_bytes,
        "mb": mb,
        "seconds": elapsed,
        "mb_per_s": speed,
    }

# -----------------------
# 3) IRON (SHA-256) e BLAKE puros
# -----------------------

def hash_iron_stream(stream: BinaryIO, chunk_size: int = 4 * 1024 * 1024) -> Dict:
    h = hashlib.sha256()
    total_bytes = 0
    t0 = time.time()
    while True:
        chunk = stream.read(chunk_size)
        if not chunk:
            break
        total_bytes += len(chunk)
        h.update(chunk)
    t1 = time.time()
    elapsed = max(1e-9, t1 - t0)
    mb = total_bytes / (1024.0 * 1024.0)
    speed = mb / elapsed
    return {
        "engine": "IRON_SHA256",
        "width_bits": 256,
        "hex": h.hexdigest(),
        "bytes": total_bytes,
        "mb": mb,
        "seconds": elapsed,
        "mb_per_s": speed,
    }

def hash_blake_stream(stream: BinaryIO, chunk_size: int = 4 * 1024 * 1024) -> Dict:
    h = hashlib.blake2b(digest_size=32)
    total_bytes = 0
    t0 = time.time()
    while True:
        chunk = stream.read(chunk_size)
        if not chunk:
            break
        total_bytes += len(chunk)
        h.update(chunk)
    t1 = time.time()
    elapsed = max(1e-9, t1 - t0)
    mb = total_bytes / (1024.0 * 1024.0)
    speed = mb / elapsed
    return {
        "engine": "BLAKE2B_256",
        "width_bits": 256,
        "hex": h.hexdigest(),
        "bytes": total_bytes,
        "mb": mb,
        "seconds": elapsed,
        "mb_per_s": speed,
    }

# -----------------------
# 4) Camada de conveniência: arquivo vs texto
# -----------------------

SMALL_THRESHOLD_BYTES = 8 * 1024 * 1024  # 8 MB

def validate_width_bits(width_bits: int) -> int:
    if width_bits < 64 or width_bits > 1024:
        raise ValueError("largura inválida: use 64..1024 bits")
    if width_bits % 64 != 0:
        raise ValueError("largura inválida: use múltiplos de 64 bits")
    return width_bits

def validate_engine(engine: str) -> str:
    allowed = {"aether", "iron", "blake"}
    if engine not in allowed:
        raise ValueError(f"engine inválido: {engine}")
    return engine

def open_binary_stream(path: str) -> BinaryIO:
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    if not os.path.isfile(path):
        raise IsADirectoryError(path)
    return open(path, "rb")

def hash_text(text: str, engine: str = "aether", width_bits: int = 256):
    engine = validate_engine(engine)
    width_bits = validate_width_bits(width_bits)
    try:
        b = text.encode("utf-8")
    except UnicodeEncodeError as exc:
        raise ValueError("falha ao codificar texto em UTF-8") from exc
    stream = BytesIO(b)
    if engine == "aether":
        # texto normalmente é pequeno → AETHER_PY
        return aether_small_stream(stream, width_bits=width_bits)
    if engine == "iron":
        return hash_iron_stream(stream)
    return hash_blake_stream(stream)

def hash_file(path: str, engine: str = "aether", width_bits: int = 256):
    engine = validate_engine(engine)
    width_bits = validate_width_bits(width_bits)
    try:
        size = os.path.getsize(path)
    except OSError as exc:
        raise OSError(f"falha ao ler tamanho do arquivo: {path}") from exc

    if engine == "aether":
        # Se for pequeno: AETHER_PY; se for grande: AETHER_TURBO_BLAKE2B
        if size <= SMALL_THRESHOLD_BYTES:
            with open_binary_stream(path) as f:
                return aether_small_stream(f, width_bits=width_bits)
        with open_binary_stream(path) as f:
            return aether_big_stream(f, width_bits=width_bits)
    if engine == "iron":
        with open_binary_stream(path) as f:
            return hash_iron_stream(f)
    with open_binary_stream(path) as f:
        return hash_blake_stream(f)

# -----------------------
# 5) CLI
# -----------------------

def print_human(res: dict):
    print(f"Engine     : {res['engine']}")
    print(f"Width      : {res['width_bits']} bits")
    print(f"Bytes      : {res['bytes']}  (~{res['mb']:.4f} MB)")
    print(f"Time (s)   : {res['seconds']:.6f}")
    print(f"Speed      : {res['mb_per_s']:.2f} MB/s")
    print(f"Hex digest : {res['hex']}")

def _positive_int(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("use um inteiro válido") from exc
    if parsed <= 0:
        raise argparse.ArgumentTypeError("use um inteiro positivo")
    return parsed

def _exit_error(message: str, code: int = 1) -> None:
    print(f"[ERROR] {message}", file=sys.stderr)
    sys.exit(code)

def _main():
    parser = argparse.ArgumentParser()
    parser.add_argument("target", help="arquivo ou texto")
    parser.add_argument(
        "-e", "--engine",
        choices=["aether", "iron", "blake"],
        default="aether",
        help="motor: aether | iron | blake",
    )
    parser.add_argument(
        "-w", "--width",
        type=_positive_int,
        default=256,
        help="largura em bits (AETHER): 64..1024",
    )
    parser.add_argument(
        "-f", "--formatted",
        action="store_true",
        help="saída humana (default: uma linha JSON-like)",
    )
    args = parser.parse_args()

    target = args.target
    engine = args.engine
    width = args.width

    # Heurística simples: se existe arquivo com esse nome → arquivo; senão → texto literal
    try_file = os.path.exists(target)

    try:
        if try_file:
            res = hash_file(target, engine=engine, width_bits=width)
        else:
            # Tratar sempre como texto se não for arquivo
            res = hash_text(target, engine=engine, width_bits=width)

        if args.formatted:
            print_human(res)
        else:
            # saída compacta, fácil de grepar/parsing
            print(res)
    except (FileNotFoundError, IsADirectoryError):
        _exit_error(f"arquivo inválido ou inexistente: {target}")
    except PermissionError:
        _exit_error(f"sem permissão para ler: {target}")
    except ValueError as exc:
        _exit_error(str(exc))
    except OSError as exc:
        _exit_error(f"erro de I/O: {exc}")
    except BrokenPipeError:
        sys.exit(0)

if __name__ == "__main__":
    _main()
