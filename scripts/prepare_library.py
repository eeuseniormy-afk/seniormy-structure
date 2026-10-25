#!/usr/bin/env python3
"""Prepara a biblioteca orquestral do SeniorMy Structure a partir do VSCO 2 Community Edition (CC0).

Uso:  python3 scripts/prepare_library.py <cache-de-downloads> <saida>
      (ex.: python3 scripts/prepare_library.py samples-cache library)

O que faz:
  1. Lista os arquivos do repositório sgossner/VSCO-2-CE num commit fixo (API do GitHub; usa
     GITHUB_TOKEN se existir) e baixa só as pastas usadas (raw.githubusercontent.com), com cache.
  2. Lê nota / camada de velocity / round robin do nome de cada arquivo e confere a oitava medindo a
     altura real (autocorrelação) — o VSCO usa C3 ou C4 como dó central conforme o instrumento.
  3. Corta o silêncio inicial, limita a duração (sustains 7,5 s com ponto de loop; curtos 1,5–2,5 s),
     iguala o volume entre notas vizinhas (mantendo a diferença natural entre camadas p/f),
     normaliza cada instrumento e grava FLAC 24 bits.
  4. Escreve library.json (zonas, faixas de teclas, camadas, RR, loops, balanço orquestral e pan),
     LICENSE-SOURCES.txt (origem e licença de cada arquivo) e README.txt.

Fonte: https://github.com/sgossner/VSCO-2-CE — licença CC0 1.0 (domínio público).
Crédito pedido pelos autores: Versilian Studios / Sam Gossner, Ivy Audio / Simon Dalzell;
página: http://vis.versilstudios.net/vsco-community.html
Requisitos: Python 3.8+, numpy, soundfile (libsndfile com FLAC).
"""
import concurrent.futures as cf
import json, os, re, sys, time, urllib.parse, urllib.request
import numpy as np
import soundfile as sf

REPO = "sgossner/VSCO-2-CE"
COMMIT = "440300901dfe9275fd84e0b7763af1f8443ae62e"     # master em 2020-08-04 (VSCO 2 CE 1.1.0)
RAW = "https://raw.githubusercontent.com/%s/%s/" % (REPO, COMMIT)
LICENSE = "CC0 1.0 Universal (public domain dedication)"
PITCH = {"c": 0, "c#": 1, "db": 1, "d": 2, "d#": 3, "eb": 3, "e": 4, "f": 5, "f#": 6, "gb": 6,
         "g": 7, "g#": 8, "ab": 8, "a": 9, "a#": 10, "bb": 10, "b": 11}
DYN = {"pp": 0, "p": 1, "mp": 2, "mf": 3, "f": 4, "ff": 5}

# (seção, articulação, pasta no VSCO, pasta de saída, tipo)
#   tipo: "sus" (loop), "short" (one-shot)
SOURCES = [
    ("vln1", "sustain",   "Strings/Violin Section/susVib", "violins/sustain",  "sus"),
    ("vln1", "spiccato",  "Strings/Violin Section/Spic",   "violins/spiccato", "short"),
    ("vln1", "pizzicato", "Strings/Violin Section/Pizz",   "violins/pizzicato","short"),
    ("vln1", "tremolo",   "Strings/Violin Section/Trem",   "violins/tremolo",  "sus"),
    ("vln1", "legato",    "Strings/Solo Violin/Arco Vib",  "violin-solo/legato", "sus"),
    ("vla",  "sustain",   "Strings/Viola Section/susvib",  "violas/sustain",   "sus"),
    ("vla",  "spiccato",  "Strings/Viola Section/spic",    "violas/spiccato",  "short"),
    ("vla",  "pizzicato", "Strings/Viola Section/pizz",    "violas/pizzicato", "short"),
    ("vla",  "tremolo",   "Strings/Viola Section/trem",    "violas/tremolo",   "sus"),
    ("vc",   "sustain",   "Strings/Cello Section/susvib",  "cellos/sustain",   "sus"),
    ("vc",   "spiccato",  "Strings/Cello Section/spic",    "cellos/spiccato",  "short"),
    ("vc",   "pizzicato", "Strings/Cello Section/pizzT",   "cellos/pizzicato", "short"),
    ("vc",   "tremolo",   "Strings/Cello Section/trem",    "cellos/tremolo",   "sus"),
    ("cb",   "sustain",   "Strings/Solo Contrabass/SusVib","basses/sustain",   "sus"),
    ("cb",   "spiccato",  "Strings/Solo Contrabass/Spic",  "basses/spiccato",  "short"),
    ("cb",   "pizzicato", "Strings/Solo Contrabass/Pizz",  "basses/pizzicato", "short"),
    ("cb",   "tremolo",   "Strings/Solo Contrabass/Trem",  "basses/tremolo",   "sus"),
    ("horns","sustain",   "Brass/F Horn/sus",              "horns/sustain",    "sus"),
    ("horns","spiccato",  "Brass/F Horn/stac",             "horns/staccato",   "short"),
]

# Balanço orquestral (dB após normalização), pan (assentos americanos: Vln I à esquerda,
# Vln II meio-esquerda, Vla meio-direita, Vc direita, Cb fundo à direita), ataque/release.
SECTION = {
    "vln1":  dict(gainDb=0.0,  pan=-0.55, release=0.45),
    "vln2":  dict(gainDb=-1.0, pan=-0.22, release=0.45),
    "vla":   dict(gainDb=-0.5, pan=0.18,  release=0.5),
    "vc":    dict(gainDb=0.0,  pan=0.42,  release=0.6),
    "cb":    dict(gainDb=1.5,  pan=0.62,  release=0.7),
    "horns": dict(gainDb=-3.0, pan=0.05,  release=0.5),
}
TARGET_RMS_DB = -20.0          # camada mais forte de cada instrumento
SUS_MAX_S, SHORT_MAX_S = {"sus": 7.5}, {"spiccato": 1.5, "pizzicato": 2.5}


def log(*a):
    print(*a, flush=True)


def http_get(url, dest=None, tries=6):
    headers = {"User-Agent": "seniormy-structure-build"}
    tok = os.environ.get("GITHUB_TOKEN")
    if tok and "api.github.com" in url:
        headers["Authorization"] = "Bearer " + tok
    for i in range(tries):
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=120) as r:
                data = r.read()
            if dest:
                tmp = dest + ".part"
                with open(tmp, "wb") as fh:
                    fh.write(data)
                os.replace(tmp, dest)
            return data
        except Exception as e:  # noqa
            if i == tries - 1:
                raise
            time.sleep(3 * (i + 1))


def list_tree(cache):
    p = os.path.join(cache, "tree-%s.json" % COMMIT[:12])
    if not os.path.exists(p):
        http_get("https://api.github.com/repos/%s/git/trees/%s?recursive=1" % (REPO, COMMIT), p)
    with open(p) as fh:
        tree = json.load(fh)
    return [e["path"] for e in tree["tree"] if e["type"] == "blob"]


def parse_name(fname):
    """→ (nota MIDI pelo nome no padrão científico C4=60, camada, rr). Camada como chave ordenável."""
    base = os.path.splitext(fname)[0]
    toks = base.split("_")
    note = None
    for t in toks:
        m = re.fullmatch(r"([A-Ga-g][#b]?)(-?\d)", t)
        if m and m.group(1).lower() in PITCH:
            note = 12 * (int(m.group(2)) + 1) + PITCH[m.group(1).lower()]
            break
    layer = 0
    rr = 0
    for t in toks:
        tl = t.lower()
        m = re.fullmatch(r"v(\d)", tl)
        if m:
            layer = int(m.group(1))
        elif tl in DYN:
            layer = DYN[tl] + 1
        m = re.fullmatch(r"rr(\d)", tl)
        if m:
            rr = int(m.group(1)) - 1
    m = re.search(r"_(\d)$", base)          # "susvib_A2_v1_1" → take 1 (RR)
    if m and not re.search(r"_v\d$", base):
        rr = max(rr, int(m.group(1)) - 1)
    return note, layer, rr


def estimate_midi(x, sr):
    """Altura média (autocorrelação normalizada) na parte estável da nota."""
    mono = x.mean(axis=1) if x.ndim > 1 else x
    start = int(0.15 * sr)
    seg = mono[start:start + int(0.5 * sr)]
    if len(seg) < 4096:
        seg = mono[:int(0.5 * sr)]
    if len(seg) < 2048 or np.max(np.abs(seg)) < 1e-4:
        return None
    seg = seg - seg.mean()
    n = len(seg)
    f = np.fft.rfft(seg, 2 * n)
    ac = np.fft.irfft(f * np.conj(f))[:n]
    ac /= (ac[0] + 1e-12)
    lo, hi = int(sr / 1400.0), int(sr / 28.0)
    if hi >= n:
        hi = n - 1
    lag = lo + int(np.argmax(ac[lo:hi]))
    # prefere o primeiro pico forte (evita escolher a suboitava)
    peak = ac[lag]
    for cand in range(lo, lag):
        if ac[cand] > 0.85 * peak and ac[cand] >= ac[cand - 1] and ac[cand] >= ac[cand + 1]:
            lag = cand
            break
    f0 = sr / lag
    return 69 + 12 * np.log2(f0 / 440.0)


def trim_and_shape(x, sr, kind, artic):
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    x = x[:, :2].astype(np.float64)
    env = np.max(np.abs(x), axis=1)
    peak = env.max() + 1e-12
    thr = peak * 10 ** (-48 / 20.0)
    idx = np.argmax(env > thr)
    idx = max(0, idx - int(0.004 * sr))
    x = x[idx:]
    if kind == "sus":
        maxlen = int(SUS_MAX_S["sus"] * sr)
    else:
        maxlen = int(SHORT_MAX_S.get(artic, 2.0) * sr)
    if len(x) > maxlen:
        x = x[:maxlen]
        fade = int((0.25 if kind == "sus" else 0.06) * sr)
        x[-fade:] *= np.linspace(1.0, 0.0, fade)[:, None]
    # fade-in de 1 ms (remove clique do corte)
    fi = min(len(x), int(0.001 * sr))
    x[:fi] *= np.linspace(0.0, 1.0, fi)[:, None]
    return x


def find_loop(x, sr):
    """Loop para sustains: fim 0,15 s antes do fim; início entre 35% e 60% com RMS mais parecido."""
    n = len(x)
    if n < int(2.5 * sr):
        return -1, -1
    end = n - int(0.15 * sr)
    win = int(0.1 * sr)
    mono = x.mean(axis=1)

    def rms(a, b):
        return float(np.sqrt(np.mean(mono[a:b] ** 2) + 1e-12))
    target = rms(end - win, end)
    best, bscore = None, 1e9
    for s in range(int(0.35 * n), int(0.6 * n), int(0.02 * sr)):
        sc = abs(np.log((rms(s, s + win) + 1e-9) / (target + 1e-9)))
        if sc < bscore:
            bscore, best = sc, s
    if best is None or end - best < int(1.0 * sr):
        return -1, -1
    return int(best), int(end)


def body_rms(x, sr):
    mono = x.mean(axis=1)
    a = int(0.05 * sr)
    b = min(len(mono), a + int(0.8 * sr))
    seg = mono[a:b] if b > a else mono
    return float(np.sqrt(np.mean(seg ** 2) + 1e-12))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    cache, out = sys.argv[1], sys.argv[2]
    os.makedirs(cache, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    paths = list_tree(cache)
    log("VSCO 2 CE @ %s: %d arquivos no repositório" % (COMMIT[:7], len(paths)))

    # 1) downloads
    jobs = []
    for sec, art, folder, outdir, kind in SOURCES:
        files = sorted(p for p in paths if os.path.dirname(p) == folder and p.lower().endswith(".wav"))
        if not files:
            raise SystemExit("pasta não encontrada no VSCO: " + folder)
        for p in files:
            jobs.append((sec, art, folder, outdir, kind, p))
    todo = []
    for j in jobs:
        dest = os.path.join(cache, "raw", j[5])
        if not os.path.exists(dest):
            todo.append((j[5], dest))
    log("baixando %d de %d arquivos..." % (len(todo), len(jobs)))
    def dl(item):
        p, dest = item
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        http_get(RAW + urllib.parse.quote(p), dest)
        return p
    with cf.ThreadPoolExecutor(max_workers=12) as ex:
        for i, _ in enumerate(ex.map(dl, todo)):
            if (i + 1) % 100 == 0:
                log("  %d/%d" % (i + 1, len(todo)))

    # 2) leitura, oitava, processamento
    instruments = {}
    sources_txt = []
    total_bytes = 0
    for sec, art, folder, outdir, kind in SOURCES:
        files = [j for j in jobs if j[2] == folder]
        items = []
        for j in files:
            p = j[5]
            x, sr = sf.read(os.path.join(cache, "raw", p), always_2d=True)
            note, layer, rr = parse_name(os.path.basename(p))
            if note is None:
                log("  (ignorado, sem nota no nome) " + p)
                continue
            items.append(dict(path=p, x=x, sr=sr, note=note, layer=layer, rr=rr))
        # oitava: compara a altura medida com o nome (científico ou +12)
        votes = {-12: 0, 0: 0, 12: 0, 24: 0}
        for it in items[:: max(1, len(items) // 12)]:
            m = estimate_midi(it["x"], it["sr"])
            if m is None:
                continue
            for off in votes:
                if abs(m - (it["note"] + off)) < 1.0:
                    votes[off] += 1
        offset = max(votes, key=lambda k: votes[k])
        if votes[offset] == 0:
            offset = 12   # padrão VSCO (dó central = C3)
        log("%-5s %-9s %-34s %3d arquivos, oitava %+d (votos %s)" % (sec, art, folder, len(items), offset, votes))

        # camadas e RR em índices 0..n-1
        layers = sorted(set(it["layer"] for it in items))
        lmap = {l: i for i, l in enumerate(layers)}
        rrs = sorted(set(it["rr"] for it in items))
        rmap = {r: i for i, r in enumerate(rrs)}
        for it in items:
            it["root"] = it["note"] + offset
            it["L"] = lmap[it["layer"]]
            it["R"] = rmap[it["rr"]]
            it["y"] = trim_and_shape(it["x"], it["sr"], kind, art)
            it["rms"] = body_rms(it["y"], it["sr"])
            del it["x"]

        # volume: suaviza diferenças entre notas vizinhas da mesma camada (±4 dB da mediana),
        # mantém a diferença natural entre camadas; camada mais forte → TARGET_RMS_DB
        for L in range(len(layers)):
            grp = [it for it in items if it["L"] == L]
            med = np.median([20 * np.log10(it["rms"]) for it in grp])
            for it in grp:
                db = 20 * np.log10(it["rms"])
                corr = np.clip(med - db, -4.0, 4.0) * 0.7
                it["gain_db"] = corr
            for it in grp:
                it["layer_med"] = med
        top = max(it["layer_med"] for it in items)
        inst_gain = TARGET_RMS_DB - top
        zones = []
        for it in items:
            g = 10 ** ((it["gain_db"] + inst_gain) / 20.0)
            y = it["y"] * g
            pk = np.max(np.abs(y))
            if pk > 0.98:
                y *= 0.98 / pk
            name = os.path.splitext(os.path.basename(it["path"]))[0] + ".flac"
            rel = outdir + "/" + name
            dest = os.path.join(out, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            sf.write(dest, y.astype(np.float32), it["sr"], subtype="PCM_24", format="FLAC")
            total_bytes += os.path.getsize(dest)
            ls, le = find_loop(y, it["sr"]) if kind == "sus" else (-1, -1)
            zones.append(dict(file=rel, root=int(it["root"]), layer=it["L"], rr=it["R"], loopStart=ls, loopEnd=le))
            sources_txt.append("%s  <=  %s%s  [%s]" % (rel, RAW, it["path"], LICENSE))
        # faixas de teclas por (camada, rr)
        for L in range(len(layers)):
            for R in range(len(rrs)):
                grp = sorted([z for z in zones if z["layer"] == L and z["rr"] == R], key=lambda z: z["root"])
                for i, z in enumerate(grp):
                    lo = 0 if i == 0 else (grp[i - 1]["root"] + z["root"]) // 2 + 1
                    hi = 127 if i == len(grp) - 1 else (z["root"] + grp[i + 1]["root"]) // 2
                    if i == 0:
                        lo = max(0, z["root"] - 5)
                    if i == len(grp) - 1:
                        hi = min(127, z["root"] + 5)
                    z["lo"], z["hi"] = int(lo), int(hi)
        for z in zones:
            z.setdefault("lo", z["root"])
            z.setdefault("hi", z["root"])
        short = kind == "short"
        cfg = SECTION[sec]
        instruments[(sec, art)] = dict(section=sec, artic=art, numLayers=len(layers), numRR=len(rrs), oneShot=short,
                                       attack=0.002 if short else 0.03, release=0.2 if short else cfg["release"],
                                       gainDb=cfg["gainDb"], pan=cfg["pan"], zones=zones)

    # Violinos II: mesmas gravações dos Violinos I, outra estante (pan, afinação, RR e início deslocados)
    for art in ("sustain", "spiccato", "pizzicato", "tremolo"):
        v1 = instruments.get(("vln1", art))
        if not v1:
            continue
        v2 = json.loads(json.dumps(v1))
        v2["section"] = "vln2"
        v2["gainDb"] = SECTION["vln2"]["gainDb"]
        v2["pan"] = SECTION["vln2"]["pan"]
        v2["detuneCents"] = -5.0
        v2["rrOffset"] = 1
        v2["extraStartFrames"] = 0 if v1["oneShot"] else 600
        instruments[("vln2", art)] = v2

    lib = dict(name="VSCO 2 CE Strings & Horns — SeniorMy edition", version=1, source=REPO, commit=COMMIT,
               license=LICENSE, credit="Versilian Studios (Sam Gossner) & Ivy Audio (Simon Dalzell) — http://vis.versilstudios.net/vsco-community.html",
               instruments=list(instruments.values()))
    with open(os.path.join(out, "library.json"), "w", encoding="utf-8") as fh:
        json.dump(lib, fh, ensure_ascii=False, indent=1)
    with open(os.path.join(out, "LICENSE-SOURCES.txt"), "w", encoding="utf-8") as fh:
        fh.write("SeniorMy Structure — biblioteca orquestral: origem e licença de cada arquivo\n")
        fh.write("Fonte: VS Chamber Orchestra: Community Edition (VSCO 2 CE) 1.1.0, Versilian Studios LLC\n")
        fh.write("Gravação: Sam Gossner & Simon Dalzell (Ivy Audio). Corte: Elan Hickler (Soundemote).\n")
        fh.write("Página: http://vis.versilstudios.net/vsco-community.html — Repositório: https://github.com/%s @ %s\n" % (REPO, COMMIT))
        fh.write("Licença: %s — https://creativecommons.org/publicdomain/zero/1.0/\n" % LICENSE)
        fh.write("Processamento SeniorMy: corte de silêncio, limite de duração, ponto de loop, igualização de volume, FLAC 24 bits.\n\n")
        fh.write("\n".join(sources_txt) + "\n")
    with open(os.path.join(out, "README.txt"), "w", encoding="utf-8") as fh:
        fh.write("SeniorMy Structure — Structure Library\n© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77 (seleção, edição e mapeamento).\n"
                 "Gravações originais: VSCO 2 CE (CC0 1.0). Veja LICENSE-SOURCES.txt.\n")
    nzones = sum(len(i["zones"]) for i in instruments.values())
    log("OK: %d instrumentos, %d zonas, %d arquivos FLAC, %.1f MB" % (len(instruments), nzones, len(sources_txt), total_bytes / 1048576.0))


if __name__ == "__main__":
    main()
