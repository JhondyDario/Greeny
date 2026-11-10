#!/usr/bin/env python3
"""Genera los recursos binarios a partir de assets/.

  app/data/mascot_small.bin  RGBA crudo 50x75 (cabecera de la app)
  app/icon.jpg               icono 256x256 para el menu de homebrew
  app/data/sfx_*.bin         sonidos de la app: PCM 16 bit mono 48 kHz
  data/snd_*.bin             sonidos del sysmodule: PCM 16 bit mono 32 kHz

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
for name, (w, h) in {"mascot_small": (50, 75)}.items():
    with open(os.path.join(ROOT, "app", "data", name + ".bin"), "wb") as f:
        f.write(fit(src, w, h).tobytes())

icon = Image.new("RGBA", (256, 256), (0xF3, 0xF7, 0xEC, 255))
m = fit(src, 220, 236)
icon.paste(m, (18, 10), m)
# JPEG lo mas estandar posible: es el formato que espera el menu de la consola
icon.convert("RGB").save(os.path.join(ROOT, "app", "icon.jpg"), quality=90, subsampling=2,
                         progressive=False, optimize=False)

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
os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
jobs = [("cargado", "data/snd_cargado.bin", 32000), ("logro", "data/snd_logro.bin", 32000),
        ("navegar", "app/data/sfx_navegar.bin", 48000), ("regresar", "app/data/sfx_regresar.bin", 48000),
        ("aceptar", "app/data/sfx_aceptar.bin", 48000)]
for name, out, rate in jobs:
    x, src_rate = load_mono(os.path.join(SND, name + ".wav"))
    n = write_pcm(resample(x, src_rate, rate), os.path.join(ROOT, out))
    print("%-9s -> %-26s %5.2f s  %4d KB" % (name, out, n / rate, n * 2 // 1024))

print("recursos generados")
