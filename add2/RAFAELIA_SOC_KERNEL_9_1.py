#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# ╔═══════════════════════════════════════════════════════════════════════╗
# ║ RAFAELIA KERNEL 9.1: ACADEMIC DISRUPTOR · BBS & CONFIG EDITION       ║
# ╠═══════════════════════════════════════════════════════════════════════╣
# ║ TYPE:    Research-Grade SOC Simulator (Float vs Fixed-Point 8.8)      ║
# ║ TARGET:  Android 15 (Termux/ARM64) | Low-Latency / High-Throughput    ║
# ║ NORM:    ISO 25010 (Usabilidade), IEEE 754 (Float), NIST (Log Integrity) ║
# ║ LOGFMT:  JSONL, schema="RAFAELIA_SOC_METRICS_v1"                      ║
# ╚═══════════════════════════════════════════════════════════════════════╝

import numpy as np
import time
import sys
import platform
import os
import argparse
import json
import math # Import math for log2 in entropy calculation

# --- ⚙️ GLOBAL HARDWARE CONFIG ---

FLOAT_TYPE = np.float32
INT_TYPE   = np.uint16      # Fixed-Point 8.8
FP_SHIFT   = 8
FP_ONE     = 1 << FP_SHIFT
FP_MAX_VAL = (255 * FP_ONE)

IS_ANDROID = ("ANDROID_ROOT" in os.environ or
              "termux" in os.getenv("SHELL", "").lower())
# Verifica se stdout é um TTY antes de tentar get_terminal_size
try:
    TERM_WIDTH = os.get_terminal_size().columns if sys.stdout.isatty() else 80
except:
    TERM_WIDTH = 80 # Fallback

CONFIG_FILE = "rafaelia_soc_config.json"
METRICS_LOG = "rafaelia_metrics.log.jsonl"
SCHEMA_VER  = "RAFAELIA_SOC_METRICS_v1"

# --- 🎨 UI / UX (ANSI) ---

C_CYAN   = "\033[36m"
C_GREEN  = "\033[32m"
C_YELLOW = "\033[33m"
C_RED    = "\033[31m"
C_RESET  = "\033[0m"
C_BOLD   = "\033[1m"

def ui_banner():
    if sys.stdout.isatty():
        os.system('cls' if os.name == 'nt' else 'clear')
    print(f"{C_CYAN}")
    # Nota: ASCII art é longa, reduzida para clareza
    print(r"""
   ██████╗  █████╗ ███████╗███████╗███████╗██╗     ██╗ █████╗
   ██╔══██╗██╔══██╗██╔════╝██╔════╝██╔════╝██║     ██║██╔══██╗
   ██████╔╝███████║█████╗  ███████╗█████╗  ██║     ██║███████║
   ██╔══██╗██╔══██║██╔══╝  ██╔══██║██╔══╝  ██║     ██║██╔══██║
   ██║  ██║██║  ██║██║     ██║  ██║███████╗███████╗██║██║  ██║
   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝╚═╝  ╚═╝
    >>> SOC KERNEL 9.1 [ACADEMIC DISRUPTOR] <<<
    >>> Restauratio Gaia: Amor · Ciência · Ação Ética
    """)
    print(f"{C_RESET}")
    print(f" {C_YELLOW}HARDWARE:{C_RESET} {platform.machine()} | {platform.system()}")
    print(f" {C_YELLOW}PRECISION:{C_RESET} Fixed-Point 8.8 (Bitwise) vs Float32")
    print(" " + "═" * (TERM_WIDTH - 2))

def ui_progress_bar(iteration, total, prefix='', length=30, last_iops=0.0):
    """Minimal progress bar com IOPS aproximado."""
    if total <= 0:
        total = 1
    percent = 100.0 * (iteration / float(total))
    filled_length = int(length * iteration // total)
    bar = '█' * filled_length + '░' * (length - filled_length)
    color = C_GREEN if last_iops > 1e7 else C_YELLOW
    sys.stdout.write(
        f'\r {prefix} |{color}{bar}{C_RESET}| {percent:4.1f}% '
        f'[{last_iops:4.1e} OPS] '
    )
    sys.stdout.flush()

# --- 📦 CONFIG (Salvar / Carregar defaults) ---

DEFAULT_CONFIG = {
    "n":       64 if IS_ANDROID else 100,
    "iters":   100,
    "alpha":   0.05,
    "diff":    0.02,
    "seed":    None,
    "scan_alpha": None
}

def load_config():
    """Carrega config padrão; se não existir, usa DEFAULT_CONFIG."""
    cfg = DEFAULT_CONFIG.copy()
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                disk_cfg = json.load(f)
            # Atualiza apenas chaves existentes
            for k, v in DEFAULT_CONFIG.items():
                cfg[k] = disk_cfg.get(k, v)
        except Exception:
            pass # Ignora arquivo corrompido, usa defaults
    return cfg

def save_config(cfg):
    """Salva config atual como default."""
    try:
        # Remove scan_alpha para salvar um default limpo
        save_cfg = cfg.copy()
        if "scan_alpha" in save_cfg:
             del save_cfg["scan_alpha"]
             
        with open(CONFIG_FILE, "w") as f:
            json.dump(save_cfg, f, indent=2)
        print(f"{C_GREEN}[OK]{C_RESET} Config salva em {CONFIG_FILE}")
    except Exception as e:
        print(f"{C_RED}[ERR]{C_RESET} Falha ao salvar config: {e}")

# --- 📐 MATH & PHYSICS KERNELS ---

def fn_calc_entropy(data: np.ndarray) -> float:
    """Shannon Entropy (densidade de informação) estável."""
    if data.size == 0:
        return 0.0

    if data.dtype == INT_TYPE:
        flat = (data >> FP_SHIFT).ravel()
        bins = 256
    else:
        flat = data.ravel()
        bins = 100

    counts, _ = np.histogram(flat, bins=bins)
    counts = counts[counts > 0]
    if counts.size == 0:
        return 0.0
    probs = counts / flat.size
    # Usa np.log2 para operações vetorizadas
    return float(-np.sum(probs * np.log2(probs)))

def fn_soc_float_kernel(T, threshold, alpha, diffusion_rate):
    """SOC em Float32 com difusão (Laplaciano 6-vizinhos)."""
    active = T > threshold
    avalanches = int(np.count_nonzero(active))

    if avalanches > 0:
        # Adição de ruído/dissipação local (Regime Caótico)
        noise = np.random.rand(*T.shape).astype(FLOAT_TYPE)
        T_active = T[active]
        T_active *= FLOAT_TYPE(0.7)
        T_active += FLOAT_TYPE(0.3) * noise[active]
        T[active] = T_active

        # Difusão (Laplaciano) via slicing
        Uc = T[1:-1, 1:-1, 1:-1]
        L = (
            T[2:, 1:-1, 1:-1] + T[:-2, 1:-1, 1:-1] +
            T[1:-1, 2:, 1:-1] + T[1:-1, :-2, 1:-1] +
            T[1:-1, 1:-1, 2:] + T[1:-1, 1:-1, :-2] -
            FLOAT_TYPE(6.0) * Uc
        )
        T[1:-1, 1:-1, 1:-1] += FLOAT_TYPE(diffusion_rate) * L

    # Threshold adaptativo (EMA)
    curr_mean = float(np.mean(T))
    threshold = (1.0 - alpha) * threshold + alpha * curr_mean
    threshold = max(0.1, min(1.0, threshold))
    return T, threshold, avalanches

def fn_soc_bitwise_kernel(T_fp, threshold_fp, step_idx):
    """SOC em Fixed-Point 8.8 com difusão e clamp (uint16)."""
    active = (T_fp >> FP_SHIFT) > (threshold_fp >> FP_SHIFT)
    avalanches = int(np.count_nonzero(active))

    if avalanches > 0:
        val = T_fp[active]

        # Decaimento/Ruído (XOR Physics - Deterministic Chaos)
        noise = (val ^ (val >> 4) ^ (step_idx & 0xFF)) & 0x1F # Noise LSB
        leak  = val >> 3  # Vazão simulada (1/8th)
        val   = val - (val >> 2) + noise - (leak * 6) # Conservative Update

        val = np.clip(val, 0, FP_MAX_VAL).astype(INT_TYPE)
        T_fp[active] = val

        # Difusão (Low-Level Bitwise)
        flow = (T_fp >> 7).astype(INT_TYPE)  # ~ T_fp / 128
        core = T_fp[1:-1, 1:-1, 1:-1]
        core += (
            flow[2:, 1:-1, 1:-1] + flow[:-2, 1:-1, 1:-1] +
            flow[1:-1, 2:, 1:-1] + flow[1:-1, :-2, 1:-1] +
            flow[1:-1, 1:-1, 2:] + flow[1:-1, 1:-1, :-2]
        )
        core[:] = np.clip(core, 0, FP_MAX_VAL).astype(INT_TYPE)

    new_thresh = int(np.mean(T_fp))
    new_thresh = max(20 * FP_ONE, min(240 * FP_ONE, new_thresh))
    return T_fp, new_thresh, avalanches

# --- 📊 HISTOGRAMAS / EXPORTAÇÃO ---

def fn_get_hist_from_array(arr: np.ndarray):
    """Histograma log-binned (para avaliar power law de avalanche sizes)."""
    # BUG FIX v9.1: np.array(list) pode ser vazio ou com mais de 1 elemento.
    if arr.size == 0: 
        return [], []
    
    data = arr.astype(np.float64)
    data = data[data > 0]
    
    if data.size == 0:
        return [], []
        
    max_val = float(data.max())
    if max_val <= 1.0:
        max_val = 10.0
        
    # Bins logarítmicos
    try:
        # Garante que logspace começa em 1, não 0
        bins = np.logspace(0, np.log10(max_val), 10)
        h, e = np.histogram(data, bins=bins)
        return h.tolist(), e.tolist()
    except ValueError:
        return [], [] # Retorna vazio se logspace falhar (ex: max_val muito perto de 0)

def fn_export_data(metrics, entropy, avalanches, args_dict):
    """Exporta dados científicos para JSONL, com Ω consistente."""
    aval_float = np.array(avalanches["float"], dtype=np.int64)
    aval_bit   = np.array(avalanches["bitwise"], dtype=np.int64)

    h_float_c, h_float_e = fn_get_hist_from_array(aval_float)
    h_bit_c,   h_bit_e   = fn_get_hist_from_array(aval_bit)

    # Coerência Ω = H_bit / H_float (Conferência de Regime)
    eps   = 1e-6
    Hf    = entropy["float"]
    Hb    = entropy["bitwise"]
    omega = Hb / (Hf + eps) if Hf > 0 else 0.0

    report = {
        "schema": SCHEMA_VER,
        "ts": time.time(),
        "ver": "9.1",
        "hardware": {
            "os": platform.system(),
            "arch": platform.machine(),
            "android": IS_ANDROID,
        },
        "params": args_dict,
        "metrics": metrics,
        "physics": {
            "entropy": entropy,
            "coherence_omega": omega
        },
        "avalanches": {
            "float_mean": float(aval_float.mean() if aval_float.size else 0.0),
            "float_std":  float(aval_float.std() if aval_float.size else 0.0),
            "bit_mean":   float(aval_bit.mean() if aval_bit.size else 0.0),
            "bit_std":    float(aval_bit.std()  if aval_bit.size else 0.0),
        },
        "histograms": {
            "float_counts": h_float_c,
            "float_edges":  h_float_e,
            "bit_counts":   h_bit_c,
            "bit_edges":    h_bit_e,
        },
    }

    try:
        with open(METRICS_LOG, "a") as f:
            f.write(json.dumps(report) + "\n")
        return omega
    except Exception as e:
        print(f"{C_RED}[ERR]{C_RESET} Falha ao escrever log: {e}")
        return omega

# --- 🚀 RUNTIME / BENCHMARK ---

def run_benchmark(config, quiet=False):
    """Executa um benchmark completo com base em um dict de config."""
    n      = int(config["n"])
    iters  = int(config["iters"])
    alpha  = float(config["alpha"])
    diff   = float(config["diff"])
    seed   = config.get("seed", None)

    if seed is not None:
        np.random.seed(int(seed))

    size        = (n, n, n)
    total_cells = n ** 3

    if not quiet:
        print(f"\n{C_BOLD}>>> INITIALIZING MATRICES ({n}^3 cells)...{C_RESET}")

    T_f = np.random.rand(*size).astype(FLOAT_TYPE)
    T_b = (np.random.rand(*size) * FP_MAX_VAL).astype(INT_TYPE)

    th_f = 0.8
    th_b = 200 * FP_ONE

    stats = {"float": [], "bitwise": []}

    # FLOAT RUN
    t0 = time.time()
    for i in range(iters):
        T_f, th_f, av = fn_soc_float_kernel(T_f, th_f, alpha, diff)
        stats["float"].append(av)
        if not quiet and (i % 5 == 0):
            dt   = time.time() - t0
            iops = (total_cells * (i + 1)) / (dt + 1e-9)
            ui_progress_bar(i + 1, iters, f"{C_CYAN}FLOAT32{C_RESET}", last_iops=iops)
    tf = time.time() - t0
    if not quiet:
        sys.stdout.write("\n")

    # BITWISE RUN
    t1 = time.time()
    for i in range(iters):
        T_b, th_b, av = fn_soc_bitwise_kernel(T_b, th_b, i)
        stats["bitwise"].append(av)
        if not quiet and (i % 5 == 0):
            dt   = time.time() - t1
            iops = (total_cells * (i + 1)) / (dt + 1e-9)
            ui_progress_bar(i + 1, iters, f"{C_GREEN}BITWISE{C_RESET}", last_iops=iops)
    tb = time.time() - t1
    if not quiet:
        sys.stdout.write("\n")

    ent_f = fn_calc_entropy(T_f)
    ent_b = fn_calc_entropy(T_b)

    metrics = {
        "float_time": tf,
        "bit_time":   tb,
        "speedup":    tf / tb if tb > 0 else 0.0,
        "cells":      total_cells,
        "iters":      iters,
    }

    entropy = {"float": ent_f, "bitwise": ent_b}

    omega = fn_export_data(metrics, entropy, stats, config)

    if not quiet:
        print(f"\n{C_BOLD}╔══════════ RESULTS SUMMARY ══════════╗{C_RESET}")
        print(f"║ {C_CYAN}Float Time:{C_RESET}   {tf:6.4f}s  (Ent: {ent_f:4.2f})  ║")
        print(f"║ {C_GREEN}Bitwise Time:{C_RESET} {tb:6.4f}s  (Ent: {ent_b:4.2f})  ║")
        print(f"║ {C_YELLOW}Speedup:{C_RESET}      {metrics['speedup']:4.2f}x FASTER           ║")
        print(f"║ {C_RED}Coherence Ω:{C_RESET}  {omega:5.3f}                 ║")
        print(f"╚═════════════════════════════════════╝")

        # Interpretação coerente de Ω
        if omega < 0.9:
            print(f"{C_YELLOW}[WARN]{C_RESET} Bitwise mais ordenado (entropia menor) que float. [Risco de Estagnação]")
        elif omega > 1.1:
            print(f"{C_YELLOW}[WARN]{C_RESET} Bitwise mais caótico (entropia maior) que float. [Risco de Saturação]")
        else:
            print(f"{C_GREEN}[OK]{C_RESET} Regimes comparáveis (criticidade saudável).")

    return omega

# --- 🧾 BBS MENU (INTERFACE INTERATIVA) ---

def bbs_menu():
    cfg = load_config()

    while True:
        ui_banner()
        print(f"{C_BOLD}CONFIG ATUAL:{C_RESET} N={cfg['n']}, iters={cfg['iters']}, "
              f"alpha={cfg['alpha']}, diff={cfg['diff']}, seed={cfg['seed']}")
        print("\n MENU:")
        print("  [1] Benchmark RÁPIDO   (N atual, iters = 20)")
        print("  [2] Benchmark PADRÃO   (config atual)")
        print("  [3] Benchmark PROFUNDO (iters = 200)")
        print("  [4] Ajustar parâmetros manualmente")
        print("  [5] Salvar config como DEFAULT")
        print("  [6] Recarregar config DEFAULT do disco")
        print("  [X] Sair")
        choice = input("\n RAFAELIA> ").strip().upper()

        if choice == "1":
            local_cfg = cfg.copy()
            local_cfg["iters"] = 20
            run_benchmark(local_cfg)
            input("\n[ENTER] continuar...")
        elif choice == "2":
            run_benchmark(cfg)
            input("\n[ENTER] continuar...")
        elif choice == "3":
            local_cfg = cfg.copy()
            local_cfg["iters"] = 200
            run_benchmark(local_cfg)
            input("\n[ENTER] continuar...")
        elif choice == "4":
            # Ajuste simples por input
            try:
                n_input = input(f"N (Grid Size) atual [{cfg['n']}]: ").strip()
                if n_input: cfg["n"] = int(n_input)

                it_input = input(f"Iters atual [{cfg['iters']}]: ").strip()
                if it_input: cfg["iters"] = int(it_input)

                a_input = input(f"Alpha (Threshold) atual [{cfg['alpha']}]: ").strip()
                if a_input: cfg["alpha"] = float(a_input)

                d_input = input(f"Diff (Difusão) atual [{cfg['diff']}]: ").strip()
                if d_input: cfg["diff"] = float(d_input)

                s_input = input(f"Seed (Reprodutibilidade) atual [{cfg['seed']}]: ").strip()
                cfg["seed"] = int(s_input) if s_input else None

            except ValueError:
                print(f"{C_RED}[ERR]{C_RESET} Entrada inválida. Mantendo valores anteriores.")
            input("\n[ENTER] continuar...")
        elif choice == "5":
            save_config(cfg)
            input("\n[ENTER] continuar...")
        elif choice == "6":
            cfg = load_config()
            print(f"{C_GREEN}[OK]{C_RESET} Config recarregada.")
            input("\n[ENTER] continuar...")
        elif choice == "X":
            print("\n[SHUTDOWN] Encerrando interface BBS...")
            break
        else:
            # qualquer outra tecla → redesenhar menu
            continue

# --- 🧠 MAIN / CLI HEADLESS ---

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["ui", "bench"], default="ui",
                        help="ui: Interactive BBS Menu. bench: Headless CLI.")
    parser.add_argument("--n", type=int, default=None, help="Grid size N.")
    parser.add_argument("--iters", type=int, default=None, help="Number of iterations.")
    parser.add_argument("--alpha", type=float, default=None, help="Threshold EMA alpha.")
    parser.add_argument("--diff", type=float, default=None, help="Diffusion rate.")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility.")
    parser.add_argument("--scan_alpha", type=str, default=None,
                        help="Alpha parameter scan: ex: '0.02,0.05,0.1'")
    args = parser.parse_args()

    if args.mode == "ui":
        bbs_menu()
    else:
        # HEADLESS BENCH (para CI / automação)
        cfg = load_config()
        # Sobrescreve config do disco com argumentos CLI se fornecidos
        if args.n is not None: cfg["n"] = args.n
        if args.iters is not None: cfg["iters"] = args.iters
        if args.alpha is not None: cfg["alpha"] = args.alpha
        if args.diff is not None: cfg["diff"] = args.diff
        if args.seed is not None: cfg["seed"] = args.seed
        
        # Modo Scan
        if args.scan_alpha:
            alphas = [float(x) for x in args.scan_alpha.split(",")]
            print(f"{C_YELLOW}*** SCAN MODE: Testing {len(alphas)} Alpha regimes ***{C_RESET}")
            for a in alphas:
                cfg["alpha"] = a
                print(f"\n{C_YELLOW}---> Running for Alpha = {a}{C_RESET}")
                run_benchmark(cfg, quiet=False)
            print(f"\n{C_GREEN}*** SCAN COMPLETE ***{C_RESET} Dados em {METRICS_LOG}")
        else:
            # Modo Benchmark Simples
            run_benchmark(cfg, quiet=False)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n {C_RED}[ABORT]{C_RESET} User Interrupt (KeyboardInterrupt).")

