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
