#!/usr/bin/env python3
"""
generate_golden_vectors.py

Gera os "golden vectors" necessários para prova:
- cria arquivos payloads (1KB, 4KB, 64KB, 1MB, 4MB)
- para cada payload: calcula CRC32C (software), prefix BLAKE3-16 (se blake3 disponível)
- gera um map.json (layout) simples usando splitmix64 KDF
- escreve metadata JSON com seed, k, parity, extent, slots, hashes

Uso:
  python3 generate_golden_vectors.py --outdir golden_vectors --seed 12345 --k 6 --parity 1

Dependências opcionais:
  pip install blake3  (para BLAKE3 real)
"""
import os
import sys
import json
import argparse
import struct

# Try to import blake3
try:
    import blake3
    HAS_BLAKE3 = True
except Exception:
    HAS_BLAKE3 = False

SIZES = [1024, 4*1024, 64*1024, 1024*1024, 4*1024*1024]  # 1KB,4KB,64KB,1MB,4MB

def crc32c_sw(data: bytes) -> int:
    # bitwise CRC32C (slow but deterministic)
    crc = 0xFFFFFFFF
    poly = 0x82F63B78
    for b in data:
        crc ^= b
        for _ in range(8):
            m = -(crc & 1)
            crc = (crc >> 1) ^ (poly & m)
    return (~crc) & 0xFFFFFFFF

def blake3_prefix16(data: bytes):
    if HAS_BLAKE3:
        d = blake3.blake3(data).digest()
        return d[:16].hex()
    else:
        # fallback: expand CRC into 16 bytes
        c = crc32c_sw(data)
        out = struct.pack('<IIII', c ^ 0x9E3779B9, (c<<1)&0xFFFFFFFF, c, (c^0x1EDC6F41)&0xFFFFFFFF)
        return out.hex()

def splitmix64(x):
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    z = x
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9 & 0xFFFFFFFFFFFFFFFF
    z = (z ^ (z >> 27)) * 0x94D049BB133111EB & 0xFFFFFFFFFFFFFFFF
    return z ^ (z >> 31), x

def slot_hash(seed, stripe, shard, role):
    x = seed ^ (stripe * 0xD6E8FEB86659FD93) ^ (shard * 0xA5A5A5A5A5A5A5A5) ^ (role * 0x9E3779B97F4A7C15)
    val, _ = splitmix64(x & 0xFFFFFFFFFFFFFFFF)
    return val

def build_simple_map(seed, stripes, k, parity, extent_bytes, total_slots, base_off=1024):
    entries = []
    for si in range(stripes):
        bitmap = [0]*((total_slots+7)//8)
        logical_base = si * extent_bytes * k
        # data shards
        for i in range(k):
            s = slot_hash(seed, si, i, 1)
            idx = s % total_slots
            # linear probe
            start = idx
            for attempt in range(total_slots):
                pos = (start + attempt) % total_slots
                byte = pos >> 3
                bit = pos & 7
                if not (bitmap[byte] & (1<<bit)):
                    bitmap[byte] |= (1<<bit)
                    entries.append({
                        "stripe_id": si, "shard_id": i, "kind": "data",
                        "logical_off": logical_base + i*extent_bytes,
                        "file_off": base_off + pos*extent_bytes, "bytes": extent_bytes
                    })
                    break
        # parity
        for p in range(parity):
            s = slot_hash(seed, si, p, 2)
            idx = s % total_slots
            start = idx
            for attempt in range(total_slots):
                pos = (start + attempt) % total_slots
                byte = pos >> 3; bit = pos & 7
                if not (bitmap[byte] & (1<<bit)):
                    bitmap[byte] |= (1<<bit)
                    entries.append({"stripe_id": si, "shard_id": p, "kind": "parity", "logical_off": 0, "file_off": base_off + pos*extent_bytes, "bytes": extent_bytes})
                    break
    return entries

def main():
    p = argparse.ArgumentParser(description="Generate Golden Vectors for ZIPRAFA proof")
    p.add_argument('--outdir', default='golden_vectors')
    p.add_argument('--seed', type=int, default=123456789)
    p.add_argument('--k', type=int, default=6)
    p.add_argument('--parity', type=int, default=1)
    p.add_argument('--extent', type=int, default=4096)
    p.add_argument('--slots', type=int, default=1000)
    args = p.parse_args()

    os.makedirs(args.outdir, exist_ok=True)

    manifest = []
    for size in SIZES:
        name = f'payload_{size}.bin'
        path = os.path.join(args.outdir, name)
        # create deterministic content from seed and size
        data = bytearray(( (args.seed + i) & 0xFF ) for i in range(size))
        with open(path, 'wb') as f:
            f.write(data)
        crc = crc32c_sw(data)
        bpref = blake3_prefix16(data)
        # build simple map (one stripe per extent-size chunk)
        stripes = max(1, (size + args.extent - 1) // args.extent)
        map_entries = build_simple_map(args.seed, stripes, args.k, args.parity, args.extent, args.slots)
        map_path = os.path.join(args.outdir, name + '.map.json')
        with open(map_path, 'w') as f:
            json.dump({"map_entries": len(map_entries), "entries": map_entries}, f, indent=2)
        meta = {
            "file": name,
            "size": size,
            "crc32c": f"{crc:08x}",
            "blake3_prefix16": bpref,
            "seed": args.seed,
            "k": args.k,
            "parity": args.parity,
            "extent": args.extent,
            "slots": args.slots,
            "stripes": stripes,
            "map": os.path.basename(map_path)
        }
        manifest.append(meta)
        with open(os.path.join(args.outdir, name + '.meta.json'), 'w') as f:
            json.dump(meta, f, indent=2)
        print(f"Generated {name} size={size} crc={crc:08x} blake3_prefix={bpref}")

    with open(os.path.join(args.outdir, 'manifest.json'), 'w') as f:
        json.dump(manifest, f, indent=2)
    print("Golden vectors generated in", args.outdir)
    if HAS_BLAKE3:
        print("blake3 available: strong digests produced")
    else:
        print("blake3 not available: using deterministic fallback prefix (not cryptographic)")

if __name__ == '__main__':
    main()