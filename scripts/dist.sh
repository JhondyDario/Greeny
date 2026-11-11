#!/usr/bin/env bash
# Arma la estructura de carpetas lista para copiar a la raiz de la SD.
# Correr despues de scripts/build.sh.
set -euo pipefail
cd "$(dirname "$0")/.."

TID=4200000000010690
[ -f greeny.nsp ] || { echo "falta greeny.nsp, corre make primero" >&2; exit 1; }
[ -f app/greeny.nro ] || { echo "falta app/greeny.nro, corre make -C app primero" >&2; exit 1; }

rm -rf dist greeny.zip
mkdir -p "dist/atmosphere/contents/$TID/flags" dist/config/greeny/games dist/switch
cp app/greeny.nro dist/switch/greeny.nro
cp greeny.nsp "dist/atmosphere/contents/$TID/exefs.nsp"
cp toolbox.json "dist/atmosphere/contents/$TID/toolbox.json"
: > "dist/atmosphere/contents/$TID/flags/boot2.flag"
cp juegos/0100000000000000.txt dist/config/greeny/games/_PLANTILLA.txt
# sonidos del sysmodule: se leen de la SD
mkdir -p dist/config/greeny/sounds
cp sd/config/greeny/sounds/*.pcm dist/config/greeny/sounds/
mkdir -p dist/config/greeny/toast
cp sd/config/greeny/toast/*.bin dist/config/greeny/toast/
# Los archivos de logros de cada juego ya no van en el zip: la app los baja del catalogo.
# Un archivo copiado a mano en la SD no se actualiza nunca solo, y eso dejaba nombres y logros viejos.

(cd dist && zip -qr ../greeny.zip .)
echo "listo: dist/ y greeny.zip"
