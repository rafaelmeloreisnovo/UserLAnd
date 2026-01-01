#!/usr/bin/env python3
import os, sys, re, json, zipfile, math, random, zlib, bz2, lzma, hashlib
from collections import Counter, deque
random.seed(42)

HOME = os.path.expanduser("~")
OUTDIR = os.path.join(HOME, ".zrf_brain")
LEDGER = os.path.join(OUTDIR, "conv_index.jsonl")   # 1 linha por conversa
STATS  = os.path.join(OUTDIR, "stats.json")         # resumo global + faixas p/ normalizar o índice
VECTORS= os.path.join(OUTDIR, "vectors.jsonl")      # vetores 256-d por conversa
os.makedirs(OUTDIR, exist_ok=True)

# ------------ util ------------
WRE = re.compile(r"\w+", re.UNICODE)
def toks(s): return [w.lower() for w in WRE.findall(s or "")]
def bigrams(arr):
    for i in range(len(arr)-1):
        yield arr[i] + "▹" + arr[i+1]

def feature_hash(words, dim=256):
    v = [0.0]*dim
    for w in words:
        h = int(hashlib.md5(w.encode()).hexdigest(), 16)
        idx = h % dim
        sign = -1.0 if (h >> 1) & 1 else 1.0
        v[idx] += sign
    # L2 normalize
    n = math.sqrt(sum(x*x for x in v)) or 1.0
    return [x/n for x in v]

def cosine(a,b):
    return sum(x*y for x,y in zip(a,b))

def linreg_log(x,y):
    n=len(x); sx=sy=sxx=sxy=0.0
    for xi,yi in zip(x,y):
        lx,ly = math.log(xi+1e-9), math.log(yi+1e-9)
        sx+=lx; sy+=ly; sxx+=lx*lx; sxy+=lx*ly
    b = (n*sxy - sx*sy)/(n*sxx - sx*sx + 1e-12)
    a = (sy - b*sx)/n
    return a,b  # y ≈ a + b*x  (em log)

def comp_sizes(blob):
    return len(zlib.compress(blob)), len(bz2.compress(blob)), len(lzma.compress(blob))
def gain(a,b): return 100.0*(b-a)/b if b>0 else 0.0

def first_user_prompt(mapping):
    for node in mapping.values():
        m = node.get("message") or {}
        if (m.get("author",{}) or {}).get("role","")=="user":
            parts = (m.get("content",{}) or {}).get("parts") or []
            for p in parts:
                if isinstance(p,str) and p.strip():
                    s = " ".join(p.split())
                    return s
    return ""

# ------------ regras ------------
RX = {
  "patente": re.compile(r"\bpatent[ea]s?\b", re.I),
  "descoberta": re.compile(r"\bdescoberta", re.I),
  "insight": re.compile(r"\binsight", re.I),
  "formula": re.compile(r"\bf[óo]rmula", re.I),
  "voynich": re.compile(r"\bvo[yi]nich\b", re.I),
  "fibo_mod": re.compile(r"fibonacci\s+modificada", re.I),
  "zipraf": re.compile(r"\b(ZIPRAF|ZRF)\b", re.I),
  "rafcode": re.compile(r"\bRAFCODE", re.I),
  "tesseract": re.compile(r"\btesseract|\bhyperformas?", re.I),
  "omegainf": re.compile(r"OmegaInfinit", re.I),
  "maya": re.compile(r"Maya Suprema", re.I),
  "qnm": re.compile(r"Quantum Neural Matrix", re.I),
}

LAYERS = {
  "Núcleo": re.compile(r"\b(rafaelia|fcea|uno|bloco|tmux|watchdog|termux|bashrc|apk|kernel|script)\b", re.I),
  "Cripto‑Fractal": re.compile(r"(ZIPRAF|ZRF|Voynich|Fibonacci\s+modificada|tesseract|hyperformas?)", re.I),
  "Descobertas/Patentes": re.compile(r"\b(patent[ea]s?|descoberta|insight|f[óo]rmula)\b", re.I),
  "Engenharia": re.compile(r"(OmegaInfinit|bobina|firmware|rom|sensor|campo|vetorial|coil|motor)", re.I),
  "Jurídico": re.compile(r"(LGPD|GDPR|Berna|licen[cç]a|manifesto|contrato|propriedade)", re.I),
  "Mercado": re.compile(r"(Binance|B3|ROI|Sharpe|volatilidade|cripto|BTC|ETH|PETR4|VALE3)", re.I),
  "Meta": re.compile(r"(Inten[cç][aã]o\s+Pura|Verbo\s+Vivo|yactos?|atemporal|multiversal)", re.I),
}

def classify_layer(text):
    best=None; bestc=0
    for k,rgx in LAYERS.items():
        c = len(rgx.findall(text or ""))
        if c>bestc: best, bestc = k, c
    return best or "Geral"

# ------------ ingest ------------
def stream_convs_from_zip(zip_path, member="conversations.json"):
    with zipfile.ZipFile(zip_path, "r") as zf:
        with zf.open(member) as fp:
            ch = fp.read(1).decode('utf-8','ignore')
            while ch and ch!='[':
                ch = fp.read(1).decode('utf-8','ignore')
            buf = ""
            while True:
                chunk = fp.read(1<<20).decode('utf-8','ignore')
                if not chunk: break
                buf += chunk
                while True:
                    s = buf.lstrip()
                    if not s: break
                    if s[0] == ']': return
                    try:
                        obj, end = json.JSONDecoder().raw_decode(s)
                    except json.JSONDecodeError:
                        break
                    yield obj
                    buf = s[end:]
                    if buf and buf[0] == ',':
                        buf = buf[1:]

def ingest(zip_path):
    conv_out = open(LEDGER, "w", encoding="utf-8")
    vec_out  = open(VECTORS,"w", encoding="utf-8")

    totals = {
      "threads":0,"messages":0,
      "roles":{"user":0,"assistant":0,"tool":0,"system":0},
      "kw":{k:0 for k in ["patente","descoberta","insight","formula"]},
      "named":{k:0 for k in ["voynich","fibo_mod","zipraf","rafcode","tesseract","omegainf","maya","qnm"]},
      "zipf_slope_range":[+1e9,-1e9],
      "comp_gain_range":[+1e9,-1e9],
      "recur_range":[+1e9,-1e9]
    }

    DIM=256
    for conv in stream_convs_from_zip(zip_path):
        cid = conv.get("conversation_id") or ""
        title = conv.get("title") or ""
        mapping = conv.get("mapping",{})
        fprompt = first_user_prompt(mapping) or ""
        counts = {"user":0,"assistant":0,"tool":0,"system":0}
        kwc = {"patente":0,"descoberta":0,"insight":0,"formula":0}
        namedc = {k:0 for k in totals["named"]}

        tok_counts = Counter()
        token_stream=[]
        msg_text_accum=[]

        # varrer mensagens
        msgs=0
        for node in mapping.values():
            m = node.get("message") or {}
            role = (m.get("author",{}) or {}).get("role","")
            counts[role] = counts.get(role,0)+1
            parts = (m.get("content",{}) or {}).get("parts") or []
            text = " ".join(p for p in parts if isinstance(p,str))
            if not text: continue
            msgs+=1; totals["messages"] += 1
            ts = toks(text)
            token_stream.extend(ts)
            tok_counts.update(ts[:5000])  # cap leve por msg

            msg_text_accum.append(text[:200])

            for key,rgx in RX.items():
                if key in kwc:   # 4 palavras‑chave
                    if rgx.search(text): kwc[key]+=1; totals["kw"][key]+=1
                if key in namedc: # itens nomeados
                    if rgx.search(text): namedc[key]+=1; totals["named"][key]+=1

        if msgs==0:
            continue

        # Vetor semântico (unigramas + bigramas)
        words = []
        words.extend(token_stream[:30000])
        words.extend(b for b in bigrams(token_stream[:20000]))
        vec = feature_hash(words, dim=DIM)

        # Métricas fractais por conversa
        pairs = tok_counts.most_common()
        ranks = [i+1 for i,_ in enumerate(pairs)]
        counts_arr = [c for _,c in pairs] or [1]
        _, zipf_slope = linreg_log(ranks, counts_arr)

        # recorrência simples (média de distância entre repetições até janela 500)
        lastpos = {}
        dists=[]
        for i,t in enumerate(token_stream[:60000]):
            if t in lastpos:
                d = i-lastpos[t]
                if d<=500: dists.append(d)
            lastpos[t]=i
        recur = (sum(dists)/len(dists)) if dists else 0.0

        # compressão real vs embaralhada
        seq = " ".join(token_stream[:60000]).encode("utf-8")
        Z0,B0,L0 = comp_sizes(seq)
        bag = token_stream[:60000]; random.shuffle(bag)
        shuf = " ".join(bag).encode("utf-8")
        Z1,B1,L1 = comp_sizes(shuf)
        cg = sum([gain(Z0,Z1),gain(B0,B1),gain(L0,L1)])/3.0

        # atualizar faixas globais
        totals["zipf_slope_range"][0] = min(totals["zipf_slope_range"][0], zipf_slope)
        totals["zipf_slope_range"][1] = max(totals["zipf_slope_range"][1], zipf_slope)
        totals["comp_gain_range"][0]  = min(totals["comp_gain_range"][0], cg)
        totals["comp_gain_range"][1]  = max(totals["comp_gain_range"][1], cg)
        totals["recur_range"][0]      = min(totals["recur_range"][0], recur)
        totals["recur_range"][1]      = max(totals["recur_range"][1], recur)

        # camada e tags
        full = f"{title} {fprompt} {' '.join(msg_text_accum)}"
        layer = classify_layer(full)
        tags_present = [k for k,rgx in RX.items() if k in namedc and rgx.search(full)]

        # escrever registros
        rec = {
          "cid": cid, "title": title,
          "messages": msgs,
          "counts": counts, "kw": kwc, "named": namedc,
          "first_prompt": (fprompt[:300]+"…" if len(fprompt)>300 else fprompt),
          "zipf_slope": zipf_slope, "comp_gain": cg, "recurrence": recur,
          "layer": layer, "tags": tags_present
        }
        conv_out.write(json.dumps(rec, ensure_ascii=False)+"\n")
        vec_out.write(json.dumps({"cid":cid,"vec":vec})+"\n")

        totals["threads"] += 1

    conv_out.close(); vec_out.close()
    # salvar stats globais (faixas p/ normalizar índice depois)
    with open(STATS,"w",encoding="utf-8") as f:
        json.dump(totals,f,ensure_ascii=False,indent=2)

def load_vectors():
    vecs=[]
    with open(VECTORS,"r",encoding="utf-8") as f:
        for line in f:
            o = json.loads(line)
            vecs.append((o["cid"], o["vec"]))
    return vecs

def load_convs():
    convs={}
    with open(LEDGER,"r",encoding="utf-8") as f:
        for line in f:
            o = json.loads(line)
            convs[o["cid"]] = o
    return convs

def normalize(x, lo, hi):
    if hi<=lo: return 0.0
    return (x-lo)/(hi-lo)

def compute_lucidity():
    # cria arquivo enriquecido com índice de lucidez
    stats = json.load(open(STATS,"r",encoding="utf-8"))
    convs = []
    with open(LEDGER,"r",encoding="utf-8") as f:
        for line in f:
            o = json.loads(line)
            z = normalize(o["zipf_slope"], *stats["zipf_slope_range"])
            c = normalize(o["comp_gain"],  *stats["comp_gain_range"])
            r = normalize(o["recurrence"], *stats["recur_range"])
            # índice composto (pesos ajustáveis)
            li = 0.50*c + 0.30*z + 0.20*(1.0 - r if r>0 else 0.5)
            o["lucidity_index"] = round(li,4)
            convs.append(o)
    convs.sort(key=lambda d:(d["kw"].get("patente",0), d["kw"].get("descoberta",0), d["lucidity_index"], d["messages"]), reverse=True)
    # sobrescreve com LI incluso
    with open(LEDGER,"w",encoding="utf-8") as f:
        for o in convs:
            f.write(json.dumps(o,ensure_ascii=False)+"\n")

def query(qtext, topk=10):
    qtok = toks(qtext)
    qwords = list(qtok)+list(bigrams(qtok))
    qvec = feature_hash(qwords, dim=256)
    vecs = load_vectors()
    convs = load_convs()
    scored=[]
    for cid,vec in vecs:
        scored.append((cosine(qvec, vec), cid))
    scored.sort(reverse=True)
    print(f"\n🔎 Top {topk} para: {qtext}\n")
    for score,cid in scored[:topk]:
        o = convs.get(cid,{})
        title = o.get("title","(sem título)")
        li = o.get("lucidity_index",0.0)
        kw = o.get("kw",{})
        fp = o.get("first_prompt","")
        print(f"• {title}  |  LI={li:.3f}  |  pat={kw.get('patente',0)} desc={kw.get('descoberta',0)} ins={kw.get('insight',0)} for={kw.get('formula',0)}  |  score={score:.3f}")
        if fp: print(f"  → {fp[:140]}{'…' if len(fp)>140 else ''}")

def main():
    if len(sys.argv)==1 or sys.argv[1] in ("ingest","index"):
        # autodetecta zip
        zips = [z for z in ["export2.zip","emula2222.zip"] if os.path.exists(z)]
        if not zips:
            print("❌ Nenhum ZIP encontrado (esperado export2.zip ou emula2222.zip).")
            sys.exit(1)
        print(f"📥 Ingest: {zips[0]} → {OUTDIR}")
        ingest(zips[0])
        compute_lucidity()
        print(f"✅ Index pronto em {OUTDIR}")
        return
    if sys.argv[1]=="query":
        if len(sys.argv)<3:
            print("Uso: python zrf_brain.py query \"texto\"")
            sys.exit(1)
        query(" ".join(sys.argv[2:]))
        return
    print("Uso: python zrf_brain.py [ingest] | query \"texto\"")

if __name__=="__main__":
    main()
