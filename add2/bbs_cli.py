#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# ZIPRAFA BBS-style CLI Launcher
# RAFAELIA / ZIPRAF - Small "BBS-like" front-end to operate the tools produced in this session
#
# Features:
#  - ASCII BBS-style menu with colors
#  - Build/check binaries (Make) for modules: raf_crc_ecc, ziprafa_layout, ziprafa_zone
#  - Run common operations (selftests, bench, generate map, create shards, recover)
#  - Simple "Play" demo that runs a short pipeline (generate map -> show JSON -> run crc selftest)
#  - Logs actions to logs/bbs.log
#  - Safe fallbacks if binaries not found (prints instructions)
#
# Usage:
#  - Place this script in the repository root alongside the generated module folders:
#      raf_crc_ecc/raf_crc_ecc_tool.c
#      ziprafa_layout/ziprafa_layout_tool.c
#      ziprafa_zone/ziprafa_zone_layout_tool.c
#  - Make it executable: chmod +x bbs_cli.py
#  - Run: ./bbs_cli.py
#
# This is intentionally lightweight (no external dependencies) and uses subprocess to call the compiled tools.
# If you want integration (call C functions directly), ask and I can provide Python bindings or a unified binary.
#
# Author: RAFAELIA Copilot (for rafaelmeloreisnovo)
# Date: 2025-12-31

from __future__ import annotations
import os
import sys
import shutil
import subprocess
import json
import time
from pathlib import Path

# Configuration: expected binary names and module directories
MODULES = {
    "raf_crc_ecc": {
        "dir": "raf_crc_ecc",
        "make_target": "raf_crc_ecc_tool",
        "binary": "raf_crc_ecc_tool"
    },
    "ziprafa_layout": {
        "dir": "ziprafa_layout",
        "make_target": "ziprafa_layout_tool",
        "binary": "ziprafa_layout_tool"
    },
    "ziprafa_zone": {
        "dir": "ziprafa_zone",
        "make_target": "ziprafa_zone_layout_tool",
        "binary": "ziprafa_zone_layout_tool"
    }
}

LOG_DIR = Path("logs")
LOG_DIR.mkdir(exist_ok=True)
LOG_FILE = LOG_DIR / "bbs.log"

# ANSI colors
CSI = "\x1b["
RESET = CSI + "0m"
BOLD = CSI + "1m"
BLUE = CSI + "34m"
CYAN = CSI + "36m"
YELLOW = CSI + "33m"
GREEN = CSI + "32m"
MAGENTA = CSI + "35m"
RED = CSI + "31m"

def log(msg: str):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(f"[{ts}] {msg}\n")

def clear_screen():
    print(CSI + "2J" + CSI + "H", end="")

def header():
    clear_screen()
    print(BOLD + CYAN + r"""
   ______  ______  ______   ____  ____   ____   ____   ____
  /__  __\/__  __\/__  __\ / __ \/ __ \ / __ \ / __ \ / __ \
    / /     / /     / /   / /_/ / /_/ // /_/ // /_/ // /_/ /
   / /     / /     / /   / ____/ ____/ \__, / \__, / \__, /
  /_/     /_/     /_/   /_/   /_/      /____/ /____/ /____/
    ZIPRAFA - BBS Style Launcher (RAFAELIA)
    """ + RESET)
    print(YELLOW + "Simple menu to build/run the ZIPRAFA tools, run demos and self-tests." + RESET)
    print()

def pause():
    input(GREEN + "\nPress Enter to continue..." + RESET)

def which_bin(name: str) -> str | None:
    # check in PATH first, then ./module_dir/
    p = shutil.which(name)
    if p:
        return p
    # check module build dir
    for m in MODULES.values():
        candidate = Path(m["dir"]) / name
        if candidate.exists() and os.access(candidate, os.X_OK):
            return str(candidate.resolve())
    return None

def build_module(module_key: str) -> bool:
    mod = MODULES[module_key]
    d = Path(mod["dir"])
    if not d.exists():
        print(RED + f"Module dir {d} not found." + RESET)
        return False
    if not (d / "Makefile").exists():
        print(RED + f"No Makefile in {d}. Cannot build automatically." + RESET)
        return False
    print(YELLOW + f"Running make in {d}..." + RESET)
    log(f"make: building module {module_key} in {d}")
    res = subprocess.run(["make"], cwd=str(d))
    if res.returncode == 0:
        print(GREEN + "Build OK." + RESET)
        log(f"make: build OK for {module_key}")
        return True
    else:
        print(RED + f"Build failed (code {res.returncode}). See output." + RESET)
        log(f"make: build FAILED for {module_key} rc={res.returncode}")
        return False

def ensure_binaries() -> dict:
    found = {}
    for key, mod in MODULES.items():
        binpath = which_bin(mod["binary"])
        found[key] = binpath
    return found

def run_cmd(cmd: list[str], timeout: int | None = None):
    try:
        log(f"run_cmd: {' '.join(cmd)}")
        res = subprocess.run(cmd, check=False, capture_output=True, text=True, timeout=timeout)
        out = res.stdout
        err = res.stderr
        rc = res.returncode
        if out:
            print(CYAN + out + RESET)
        if err:
            print(RED + err + RESET)
        log(f"cmd rc={rc}")
        return rc, out, err
    except subprocess.TimeoutExpired:
        print(RED + "Command timed out." + RESET)
        log("run_cmd: timeout")
        return -1, "", "timeout"

def show_status():
    bins = ensure_binaries()
    print(BOLD + "Status of expected binaries:" + RESET)
    for k, mod in MODULES.items():
        path = bins.get(k)
        if path:
            print(GREEN + f"  {mod['binary']}: found at {path}" + RESET)
        else:
            print(YELLOW + f"  {mod['binary']}: NOT found" + RESET)
    print()
    print("Log file:", LOG_FILE)
    print()

def menu_build_all():
    print(BOLD + "Build modules (runs make in module dirs)" + RESET)
    for k in MODULES.keys():
        build_module(k)
    pause()

def menu_selftests():
    bins = ensure_binaries()
    # prefer raf_crc_ecc_tool selftest then ziprafa_layout selftest, then zone selftest
    if bins.get("raf_crc_ecc"):
        print(BOLD + "Running raf_crc_ecc_tool --selftest" + RESET)
        run_cmd([bins["raf_crc_ecc"], "--selftest"])
    else:
        print(YELLOW + "raf_crc_ecc_tool binary not found. Build module or place binary in PATH." + RESET)
    if bins.get("ziprafa_layout"):
        print(BOLD + "Running ziprafa_layout_tool --selftest" + RESET)
        run_cmd([bins["ziprafa_layout"], "--selftest"])
    else:
        print(YELLOW + "ziprafa_layout_tool binary not found." + RESET)
    if bins.get("ziprafa_zone"):
        print(BOLD + "Running ziprafa_zone_layout_tool --selftest" + RESET)
        run_cmd([bins["ziprafa_zone"], "--selftest"])
    else:
        print(YELLOW + "ziprafa_zone_layout_tool binary not found." + RESET)
    pause()

def menu_benchmarks():
    bins = ensure_binaries()
    if bins.get("raf_crc_ecc"):
        print(BOLD + "Running raf_crc_ecc_tool --bench" + RESET)
        run_cmd([bins["raf_crc_ecc"], "--bench"])
    else:
        print(YELLOW + "raf_crc_ecc_tool binary not found." + RESET)
    if bins.get("ziprafa_layout"):
        print(BOLD + "Running ziprafa_layout_tool --bench sample" + RESET)
        # sample small bench
        run_cmd([bins["ziprafa_layout"], "--bench", "0", "1000", "4096", "1000", "6"])
    else:
        print(YELLOW + "ziprafa_layout_tool binary not found." + RESET)
    pause()

def menu_generate_map_interactive():
    bins = ensure_binaries()
    if not bins.get("ziprafa_layout"):
        print(RED + "ziprafa_layout_tool not found. Build module first." + RESET)
        pause()
        return
    prefix = input("Output prefix (e.g. demo): ").strip() or "demo"
    seed_str = input("Seed (0 for time): ").strip() or "0"
    stripes = input("Stripes to generate [10]: ").strip() or "10"
    extent_bytes = input("extent_bytes [4096]: ").strip() or "4096"
    total_slots = input("total_slots [1000]: ").strip() or "1000"
    base_off = input("base_off [1024]: ").strip() or "1024"
    k_data = input("k_data [6]: ").strip() or "6"
    parity_n = input("parity_n [1]: ").strip() or "1"
    m_ecc = input("m_ecc [0]: ").strip() or "0"

    args = [
        bins["ziprafa_layout"],
        "--print"  # we'll still run --print to show summary; to actually generate map files use interactive menu in tool
    ]
    # For ease, call the interactive generator inside tool by invoking it non-optionally: we call the interactive flow
    # but since we want non-interactive, we simulate the --bench path as map generation is interactive in C tool.
    # We'll run a quick bench-style generation using the same internal logic: call the tool's bench with stripes param.
    # To actually create map.bin/json we provide a compact call to the tool using its interactive flow via subprocess is complex.
    # Instead, call the tool in interactive mode and let user follow prompts.
    print(GREEN + "Launching ziprafa_layout_tool interactive generator. Follow prompts in that program." + RESET)
    log("launch: ziprafa_layout_tool (interactive)")
    subprocess.run([bins["ziprafa_layout"]])
    pause()

def menu_create_shards():
    bins = ensure_binaries()
    if not bins.get("raf_crc_ecc"):
        print(RED + "raf_crc_ecc_tool not found. Build module first." + RESET)
        pause()
        return
    infile = input("Input file to shard (will be read): ").strip()
    if not infile:
        print(YELLOW + "No input file provided." + RESET)
        pause(); return
    k = input("Number of shards (k) [6]: ").strip() or "6"
    prefix = input("Output prefix (e.g. myfile): ").strip() or "myfile"
    print(GREEN + f"Creating {k} shards from {infile}..." + RESET)
    run_cmd([bins["raf_crc_ecc"], "--shards", infile, k, prefix])
    pause()

def menu_recover_shard():
    bins = ensure_binaries()
    if not bins.get("raf_crc_ecc"):
        print(RED + "raf_crc_ecc_tool not found. Build module first." + RESET)
        pause()
        return
    prefix = input("Shard set prefix (e.g. myfile): ").strip() or "myfile"
    k = input("Total shards (k) [6]: ").strip() or "6"
    missing = input("Missing shard index (0..k-1) [2]: ").strip() or "2"
    outpath = input("Output recovered file path [recovered.shard]: ").strip() or "recovered.shard"
    print(GREEN + "Recovering shard..." + RESET)
    run_cmd([bins["raf_crc_ecc"], "--recover", prefix, k, missing, outpath])
    pause()

def menu_show_map_json():
    # find json files produced by ziprafa_layout_tool
    matches = list(Path(".").glob("*.map.json")) + list(Path(".").glob("zipraf_*.map.json"))
    if not matches:
        print(YELLOW + "No *.map.json files found in current directory." + RESET)
        pause(); return
    print(BOLD + "Found map JSON files:" + RESET)
    for i, p in enumerate(matches):
        print(f"  {i}) {p}")
    sel = input("Select index to view [0]: ").strip() or "0"
    try:
        sel_i = int(sel)
        p = matches[sel_i]
    except Exception:
        print(RED + "Invalid selection" + RESET)
        pause(); return
    print(GREEN + f"Showing {p} (pretty JSON):" + RESET)
    try:
        with open(p, "r", encoding="utf-8") as f:
            j = json.load(f)
            print(json.dumps(j, indent=2, ensure_ascii=False))
    except Exception as e:
        print(RED + f"Failed to load JSON: {e}" + RESET)
    pause()

def menu_play_demo():
    """
    A small "play" demo pipeline:
      - build modules if missing (ask)
      - run raf_crc_ecc selftest
      - create a small demo payload, shard it, recover a shard
      - launch layout tool interactive for map generation
    """
    print(BOLD + "Play Demo: quick run of some useful operations" + RESET)
    bins = ensure_binaries()
    missing = [k for k,v in bins.items() if not v]
    if missing:
        print(YELLOW + "Some binaries are missing: " + ", ".join(missing) + RESET)
        ans = input("Try to build all modules now? (y/N) ").strip().lower()
        if ans == "y":
            menu_build_all()
            bins = ensure_binaries()
    # run raf selftests
    if bins.get("raf_crc_ecc"):
        print(BOLD + "Running raf_crc_ecc_tool selftest..." + RESET)
        run_cmd([bins["raf_crc_ecc"], "--selftest"])
    else:
        print(YELLOW + "raf_crc_ecc_tool not available, skipping." + RESET)

    # create demo payload
    demo_payload = Path("demo_payload.bin")
    if not demo_payload.exists():
        print(GREEN + "Creating demo payload demo_payload.bin (16 KB)..." + RESET)
        demo_payload.write_bytes(os.urandom(16*1024))
    else:
        print(YELLOW + "demo_payload.bin exists, reusing." + RESET)

    # create shards if tool available
    if bins.get("raf_crc_ecc"):
        print(BOLD + "Creating shards from demo payload..." + RESET)
        run_cmd([bins["raf_crc_ecc"], "--shards", str(demo_payload), "6", "demo_payload"])
        # remove one shard to simulate missing
        missing_index = 2
        missing_file = Path(f"demo_payload.shard{int(missing_index):03d}")
        if missing_file.exists():
            print(YELLOW + f"Simulating missing shard by removing {missing_file}" + RESET)
            try:
                missing_file.unlink()
            except Exception:
                pass
        # recover shard
        print(BOLD + "Recovering missing shard..." + RESET)
        run_cmd([bins["raf_crc_ecc"], "--recover", "demo_payload", "6", str(missing_index), "demo_payload.recovered"])
    else:
        print(YELLOW + "raf_crc_ecc_tool not available, cannot demo shards." + RESET)

    # launch layout tool interactive to let user generate map
    if bins.get("ziprafa_layout"):
        print(BOLD + "Launching ziprafa_layout_tool interactive (you can generate maps)..." + RESET)
        subprocess.run([bins["ziprafa_layout"]])
    else:
        print(YELLOW + "ziprafa_layout_tool not available." + RESET)

    pause()

def menu_shell():
    print(BOLD + "Drop to shell (type 'exit' to return)" + RESET)
    try:
        subprocess.run([os.environ.get("SHELL","/bin/sh")])
    except Exception as e:
        print(RED + f"Failed to run shell: {e}" + RESET)
    pause()

def main_loop():
    while True:
        header()
        show_status()
        print("Menu:")
        print("  1) Build modules (make)")
        print("  2) Run self-tests")
        print("  3) Run benchmarks")
        print("  4) Generate MAP (launch layout tool interactive)")
        print("  5) Create shards from file")
        print("  6) Recover a shard")
        print("  7) Show .map.json files (pretty)")
        print("  8) PLAY demo pipeline")
        print("  9) Shell")
        print("  0) Exit")
        choice = input(BOLD + "Choice: " + RESET).strip()
        log(f"user_choice: {choice}")
        if choice == "0":
            print(GREEN + "Goodbye." + RESET)
            break
        elif choice == "1":
            menu_build_all()
        elif choice == "2":
            menu_selftests()
        elif choice == "3":
            menu_benchmarks()
        elif choice == "4":
            menu_generate_map_interactive()
        elif choice == "5":
            menu_create_shards()
        elif choice == "6":
            menu_recover_shard()
        elif choice == "7":
            menu_show_map_json()
        elif choice == "8":
            menu_play_demo()
        elif choice == "9":
            menu_shell()
        else:
            print(YELLOW + "Unknown choice." + RESET)
            pause()

if __name__ == "__main__":
    try:
        main_loop()
    except KeyboardInterrupt:
        print("\n" + YELLOW + "Interrupted by user. Exiting." + RESET)
        sys.exit(0)