#!/usr/bin/env python3
"""Genera los recursos binarios a partir de assets/.

  app/data/mascot_big.bin    RGBA crudo 240x240: la cara de la mascota (introduccion y Acerca de)
  app/data/buttons.bin       8 iconos de botones RGBA 38x38 seguidos (pistas de abajo y pestañas)
  app/data/bg_*.bin          fondos claro y oscuro, guardados como JPEG (la app los decodifica al abrir)
  app/data/mascot_small.bin  RGBA crudo 66x66 (insignia de la cabecera: el logo G)
  app/icon.jpg               icono 256x256 para el menu de homebrew (de assets/app-icon.png)
  web/img/*.png              logo e iconos de la web
  app/data/sfx_*.bin         sonidos de la app: PCM 16 bit mono 48 kHz
  sd/config/greeny/sounds/   sonidos del sysmodule: PCM 16 bit mono 32 kHz (van a la SD, no dentro del sysmodule)

Los sonidos se recortan (silencio al inicio y al final) y se pasan a mono. Los
del sysmodule van a 32 kHz porque ahi la memoria es muy limitada.

Si cambias los tamaños, cambia tambien BIG_W/BIG_H/SMALL_W/SMALL_H en app/source/main.c.
"""
import os
import sys
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
# la cara de la mascota, para la introduccion y Acerca de
face = Image.open(os.path.join(ROOT, "assets", "cara.png")).convert("RGBA")
face = face.crop(face.getbbox())
with open(os.path.join(ROOT, "app", "data", "mascot_big.bin"), "wb") as f:
    f.write(fit(face, 240, 240).tobytes())

# iconos de botones para las pistas de abajo, todos seguidos en un archivo.
# El orden es el de UI_BTN_* en app/source/ui.h.
with open(os.path.join(ROOT, "app", "data", "buttons.bin"), "wb") as f:
    for name in ("boton_a", "boton_b", "boton_x", "boton_y", "boton_mas", "boton_menos", "boton_l", "boton_r"):
        im = Image.open(os.path.join(ROOT, "assets", "icons", name + ".png")).convert("RGBA")
        f.write(fit(im.crop(im.getbbox()), 38, 38).tobytes())

# insignia de la cabecera: el logo "G"
logo = Image.open(os.path.join(ROOT, "assets", "logo-g.png")).convert("RGBA")
logo = logo.crop(logo.getbbox())
with open(os.path.join(ROOT, "app", "data", "mascot_small.bin"), "wb") as f:
    f.write(fit(logo, 66, 66).tobytes())

# fondos: van como JPEG dentro de la app (crudos serian 3.7 MB cada uno)
for name in ("claro", "oscuro"):
    bg = Image.open(os.path.join(ROOT, "assets", "fondo_" + name + ".png")).convert("RGB").resize((1280, 720), Image.LANCZOS)
    bg.save(os.path.join(ROOT, "app", "data", "bg_" + name + ".bin"), format="JPEG", quality=88, subsampling=0,
            progressive=False, optimize=False)

# icono de la app. JPEG lo mas estandar posible: es el formato que espera el menu de la consola
icon = Image.open(os.path.join(ROOT, "assets", "app-icon.png")).convert("RGB").resize((256, 256), Image.LANCZOS)
icon.save(os.path.join(ROOT, "app", "icon.jpg"), quality=92, subsampling=2, progressive=False, optimize=False)

# imagenes de la web
web = os.path.join(ROOT, "web", "img")
fit(logo, 192, 192).save(os.path.join(web, "icon.png"), optimize=True)
fit(face, 420, 420).save(os.path.join(web, "mascot.png"), optimize=True)
for name in ("usuario", "salir", "menu"):
    im = Image.open(os.path.join(ROOT, "assets", "icons", name + ".png")).convert("RGBA")
    fit(im.crop(im.getbbox()), 56, 56).save(os.path.join(web, name + ".png"), optimize=True)

# ---------- imagenes de la app ----------
# Cada una va como PNG dentro de la app (app/data/img_<nombre>.bin) y se decodifica al abrir.
# La lista con sus tamaños se escribe en app/source/ui_images.h, que es lo que lee el codigo.
A = os.path.join(ROOT, "assets")
BADGES = ["primer_logro", "logros_10", "logros_50", "logros_200", "primer_platino", "platinos_5", "juegos_3", "juegos_10",
          "puntos_100", "puntos_500", "puntos_2000"]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paises import PAISES  # los paises que se pueden elegir, con su bandera en assets/flags/

AVATARS = sorted(f[:-4] for f in os.listdir(os.path.join(A, "avatars")) if f.endswith(".png"))
app_images = [  # (nombre, archivo en assets/, ancho, alto, espejo)
    ("medal_bronce", "medals/bronce.png", 64, 64, False), ("medal_plata", "medals/plata.png", 64, 64, False),
    ("medal_oro", "medals/oro.png", 64, 64, False),
    ("ach_fill", "relleno_logro.png", 120, 120, False),
    ("lock", "logro_bloqueado.png", 44, 44, False), ("check", "logro_desbloqueado.png", 44, 44, False),
    ("trophy", "trofeo.png", 72, 72, False), ("cover_fill", "relleno_caratula.png", 256, 256, False),
    ("stat_juegos", "stats/juegos.png", 56, 56, False), ("stat_logros", "stats/logros.png", 56, 56, False),
    ("stat_completados", "stats/completados.png", 56, 56, False), ("stat_puntos", "stats/puntos.png", 56, 56, False),
    ("stat_seguidores", "stats/seguidores.png", 56, 56, False), ("stat_siguiendo", "stats/siguiendo.png", 56, 56, False),
    ("st_sin_empezar", "estado/sin_empezar.png", 40, 40, False), ("st_prueba", "estado/prueba.png", 40, 40, False),
    ("st_jugable", "estado/jugable.png", 40, 40, False), ("st_perfecto", "estado/perfecto.png", 40, 40, False),
    ("star", "icons/estrella.png", 56, 56, False),  # puntos: perfil y logro al detalle
    ("arrow_r", "icons/flecha.png", 72, 72, False), ("arrow_l", "icons/flecha.png", 72, 72, True),
    ("toggle_on", "icons/toggle_on.png", 92, 44, False), ("toggle_off", "icons/toggle_off.png", 92, 44, False),
    ("logo", "logo-greeny-detalles.png", 300, 152, False),
] + [("m_" + m, "mascota/" + m + ".png", 260, 260, False)
     for m in ("buscando", "celebrando", "diagnostico", "durmiendo", "error", "saludando", "sorprendido", "triste")
] + [("badge_" + b, "badges/" + b + ".png", 96, 96, False) for b in BADGES + ["bloqueada"]
] + [("avatar_" + a, "avatars/" + a + ".png", 160, 160, False) for a in AVATARS
] + [("flag_" + code.lower(), "flags/" + code.lower() + ".png", 40, 40, False) for code, _, _ in PAISES]

for old in os.listdir(os.path.join(ROOT, "app", "data")):
    if old.startswith("img_"):
        os.remove(os.path.join(ROOT, "app", "data", old))
total = 0
for name, file, w, h, mirror in app_images:
    im = Image.open(os.path.join(A, file)).convert("RGBA")
    im = fit(im.crop(im.getbbox()), w, h)
    if mirror:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    out = os.path.join(ROOT, "app", "data", "img_" + name + ".bin")
    im.save(out, format="PNG", optimize=True)
    total += os.path.getsize(out)
with open(os.path.join(ROOT, "app", "source", "ui_images.h"), "w") as f:
    f.write("/* Generado por scripts/assets.py. No editar a mano.\n * UI_IMG(nombre, ancho, alto): imagenes PNG que la app lleva dentro. */\n")
    for name, _, w, h, _ in app_images:
        f.write("UI_IMG(%s, %d, %d)\n" % (name, w, h))
    f.write("#define UI_AVATAR_COUNT %d\n" % len(AVATARS))
    f.write("#define UI_COUNTRY_COUNT %d\n" % len(PAISES))
with open(os.path.join(ROOT, "app", "source", "ui_countries.h"), "w") as f:
    f.write("/* Generado por scripts/assets.py desde scripts/paises.py. No editar a mano.\n * UI_COUNTRY(codigo, nombre en español, nombre en ingles), en el mismo orden que las banderas. */\n")
    for code, es, en in PAISES:
        f.write('UI_COUNTRY("%s", "%s", "%s")\n' % (code, es, en))
print("imagenes de la app: %d, %d KB" % (len(app_images), total // 1024))

# ---------- imagenes de la web ----------
def web_copy(file, out, w, h):
    im = Image.open(os.path.join(A, file)).convert("RGBA")
    os.makedirs(os.path.dirname(os.path.join(web, out)), exist_ok=True)
    fit(im.crop(im.getbbox()), w, h).save(os.path.join(web, out), optimize=True)

for b in BADGES + ["bloqueada", "donador"]:  # "donador" solo se ve en la web por ahora
    web_copy("badges/%s.png" % b, "badges/%s.png" % b, 128, 128)
for a in AVATARS:
    web_copy("avatars/%s.png" % a, "avatars/%s.png" % a, 192, 192)
for t in ("bronce", "plata", "oro"):
    web_copy("medals/%s.png" % t, "medal_%s.png" % t, 64, 64)
for st in ("juegos", "logros", "completados", "puntos", "seguidores", "siguiendo"):
    web_copy("stats/%s.png" % st, "stat_%s.png" % st, 96, 96)
for st in ("sin_empezar", "prueba", "jugable", "perfecto"):
    web_copy("estado/%s.png" % st, "st_%s.png" % st, 48, 48)
for m in ("buscando", "celebrando", "durmiendo", "error", "saludando", "triste"):
    web_copy("mascota/%s.png" % m, "m_%s.png" % m, 320, 320)
for code, _, _ in PAISES:
    web_copy("flags/%s.png" % code.lower(), "flags/%s.png" % code.lower(), 48, 48)
with open(os.path.join(ROOT, "web", "js", "countries.js"), "w") as f:
    f.write("// Generado por scripts/assets.py desde scripts/paises.py. No editar a mano.\nwindow.GreenyCountries = [\n")
    for code, es, en in PAISES:
        f.write('  { code: "%s", es: "%s", en: "%s" },\n' % (code, es, en))
    f.write("];\n")
web_copy("relleno_caratula.png", "cover_fill.png", 256, 256)
web_copy("relleno_logro.png", "ach_fill.png", 128, 128)
web_copy("trofeo.png", "trophy.png", 96, 96)
web_copy("logo-greeny-detalles.png", "logo.png", 600, 304)
web_copy("logo-greeny.png", "logo_simple.png", 360, 164)
web_copy("app-icon.png", "favicon.png", 192, 192)

# ---------- imagenes del aviso dentro del juego ----------
# RGBA crudo de 60x60 (TOAST_BADGE en source/toast_draw.h). Van a la SD (no dentro del sysmodule, que debe pesar poco); la app los lleva y los instala.
TOAST = os.path.join(ROOT, "sd", "config", "greeny", "toast")
os.makedirs(TOAST, exist_ok=True)
for name, file in (("bronce", "medals/bronce.png"), ("plata", "medals/plata.png"), ("oro", "medals/oro.png"),
                   ("ok", "mascota/saludando.png"), ("aviso", "mascota/error.png")):
    im = Image.open(os.path.join(A, file)).convert("RGBA")
    with open(os.path.join(TOAST, name + ".bin"), "wb") as f:
        f.write(fit(im.crop(im.getbbox()), 60, 60).tobytes())

# ---------- sonidos ----------
import wave

import numpy as np


def load_mono(path):
    with wave.open(path) as w:
        assert w.getsampwidth() == 2, path + ": se espera WAV de 16 bit"
        rate, ch = w.getframerate(), w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
    x = x.reshape(-1, ch).mean(axis=1)
    loud = np.nonzero(np.abs(x) > 10 ** (-50 / 20))[0]  # recortar silencio por debajo de -50 dB
    if len(loud):
        x = x[max(0, loud[0] - rate // 200): min(len(x), loud[-1] + rate // 20)]
    return x, rate


def resample(x, src, dst):
    if src == dst:
        return x
    g = np.gcd(src, dst)
    up, down = dst // g, src // g
    y = np.zeros(len(x) * up)
    y[::up] = x * up
    taps = 32 * max(up, down) + 1  # pasa-bajos sinc con ventana, corte en la Nyquist mas baja
    n = np.arange(taps) - (taps - 1) / 2
    h = np.sinc(n / max(up, down)) / max(up, down) * np.hamming(taps)
    return np.convolve(y, h, mode="same")[::down]


def write_pcm(x, path):
    fade = min(len(x), 480)
    x = x.copy()
    x[-fade:] *= np.linspace(1, 0, fade)  # sin chasquido al final
    peak = np.max(np.abs(x)) or 1.0
    x = x / max(peak, 1.0)
    with open(path, "wb") as f:
        f.write((x * 32767).astype("<i2").tobytes())
    return len(x)


SND = os.path.join(ROOT, "assets", "sounds")
os.makedirs(os.path.join(ROOT, "sd", "config", "greeny", "sounds"), exist_ok=True)
jobs = [("cargado", "sd/config/greeny/sounds/cargado.pcm", 32000), ("logro", "sd/config/greeny/sounds/logro.pcm", 32000),
        ("navegar", "app/data/sfx_navegar.bin", 48000), ("regresar", "app/data/sfx_regresar.bin", 48000),
        ("aceptar_1", "app/data/sfx_aceptar_1.bin", 48000), ("aceptar_2", "app/data/sfx_aceptar_2.bin", 48000),
        ("abrir_menu", "app/data/sfx_abrir_menu.bin", 48000)]
for name, out, rate in jobs:
    x, src_rate = load_mono(os.path.join(SND, name + ".wav"))
    n = write_pcm(resample(x, src_rate, rate), os.path.join(ROOT, out))
    print("%-9s -> %-26s %5.2f s  %4d KB" % (name, out, n / rate, n * 2 // 1024))

print("recursos generados")
