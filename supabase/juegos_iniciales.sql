-- Generado por scripts/juegos_sql.py. Juegos nuevos del catalogo, con estado "sin empezar".
-- Pegalo una vez en el editor SQL de Supabase. No cambia los juegos que ya existan.

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('0100000000010000', 'Super Mario Odyssey', '1.4.1', null, $greeny$# Super Mario Odyssey
# Va en  sdmc:/config/greeny/games/0100000000010000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Energilunas totales, monedas amarillas (0 a 9999) y monedas moradas por reino.

game = Super Mario Odyssey
version = 1.4.1

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('0100000000010000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('010015100B514000', 'Super Mario Bros. Wonder', '1.2.0', null, $greeny$# Super Mario Bros. Wonder
# Va en  sdmc:/config/greeny/games/010015100B514000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Semillas maravilla y monedas flor (0 a 999).

game = Super Mario Bros. Wonder
version = 1.2.0

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('010015100B514000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('0100152000022000', 'Mario Kart 8 Deluxe', '4.0.0', null, $greeny$# Mario Kart 8 Deluxe
# Va en  sdmc:/config/greeny/games/0100152000022000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Monedas totales acumuladas y monedas en carrera (0 a 10).

game = Mario Kart 8 Deluxe
version = 4.0.0

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('0100152000022000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('01004D300C5AE000', 'Kirby and the Forgotten Land', '1.1.0', null, $greeny$# Kirby and the Forgotten Land
# Va en  sdmc:/config/greeny/games/01004D300C5AE000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Waddle Dees rescatados (total del pueblo) y monedas estrella.

game = Kirby and the Forgotten Land
version = 1.1.0

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('01004D300C5AE000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('01006A800016E000', 'Super Smash Bros. Ultimate', '13.0.5', null, $greeny$# Super Smash Bros. Ultimate
# Va en  sdmc:/config/greeny/games/01006A800016E000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Oro acumulado, espiritus conseguidos o porcentaje de aventura.

game = Super Smash Bros. Ultimate
version = 13.0.5

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('01006A800016E000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('01009AA000FAA000', 'Sonic Mania', '1.0.4', null, $greeny$# Sonic Mania
# Va en  sdmc:/config/greeny/games/01009AA000FAA000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Anillos (0 a 999), vidas y Esmeraldas del Caos (0 a 7).

game = Sonic Mania
version = 1.0.4

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('01009AA000FAA000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('01009BF0072D4000', 'Captain Toad: Treasure Tracker', '1.4.2', null, $greeny$# Captain Toad: Treasure Tracker
# Va en  sdmc:/config/greeny/games/01009BF0072D4000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Superdiamantes totales y monedas acumuladas.

game = Captain Toad: Treasure Tracker
version = 1.4.2

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('01009BF0072D4000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;

insert into public.games (title_id, name, version, build_id, definition, published, status) values
  ('0100DCA0064A6000', 'Luigi''s Mansion 3', '1.4.0', null, $greeny$# Luigi's Mansion 3
# Va en  sdmc:/config/greeny/games/0100DCA0064A6000.txt
#
# SIN EMPEZAR. Todavia no se han buscado las direcciones de memoria de este juego, asi que
# solo trae un logro que no depende de ninguna: jugar un rato. Sin "build" a proposito: ese
# logro sirve con cualquier version. "version" solo dice la ultima para Switch 1, para la que
# se haran los logros de verdad.
#
# Ideas para los logros de verdad (hay que encontrarlas con el buscador de valores):
#   Dinero acumulado, joyas (6 por piso) y Boos atrapados.

game = Luigi's Mansion 3
version = 1.4.0

[primera_partida]
name = Primera partida
desc = Juega 2 minutos seguidos
name.en = First session
desc.en = Play for 2 minutes straight
points = 5
# main+0x4 nunca es cero en un juego: la condicion es "el juego esta abierto", durante 2 minutos
hold = 120000
when = main+0x4 u32 != 0
$greeny$, true, 'sin_empezar')
  on conflict (title_id) do nothing;
insert into public.achievements (title_id, ach_id, name_es, desc_es, name_en, desc_en, points, tier, sort) values
  ('0100DCA0064A6000', 'primera_partida', 'Primera partida', 'Juega 2 minutos seguidos', 'First session', 'Play for 2 minutes straight', 5, 'bronce', 0) on conflict (title_id, ach_id) do nothing;
