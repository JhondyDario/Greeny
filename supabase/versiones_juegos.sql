-- Ultima version para Switch 1 de cada juego del catalogo (noviembre de 2026).
-- Pone el numero en la ficha del juego y la linea "version =" de su archivo de logros.
-- Correr una vez en el SQL Editor de Supabase. Super Mario 3D World + Bowser's Fury ya tiene la 1.1.0, que es la ultima.
begin;
update public.games set version = '4.0.0',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 4.0.0', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 4.0.0', 'n') end
 where title_id = '0100152000022000';
update public.games set version = '1.4.1',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.4.1', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.4.1', 'n') end
 where title_id = '0100000000010000';
update public.games set version = '1.4.0',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.4.0', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.4.0', 'n') end
 where title_id = '0100DCA0064A6000';
update public.games set version = '1.1.0',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.1.0', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.1.0', 'n') end
 where title_id = '01004D300C5AE000';
update public.games set version = '13.0.5',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 13.0.5', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 13.0.5', 'n') end
 where title_id = '01006A800016E000';
update public.games set version = '1.4.2',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.4.2', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.4.2', 'n') end
 where title_id = '01009BF0072D4000';
update public.games set version = '1.2.0',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.2.0', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.2.0', 'n') end
 where title_id = '010015100B514000';
update public.games set version = '1.0.4',
       definition = case when definition ~ '(^|\n)version[ \t]*='
                         then regexp_replace(definition, '^version[ \t]*=[^\n]*', 'version = 1.0.4', 'n')
                         else regexp_replace(definition, '^(game[ \t]*=[^\n]*)', E'\\1\nversion = 1.0.4', 'n') end
 where title_id = '01009AA000FAA000';
commit;
select title_id, name, version from public.games order by name;
