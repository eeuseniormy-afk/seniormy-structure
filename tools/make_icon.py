#!/usr/bin/env python3
"""Gera Resources/icon.png (1024, fundo transparente), icon.ico e logo-256.png a partir de
Resources/icon-source.png (squircle sobre fundo claro). Uso: python3 tools/make_icon.py"""
import os
from collections import deque
import numpy as np
from PIL import Image, ImageFilter

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")
im = Image.open(os.path.join(R, "icon-source.png")).convert("RGB")
a = np.asarray(im).astype(np.int32)
H, W, _ = a.shape
lum = a.mean(2); chroma = a.max(2) - a.min(2)
bg_like = (lum > 175) & (chroma < 40)
mask = np.zeros((H, W), bool); q = deque()
for p in [(0, 0), (0, W - 1), (H - 1, 0), (H - 1, W - 1)]:
    if bg_like[p]: q.append(p); mask[p] = True
while q:
    y, x = q.popleft()
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        ny, nx = y + dy, x + dx
        if 0 <= ny < H and 0 <= nx < W and not mask[ny, nx] and bg_like[ny, nx]:
            mask[ny, nx] = True; q.append((ny, nx))
fg = Image.fromarray((~mask).astype(np.uint8) * 255).filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(1.0))
rgba = im.convert("RGBA"); rgba.putalpha(fg)
x0, y0, x1, y1 = fg.point(lambda v: 255 if v > 8 else 0).getbbox()
s = int(max(x1 - x0, y1 - y0) * 1.04); cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
box = (int(cx - s / 2), int(cy - s / 2), int(cx - s / 2) + s, int(cy - s / 2) + s)
icon = rgba.crop(box).resize((1024, 1024), Image.LANCZOS)
icon.save(os.path.join(R, "icon.png"))
icon.save(os.path.join(R, "icon.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
icon.resize((256, 256), Image.LANCZOS).save(os.path.join(R, "logo-256.png"))
print("ok", box)
