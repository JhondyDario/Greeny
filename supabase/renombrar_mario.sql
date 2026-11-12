-- Pone el nombre completo de Super Mario 3D World + Bowser's Fury en el catalogo.
-- Cambia el nombre y la linea "game =" de su archivo de logros. Correr una vez en el SQL Editor.
-- Al cambiar, el juego queda con fecha de actualizacion nueva y las consolas bajan el archivo otra vez.
update public.games
   set name = 'Super Mario 3D World + Bowser''s Fury',
       definition = regexp_replace(definition, '^game[ \t]*=[^\n]*', 'game    = Super Mario 3D World + Bowser''s Fury', 'n')
 where title_id = '010028600EBDA000';

select title_id, name, (regexp_match(definition, '^game[^\n]*', 'n'))[1] as linea_game from public.games where title_id = '010028600EBDA000';
