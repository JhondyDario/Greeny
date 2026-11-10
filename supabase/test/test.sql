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
    select count(distinct friend_code) into n from public.profiles where friend_code ~ '^GR-[0-9]{4}-[0-9]{4}-[0-9]{4}$';
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
 ('010028600EBDA000', 'Super Mario 3D World', '1.1.0', 'game = x', true),
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
    begin insert into storage.objects (bucket_id, name) values ('icons', 'x.png');
    exception when insufficient_privilege then ok := true; end;
    assert ok, 'quien no es admin no sube imagenes';

    assert not public.is_admin(), 'Ana no es admin';
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
        assert code ~ '^GR-[0-9]{4}-[0-9]{4}-[0-9]{4}$', 'formato del codigo: ' || coalesce(code, 'null');
        who := public.follow_by_code(lower(replace(code, '-', ' ')));   -- con espacios y en minusculas tambien vale
        assert who = 'Eland', 'seguir por codigo: ' || who;
        who := public.follow_by_code(code);                              -- repetir no falla ni duplica
        select count(*) into n from public.follows where follower_id = auth.uid();
        assert n = 2, 'jefe sigue a dos: ' || n;

        ok := false;
        begin perform public.follow_by_code('GR-0000-0000-0000');
        exception when others then ok := sqlerrm = 'no hay nadie con ese codigo de amigo'; end;
        assert ok, 'codigo que no existe';
        ok := false;
        begin perform public.follow_by_code('123');
        exception when others then ok := sqlerrm = 'el codigo de amigo tiene 12 numeros'; end;
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
        begin update public.profiles set friend_code = 'GR-1111-1111-1111' where id = auth.uid();
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

select 'TODAS LAS PRUEBAS PASARON' as resultado;
