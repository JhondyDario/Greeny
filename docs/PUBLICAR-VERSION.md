# Publicar una versión nueva

La app de la consola se actualiza sola desde las **releases de GitHub**. Al abrirse
pregunta cuál es la última. Si es más nueva que la que tiene, la ofrece en pantalla y,
con A, baja `greeny.nro` y se reemplaza a sí misma.
Lo mismo se puede hacer a mano en *Opciones → Ayuda → Actualizar Greeny*.

## Pasos

1. Sube la versión en **dos sitios** (tienen que ser iguales):
   - `app/source/ui.h` → `#define UI_APP_VERSION "1.0.0"`
   - `app/Makefile` → `APP_VERSION := 1.0.0` (y `overlay/Makefile`, para que el overlay diga lo mismo)
2. Haz commit y súbelo a `main`.
3. Crea la etiqueta y súbela:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
4. GitHub Actions ("Publicar versión") compila y crea la release con `greeny.zip` y `greeny.nro`.
   Si la etiqueta no coincide con `UI_APP_VERSION`, se detiene sin publicar nada.
5. Edita la release en GitHub y escribe **qué trae** en la descripción. Ese texto es el que
   la consola muestra en "QUÉ TRAE". Escríbelo corto, con guiones:
   ```
   - Avisos dentro de la app (ZL o ZR)
   - Comentarios con me gusta
   ```

## Cosas que conviene saber

- La app solo acepta descargas de `https://github.com/`, comprueba que el archivo sea un NRO
  de verdad y que pese lo mismo que dice la release. Si algo falla, la app vieja se queda igual.
- La app nueva trae dentro el sysmodule, el overlay de Tesla y los sonidos. Al abrirla, los
  instala, y si el sysmodule cambió, pide reiniciar la consola.
- La versión que se compara es la de la etiqueta (`v1.0.0`). Las versiones de prueba se marcan
  como *pre-release* en GitHub: la API de "última versión" no las ofrece.
