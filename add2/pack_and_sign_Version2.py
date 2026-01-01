#!/usr/bin/env python3
"""
pack_and_sign.py

Create a sample .zipraf (ZIP) package with manifest.json, payload(s) and signature.

Usage:
  python3 pack_and_sign.py --payload golden_vectors/payload_1024.bin --seedfile demo_keypair.txt --out sample.zipraf

Notes:
  - Requires signer_tool binary compiled from signer_tool.c (or you can sign with python ed25519 libs).
  - If blake3 is installed, will compute Blake3; otherwise fallback prefix used.
"""

import os, sys, json, argparse, subprocess, hashlib
from pathlib import Path

try:
    import blake3
    HAS_BLAKE3 = True
except Exception:
    HAS_BLAKE3 = False

def sha3_256_file(path):
    h = hashlib.sha3_256()
    with open(path,'rb') as f:
        while True:
            b = f.read(8192)
            if not b: break
            h.update(b)
    return h.hexdigest()

def blake3_256_file(path):
    if HAS_BLAKE3:
        h = blake3.blake3()
        with open(path,'rb') as f:
            while True:
                b = f.read(8192)
                if not b: break
                h.update(b)
        return h.hexdigest()
    else:
        # fallback: sha3 as substitute in demo
        return sha3_256_file(path)

def pack(payload_path, seedfile, outpath):
    payload = Path(payload_path)
    if not payload.exists():
        print("payload not found:", payload)
        return 2
    # compute hashes
    sha3 = sha3_256_file(payload)
    bl3 = blake3_256_file(payload)
    manifest = {
        "file": payload.name,
        "sha3_256": sha3,
        "blake3_256": bl3,
        "timestamp": int(__import__('time').time())
    }
    tmpdir = Path("tmp_pack")
    if tmpdir.exists():
        import shutil; shutil.rmtree(tmpdir)
    tmpdir.mkdir()
    # copy payload
    dest_payload = tmpdir / payload.name
    dest_payload.write_bytes(payload.read_bytes())
    # write manifest
    manifest_path = tmpdir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))
    # create zip (zipraf)
    import zipfile
    with zipfile.ZipFile(outpath, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(str(dest_payload), dest_payload.name)
        z.write(str(manifest_path), "manifest.json")
    print("Packed", outpath)
    # sign zip using signer_tool if present
    if shutil_ava := shutil_which("signer_tool"):
        sigfile = outpath + ".sig"
        rc = subprocess.run(["./signer_tool", "sign", seedfile, outpath, sigfile])
        if rc.returncode == 0:
            print("Signed artifact ->", sigfile)
        else:
            print("signer_tool failed, rc=", rc.returncode)
    else:
        print("signer_tool not found; skipping signature (compile signer_tool.c to sign)")

def shutil_which(name):
    import shutil
    p = shutil.which(name)
    if p:
        return p
    return None

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--payload", required=True)
    p.add_argument("--seedfile", default="demo_keypair.txt")
    p.add_argument("--out", default="sample.zipraf")
    args = p.parse_args()
    pack(args.payload, args.seedfile, args.out)

if __name__ == '__main__':
    main()