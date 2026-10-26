#!/usr/bin/env python3
"""SeniorMy Acoustic Guitars — prepara a biblioteca de amostras (FLAC 24 bits + library.json).

Uso: python3 scripts/prepare_library.py <pasta-de-downloads> <pasta-de-saida>

Fontes (todas verificadas como CC0 1.0 — ver docs/BIBLIOTECA-AMOSTRAS.md):
  steel   Martin HD-28 (Jeff Learman, Discord SFZ GM Bank)            CC0
  nylon   FreePats Spanish Classical Guitar 2019-06-18 (roberto@zenvoid.org)  CC0
  ukulele FreePats Ukulele 2026-08-11 (Mateusz Dąbrowski)              CC0
  banjo   sfzinstruments/ganjo — SX 6-string banjo (itsclipping)       CC0
  sitar   Discord SFZ GM Bank 105-Sitar (Dr. Narayan Bhagawan Raikar)  CC0

Processamento: corte do silêncio inicial, verificação de afinação (autocorrelação, corrige desvios
< 100 cents e informa), "desenrolar" de loops (Martin) com decaimento natural, normalização por
instrumento, fade final, mono → FLAC 24 bits. Dependências: numpy, soundfile, libarchive-c.
© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77 (seleção, edição e mapeamento).
"""
import json, os, re, struct, sys, urllib.request, urllib.parse
import numpy as np
import soundfile as sf

UA = {"User-Agent": "Mozilla/5.0 (SeniorMy library prep)"}
GM = "sfzinstruments/Discord-SFZ-GM-Bank"
GM_COMMIT = "7a9c478fe331f94f246d33332f0adedb25bbbe27"
SOURCES = {
    "nylon": ("archive", "https://freepats.zenvoid.org/Guitar/SpanishClassicalGuitar/SpanishClassicalGuitar-SFZ+FLAC-20190618.7z"),
    "ukulele": ("archive", "https://github.com/freepats/ukulele1/releases/download/2026-08-11/Ukulele-SFZ+FLAC-20260811.7z"),
    "steel": ("github", GM, "Discord GM/Melodic/026-Acoustic Guitar (steel)", GM_COMMIT),
    "sitar": ("github", GM, "Discord GM/Melodic/105-Sitar", GM_COMMIT),
    "banjo": ("github", "sfzinstruments/ganjo", "", "ccff5cd5cd3b513873a48994c07724d9d3c39e1c"),
}
NOTE = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def key(v):
    v = v.strip().lower()
    if re.fullmatch(r"-?\d+", v): return int(v)
    m = re.fullmatch(r"([a-g])([#b]?)(-?\d)", v)
    n = NOTE[m.group(1)] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return 12 * (int(m.group(3)) + 1) + n


def get(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0: return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=120) as r, open(dst + ".part", "wb") as f: f.write(r.read())
    os.replace(dst + ".part", dst)


def gh_tree(repo, path, ref, dst):
    hdr = dict(UA)
    if os.environ.get("GITHUB_TOKEN"): hdr["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]
    u = f"https://api.github.com/repos/{repo}/contents/{urllib.parse.quote(path)}?ref={ref}"
    items = json.load(urllib.request.urlopen(urllib.request.Request(u, headers=hdr)))
    for it in items:
        if it["type"] == "dir":
            if it["name"] in ("Common",): gh_tree(repo, it["path"], ref, os.path.join(dst, it["name"]))
        elif re.search(r"\.(wav|flac|sfz|md|txt)$", it["name"], re.I) or it["name"] == "LICENSE":
            get(it["download_url"], os.path.join(dst, it["name"]))


def fetch(cache):
    for name, src in SOURCES.items():
        d = os.path.join(cache, name)
        if os.path.exists(os.path.join(d, ".done")): continue
        print("baixando", name, flush=True)
        if src[0] == "archive":
            a = os.path.join(cache, os.path.basename(src[1])); get(src[1], a)
            import libarchive
            os.makedirs(d, exist_ok=True); cwd = os.getcwd(); os.chdir(d)
            try: libarchive.extract_file(a)
            except Exception as e: print("  aviso (extração parcial):", e)
            os.chdir(cwd)
        else:
            gh_tree(src[1], src[2], src[3], d)
        open(os.path.join(d, ".done"), "w").close()


def parse_sfz(path):
    """Regiões simples: sample, pitch_keycenter, lokey, hikey, key, tune, seq_position."""
    txt = open(path, encoding="utf-8", errors="replace").read()
    txt = re.sub(r"//[^\n]*", "", txt)
    regions, glob = [], {}
    for tag, body in re.findall(r"<(\w+)>([^<]*)", txt):
        ops = dict(re.findall(r"(\w+)=(.+?)(?=\s+\w+=|\s*$)", body.strip().replace("\n", " ")))
        if tag == "region":
            r = dict(glob); r.update(ops); regions.append(r)
        elif tag in ("group", "global", "master"):
            glob = {k: v for k, v in ops.items() if k in ("tune",)}
    out, seen = [], set()
    for r in regions:
        if "sample" not in r or r["sample"].startswith("*"): continue
        s = r["sample"].replace("\\", "/").strip()
        if s in seen: continue
        seen.add(s)
        root = key(r.get("pitch_keycenter", r.get("key", "60")))
        lo = key(r.get("lokey", r.get("key", str(root)))); hi = key(r.get("hikey", r.get("key", str(root))))
        out.append(dict(file=s, root=root, lo=lo, hi=hi, tune=float(r.get("tune", 0)), seq=int(r.get("seq_position", 1))))
    return out


def wav_loop(path):
    try:
        d = open(path, "rb").read(); i = d.find(b"smpl")
        if i < 0 or struct.unpack("<I", d[i + 36:i + 40])[0] == 0: return None
        return struct.unpack("<II", d[i + 52:i + 60])
    except Exception: return None


def f0(x, sr, expect_hz):
    """Afinação estimada (Hz) por autocorrelação perto do esperado; None se incerta."""
    n0 = int(0.08 * sr); seg = x[n0:n0 + int(0.25 * sr)]
    if len(seg) < 2048: seg = x[:int(0.3 * sr)]
    seg = seg - seg.mean()
    if np.max(np.abs(seg)) < 1e-4: return None
    lo, hi = int(sr / (expect_hz * 1.12)), int(sr / (expect_hz / 1.12)) + 1
    if hi >= len(seg) // 2: return None
    w = seg * np.hanning(len(seg))
    ac = np.fft.irfft(np.abs(np.fft.rfft(w, 2 * len(w))) ** 2)[:hi + 2]
    k = lo + int(np.argmax(ac[lo:hi]))
    if ac[k] < 0.3 * ac[0]: return None
    a, b, c = ac[k - 1], ac[k], ac[k + 1]
    off = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0
    return sr / (k + off)


def process(x, sr, loop=None, unroll_s=0.0):
    if x.ndim > 1: x = x.mean(axis=1)
    x = x.astype(np.float64)
    if loop and unroll_s > 0:
        s, e = loop
        if 0 < s < e < len(x) and e - s > 256:
            body = x[:e]; cyc = x[s:e]; need = int(unroll_s * sr) - len(body)
            reps = max(0, int(np.ceil(need / len(cyc))))
            tail = np.tile(cyc, reps)[:max(0, need)]
            t = np.arange(len(tail)) / sr
            tail *= np.exp(-t * 6.9 / max(0.5, unroll_s - len(body) / sr))
            x = np.concatenate([body, tail])
    thr = 0.02 * np.max(np.abs(x))
    on = int(np.argmax(np.abs(x) > thr)); on = max(0, on - int(0.002 * sr))
    x = x[on:]
    env = np.abs(x); level = 20 * np.log10(np.maximum(env, 1e-9) / env.max())
    above = np.where(level > -62)[0]
    end = min(len(x), (above[-1] if len(above) else len(x)) + int(0.05 * sr))
    x = x[:end]
    fl = min(len(x) // 4, int(0.12 * sr)); x[-fl:] *= np.linspace(1, 0, fl) ** 2
    fi = int(0.0015 * sr); x[:fi] *= np.linspace(0, 1, fi)
    return x


def build(cache, out):
    lib = {"version": 1, "product": "SeniorMy Acoustic Guitars", "instruments": {}}
    report = []

    def emit(inst, items, base, gain_db=0.0, unroll=None):
        zones = []; peaks = []
        os.makedirs(os.path.join(out, inst), exist_ok=True)
        procd = []
        for it in items:
            p = os.path.join(base, it["file"])
            if not os.path.exists(p): report.append(f"{inst}: falta {it['file']}"); continue
            try: x, sr = sf.read(p, always_2d=False)
            except Exception as e: report.append(f"{inst}: ilegível {it['file']} ({e})"); continue
            loop = wav_loop(p) if unroll else None
            y = process(x, sr, loop, unroll(it["root"]) if unroll else 0)
            hz = 440 * 2 ** ((it["root"] - 69 + it["tune"] / 100) / 12)
            est = f0(y, sr, hz); fix = 0.0
            if est:
                cents = 1200 * np.log2(est / hz)
                if abs(cents) < 100: fix = -cents if abs(cents) > 4 else 0.0
                else: report.append(f"{inst}: {it['file']} desvio {cents:+.0f} cents (mantido)")
            pk = np.max(np.abs(y[:int(0.25 * sr)]))
            procd.append((it, y, sr, it["tune"] + fix)); peaks.append(pk)
        ref = np.median(peaks) if peaks else 1.0
        for (it, y, sr, tune), pk in zip(procd, peaks):
            # equaliza nível entre amostras vizinhas (metade do desvio — mantém o caráter)
            g = (ref / max(pk, 1e-6)) ** 0.5
            y = y * g
            name = re.sub(r"[^A-Za-z0-9#_-]+", "_", os.path.splitext(os.path.basename(it["file"]))[0])
            fn = f"{inst}/{name}.flac"
            zones.append(dict(file=fn, root=it["root"], lo=it["lo"], hi=it["hi"], tune=round(float(tune), 2),
                              rr=it.get("rr", 0), gain=1.0))
            procd_y = y
            sf.write(os.path.join(out, fn), procd_y.astype(np.float32), sr, subtype="PCM_24", format="FLAC")
        # normalização global do instrumento: pico máximo −1 dBFS
        mx = 0.0
        for z in zones: mx = max(mx, np.max(np.abs(sf.read(os.path.join(out, z["file"]))[0])))
        k = 10 ** (-1 / 20) / max(mx, 1e-6)
        for z in zones:
            pth = os.path.join(out, z["file"]); y, sr = sf.read(pth)
            sf.write(pth, (y * k).astype(np.float32), sr, subtype="PCM_24", format="FLAC")
        numrr = max([z["rr"] for z in zones] + [0]) + 1
        lib["instruments"][inst] = {"zones": zones, "numRR": numrr, "gainDb": gain_db}
        print(f"  {inst}: {len(zones)} amostras, rr={numrr}", flush=True)

    # Violão de aço — Martin HD-28 (loops desenrolados até um decaimento natural)
    d = os.path.join(cache, "steel"); sfz = [f for f in os.listdir(d) if f.endswith(".sfz")][0]
    items = parse_sfz(os.path.join(d, sfz))
    emit("steel", items, d, 0.0, unroll=lambda r: float(np.clip(14 - (r - 40) * 0.2, 4.5, 14)))

    # Violão de nylon — FreePats
    d = os.path.join(cache, "nylon"); sub = [x for x in os.listdir(d) if os.path.isdir(os.path.join(d, x))][0]
    sfz = [f for f in os.listdir(os.path.join(d, sub)) if f.endswith(".sfz")][0]
    emit("nylon", parse_sfz(os.path.join(d, sub, sfz)), os.path.join(d, sub))

    # Ukulele — FreePats (afinações finas do SFZ original preservadas)
    d = os.path.join(cache, "ukulele"); sub = [x for x in os.listdir(d) if os.path.isdir(os.path.join(d, x))][0]
    sfz = [f for f in os.listdir(os.path.join(d, sub)) if f.endswith(".sfz")][0]
    items = [r for r in parse_sfz(os.path.join(d, sub, sfz)) if r["root"] >= 55 and "tap" not in r["file"] and "chuck" not in r["file"]]
    emit("ukulele", items, os.path.join(d, sub))
    tap = os.path.join(d, sub, "samples", "body-tap.flac")
    if os.path.exists(tap):
        os.makedirs(os.path.join(out, "noises"), exist_ok=True)
        y = process(sf.read(tap)[0], 48000); y = y / np.max(np.abs(y)) * 0.89
        sf.write(os.path.join(out, "noises", "body-tap.flac"), y.astype(np.float32), 48000, subtype="PCM_24", format="FLAC")
        lib["noises"] = {"bodyTap": "noises/body-tap.flac"}

    # Banjo — ganjo (até 4 round robins por nota)
    d = os.path.join(cache, "banjo")
    items = []; cnt = {}
    for r in parse_sfz(os.path.join(d, "ganjo.sfz")):
        c = cnt.get(r["root"], 0)
        if c >= 4: continue
        cnt[r["root"]] = c + 1; r["rr"] = c; items.append(r)
    emit("banjo", items, d, -1.0)

    # Cítara indiana (sitar) — só os pinçados (Str*)
    d = os.path.join(cache, "sitar")
    items = [r for r in parse_sfz(os.path.join(d, "Sitar.sfz")) if os.path.basename(r["file"]).startswith("Str")]
    emit("sitar", items, d, -2.0)

    with open(os.path.join(out, "library.json"), "w", encoding="utf-8") as f: json.dump(lib, f, indent=1, ensure_ascii=False)
    with open(os.path.join(out, "README.txt"), "w", encoding="utf-8") as f:
        f.write("SeniorMy Acoustic Guitars — Biblioteca de amostras\n"
                "© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77 (seleção, edição e mapeamento).\n"
                "Gravações originais em domínio público (CC0 1.0). Veja LICENSE-SOURCES.txt.\n")
    with open(os.path.join(out, "LICENSE-SOURCES.txt"), "w", encoding="utf-8") as f:
        f.write(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources", "legal", "THIRD-PARTY-NOTICES.txt"), encoding="utf-8").read())
    print("\n".join(report) if report else "afinação: tudo dentro de ±100 cents")


if __name__ == "__main__":
    cache, out = sys.argv[1], sys.argv[2]
    os.makedirs(cache, exist_ok=True); os.makedirs(out, exist_ok=True)
    fetch(cache); build(cache, out)
