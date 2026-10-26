#!/usr/bin/env python3
"""Gera Resources/icon.png (1024, fundo transparente), icon.ico e logo-256.png a partir de
Resources/icon-source.png (PNG RGBA já transparente). Uso: python3 tools/make_icon.py"""
import os
from PIL import Image
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")
im = Image.open(os.path.join(R, "icon-source.png")).convert("RGBA")
bb = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox() or (0, 0) + im.size
x0, y0, x1, y1 = bb
s = int(max(x1 - x0, y1 - y0) * 1.02); cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0))
canvas.paste(im.crop(bb), (int(s / 2 - (x1 - x0) / 2), int(s / 2 - (y1 - y0) / 2)))
icon = canvas.resize((1024, 1024), Image.LANCZOS)
icon.save(os.path.join(R, "icon.png"))
icon.save(os.path.join(R, "icon.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
icon.resize((256, 256), Image.LANCZOS).save(os.path.join(R, "logo-256.png"))
print("ok", bb)
