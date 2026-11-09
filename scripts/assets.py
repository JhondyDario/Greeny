#!/usr/bin/env python3
"""Genera los recursos de la app a partir de assets/mascot.png.

  app/data/mascot_big.bin    RGBA crudo 220x330 (pantalla de inicio)
  app/data/mascot_small.bin  RGBA crudo 50x75   (cabecera)
  app/icon.jpg               icono 256x256 para el menu de homebrew

Si cambias los tamaños, cambia tambien BIG_W/BIG_H/SMALL_W/SMALL_H en app/source/main.c.
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = Image.open(os.path.join(ROOT, "assets", "mascot.png")).convert("RGBA")
src = src.crop(src.getbbox())  # quitar el margen transparente


def fit(img, w, h):
    """Escala manteniendo proporcion y centra en un lienzo transparente w x h."""
    scale = min(w / img.width, h / img.height)
    r = img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste(r, ((w - r.width) // 2, (h - r.height) // 2), r)
    return out


os.makedirs(os.path.join(ROOT, "app", "data"), exist_ok=True)
for name, (w, h) in {"mascot_big": (220, 330), "mascot_small": (50, 75)}.items():
    with open(os.path.join(ROOT, "app", "data", name + ".bin"), "wb") as f:
        f.write(fit(src, w, h).tobytes())

icon = Image.new("RGBA", (256, 256), (0xF3, 0xF7, 0xEC, 255))
m = fit(src, 220, 236)
icon.paste(m, (18, 10), m)
icon.convert("RGB").save(os.path.join(ROOT, "app", "icon.jpg"), quality=92)
print("recursos generados")
