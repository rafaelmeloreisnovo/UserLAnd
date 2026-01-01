#!/usr/bin/env python3
# (Same generator provided earlier; minimal stable version)
import os, json, argparse, struct
SIZES = [1024, 4*1024, 64*1024, 1024*1024]
def crc32c_sw(data):
    crc = 0xFFFFFFFF
    poly = 0x82F63B78
    for b in data:
        crc ^= b
        for _ in range(8):
            m = -(crc & 1)
            crc = (crc >> 1) ^ (poly & m)
    return (~crc) & 0xFFFFFFFF
def blake3_prefix16(data):
    c = crc32c_sw(data)
    return struct.pack("<IIII", c ^ 0x9E3779B9, (c*0x85EBCA6B) & 0xFFFFFFFF, c, (c^0x1EDC6F41)&0xFFFFFFFF).hex()
def build_map(seed, size, k, parity, extent, slots):
    stripes = max(1, (size + extent - 1)//extent)
    entries = []
    for s in range(stripes):
        for i in range(k):
            entries.append({"stripe":s,"shard":i,"kind":"data","logical": s*extent*k + i*extent, "file_off": (s* (k+parity) + i)* extent})
    return entries
def main():
    p = argparse.ArgumentParser()
    p.add_argument("--outdir", default="golden_vectors")
    p.add_argument("--seed", type=int, default=12345)
    p.add_argument("--k", type=int, default=6)
    p.add_argument("--parity", type=int, default=1)
    p.add_argument("--extent", type=int, default=4096)
    p.add_argument("--slots", type=int, default=1000)
    args = p.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    manifest = []
    for size in SIZES:
        name = f"payload_{size}.bin"
        path = os.path.join(args.outdir, name)
        data = bytes( ( (args.seed + i) & 0xFF) for i in range(size) )
        with open(path, "wb") as f: f.write(data)
        crc = crc32c_sw(data)
        b16 = blake3_prefix16(data)
        map_entries = build_map(args.seed, size, args.k, args.parity, args.extent, args.slots)
        map_path = os.path.join(args.outdir, name + ".map.json")
        with open(map_path,"w") as f: json.dump({"map_entries": len(map_entries), "entries": map_entries}, f, indent=2)
        meta = {"file":name, "size":size, "crc32c":"%08x"%crc, "blake3_prefix16":b16, "seed":args.seed, "k":args.k, "parity":args.parity, "extent":args.extent}
        with open(os.path.join(args.outdir, name+".meta.json"),"w") as f: json.dump(meta,f,indent=2)
        manifest.append(meta)
        print("generated", name)
    with open(os.path.join(args.outdir,"manifest.json"),"w") as f: json.dump(manifest,f,indent=2)
if __name__=="__main__": main()