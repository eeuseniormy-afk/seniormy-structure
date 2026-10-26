#!/usr/bin/env python3
"""Envia o código do SeniorMy Acoustic Guitars para um repositório GitHub via API (sem git local).
Inclui a biblioteca de amostras (library/, FLAC CC0). Só envia blobs que mudaram (compara o SHA git).

Uso:
  python3 seniormy-structure-push.py [--repo dono/repo] [--branch nome] [--root pasta]
                                     [--message "msg"] [--dry-run] [--orphan]

Padrões (decisão do usuário em 2026-10-25): repo eeuseniormy-afk/seniormy-structure (novo, público),
branch main, pasta /workspace/work/seniormy-structure.

- Repositório vazio (recém-criado): cria o primeiro commit pela API de conteúdos e depois envia tudo.
- Branch inexistente: cria o branch (órfão, sem pai) — útil para um branch "seniormy-structure" num repo
  existente, ex.: --repo eeuseniormy-afk/seniormy-keys --branch seniormy-structure.
- --orphan: o novo commit não tem pai e o ref é atualizado à força (reescreve o histórico do branch).
- Nunca toca outros branches nem tags. A biblioteca de amostras (library/), builds e artefatos ficam de fora;
  a biblioteca é gerada no CI por scripts/prepare_library.py.
- Autenticação: o gateway do ambiente Hark (mesmo esquema dos outros scripts de push) ou a variável
  GITHUB_TOKEN. Enviar .github/workflows exige permissão "workflow" no token/app.
"""
import argparse, base64, hashlib, json, os, subprocess, sys

EXCLUDE_PREFIXES = ("_deps/", "build", "art/", "dist/", "pkgroot/", "pkgs/", "shots/", "logs/",
                    "samples-cache/", "scripts/mac/res/", "zipstage/", "venv/", ".git/")
EXCLUDE_FILES = ("scripts/win/license-pt-BR.txt", "scripts/win/license-en.txt")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="eeuseniormy-afk/seniormy-structure")
    ap.add_argument("--branch", default="acoustic-guitars")
    ap.add_argument("--root", default="/workspace/work/seniormy-acoustic-guitars")
    ap.add_argument("--message", default="SeniorMy Acoustic Guitars")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--orphan", action="store_true")
    a = ap.parse_args()
    base = "https://api.github.com/repos/" + a.repo

    def api(method, path, data=None):
        cmd = ["curl", "-s", "-X", method, "-H", "Accept: application/vnd.github+json",
               "-H", "X-GitHub-Api-Version: 2022-11-28", "-w", "\n%{http_code}"]
        tok = os.environ.get("GITHUB_TOKEN")
        if tok:
            cmd += ["-H", "Authorization: Bearer " + tok]
        cmd.append(base + path)
        if data is not None:
            cmd += ["-d", "@-"]
        out = subprocess.run(cmd, input=json.dumps(data).encode() if data is not None else None,
                             capture_output=True).stdout.decode()
        body, _, code = out.rpartition("\n")
        try:
            js = json.loads(body) if body.strip() else {}
        except ValueError:
            js = {"raw": body}
        return int(code or 0), js

    files = []
    for dp, dn, fn in os.walk(a.root):
        dn[:] = [d for d in dn if d not in (".git", "__pycache__", "venv")]
        for f in fn:
            p = os.path.relpath(os.path.join(dp, f), a.root).replace(os.sep, "/")
            if p.startswith(EXCLUDE_PREFIXES) or p in EXCLUDE_FILES or p.endswith((".pyc", ".DS_Store")):
                continue
            files.append(p)
    files.sort()
    total = sum(os.path.getsize(os.path.join(a.root, f)) for f in files)
    print("%d arquivos, %.1f MB -> %s (branch %s)" % (len(files), total / 1048576.0, a.repo, a.branch))
    if a.dry_run:
        print("\n".join(files))
        return

    code, repo = api("GET", "")
    if code != 200:
        sys.exit("repositório inacessível (%d): %s" % (code, repo.get("message")))

    # repositório vazio → primeiro commit pela API de conteúdos
    code, ref = api("GET", "/git/ref/heads/" + a.branch)
    if code != 200:
        c2, _ = api("GET", "/commits?per_page=1")
        if c2 == 409:   # repositório vazio: a API Git só funciona depois do primeiro commit
            print("repositório vazio: criando o commit inicial em", a.branch)
            c3, r3 = api("PUT", "/contents/README.md", {
                "message": "Inicializa o repositório",
                "content": base64.b64encode(open(os.path.join(a.root, "README.md"), "rb").read()).decode(),
                "branch": a.branch})
            if c3 not in (200, 201):
                sys.exit("falha ao inicializar (%d): %s" % (c3, r3.get("message")))
            code, ref = api("GET", "/git/ref/heads/" + a.branch)

    existing = set()
    if code == 200:
        c0, tr0 = api("GET", "/git/trees/%s?recursive=1" % ref["object"]["sha"])
        if c0 == 200:
            existing = {e["sha"] for e in tr0.get("tree", []) if e.get("type") == "blob"}
    cache_path = os.path.join(os.path.dirname(os.path.abspath(a.root)), ".push-cache-" + a.branch + ".json")
    try:
        existing |= set(json.load(open(cache_path)))
    except Exception:
        pass
    shas = {}
    todo = []
    for f in files:
        data = open(os.path.join(a.root, f), "rb").read()
        sha = hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()
        shas[f] = sha
        if sha not in existing:
            todo.append((f, sha))
    print("blobs a enviar: %d de %d" % (len(todo), len(files)))
    import concurrent.futures, time
    deadline = time.time() + float(os.environ.get("PUSH_BUDGET_S", "95"))
    def up(item):
        f, sha = item
        if time.time() > deadline:
            return None
        data = open(os.path.join(a.root, f), "rb").read()
        c, b = api("POST", "/git/blobs", {"content": base64.b64encode(data).decode(), "encoding": "base64"})
        if c != 201:
            raise SystemExit("falha no blob %s (%d): %s" % (f, c, b.get("message")))
        return b["sha"]
    done = 0
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        for res in ex.map(up, todo):
            if res:
                existing.add(res); done += 1
    json.dump(sorted(existing), open(cache_path, "w"))
    if done < len(todo):
        print("PARCIAL: %d/%d blobs enviados; rode de novo para continuar" % (done, len(todo)))
        return
    uploaded = done
    tree = []
    for f in files:
        mode = "100755" if f.endswith(".sh") or f.endswith("postinstall") or f.endswith(".py") else "100644"
        tree.append({"path": f, "mode": mode, "type": "blob", "sha": shas[f]})
    c, t = api("POST", "/git/trees", {"tree": tree})
    if c != 201:
        sys.exit("falha na árvore (%d): %s" % (c, t.get("message")))
    parents = [] if (a.orphan or code != 200) else [ref["object"]["sha"]]
    c, commit = api("POST", "/git/commits", {"message": a.message, "tree": t["sha"], "parents": parents})
    if c != 201:
        sys.exit("falha no commit (%d): %s" % (c, commit.get("message")))
    if code == 200:
        c, r = api("PATCH", "/git/refs/heads/" + a.branch, {"sha": commit["sha"], "force": bool(a.orphan)})
    else:
        c, r = api("POST", "/git/refs", {"ref": "refs/heads/" + a.branch, "sha": commit["sha"]})
    if c not in (200, 201):
        sys.exit("falha ao atualizar o branch (%d): %s" % (c, r.get("message")))
    print("ok:", a.repo, a.branch, (parents[0][:7] if parents else "novo"), "->", commit["sha"][:7])
    print("blobs enviados: %d de %d" % (uploaded, len(files)))


if __name__ == "__main__":
    main()
