/* La implementacion de stb_image, una sola vez para toda la app.
 * JPEG para las caratulas y los fondos; PNG para los iconos de logros. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "third_party/stb_image.h"
#pragma GCC diagnostic pop
