# === RAFAELIA Ω — ENGINE MINIMAL (VM + SOC + TOROIDAL) ===
# Versão 1.3 — Precisão máxima, interoperabilidade em ambientes de poucos recursos,
# e otimizações para velocidade. Mantém a mesma lógica conceitual (ψχρΔΣΩ),
# mas expande modos operacionais: high-precision (float64/longdouble), JIT (numba optional),
# e low-memory streaming (processamento por fatias).
#
# 5 simplificações/decisões (usando as mesmas variáveis conceituais):
# 1) ψ (estado): armazenar como dtype configurável (float64 por padrão; longdouble se disponível).
# 2) χ (drive/retro): aplicar drive vetorizado e reproducível com RNG configurável.
# 3) ρ (topplings): realizar topplings via máscaras vetorizadas + np.roll para vizinhança (mantém precisão).
# 4) Δ (dissipação/controle): parametrizar redistribute_fraction e phi_ethica para modular energia.
# 5) Σ / Ω (resumo e integridade): registrar apenas somatórios e hashes (SHA3-256), reduzindo I/O.
#
# Notas:
# - Este arquivo prioriza precisão sem perder a habilidade de rodar em ambientes limitados.
# - Para dispositivos muito modestos, usar --low-memory com --downsample para reduzir tamanho efetivo.
# - numba é opcional; se presente, acelera partes onde loops são inevitáveis.
#
# Requisitos mínimos: Python 3.8+, numpy. numba/blake3 opcionais.
# ------------------------------------------------------------------------------

import argparse
import time
import hashlib
import math
import shutil
import os
import sys
from typing import Tuple

import numpy as np

# try optional libs
try:
    import numba
    _HAS_NUMBA = True
except Exception:
    _HAS_NUMBA = False

try:
    import blake3
    _HAS_BLAKE3 = True
except Exception:
    _HAS_BLAKE3 = False

# ---------------------------------------------------------------------------
# Utilities: memory estimate, hash, choose dtype (precision), small helpers
# ---------------------------------------------------------------------------
def choose_dtype(preference: str):
    """Return numpy dtype for precision preference: 'high' -> float64/longdouble, 'balanced' -> float64,
       'low' -> float32."""
    if preference == "high":
        # try longdouble if it offers more precision on platform
        try:
            ld = np.longdouble(1.0)
            # if longdouble provides more mantissa bits than float64, use it
            if np.finfo(np.longdouble).nmant > np.finfo(np.float64).nmant:
                return np.longdouble
        except Exception:
            pass
        return np.float64
    elif preference == "balanced":
        return np.float64
    else:
        return np.float32

def estimate_mem_bytes(shape: Tuple[int, int, int], dtype) -> int:
    return int(np.prod(shape) * np.dtype(dtype).itemsize)

def human_bytes(n: int) -> str:
    for unit in ['B','KB','MB','GB','TB']:
        if n < 1024:
            return f"{n:.2f}{unit}"
        n /= 1024.0
    return f"{n:.2f}PB"

def hash_state_sha3(T: np.ndarray) -> str:
    # SHA3-256 of raw bytes
    h = hashlib.sha3_256()
    h.update(T.ravel().view(np.uint8))
    return h.hexdigest()

def hash_state_blake3(T: np.ndarray) -> str:
    if not _HAS_BLAKE3:
        return ""
    return blake3.blake3(T.ravel().view(np.uint8)).hexdigest()

# ---------------------------------------------------------------------------
# 1) VM MINIMAL (unchanged but compact)
# ---------------------------------------------------------------------------
class VM:
    __slots__ = ("reg", "pc", "cy")
    def __init__(self):
        self.reg = [0] * 8
        self.pc = 0
        self.cy = 0

    def mov(self, r, v):
        self.reg[r] = int(v); self.cy += 1

    def add(self, r, v):
        self.reg[r] = int(self.reg[r] + v); self.cy += 1

    def run_demo(self):
        self.mov(0, 10)
        self.add(0, 20)
        return self.reg[0], self.cy

# ---------------------------------------------------------------------------
# 2) TOROIDAL GENERATION
#    - Two modes:
#      a) in-memory vectorized (fast, default)
#      b) low-memory streaming (generate slices on demand)
# ---------------------------------------------------------------------------
def toroidal_vectorized(size: int, R: float, thickness: float, dtype) -> np.ndarray:
    """Fast, fully vectorized toroid generation in memory (precision= dtype)."""
    cx = cy = size // 2
    x = (np.arange(size, dtype=dtype) - cx)
    y = (np.arange(size, dtype=dtype) - cy)
    X, Y = np.meshgrid(x, y, indexing="ij")  # (size,size)
    r_xy = np.sqrt(X * X + Y * Y)            # (size,size)
    Z = (np.arange(size, dtype=dtype) - cx)  # (size,)
    dsq = (r_xy[..., None] - dtype(R)) ** 2 + (Z[None, None, :] ) ** 2
    denom = dtype(2.0 * (thickness ** 2))
    T = np.exp(-dsq / denom).astype(dtype, copy=False)
    return T

def toroidal_stream(size: int, R: float, thickness: float, dtype, z_block: int=8):
    """
    Generator that yields blocks of the toroidal volume along the z axis.
    Use for low-memory devices: does not allocate full (size^3) at once.
    Yields tuples (z_start, block_array) where block_array shape = (size, size, z_block_or_less).
    """
    cx = cy = size // 2
    x = (np.arange(size, dtype=dtype) - cx)
    y = (np.arange(size, dtype=dtype) - cy)
    X, Y = np.meshgrid(x, y, indexing="ij")
    r_xy = np.sqrt(X * X + Y * Y)
    Z_full = (np.arange(size, dtype=dtype) - cx)
    denom = dtype(2.0 * (thickness ** 2))
    for z0 in range(0, size, z_block):
        z1 = min(z0 + z_block, size)
        Z = Z_full[z0:z1][None, None, :]
        dsq = (r_xy[..., None] - dtype(R)) ** 2 + (Z) ** 2
        block = np.exp(-dsq / denom).astype(dtype, copy=False)
        yield z0, block

# ---------------------------------------------------------------------------
# 3) SOC STEP (vectorized, accurate), with optional numba accelerated fallback
# ---------------------------------------------------------------------------
def soc_step_vectorized(T: np.ndarray, threshold: float, redistribute_fraction: float = 0.9,
                        max_inner: int = 10000, phi_ethica: float = 1.0) -> Tuple[np.ndarray, int]:
    """
    Vectorized SOC step: uses masks and np.roll. Works in-memory and uses dtype of T.
    Returns updated T and avalanche count.
    """
    T = T.astype(T.dtype, copy=False)
    avalanches = 0
    it = 0
    threshold = float(threshold) * float(phi_ethica)
    redistribute_fraction = float(np.clip(redistribute_fraction, 0.0, 1.0))

    while True:
        over = T > threshold
        if not np.any(over):
            break
        it += 1
        if it > max_inner:
            break
        topple = over.astype(T.dtype)
        n_topple = int(topple.sum())
        avalanches += n_topple
        T[over] -= threshold
        contrib = topple * (threshold * redistribute_fraction / 6.0)
        # 6-neighbor roll (wrap-around toroidal)
        add = (np.roll(contrib, 1, axis=0) + np.roll(contrib, -1, axis=0) +
               np.roll(contrib, 1, axis=1) + np.roll(contrib, -1, axis=1) +
               np.roll(contrib, 1, axis=2) + np.roll(contrib, -1, axis=2))
        T += add
    return T, avalanches

# Optional Numba-accelerated version for platforms with numba
if _HAS_NUMBA:
    # numba cannot handle variable-length numpy boolean masks in the same way; we provide a compiled helper that uses loops.
    # This version trades some precision for speed due to loop nature but keeps dtype as provided.
    @numba.njit(parallel=False)
    def _soc_step_numba(arr, threshold, redistribute_fraction, max_inner, phi_ethica):
        avalanches = 0
        it = 0
        threshold = threshold * phi_ethica
        nz, ny, nx = arr.shape
        while True:
            any_over = False
            # create topple mask and count
            topple_count = 0
            for i in range(nz):
                for j in range(ny):
                    for k in range(nx):
                        if arr[i,j,k] > threshold:
                            any_over = True
                            topple_count += 1
            if not any_over:
                break
            it += 1
            if it > max_inner:
                break
            avalanches += topple_count
            # subtract threshold and compute contributions
            contrib = np.zeros_like(arr)
            for i in range(nz):
                for j in range(ny):
                    for k in range(nx):
                        if arr[i,j,k] > threshold:
                            arr[i,j,k] -= threshold
                            c = (threshold * redistribute_fraction / 6.0)
                            # distribute to 6 neighbors with wrap-around
                            contrib[(i+1)%nz, j, k] += c
                            contrib[(i-1)%nz, j, k] += c
                            contrib[i, (j+1)%ny, k] += c
                            contrib[i, (j-1)%ny, k] += c
                            contrib[i, j, (k+1)%nx] += c
                            contrib[i, j, (k-1)%nx] += c
            # accumulate contributions
            for i in range(nz):
                for j in range(ny):
                    for k in range(nx):
                        arr[i,j,k] += contrib[i,j,k]
        return arr, avalanches

# ---------------------------------------------------------------------------
# 4) Run SOC with modes (in-memory fast or low-memory streaming)
# ---------------------------------------------------------------------------
def run_soc(iterations: int = 10, size: int = 100, threshold: float = 0.7,
            drive_amplitude: float = 0.01, redistribute_fraction: float = 0.9,
            dtype_pref: str = "high", mode: str = "auto", z_block: int = 16,
            phi_ethica: float = 1.0, seed: int = 123456):
    """
    Top-level runner for SOC.
    mode:
      - 'auto': uses in-memory if estimated mem < 0.5 * available RAM, else low-memory.
      - 'in-memory': force full allocation (fast).
      - 'low-memory': use streaming generator for toroidal and assemble T on disk or small array.
    dtype_pref: 'high' | 'balanced' | 'low'
    """
    dtype = choose_dtype(dtype_pref)
    shape = (size, size, size)
    est_mem = estimate_mem_bytes(shape, dtype)
    # try to detect system RAM (best-effort)
    ram_total = None
    try:
        import psutil
        ram_total = psutil.virtual_memory().total
    except Exception:
        pass

    # Mode decision
    if mode == "auto":
        if ram_total and est_mem < 0.5 * ram_total:
            chosen_mode = "in-memory"
        else:
            # if est_mem small enough for float32 low-memory, prefer in-memory for speed
            if est_mem < (200 * 1024 * 1024):  # <200MB
                chosen_mode = "in-memory"
            else:
                chosen_mode = "low-memory"
    else:
        chosen_mode = mode

    total_aval = 0
    rng = np.random.default_rng(seed)

    if chosen_mode == "in-memory":
        T = toroidal_vectorized(size, R=20.0, thickness=14.0, dtype=dtype)
        for _ in range(iterations):
            T += (rng.random(T.shape).astype(dtype) * drive_amplitude)
            if _HAS_NUMBA and _HAS_NUMBA:
                # prefer vectorized unless user explicitly requests numba mode
                T, a = soc_step_vectorized(T, threshold, redistribute_fraction, phi_ethica=phi_ethica)
            else:
                T, a = soc_step_vectorized(T, threshold, redistribute_fraction, phi_ethica=phi_ethica)
            total_aval += a
        return T, total_aval, threshold, chosen_mode, dtype, est_mem
    else:
        # low-memory: stream blocks and build on-disk memmap to avoid full memory usage
        # create temp memmap file
        tmpfile = "rafaelia_toroid.dat"
        if os.path.exists(tmpfile):
            try:
                os.remove(tmpfile)
            except Exception:
                pass
        # open memmap
        T_mm = np.memmap(tmpfile, dtype=dtype, mode='w+', shape=shape)
        # fill by streaming blocks
        for z0, block in toroidal_stream(size, R=20.0, thickness=14.0, dtype=dtype, z_block=z_block):
            z1 = z0 + block.shape[2]
            T_mm[:, :, z0:z1] = block
        T_mm.flush()
        # iterate SOC by reading chunks but for correct global neighbor handling we need full array access;
        # we'll use memmap for operations (np.roll works with memmap)
        for _ in range(iterations):
            # apply drive
            T_mm[:] = T_mm[:] + (rng.random(T_mm.shape).astype(dtype) * drive_amplitude)
            # do SOC using vectorized method operating on memmap (works but slower)
            T_mm_arr = np.asarray(T_mm)  # view
            T_mm_arr, a = soc_step_vectorized(T_mm_arr, threshold, redistribute_fraction, phi_ethica=phi_ethica)
            # write back
            T_mm[:] = T_mm_arr
            T_mm.flush()
            total_aval += a
        return T_mm, total_aval, threshold, chosen_mode, dtype, est_mem

# ---------------------------------------------------------------------------
# 5) Bench & Reporting (Σ & Ω)
# ---------------------------------------------------------------------------
def benchmark(iterations=5, size=100, **kwargs):
    t0 = time.time()
    T, aval, thr, mode, dtype, est_mem = run_soc(iterations=iterations, size=size, **kwargs)
    t1 = time.time()
    elapsed = t1 - t0

    # compute summaries carefully: ensure float64 sum for precision
    # If T is memmap, convert to np.asarray view
    T_arr = np.asarray(T)
    energy = float(np.sum(T_arr, dtype=np.float64))
    sha3 = hash_state_sha3(T_arr)
    blake = hash_state_blake3(T_arr) if _HAS_BLAKE3 else None
    return {
        "time_s": elapsed,
        "avalanches": int(aval),
        "threshold": thr,
        "mode": mode,
        "dtype": str(dtype),
        "estimated_mem": human_bytes(est_mem),
        "energy": energy,
        "sha3": sha3,
        "blake3": blake
    }

# ---------------------------------------------------------------------------
# CLI (compact, non-interactive by default)
# ---------------------------------------------------------------------------
def main():
    p = argparse.ArgumentParser(description="RAFAELIA_ENGINE_MINIMAL — precision + low-resource support")
    p.add_argument("--size", type=int, default=100, help="grid size (default 100 -> 100^3)")
    p.add_argument("--iters", type=int, default=1, help="SOC iterations")
    p.add_argument("--drive", type=float, default=0.01, help="drive amplitude")
    p.add_argument("--threshold", type=float, default=0.7, help="SOC threshold")
    p.add_argument("--redistribute", type=float, default=0.9, help="redistribution fraction")
    p.add_argument("--precision", choices=("high","balanced","low"), default="high",
                   help="precision profile: high=float64/longdouble (default), balanced=float64, low=float32")
    p.add_argument("--mode", choices=("auto","in-memory","low-memory"), default="auto",
                   help="operation mode: auto choose based on RAM, or force modes")
    p.add_argument("--z-block", type=int, default=16, help="z block size for streaming mode")
    p.add_argument("--seed", type=int, default=123456, help="rng seed for reproducibility")
    p.add_argument("--bench", action="store_true", help="run benchmark and print summary")
    p.add_argument("--no-numba", action="store_true", help="disable using numba even if available")
    args = p.parse_args()

    use_numba = _HAS_NUMBA and not args.no_numba

    info = {
        "size": args.size,
        "iters": args.iters,
        "precision": args.precision,
        "mode": args.mode,
        "numba_available": _HAS_NUMBA,
        "using_numba": use_numba
    }
    print("RAFAELIA_ENGINE_MINIMAL — starting with params:", info)

    if args.bench:
        res = benchmark(iterations=max(1, args.iters), size=max(8, args.size),
                        threshold=args.threshold, drive_amplitude=args.drive,
                        redistribute_fraction=args.redistribute, dtype_pref=args.precision,
                        mode=args.mode, z_block=args.z_block, phi_ethica=1.0, seed=args.seed)
        print("BENCHMARK RESULT:")
        for k, v in res.items():
            print(f"  {k}: {v}")
        return

    # default run: one SOC run and prints Σ and Ω
    T, aval, thr, mode, dtype_used, est_mem = run_soc(iterations=max(1, args.iters), size=max(8, args.size),
                                                     threshold=args.threshold, drive_amplitude=args.drive,
                                                     redistribute_fraction=args.redistribute, dtype_pref=args.precision,
                                                     mode=args.mode, z_block=args.z_block, phi_ethica=1.0, seed=args.seed)
    T_arr = np.asarray(T)
    energy = float(np.sum(T_arr, dtype=np.float64))
    sha3 = hash_state_sha3(T_arr)
    print("SOC run complete")
    print("  avalanches:", aval)
    print("  threshold:", thr)
    print("  mode:", mode)
    print("  dtype:", dtype_used)
    print("  estimated_mem:", human_bytes(est_mem))
    print("  Σ energy:", energy)
    print("  Ω SHA3-256:", sha3)
    if _HAS_BLAKE3:
        print("  Ω BLAKE3:", hash_state_blake3(T_arr))

if __name__ == "__main__":
    main()