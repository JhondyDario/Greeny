#!/usr/bin/env python3
"""Genera supabase/juegos_iniciales.sql: mete en el catalogo los juegos de juegos/*.txt que
todavia no esten, con el estado "sin empezar". Se pega una vez en el editor SQL de Supabase.

No toca un juego que ya exista (por ejemplo Mario 3D World, que se publica desde la web).
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = {"0100000000000000", "010028600EBDA000"}  # la plantilla, y el que ya esta publicado


def parse(text):
    game = {"name": None, "version": None, "build": None, "achs": []}
    cur = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            cur = {"id": line[1:-1].strip(), "name": None, "desc": "", "name_en": None, "desc_en": None, "points": 10}
            game["achs"].append(cur)
            continue
        key, _, val = line.partition("=")
        key, val = key.strip(), val.strip()
        if cur is None:
            if key == "game":
                game["name"] = val
            elif key in ("version", "build"):
                game[key] = val
        elif key == "name":
            cur["name"] = val
        elif key == "desc":
            cur["desc"] = val
        elif key == "name.en":
            cur["name_en"] = val
        elif key == "desc.en":
            cur["desc_en"] = val
        elif key == "points":
            cur["points"] = int(val)
    return game


def q(v):
    return "null" if v is None else "'" + v.replace("'", "''") + "'"


out = ["-- Generado por scripts/juegos_sql.py. Juegos nuevos del catalogo, con estado \"sin empezar\".",
       "-- Pegalo una vez en el editor SQL de Supabase. No cambia los juegos que ya existan.", ""]
for name in sorted(os.listdir(os.path.join(ROOT, "juegos"))):
    m = re.fullmatch(r"([0-9A-F]{16})\.txt", name)
    if not m or m.group(1) in SKIP:
        continue
    tid = m.group(1)
    text = open(os.path.join(ROOT, "juegos", name), encoding="utf-8").read()
    g = parse(text)
    out.append("insert into public.games (title_id, name, version, build_id, definition, published, status) values")
    out.append("  (%s, %s, %s, %s, $greeny$%s$greeny$, true, 'sin_empezar')" % (q(tid), q(g["name"] or tid), q(g["version"]), q(g["build"]), text))
    out.append("  on conflict (title_id) do nothing;")
    for i, a in enumerate(g["achs"]):
        tier = "bronce" if a["points"] <= 10 else "plata" if a["points"] <= 25 else "oro"
        out.append("insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values")
        out.append("  (%s, %s, %s, %s, %s, %s, %d, '%s', %d) on conflict (title_id, ach_id) do nothing;"
                   % (q(tid), q(a["id"]), q(a["name"] or a["id"]), q(a["desc"]), q(a["name_en"]), q(a["desc_en"]), a["points"], tier, i))
    out.append("")
open(os.path.join(ROOT, "supabase", "juegos_iniciales.sql"), "w", encoding="utf-8").write("\n".join(out))
print("supabase/juegos_iniciales.sql listo")
