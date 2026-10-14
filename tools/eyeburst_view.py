"""EYEBURST viewer (2026-10-13): find the flicker frames in a recording made by `bo eyeburst N`.

Usage: python tools/eyeburst_view.py <captures\\eyeburst_HHMMSS folder>

Reads index.txt + eye.raw + hud.raw (RGB, 1/step size). For each eye (sign -1 left, +1 right, 0 untagged) the pictures
are compared with that eye's previous and next picture: a flash = differs from both neighbours while the neighbours
match each other. Writes flashes.txt (ranked) and flash_<seq>.png (rows: eye before / flash / after, HUD sheet the same;
plus the OTHER eye's picture nearest in time, to test "the other eye's content").
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def load(folder: Path):
    lines = (folder / "index.txt").read_text().splitlines()
    head = lines[0].split()
    ew, eh = map(int, head[2].split("x"))
    hw, hh = map(int, head[6].split("x"))
    step = int(head[10])
    ew, eh, hw, hh = ew // step, eh // step, hw // step, hh // step
    rows = []
    for ln in lines:
        if ln.startswith("#") or not ln.strip():
            continue
        a, b = ln.split("|")
        f = a.split()
        rows.append(dict(seq=int(f[0]), ms=float(f[1]), sign=int(f[2]), gen=int(f[3]), afw=int(f[4]),
                         counters=b.split()))
    n = len(rows)
    eye = np.fromfile(folder / "eye.raw", np.uint8)[: n * ew * eh * 3].reshape(n, eh, ew, 3)
    hud = np.fromfile(folder / "hud.raw", np.uint8)[: n * hw * hh * 3].reshape(n, hh, hw, 3)
    return rows, eye, hud


def load_final(folder: Path):
    """EYEBURST2: the finished headset images (final.raw + final.txt); None for older records."""
    p = folder / "final.txt"
    if not p.exists():
        return None
    lines = p.read_text().splitlines()
    head = [ln for ln in lines if ln.startswith("# final")]
    if not head:
        return None
    f = head[0].split()
    w, h = map(int, f[2].split("x"))
    step = int(f[6])
    w, h = w // step, h // step
    rows = []
    for ln in lines:
        if ln.startswith("#") or not ln.strip():
            continue
        a = ln.split()
        rows.append(dict(fseq=int(a[0]), eye=int(a[1]), seq=int(a[2])))
    pics = np.fromfile(folder / "final.raw", np.uint8)[: len(rows) * w * h * 3].reshape(len(rows), h, w, 3)
    return rows, pics


def lit(pics):
    return (pics.max(3) > 8).reshape(len(pics), -1).sum(1)


def diff(a, b):
    return float(np.abs(a.astype(np.int16) - b.astype(np.int16)).mean())


def main():
    folder = Path(sys.argv[1])
    rows, eye, hud = load(folder)
    out = []
    for sign in (-1, 1, 0):
        idx = [i for i, r in enumerate(rows) if r["sign"] == sign]
        for k in range(1, len(idx) - 1):
            p, c, nx = idx[k - 1], idx[k], idx[k + 1]
            for name, pics in (("eye", eye), ("hud", hud)):
                s = min(diff(pics[c], pics[p]), diff(pics[c], pics[nx])) - diff(pics[nx], pics[p])
                out.append((s, name, sign, c, p, nx))
    out.sort(reverse=True)
    signs = {-1: "L", 1: "R", 0: "untagged"}
    with open(folder / "flashes.txt", "w") as f:
        f.write(f"{len(rows)} presents; eyes: " + ", ".join(
            f"{signs[s]} {sum(r['sign'] == s for r in rows)}" for s in (-1, 1, 0)) + "\n")
        f.write("score layer eye seq (prev, next) gen | counters\n")
        for s, name, sign, c, p, nx in out[:40]:
            r = rows[c]
            f.write(f"{s:7.2f} {name} {signs[sign]} {r['seq']} ({rows[p]['seq']}, {rows[nx]['seq']}) gen {r['gen']} | "
                    f"{' '.join(r['counters'])}\n")
    saved = 0
    for s, name, sign, c, p, nx in out:
        if s < 2.0 or saved >= 8:
            break
        other = [i for i, r in enumerate(rows) if r["sign"] == -sign and sign != 0]
        o = min(other, key=lambda i: abs(i - c)) if other else c
        top = np.concatenate([eye[p], eye[c], eye[nx], eye[o]], 1)
        bot = np.concatenate([hud[p], hud[c], hud[nx], hud[o]], 1)
        if bot.shape[1] != top.shape[1]:
            bot = np.array(Image.fromarray(bot).resize((top.shape[1], bot.shape[0])))
        Image.fromarray(np.concatenate([top, bot], 0)).save(folder / f"flash_{rows[c]['seq']}_{name}.png")
        saved += 1
    print((folder / "flashes.txt").read_text())
    # Per-present states (EYEBURST2 columns) and how lit each stream is, so a picture stuck in one of them shows up.
    el, hl = lit(eye), lit(hud)
    print("seq sign | eye_lit hud_lit | states (last columns of index.txt)")
    for i in range(0, len(rows), max(1, len(rows) // 40)):
        r = rows[i]
        print(f"{r['seq']:5d} {r['sign']:+d} | {el[i]:6d} {hl[i]:6d} | {' '.join(r['counters'][-3:])}")
    fin = load_final(folder)
    if fin:
        frows, fpics = fin
        fl = lit(fpics)
        print(f"\nfinished headset images: {len(frows)}; lit pixels per image (every ~40th):")
        for i in range(0, len(frows), max(1, len(frows) // 40)):
            print(f"  fseq {frows[i]['fseq']:5d} eye {frows[i]['eye']} at present {frows[i]['seq']:5d}: lit {fl[i]}")
        for k, i in enumerate([0, len(frows) // 2, len(frows) - 1]):
            Image.fromarray(fpics[i]).save(folder / f"final_{k}_{frows[i]['fseq']}_eye{frows[i]['eye']}.png")


if __name__ == "__main__":
    main()
