#!/usr/bin/env bash
# Compila todo en el orden correcto. La app lleva dentro el sysmodule y sus
# sonidos, asi que primero se compila el sysmodule y luego la app.
# Hace falta devkitA64 (o correr esto dentro de la imagen devkitpro/devkita64).
set -euo pipefail
cd "$(dirname "$0")/.."

make
cp greeny.nsp app/data/sysmodule.bin
cp sd/config/greeny/sounds/cargado.pcm app/data/snd_cargado.bin
cp sd/config/greeny/sounds/logro.pcm app/data/snd_logro.bin
for t in bronce plata oro ok aviso; do cp sd/config/greeny/toast/$t.bin app/data/toast_$t.bin; done
make -C overlay
cp overlay/greeny.ovl app/data/overlay.bin
make -C app
echo "listo: greeny.nsp y app/greeny.nro"
