#!/usr/bin/env python3
import os, re, json, zipfile, sys
from collections import Counter, defaultdict

FORMULA_RE = re.compile(r"(∫|Σ|Ω|Δ|Φ|ψ|χ|ρ|λ|π|φ|√|->|→|↔|∞|=)")
KEY_RE = re.compile(r"\b(lei|axioma|teorema|conjectura|hip[oó]tese|modelo|protocolo|kernel)\b", re.IGNORECASE)

CATS = {
  "Matemática/Geometria": ["fibonacci","tribonacci","sqrt","sin","cos","tan","pi","φ","phi","toroid","tesseract","zeta","prime","matrix","vector","angle","entropy"],
  "Criptografia/Integridade": ["crc","checksum","hash","sha","blake","signature","assinatura","parity","zipraf","bitraf","zrf","netraf","merkle"],
  "Arquitetura/Execução": ["orchestr","cli","build","compile","kernel","engine","thread","pipeline","module","api","driver","termux","gradle","ndk","apk","actions","ci","yml"],
  "Cognitivo/IA": ["cortex","memory","agent","model","learn","predict","frontier","state","embedding","vetor"],
  "Ética/Governança": ["ethica","govern","audit","policy","nist","iso","guard","pre6","compliance","jurid"],
  "Simbólico/Cultural": ["bagua","tao","hexagram","mandala","megalit","malta","orion","venus","marte","jupiter","saturno"]
}

def classify(s: str):
    l = s.lower()
    out = []
    for k, kws in CATS.items():
        if any(w in l for w in kws):
            out.append(k)
    return out or ["Outros"]

def walk_json_text(obj, out):
    if obj is None:
        return
    if isinstance(obj, str):
        out.append(obj)
    elif isinstance(obj, dict):
        for k,v in obj.items():
            if isinstance(k, str): out.append(k)
            walk_json_text(v, out)
    elif isinstance(obj, list):
        for x in obj:
            walk_json_text(x, out)

def scan_text_blob(text, stats):
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        if KEY_RE.search(line):
            stats["keys"].append(line)
        if FORMULA_RE.search(line):
            if 3 <= len(line) <= 240:
                stats["formulas"].append(re.sub(r"\s+"," ", line))

def scan_path(path):
    stats = {"formulas": [], "keys": []}

    if path.lower().endswith(".json"):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                data = json.load(f)
            texts=[]
            walk_json_text(data, texts)
            scan_text_blob("\n".join(texts), stats)
        except Exception as e:
            print(f"[WARN] JSON falhou: {path} :: {e}", file=sys.stderr)

    elif path.lower().endswith(".zip"):
        try:
            with zipfile.ZipFile(path, "r") as z:
                for name in z.namelist():
                    if name.lower().endswith(".json"):
                        try:
                            raw = z.read(name)
                            data = json.loads(raw.decode("utf-8", errors="replace"))
                            texts=[]
                            walk_json_text(data, texts)
                            scan_text_blob("\n".join(texts), stats)
                        except Exception:
                            continue
        except Exception as e:
            print(f"[WARN] ZIP falhou: {path} :: {e}", file=sys.stderr)

    return stats

def uniq(seq):
    seen=set(); out=[]
    for x in seq:
        if x not in seen:
            seen.add(x); out.append(x)
    return out

def main():
    if len(sys.argv) < 2:
        print("Uso: python3 raf_inventory.py <pasta_ou_arquivo_json_zip>")
        sys.exit(2)

    target = sys.argv[1]
    paths=[]
    if os.path.isdir(target):
        for root, _, files in os.walk(target):
            for fn in files:
                if fn.lower().endswith((".json",".zip")):
                    paths.append(os.path.join(root, fn))
    else:
        paths=[target]

    all_formulas=[]
    all_keys=[]
    for p in paths:
        st = scan_path(p)
        all_formulas.extend(st["formulas"])
        all_keys.extend(st["keys"])

    all_formulas = uniq(all_formulas)
    all_keys = uniq(all_keys)

    cat_count = Counter()
    for f in all_formulas:
        for c in classify(f):
            cat_count[c]+=1

    print("=== RAFAELIA :: INVENTÁRIO (JSON->TEXTO) ===")
    print(f"Arquivos processados: {len(paths)}")
    print(f"Linhas tipo-fórmula (únicas): {len(all_formulas)}")
    print(f"Linhas com 'Lei/Axioma/Teorema/Hipótese...' (únicas): {len(all_keys)}")
    print("\n-- Fórmulas por área (top 10) --")
    for k,v in cat_count.most_common(10):
        print(f"{k:22s} : {v}")

    print("\n-- Amostra de 'Lei/Axioma/Teorema/Hipótese...' (top 30) --")
    for x in all_keys[:30]:
        print("•", x[:200])

if __name__ == "__main__":
    main()
