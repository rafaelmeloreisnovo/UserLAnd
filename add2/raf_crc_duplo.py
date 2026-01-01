import json, zlib, hashlib, re
from fractions import Fraction

def crc32_bytes(b: bytes) -> int:
    return zlib.crc32(b) & 0xffffffff

def sha3(b: bytes) -> str:
    return hashlib.sha3_256(b).hexdigest()

# --- Canonicalização simples (expandível) ---
# Ideia: transformar números em frações normalizadas e serializar com ordenação fixa.

num_re = re.compile(r'(?<![\w.])([+-]?\d+(?:\.\d+)?)')

def canonicalize_expr(expr: str) -> dict:
    # 1) normaliza espaços
    s = " ".join(expr.strip().split())

    # 2) captura números e converte em Fraction (evita 1/3 virar 0.333...)
    nums = []
    def repl(m):
        x = m.group(1)
        try:
            f = Fraction(x).limit_denominator(10**6)
        except Exception:
            f = None
        nums.append((x, str(f) if f else x))
        return f"§N{len(nums)-1}§"

    s2 = num_re.sub(repl, s)

    # 3) objeto canônico
    return {
        "expr_template": s2,
        "nums": nums,  # lista ordenada
        "rules": ["space-normalized", "numbers->Fraction(limit 1e6)"],
        "version": "canon_v1"
    }

def canonical_bytes(expr: str) -> bytes:
    obj = canonicalize_expr(expr)
    return json.dumps(obj, sort_keys=True, separators=(",", ":")).encode("utf-8")

# --- Demo: "mesma relação" em escrita/base diferente ---
EXPRS = [
    "a^2 + b^2 = c^2",
    "a² + b² = c²",               # outra grafia
    "  a^2   +   b^2   =  c^2  ",  # ruído
    "a^2+b^2=c^2"
]

print("== RAF CRC DUPLO :: byte vs canônico ==")
for e in EXPRS:
    b = e.encode("utf-8")
    cb = canonical_bytes(e)
    print("\nEXPR:", repr(e))
    print("  CRC32(bytes) :", hex(crc32_bytes(b)))
    print("  SHA3(bytes)  :", sha3(b)[:16], "...")
    print("  CRC32(canon) :", hex(crc32_bytes(cb)))
    print("  SHA3(canon)  :", sha3(cb)[:16], "...")
