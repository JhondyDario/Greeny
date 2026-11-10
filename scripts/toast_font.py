#!/usr/bin/env python3
"""Genera source/toast_font.h: las letras del aviso que sale dentro del juego.

El sysmodule no puede cargar una fuente de verdad (tiene muy poca memoria), asi
que aqui se dibuja cada letra una vez y se guarda como una imagen pequeña con 16
niveles de transparencia. Fuente: Inter (SIL Open Font License).

Uso: python3 scripts/toast_font.py [carpeta con Inter-Bold.otf e Inter-Medium.otf]
"""
import sys
from pathlib import Path
from PIL import ImageFont

FONT_DIR = Path(sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter")
OUT = Path(__file__).resolve().parent.parent / "source" / "toast_font.h"
CHARS = list(range(32, 127)) + list(range(161, 256))  # ASCII y Latin-1 (acentos, ñ, ¿, ¡)
FONTS = [("big", "Inter-Bold.otf", 20), ("small", "Inter-Medium.otf", 14)]


def build(name, file, size):
    font = ImageFont.truetype(str(FONT_DIR / file), size)
    ascent, descent = font.getmetrics()
    data, glyphs = bytearray(), {}
    for cp in CHARS:
        ch = chr(cp)
        adv = round(font.getlength(ch))
        box = font.getbbox(ch)
        mask = font.getmask(ch)
        w, h = mask.size
        if not box or w == 0 or h == 0:
            glyphs[cp] = (0, 0, 0, 0, 0, adv)
            continue
        off = len(data)
        px = [mask.getpixel((x, y)) for y in range(h) for x in range(w)]
        nib = [(p * 15 + 127) // 255 for p in px]
        if len(nib) % 2:
            nib.append(0)
        for i in range(0, len(nib), 2):
            data.append(nib[i] | (nib[i + 1] << 4))
        glyphs[cp] = (off, w, h, box[0], box[1], adv)
    out = [f"static const unsigned char tf_{name}_data[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        out.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const tf_glyph tf_{name}_glyphs[256] = {{")
    for cp in range(256):
        g = glyphs.get(cp, (0, 0, 0, 0, 0, 0))
        out.append("    {%d,%d,%d,%d,%d,%d}," % g)
    out.append("};")
    out.append(f"static const tf_font tf_{name} = {{ tf_{name}_glyphs, tf_{name}_data, {ascent + descent} }};")
    return out, len(data)


lines = ["/* Generado por scripts/toast_font.py. No editar a mano.",
         " * Letras del aviso dentro del juego, dibujadas con Inter (SIL Open Font License). */",
         "#pragma once",
         "typedef struct { unsigned short off; unsigned char w, h; signed char x, y; unsigned char adv; } tf_glyph;",
         "typedef struct { const tf_glyph *glyphs; const unsigned char *data; int line_h; } tf_font;", ""]
total = 0
for f in FONTS:
    l, n = build(*f)
    lines += l + [""]
    total += n
OUT.write_text("\n".join(lines))
print(f"{OUT}: {total} bytes de letras")
