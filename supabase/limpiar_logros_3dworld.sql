-- Super Mario 3D World + Bowser's Fury: borra del catalogo los logros que ya no existen en el archivo
-- (se renombraron o se cambiaron por otros). Correr una vez en el SQL Editor de Supabase, DESPUES de
-- subir el archivo nuevo desde la web. Los desbloqueos de esos logros viejos se borran con ellos.
delete from public.achievements
 where title_id = '010028600EBDA000'
   and ach_id in ('monedas_5', 'monedas_50', 'monedas_90', 'bf_soles_50', 'bf_soles_100');

-- para comprobar: tiene que dar el mismo numero que logros tiene el archivo
select count(*) as logros from public.achievements where title_id = '010028600EBDA000';
