#!/usr/bin/env python3
# (Stable harness adapted earlier)
import sys, os, json, argparse, random
def crc32c_sw(data):
    crc = 0xFFFFFFFF; poly = 0x82F63B78
    for b in data:
        crc ^= b
        for _ in range(8):
            m = -(crc & 1)
            crc = (crc >> 1) ^ (poly & m)
    return (~crc) & 0xFFFFFFFF
def split_into_shards(data,k):
    plen = len(data); shard_size = (plen + k -1)//k; shards=[]
    for i in range(k):
        off = i*shard_size; part = data[off:off+shard_size]
        if len(part) < shard_size: part = part + bytes([0])*(shard_size - len(part))
        shards.append(bytearray(part))
    return shards, shard_size
def xor_parity(shards,k):
    ssize = len(shards[0]); par = bytearray(ssize)
    for i in range(k):
        for b in range(ssize): par[b] ^= shards[i][b]
    return par
def flip_one_bit(shard):
    idx = random.randrange(len(shard)); bit = 1 << random.randrange(8); shard[idx] ^= bit; return idx,bit
def main():
    p = argparse.ArgumentParser()
    p.add_argument("--dir", default="golden_vectors")
    p.add_argument("--out", default="proof_report.json")
    p.add_argument("--k", type=int, default=6)
    p.add_argument("--parity", type=int, default=1)
    args = p.parse_args()
    mf = os.path.join(args.dir, "manifest.json")
    if not os.path.exists(mf): print("manifest not found"); return 2
    manifest = json.load(open(mf))
    report = {"total":len(manifest),"passed":0,"failed":0,"details":[]}
    random.seed(42)
    for m in manifest:
        fname = os.path.join(args.dir, m["file"]); data = open(fname,"rb").read()
        shards, shard_size = split_into_shards(data, args.k)
        par = xor_parity(shards, args.k)
        idx = random.randrange(args.k)
        orig = shards[idx][:]; flip_one_bit(shards[idx])
        rec = bytearray(shard_size)
        for b in range(shard_size): rec[b] = par[b]
        for i in range(args.k):
            if i==idx: continue
            for b in range(shard_size): rec[b] ^= shards[i][b]
        recovered_payload = b"".join(rec[i:i+shard_size] for i in [0])  # simplified
        # rebuild full from shards with recovered shard replaced
        shards[idx] = rec
        reassembled = b''.join(shards[i] for i in range(args.k))[:len(data)]
        ok_crc = crc32c_sw(reassembled) == int(m["crc32c"],16)
        detail = {"file":m["file"], "corrupt_shard":idx, "crc_ok":ok_crc}
        if ok_crc: report["passed"] += 1
        else: report["failed"] += 1
        report["details"].append(detail)
    open(args.out,"w").write(json.dumps(report,indent=2))
    print("report written to", args.out)
if __name__=="__main__": main()