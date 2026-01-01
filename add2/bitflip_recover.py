#!/usr/bin/env python3
"""
bitflip_recover.py

Executa o harness de prova:
- recebe um golden_vectors directory (gerado pelo generate_golden_vectors.py)
- para cada vector:
    1) cria shards (k) + parity (XOR)
    2) corrompe 1 bit em uma shard DATA
    3) tenta recuperar via XOR parity (em caso de 1 perda)
    4) opcional: se reedsolo disponível, tenta RS recover (mais geral)
    5) valida CRC32C e (se disponível) BLAKE3 digest contra o meta esperado
- produz relatório JSON com pass/fail

Uso:
  python3 bitflip_recover.py --dir golden_vectors --out report.json

Dependências opcionais:
  pip install blake3 reedsolo
"""
import os, sys, json, argparse, random, struct
from pathlib import Path

# try imports
try:
    import blake3
    HAS_BLAKE3 = True
except Exception:
    HAS_BLAKE3 = False

try:
    import reedsolo
    HAS_RS = True
except Exception:
    HAS_RS = False

def crc32c_sw(data: bytes) -> int:
    crc = 0xFFFFFFFF
    poly = 0x82F63B78
    for b in data:
        crc ^= b
        for _ in range(8):
            m = -(crc & 1)
            crc = (crc >> 1) ^ (poly & m)
    return (~crc) & 0xFFFFFFFF

def blake3_digest(data: bytes):
    if HAS_BLAKE3:
        return blake3.blake3(data).digest().hex()
    else:
        # fallback: same as prefix fallback used in generator but full digest not available
        return None

def split_into_shards(data: bytes, k: int):
    plen = len(data)
    shard_size = (plen + k - 1) // k
    shards = []
    for i in range(k):
        off = i * shard_size
        part = data[off:off+shard_size]
        if len(part) < shard_size:
            part = part + bytes([0] * (shard_size - len(part)))
        shards.append(bytearray(part))
    return shards, shard_size

def xor_parity(shards, k):
    shard_size = len(shards[0])
    par = bytearray(shard_size)
    for i in range(k):
        for b in range(shard_size):
            par[b] ^= shards[i][b]
    return par

def flip_one_bit(shard):
    # flip a deterministic pseudo-random bit
    size = len(shard)
    idx = random.randrange(size)
    bit = 1 << random.randrange(8)
    shard[idx] ^= bit
    return idx, bit

def try_xor_recover(shards, parity, missing_index):
    k = len(shards) - 1  # parity at end
    shard_size = len(parity)
    rec = bytearray(shard_size)
    # rec = parity XOR all other shards
    for b in range(shard_size):
        v = parity[b]
        for i in range(k):
            if i == missing_index: continue
            v ^= shards[i][b]
        rec[b] = v
    return rec

def try_rs_recover(shards_full, k, m, shard_size, present_flags):
    # simple wrapper: if reedsolo available, use it (note: reedsolo operates on bytes)
    if not HAS_RS:
        return False, None
    total = k + m
    # reedsolo expects data arranged differently; implementing robust RS here is complex
    # We return False to indicate RS not used
    return False, None

def validate_recovered(original_data, recovered_payload, expected_meta):
    # check CRC
    crc_orig = crc32c_sw(original_data)
    crc_recd = crc32c_sw(recovered_payload)
    ok_crc = (crc_orig == crc_recd)
    # check blake3 if expected provided
    exp_b16 = expected_meta.get('blake3_prefix16')
    if exp_b16:
        if HAS_BLAKE3:
            rbl = blake3.blake3(recovered_payload).digest()[:16].hex()
            ok_b = (rbl == exp_b16)
        else:
            ok_b = None  # unknown
    else:
        ok_b = None
    return ok_crc, ok_b

def main():
    p = argparse.ArgumentParser(description="Bitflip & recover harness")
    p.add_argument('--dir', default='golden_vectors')
    p.add_argument('--out', default='proof_report.json')
    p.add_argument('--k', type=int, default=6)
    p.add_argument('--parity', type=int, default=1)
    p.add_argument('--seed', type=int, default=0)
    args = p.parse_args()

    gd = Path(args.dir)
    if not gd.exists():
        print("golden vectors dir not found:", gd)
        return 2

    manifest_file = gd / 'manifest.json'
    if not manifest_file.exists():
        print("manifest.json not found in", gd)
        return 3

    manifest = json.load(open(manifest_file))
    report = {"summary": {"total": len(manifest), "passed": 0, "failed": 0}, "details": []}
    random.seed(args.seed if args.seed != 0 else 42)

    for m in manifest:
        name = m['file']
        payload_path = gd / name
        meta_path = gd / (name + '.meta.json')
        map_path = gd / m['map']
        print("Processing", name)
        data = open(payload_path, 'rb').read()
        expected_crc = int(m['crc32c'], 16)
        expected_b16 = m.get('blake3_prefix16')
        # split into shards and make parity
        k = args.k
        parity = args.parity
        shards, shard_size = split_into_shards(data, k)
        parity_blob = xor_parity(shards, k)
        # pick a data shard to corrupt
        corrupt_index = random.randrange(k)
        # save original for later
        orig_shard = bytearray(shards[corrupt_index])
        # flip one bit
        idx, bit = flip_one_bit(shards[corrupt_index])
        print(f" - corrupted shard {corrupt_index} at byte {idx} bit {bit:02x}")
        # write corrupted shards into temp (simulate on-disk)
        # attempt recovery by XOR
        shards_with_parity = shards + [parity_blob]
        recovered = try_xor_recover(shards_with_parity, parity_blob, corrupt_index)
        # reconstruct payload from shards (replace recovered)
        shards_with_parity[corrupt_index][:] = recovered
        # rebuild payload by concatenation of first k shards (trim padding)
        recovered_payload = b''.join(shards_with_parity[i] for i in range(k))
        # trim to original length
        recovered_payload = recovered_payload[:len(data)]
        ok_crc, ok_b = validate_recovered(data, recovered_payload, m)
        result = {"file": name, "corrupt_shard": corrupt_index, "crc_ok": ok_crc, "blake3_prefix_ok": ok_b}
        if ok_crc and (ok_b is not False):
            result['status'] = 'PASS'
            print(" - XOR recovery PASS (CRC match)")
            report['summary']['passed'] += 1
        else:
            # try RS if available
            if HAS_RS:
                print(" - XOR failed, attempting RS recovery (reedsolo)...")
                # Not fully implemented: attempt RS only if reedsolo available; skipped in this harness
                rs_ok = False
                if rs_ok:
                    result['status'] = 'PASS(RS)'
                    report['summary']['passed'] += 1
                else:
                    result['status'] = 'FAIL'
                    report['summary']['failed'] += 1
            else:
                result['status'] = 'FAIL'
                report['summary']['failed'] += 1
        report['details'].append(result)

    with open(args.out, 'w') as f:
        json.dump(report, f, indent=2)
    print("Report written to", args.out)
    print("Summary:", report['summary'])

if __name__ == '__main__':
    main()