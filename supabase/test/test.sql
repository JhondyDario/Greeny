-- Pruebas del esquema. Cada bloque falla con una excepcion si algo no se cumple.
\set ON_ERROR_STOP on

-- usuarios de prueba: el trigger debe crear los perfiles
insert into auth.users (id, email, raw_user_meta_data, raw_app_meta_data) values
 ('00000000-0000-0000-0000-00000000000a', 'ana@example.com',  '{"username":"Ana_99"}', '{"provider":"email"}'),
 ('00000000-0000-0000-0000-00000000000b', 'ana@otro.com',     '{"username":"ana_99"}', '{"provider":"email"}'),
 ('00000000-0000-0000-0000-00000000000c', 'x@example.com',    '{"user_name":"El Ñandú!!","full_name":"El Ñandú"}', '{"provider":"discord"}'),
 ('00000000-0000-0000-0000-00000000000d', 'jefe@example.com', '{}', '{"provider":"email"}');
insert into public.admins values ('00000000-0000-0000-0000-00000000000d');
insert into auth.users (id, email, raw_user_meta_data, raw_app_meta_data) values
 ('00000000-0000-0000-0000-00000000000e', 'falso@example.com', '{"username":"Admin"}', '{"provider":"email"}');
do $$
declare u text;
begin
    select username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000e';
    assert u = 'jugador', 'quien pide un nombre reservado recibe uno generico: ' || u;
    delete from auth.users where id = '00000000-0000-0000-0000-00000000000e';
end $$;

do $$
declare n int; u text;
begin
    select count(distinct friend_code) into n from public.profiles where friend_code ~ '^GR-[0-9]{6}$';
    assert n = 4, 'cada perfil nuevo recibe un codigo de amigo distinto: ' || n;
    select count(*) into n from public.profiles;
    assert n = 4, 'deben existir 4 perfiles, hay ' || n;
    select username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000a';
    assert u = 'Ana_99', 'username pedido: ' || u;
    select username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000b';
    assert u like 'ana_99\_%', 'el duplicado (sin importar mayusculas) recibe sufijo: ' || u;
    select username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000c';
    assert u = 'Eland', 'se limpian caracteres raros: ' || u;
    select discord_username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000c';
    assert u = 'El Ñandú!!', 'discord_username: ' || coalesce(u, 'null');
    select username into u from public.profiles where id = '00000000-0000-0000-0000-00000000000d';
    assert u = 'jefe', 'username desde el correo: ' || u;
end $$;

-- catalogo (como superusuario, igual que lo hara el panel de Supabase)
insert into public.games (title_id, name, version, definition, published) values
 ('010028600EBDA000', 'Super Mario 3D World + Bowser''s Fury', '1.1.0', 'game = x', true),
 ('0100AAAAAAAAA000', 'Oculto', '1.0.0', 'game = y', false);
insert into public.achievements (title_id, ach_id, name_es, points) values
 ('010028600EBDA000', 'monedas_5', 'Cinco cheles', 5),
 ('010028600EBDA000', 'monedas_50', 'Medio camino', 10),
 ('0100AAAAAAAAA000', 'secreto', 'Secreto', 10);

-- ===== visitante sin sesion =====
set role anon;
do $$
declare n int; r record; ok boolean;
begin
    select count(*) into n from public.games;
    assert n = 1, 'anon solo ve juegos publicados: ' || n;
    select count(*) into n from public.achievements;
    assert n = 2, 'anon solo ve logros de juegos publicados: ' || n;
    select count(*) into n from public.caption_presets;
    assert n = 12, 'frases sugeridas: ' || n;

    ok := false;
    begin insert into public.unlocks values ('00000000-0000-0000-0000-00000000000a', '010028600EBDA000', 'monedas_5', now());
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'anon no puede insertar logros directamente';

    ok := false;
    begin insert into public.games (title_id, name, definition) values ('0100BBBBBBBBB000', 'x', 'y');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'anon no puede crear juegos';

    ok := false;
    begin perform public.device_link_claim('ABCDEF');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'anon no puede reclamar codigos';

    ok := false;
    begin perform public.hash_token('x');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'las funciones internas no son publicas';
end $$;

-- la consola pide un codigo
create temp table t_link as select * from public.device_link_start('Switch de Ana');
grant all on t_link to authenticated, anon;
do $$
declare r record; w record; ok boolean;
begin
    select * into r from t_link;
    assert r.link_code ~ '^[A-HJ-NP-Z2-9]{6}$', 'formato del codigo: ' || r.link_code;
    assert char_length(r.device_token) = 64, 'largo de la llave';
    select * into w from public.device_whoami(r.device_token);
    assert w.linked = false and w.username is null, 'antes de vincular: linked = false';
    select count(*) into r from public.device_whoami('llave-inventada');
    assert r.count = 0, 'una llave inventada no devuelve nada';

    ok := false;
    begin perform public.device_push_unlocks((select device_token from t_link), '[]'::jsonb);
    exception when others then ok := sqlerrm = 'consola no vinculada'; end;
    assert ok, 'sin vincular no se pueden subir logros';
end $$;
reset role;

-- ===== Ana con sesion =====
set role authenticated;
set request.jwt.claim.sub = '00000000-0000-0000-0000-00000000000a';
do $$
declare ok boolean; name text; n int;
begin
    ok := false;
    begin perform public.device_link_claim('ZZZZZZ');
    exception when others then ok := sqlerrm = 'codigo incorrecto o vencido'; end;
    assert ok, 'codigo incorrecto';

    name := public.device_link_claim(lower((select link_code from t_link)) || '  ');
    assert name = 'Switch de Ana', 'vincular (sin importar mayusculas ni espacios): ' || name;

    ok := false;
    begin perform public.device_link_claim((select link_code from t_link));
    exception when others then ok := sqlerrm = 'codigo incorrecto o vencido'; end;
    assert ok, 'un codigo no se puede usar dos veces';

    select count(*) into n from public.devices;
    assert n = 1, 'Ana ve su consola: ' || n;

    ok := false;
    begin perform token_hash from public.devices;
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'el hash de la llave no se puede leer';

    -- perfil: puede cambiar lo suyo, no lo ajeno ni columnas protegidas
    update public.profiles set caption = 'Cazatrofeos', display_name = 'Ana' where id = auth.uid();
    update public.profiles set caption = 'hackeado' where id = '00000000-0000-0000-0000-00000000000d';
    get diagnostics n = row_count;
    assert n = 0, 'no puede editar el perfil de otro';

    ok := false;
    begin update public.profiles set created_at = now() where id = auth.uid();
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'no puede cambiar created_at';

    ok := false;
    begin update public.profiles set caption = repeat('x', 61) where id = auth.uid();
    exception when check_violation then ok := true; end;
    assert ok, 'caption de mas de 60 caracteres se rechaza';

    ok := false;
    begin update public.profiles set username = 'Greeny' where id = auth.uid();
    exception when check_violation then ok := true; end;
    assert ok, 'no puede tomar un nombre reservado';

    ok := false;
    begin update public.profiles set username = 'JEFE' where id = auth.uid();
    exception when unique_violation then ok := true; end;
    assert ok, 'no puede tomar un username que ya existe (sin importar mayusculas)';

    ok := false;
    begin insert into public.unlocks values (auth.uid(), '010028600EBDA000', 'monedas_50', now());
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'ni con sesion se insertan logros a mano';

    ok := false;
    begin insert into public.games (title_id, name, definition) values ('0100BBBBBBBBB000', 'x', 'y');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'quien no es admin no crea juegos';

    ok := false;
    begin insert into public.caption_presets (id, text_es, text_en) values (99, 'x', 'x');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'quien no es admin no toca las frases sugeridas';

    ok := false;
    begin insert into storage.objects (bucket_id, name) values ('icons', 'x.png');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'quien no es admin no sube imagenes';

    assert not public.is_admin(), 'Ana no es admin';

    -- foto de perfil: ya no se sube ninguna, ni en la carpeta propia
    ok := false;
    begin insert into storage.objects (bucket_id, name) values ('avatars', auth.uid()::text || '/foto.jpg');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'ya no se suben fotos propias';
    update public.profiles set avatar = 7 where id = auth.uid();
    ok := false;
    begin update public.profiles set avatar_url = 'https://example.com/x.png' where id = auth.uid();
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'no se puede poner un enlace como foto';
end $$;
reset role;
reset request.jwt.claim.sub;

-- ===== la consola sube logros =====
set role anon;
do $$
declare tok text := (select device_token from t_link); n int; w record;
begin
    select * into w from public.device_whoami(tok);
    assert w.linked and w.username = 'Ana_99' and w.caption = 'Cazatrofeos', 'whoami tras vincular';

    n := public.device_push_unlocks(tok, '[
        {"title_id":"010028600EBDA000","ach_id":"monedas_5","unlocked_at":1794264260},
        {"title_id":"010028600EBDA000","ach_id":"monedas_5","unlocked_at":1794264999},
        {"title_id":"010028600EBDA000","ach_id":"monedas_50","unlocked_at":0},
        {"title_id":"010028600EBDA000","ach_id":"no_existe","unlocked_at":5},
        {"title_id":"FFFF","ach_id":"x"},
        {"basura":true}
    ]'::jsonb);
    assert n = 2, 'solo cuentan los logros que existen, sin duplicados: ' || n;

    n := public.device_push_unlocks(tok, '[{"title_id":"010028600EBDA000","ach_id":"monedas_5","unlocked_at":1}]'::jsonb);
    assert n = 0, 'volver a subir lo mismo no duplica: ' || n;

    n := public.device_push_unlocks(tok, '[{"title_id":"010028600EBDA000","ach_id":"monedas_50","unlocked_at":99999999999}]'::jsonb);
    assert n = 0, 'ya estaba';
end $$;
reset role;

do $$
declare t timestamptz; n int; r record;
begin
    select unlocked_at into t from public.unlocks where ach_id = 'monedas_5';
    assert t = to_timestamp(1794264260), 'se conserva la primera fecha: ' || t;
    select unlocked_at into t from public.unlocks where ach_id = 'monedas_50';
    assert t <= now() and t > now() - interval '1 minute', 'fecha 0 = ahora';
    select count(*) into n from public.unlocks;
    assert n = 2, 'total de logros guardados: ' || n;

    select * into r from public.profile_stats where username = 'Ana_99';
    assert r.achievements = 2 and r.points = 15 and r.games = 1, 'estadisticas del perfil';
    assert r.platinums = 1, 'Mario tiene 2 logros en la prueba y Ana tiene los 2: un platino, hay ' || r.platinums;
    assert r.followers = 0 and r.following = 0, 'sin seguidores todavia';
    select * into r from public.profile_stats where username = 'jefe';
    assert r.achievements = 0 and r.points = 0 and r.games = 0 and r.platinums = 0, 'usuario sin logros: todo en cero';
    select * into r from public.achievement_stats where ach_id = 'monedas_5';
    assert r.unlocked_by = 1 and r.players = 1, 'rareza';
    select * into r from public.achievement_stats where ach_id = 'secreto';
    assert r.unlocked_by = 0 and r.players = 0, 'logro que nadie tiene';
end $$;

-- ===== seguir =====
set role authenticated;
set request.jwt.claim.sub = '00000000-0000-0000-0000-00000000000d';
do $$
declare ok boolean; n int;
begin
    insert into public.follows (follower_id, followed_id) values (auth.uid(), '00000000-0000-0000-0000-00000000000a');
    ok := false;
    begin insert into public.follows (follower_id, followed_id) values ('00000000-0000-0000-0000-00000000000a', auth.uid());
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'nadie puede hacer que otro lo siga';
    ok := false;
    begin insert into public.follows (follower_id, followed_id) values (auth.uid(), auth.uid());
    exception when check_violation then ok := true; end;
    assert ok, 'no se puede seguir a uno mismo';
    ok := false;
    begin insert into public.follows (follower_id, followed_id) values (auth.uid(), '00000000-0000-0000-0000-00000000000a');
    exception when unique_violation then ok := true; end;
    assert ok, 'no se sigue dos veces';
    select followers into n from public.profile_stats where username = 'Ana_99';
    assert n = 1, 'Ana tiene un seguidor: ' || n;

    -- codigo de amigo
    declare code text; who text;
    begin
        select friend_code into code from public.profiles where username = 'Eland';
        assert code ~ '^GR-[0-9]{6}$', 'formato del codigo: ' || coalesce(code, 'null');
        who := public.follow_by_code(lower(replace(code, '-', ' ')));   -- con espacios y en minusculas tambien vale
        assert public.follow_by_code(substr(code, 4)) = 'Eland', 'solo los seis numeros tambien vale';
        assert who = 'Eland', 'seguir por codigo: ' || who;
        who := public.follow_by_code(code);                              -- repetir no falla ni duplica
        select count(*) into n from public.follows where follower_id = auth.uid();
        assert n = 2, 'jefe sigue a dos: ' || n;

        ok := false;
        begin perform public.follow_by_code(case when code = 'GR-000000' then 'GR-000001' else 'GR-000000' end);
        exception when others then ok := sqlerrm = 'no hay nadie con ese codigo de amigo'; end;
        assert ok, 'codigo que no existe';
        ok := false;
        begin perform public.follow_by_code('123');
        exception when others then ok := sqlerrm = 'el codigo de amigo tiene 6 numeros'; end;
        assert ok, 'codigo corto';
        ok := false;
        begin perform public.follow_by_code((select friend_code from public.profiles where id = auth.uid()));
        exception when others then ok := sqlerrm = 'ese es tu propio codigo'; end;
        assert ok, 'no te sigues a ti mismo';
        ok := false;
        begin perform public.follow_code('00000000-0000-0000-0000-00000000000a', code);
        exception when insufficient_privilege then ok := true; end;
        assert ok, 'la funcion interna no se puede llamar directo (permitiria seguir en nombre de otro)';
        ok := false;
        begin update public.profiles set friend_code = 'GR-111111' where id = auth.uid();
        exception when insufficient_privilege then ok := true; end;
        assert ok, 'nadie se cambia el codigo a mano';
    end;
    select following into n from public.profile_stats where username = 'jefe';
    assert n = 2, 'jefe sigue a dos (Ana y Eland): ' || n;
end $$;
reset role;
reset request.jwt.claim.sub;

set role anon;
do $$
declare j jsonb;
begin
    assert public.device_follow_by_code((select device_token from t_link), (select friend_code from public.profiles where username = 'jefe')) = 'jefe',
           'la consola de Ana sigue a jefe por codigo';
    j := public.device_profile((select device_token from t_link));
    assert j ->> 'friend_code' ~ '^GR-', 'device_profile trae el codigo de amigo';
    assert (j ->> 'following')::int = 1, 'Ana ahora sigue a uno';
    assert j ->> 'username' = 'Ana_99' and (j ->> 'followers')::int = 1 and (j ->> 'platinums')::int = 1
       and (j ->> 'points')::int = 15 and j ->> 'caption' = 'Cazatrofeos', 'device_profile: ' || j::text;
    assert public.device_profile('llave-inventada') is null, 'llave inventada: null';
end $$;
reset role;

-- ===== administrador =====
set role authenticated;
set request.jwt.claim.sub = '00000000-0000-0000-0000-00000000000d';
do $$
declare n int; t1 timestamptz; t2 timestamptz; tok text;
begin
    assert public.is_admin(), 'jefe es admin';
    select count(*) into n from public.games;
    assert n = 2, 'el admin ve tambien los no publicados: ' || n;
    insert into public.games (title_id, name, definition) values ('0100BBBBBBBBB000', 'Nuevo', 'game = z');
    declare st text; ok boolean := false;
    begin
        select status into st from public.games where title_id = '0100BBBBBBBBB000';
        assert st = 'jugable', 'estado por defecto: ' || st;
        update public.games set status = 'perfecto', name = 'Nuevo nombre', version = '2.0.0' where title_id = '0100BBBBBBBBB000';
        begin update public.games set status = 'inventado' where title_id = '0100BBBBBBBBB000';
        exception when check_violation then ok := true; end;
        assert ok, 'un estado que no existe se rechaza';
        -- frases sugeridas: el admin puede añadir, cambiar y quitar
        insert into public.caption_presets (id, text_es, text_en, sort) values (50, 'Nueva frase', 'New phrase', 500);
        update public.caption_presets set text_es = 'Frase cambiada' where id = 50;
        delete from public.caption_presets where id = 50;
    end;
    insert into public.achievements (title_id, ach_id, name_es) values ('0100BBBBBBBBB000', 'a', 'A');
    insert into storage.objects (bucket_id, name) values ('icons', '0100BBBBBBBBB000/a.png');
    select updated_at into t1 from public.games where title_id = '0100BBBBBBBBB000';
    perform pg_sleep(0.01);
    update public.games set published = true where title_id = '0100BBBBBBBBB000';
    select updated_at into t2 from public.games where title_id = '0100BBBBBBBBB000';
    assert t2 >= t1, 'updated_at se mueve';

    -- registrar una consola directamente (inicio de sesion con correo en la consola)
    tok := public.device_register('  ');
    assert char_length(tok) = 64, 'llave de device_register';
    select count(*) into n from public.devices where name = 'Switch';
    assert n = 1, 'nombre por defecto y solo ve la suya: ' || n;
    delete from public.devices;
    get diagnostics n = row_count;
    assert n = 1, 'solo puede desvincular la suya: ' || n;
end $$;
reset role;
reset request.jwt.claim.sub;

do $$
declare n int;
begin
    select count(*) into n from public.devices;
    assert n = 1, 'la consola de Ana sigue ahi: ' || n;
    -- borrar un usuario se lleva su perfil, logros y consolas
    delete from auth.users where id = '00000000-0000-0000-0000-00000000000a';
    select count(*) into n from public.unlocks;
    assert n = 0, 'logros borrados en cascada';
    select count(*) into n from public.devices;
    assert n = 0, 'consolas borradas en cascada';
end $$;

-- bronce, plata y oro
do $$
begin
    assert (select tier from public.achievements where ach_id = 'monedas_5') = 'bronce', 'tier por defecto';
    begin
        update public.achievements set tier = 'diamante' where ach_id = 'monedas_5';
        assert false, 'acepto un tier que no existe';
    exception when check_violation then null;
    end;
    update public.achievements set tier = 'oro' where ach_id = 'monedas_5';
end $$;

-- foto de perfil: un numero de la lista, al azar al crear la cuenta
do $$
declare lo int; hi int; total int; who uuid;
begin
    select min(avatar), max(avatar), count(*) into lo, hi, total from public.profiles;
    assert total > 0 and lo >= 1 and hi <= 36, 'fotos al crear la cuenta: ' || coalesce(lo, -1) || '..' || coalesce(hi, -1);
    select id into who from public.profiles limit 1;
    begin
        update public.profiles set avatar = 99 where id = who;
        assert false, 'acepto una foto fuera de rango';
    exception when check_violation then null;
    end;
end $$;

-- ranking
do $$
declare j jsonb; who uuid; n int;
begin
    select count(*) into n from public.profile_stats where points > 0;
    j := public.ranking_for(null, 'global', null, null);
    assert (j ->> 'total')::int = n, 'ranking global: ' || j::text;
    assert jsonb_array_length(j -> 'top') = least(n, 10), 'top 10';
    if n > 0 then
        assert (j -> 'top' -> 0 ->> 'position')::int = 1 and j -> 'top' -> 0 ? 'username' and not (j -> 'top' -> 0 ? 'id'), 'fila del ranking: ' || j::text;
        select id into who from public.profile_stats order by points desc limit 1;
        j := public.ranking_for(who, 'global', null, null);
        assert (j -> 'me' ->> 'position')::int = 1, 'mi posicion';
        update public.profiles set country = 'DO' where id = who;
        j := public.ranking_for(who, 'country', 'do', null);
        assert (j ->> 'total')::int = 1, 'ranking por pais: ' || j::text;
        j := public.ranking_for(who, 'friends', null, null);
        assert (j ->> 'total')::int >= 1, 'ranking de amigos me incluye';
        j := public.ranking_for(who, 'global', null, '010028600EBDA000');
        assert j ? 'top', 'ranking por juego';
    end if;
    begin
        update public.profiles set country = 'dominicana' where id = (select id from public.profiles limit 1);
        assert false, 'acepto un pais invalido';
    exception when check_violation then null;
    end;
end $$;

-- donador: solo lo pone un administrador, y nadie a si mismo
do $$
declare who uuid; ok boolean := false;
begin
    select id into who from public.profiles limit 1;
    assert not (select donor from public.profiles where id = who), 'nadie nace donador';
    perform set_config('request.jwt.claim.sub', who::text, true);
    perform set_config('role', 'authenticated', true);
    begin update public.profiles set donor = true where id = who;
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'un usuario no puede marcarse como donador';
    reset role;
end $$;

-- postulaciones: cada quien la suya, una por rol; no puede aceptarse a si mismo
do $$
declare who uuid; ok boolean := false; n int;
begin
    select id into who from public.profiles where id <> '00000000-0000-0000-0000-00000000000d' limit 1;
    perform set_config('request.jwt.claim.sub', who::text, true);
    perform set_config('role', 'authenticated', true);
    insert into public.applications (user_id, role, contact, message) values (who, 'beta_tester', 'ana#1234', 'tengo una switch');
    begin insert into public.applications (user_id, role, contact) values (who, 'beta_tester', 'otra vez');
    exception when unique_violation then ok := true; end;
    assert ok, 'una sola postulacion por rol';
    ok := false;
    begin insert into public.applications (user_id, role, contact) values ('00000000-0000-0000-0000-00000000000d', 'arte', 'suplantar');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'no se postula en nombre de otro';
    update public.applications set status = 'aceptada' where user_id = who;
    get diagnostics n = row_count;
    assert n = 0, 'no puede aceptarse a si mismo';
    reset role;
    perform set_config('request.jwt.claim.sub', '00000000-0000-0000-0000-00000000000d', true);
    perform set_config('role', 'authenticated', true);
    update public.applications set status = 'aceptada' where user_id = who;
    get diagnostics n = row_count;
    assert n = 1, 'el administrador la acepta';
    reset role;
end $$;

-- donador por correo: solo un administrador, y solo devuelve el usuario
do $$
declare who text; expect text; ok boolean := false;
begin
    select username into expect from public.profiles where id = '00000000-0000-0000-0000-00000000000b';
    perform set_config('request.jwt.claim.sub', '00000000-0000-0000-0000-00000000000b', true);
    begin perform public.set_donor_email('ana@otro.com', true);
    exception when raise_exception then ok := true; end;
    assert ok, 'quien no es admin no da medallas';
    perform set_config('request.jwt.claim.sub', '00000000-0000-0000-0000-00000000000d', true);
    who := public.set_donor_email('ANA@Otro.com', true);
    assert who = expect, 'busca el correo sin importar mayusculas: ' || coalesce(who, 'null');
    assert (select donor from public.profiles where username = expect), 'quedo como donador';
    perform public.set_donor(expect, false);
    assert not (select donor from public.profiles where username = expect), 'se le puede quitar';
    ok := false;
    begin perform public.set_donor_email('nadie@example.com', true);
    exception when raise_exception then ok := true; end;
    assert ok, 'un correo sin cuenta da error';
end $$;

-- comentarios y me gusta: los me gusta dan puntos al autor, con tope
do $$
declare a uuid := '00000000-0000-0000-0000-00000000000b'; b uuid := '00000000-0000-0000-0000-00000000000d';
        rid bigint; ok boolean := false; before int; after int; j jsonb;
begin
    select points into before from public.profile_stats where id = a;
    perform set_config('request.jwt.claim.sub', a::text, true);
    set local role authenticated;
    insert into public.reviews (title_id, user_id, body) values ('010028600EBDA000', a, 'Muy buen juego') returning id into rid;
    begin insert into public.review_likes (review_id, user_id) values (rid, a);
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'no te das me gusta a ti mismo';
    ok := false;
    begin insert into public.reviews (title_id, user_id, body) values ('010028600EBDA000', b, 'suplantar');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'no comentas en nombre de otro';
    reset role;
    perform set_config('request.jwt.claim.sub', b::text, true);
    set local role authenticated;
    insert into public.review_likes (review_id, user_id) values (rid, b);
    reset role;
    select points into after from public.profile_stats where id = a;
    assert after = before + 2, 'un me gusta da 2 puntos: ' || before || ' -> ' || after;
    j := public.reviews_for(b, '010028600EBDA000');
    assert jsonb_array_length(j) = 1 and (j -> 0 ->> 'likes')::int = 1 and (j -> 0 ->> 'liked')::boolean
           and not (j -> 0 ->> 'mine')::boolean and not (j -> 0 ? 'user_id'), 'lista de comentarios: ' || j::text;
end $$;

-- entrar desde la consola con contraseña, con freno de intentos
do $$
declare uid uuid := '00000000-0000-0000-0000-00000000000b'; r jsonb;
begin
    update auth.users set email = 'Prueba@Greeny.test', encrypted_password = extensions.crypt('secreto123', extensions.gen_salt('bf')),
           email_confirmed_at = now() where id = uid;
    set local role anon;
    r := public.device_login_password(' prueba@greeny.test', 'secreto123', 'Switch de prueba');
    reset role;
    assert length(r ->> 'token') >= 32, 'debe devolver una llave: ' || r::text;
    assert (select user_id from public.devices where token_hash = public.hash_token(r ->> 'token')) = uid, 'la consola queda a nombre del usuario';
    for i in 1..5 loop
        r := public.device_login_password('prueba@greeny.test', 'mala', 'x');
        assert r ->> 'error' like 'Correo o contrase%' and not r ? 'token', 'contraseña mala rechazada';
    end loop;
    r := public.device_login_password('prueba@greeny.test', 'secreto123', 'x');
    assert r ->> 'error' like 'Demasiados intentos%', 'tras 5 fallos frena aunque la contraseña sea buena: ' || r::text;
    r := public.device_login_password('nadie@greeny.test', 'x', 'x');
    assert r ->> 'error' like 'Correo o contrase%', 'correo que no existe: mismo mensaje';
end $$;
do $$
declare ok boolean := false;
begin
    set local role anon;
    begin perform count(*) from public.login_attempts; exception when insufficient_privilege then ok := true; end;
    reset role;
    assert ok, 'nadie lee los intentos';
end $$;

-- notificaciones: me gusta, seguir, logro reciente y medalla; leer y marcar
do $$
declare a uuid := '00000000-0000-0000-0000-00000000000b'; b uuid := '00000000-0000-0000-0000-00000000000d';
        j jsonb; n0 int; tok text; ok boolean := false;
begin
    delete from public.notifications;
    -- seguir avisa una sola vez
    delete from public.follows where follower_id = b and followed_id = a;
    insert into public.follows (follower_id, followed_id) values (b, a);
    delete from public.follows where follower_id = b and followed_id = a;
    insert into public.follows (follower_id, followed_id) values (b, a);
    assert (select count(*) from public.notifications where user_id = a and kind = 'follow') = 1, 'seguir avisa una vez';
    -- un logro reciente avisa; uno viejo no
    delete from public.unlocks where user_id = b;
    insert into public.unlocks (user_id, title_id, ach_id, unlocked_at)
        select b, title_id, ach_id, now() from public.achievements where title_id = '010028600EBDA000' limit 1;
    insert into public.unlocks (user_id, title_id, ach_id, unlocked_at)
        select b, title_id, ach_id, now() - interval '30 days' from public.achievements where title_id = '010028600EBDA000' offset 1 limit 1;
    assert (select count(*) from public.notifications where user_id = b and kind = 'achievement') = 1, 'solo el logro reciente avisa';
    assert exists (select 1 from public.notifications where user_id = b and kind = 'badge' and badge = 'primer_logro'), 'medalla primer logro';
    n0 := (select count(*) from public.notifications where user_id = b and kind = 'badge');
    perform public.check_badges(b);
    assert (select count(*) from public.notifications where user_id = b and kind = 'badge') = n0, 'la medalla no se repite';
    -- la lista y marcar como leido desde la consola
    tok := 'llave-de-prueba-notificaciones-0000000000000';
    insert into public.devices (token_hash, user_id, name) values (public.hash_token(tok), a, 'S');
    j := public.device_notifications(tok);
    assert (j ->> 'unread')::int >= 1 and j -> 'items' -> 0 ->> 'actor' is not null, 'lista: ' || j::text;
    perform public.device_notifications_read(tok, ((j -> 'items' -> 0 ->> 'id')::bigint));
    assert (public.device_notifications(tok) ->> 'unread')::int = (j ->> 'unread')::int - 1, 'marcar una';
    perform public.device_notifications_read(tok, null);
    assert (public.device_notifications(tok) ->> 'unread')::int = 0, 'marcar todas';
    -- nadie lee las de otro
    perform set_config('request.jwt.claim.sub', b::text, true);
    set local role authenticated;
    assert (select count(*) from public.notifications where user_id = a) = 0, 'no se ven las de otro';
    reset role;
end $$;

do $$
declare j jsonb; z uuid := '00000000-0000-0000-0000-0000000000ee';
begin
    j := public.activity_now();
    assert j ? 'online' and j ? 'today', 'actividad';
    insert into auth.users (id, email, raw_user_meta_data) values (z, 'borrar@greeny.test', '{"username":"borrame"}');
    assert exists (select 1 from public.profiles where id = z), 'perfil creado';
    perform set_config('request.jwt.claim.sub', z::text, true);
    set local role authenticated;
    perform public.delete_my_account();
    reset role;
    assert not exists (select 1 from public.profiles where id = z) and not exists (select 1 from auth.users where id = z), 'cuenta borrada';
end $$;

-- orden de los comentarios: el tuyo, luego tus amigos, luego el resto
do $$
declare me uuid := '00000000-0000-0000-0000-0000000000c1'; amigo uuid := '00000000-0000-0000-0000-0000000000c2';
        otro uuid := '00000000-0000-0000-0000-0000000000c3'; fan uuid := '00000000-0000-0000-0000-0000000000c4';
        j jsonb; r bigint;
begin
    insert into auth.users (id, email, raw_user_meta_data) values
        (me, 'c1@g.test', '{"username":"orden_yo"}'), (amigo, 'c2@g.test', '{"username":"orden_amigo"}'),
        (otro, 'c3@g.test', '{"username":"orden_otro"}'), (fan, 'c4@g.test', '{"username":"orden_fan"}');
    insert into public.games (title_id, name, definition, published) values ('0100ABCDEF000000', 'Orden', 'game = Orden', true) on conflict do nothing;
    insert into public.reviews (title_id, user_id, body) values ('0100ABCDEF000000', otro, 'muy querido') returning id into r;
    insert into public.review_likes (review_id, user_id) values (r, fan), (r, amigo);
    insert into public.reviews (title_id, user_id, body) values ('0100ABCDEF000000', amigo, 'de mi amigo');
    insert into public.reviews (title_id, user_id, body) values ('0100ABCDEF000000', me, 'el mio');
    insert into public.follows (follower_id, followed_id) values (me, amigo);
    j := public.reviews_for(me, '0100ABCDEF000000');
    assert j -> 0 ->> 'username' = 'orden_yo' and j -> 1 ->> 'username' = 'orden_amigo' and j -> 2 ->> 'username' = 'orden_otro', 'orden: ' || j::text;
    assert (j -> 1 ->> 'friend')::boolean and not (j -> 2 ->> 'friend')::boolean, 'marca de amigo';
    j := public.reviews_for(null, '0100ABCDEF000000');
    assert j -> 0 ->> 'username' = 'orden_otro', 'sin sesion: el mas querido primero';
end $$;

-- en un juego sin empezar no se puede comentar
do $$
declare yo uuid := '00000000-0000-0000-0000-0000000000c1'; ok boolean := false; tok text := 'llave-comentarios-sin-empezar-000000000000';
begin
    insert into public.games (title_id, name, definition, published, status) values ('0100ABCDEF100000', 'Nuevo', 'game = Nuevo', true, 'sin_empezar');
    perform set_config('request.jwt.claim.sub', yo::text, true);
    set local role authenticated;
    begin insert into public.reviews (title_id, user_id, body) values ('0100ABCDEF100000', yo, 'hola');
    exception when insufficient_privilege then ok := true; end;
    reset role;
    assert ok, 'web: no se comenta un juego sin empezar';
    insert into public.devices (token_hash, user_id, name) values (public.hash_token(tok), yo, 'S');
    ok := false;
    begin perform public.device_review_save(tok, '0100ABCDEF100000', 'hola');
    exception when others then ok := sqlerrm like '%no se puede comentar%'; end;
    assert ok, 'consola: no se comenta un juego sin empezar';
    update public.games set status = 'prueba' where title_id = '0100ABCDEF100000';
    assert public.device_review_save(tok, '0100ABCDEF100000', 'ya tiene logros'), 'en prueba si se comenta';
end $$;

select 'TODAS LAS PRUEBAS PASARON' as resultado;
