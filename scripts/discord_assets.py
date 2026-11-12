#!/usr/bin/env python3
"""Imagenes para el servidor de Discord de Greeny, hechas con los dibujos del proyecto.

  docs/discord/icono_servidor.png      512x512
  docs/discord/emojis/*.png            128x128, para subir como emojis propios (50 gratis)
  docs/discord/cabeceras/*.png         1200x400, para poner arriba de bienvenida, reglas y anuncios

Uso: python3 scripts/discord_assets.py
"""
import os
from PIL import Image, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
A = os.path.join(ROOT, "assets")
OUT = os.path.join(ROOT, "docs", "discord")


def load(rel):
    im = Image.open(os.path.join(A, rel)).convert("RGBA")
    return im.crop(im.getbbox())


def fit(im, w, h):
    """Escala sin deformar y centra en un lienzo transparente de w x h."""
    s = min(w / im.width, h / im.height)
    im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
    out = Image.new("RGBA", (w, h))
    out.alpha_composite(im, ((w - im.width) // 2, (h - im.height) // 2))
    return out


def emoji(rel, name):
    # un poco de margen: Discord los muestra pequeños y los bordes pegados se ven cortados
    fit(load(rel), 120, 120).crop((-4, -4, 124, 124)).save(os.path.join(OUT, "emojis", name + ".png"), optimize=True)


def header(name, pose, logo_side="left"):
    w, h = 1200, 400
    bg = Image.open(os.path.join(A, "fondo_oscuro.png")).convert("RGBA")
    s = max(w / bg.width, h / bg.height)
    bg = bg.resize((round(bg.width * s), round(bg.height * s)), Image.LANCZOS)
    bg = bg.crop(((bg.width - w) // 2, (bg.height - h) // 2, (bg.width - w) // 2 + w, (bg.height - h) // 2 + h))
    shade = Image.new("RGBA", (w, h), (4, 18, 10, 90))
    bg.alpha_composite(shade)
    logo = fit(load("logo-greeny-detalles.png"), 560, 282)
    m = fit(load("mascota/" + pose + ".png"), 330, 330)
    glow = Image.new("RGBA", (w, h))
    lx, mx = (90, 790) if logo_side == "left" else (610, 80)
    glow.alpha_composite(m, (mx, 40))
    glow = glow.filter(ImageFilter.GaussianBlur(28))
    bg.alpha_composite(glow)
    bg.alpha_composite(logo, (lx, (h - logo.height) // 2))
    bg.alpha_composite(m, (mx, 40))
    bg.convert("RGB").save(os.path.join(OUT, "cabeceras", name + ".png"), optimize=True)


def main():
    os.makedirs(os.path.join(OUT, "emojis"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "cabeceras"), exist_ok=True)
    fit(load("logo-g.png"), 460, 460).crop((-26, -26, 486, 486)).save(os.path.join(OUT, "icono_servidor.png"), optimize=True)
    for pose, name in [("saludando", "greeny_hola"), ("celebrando", "greeny_fiesta"), ("buscando", "greeny_busca"),
                       ("triste", "greeny_triste"), ("error", "greeny_error"), ("durmiendo", "greeny_zzz"),
                       ("sorprendido", "greeny_wow"), ("diagnostico", "greeny_revisa")]:
        emoji("mascota/%s.png" % pose, name)
    for rel, name in [("medals/bronce.png", "bronce"), ("medals/plata.png", "plata"), ("medals/oro.png", "oro"),
                      ("badges/primer_platino.png", "platino"), ("badges/donador.png", "donador"),
                      ("icons/estrella.png", "punto"), ("trofeo.png", "trofeo"), ("logro_desbloqueado.png", "logro"),
                      ("logo-g.png", "greeny")]:
        emoji(rel, name)
    header("bienvenida", "saludando")
    header("reglas", "diagnostico", "right")
    header("anuncios", "celebrando")
    print("listo:", OUT)


if __name__ == "__main__":
    main()
