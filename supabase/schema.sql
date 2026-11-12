-- =============================================================================
-- Greeny - esquema de la base de datos (Supabase / Postgres)
--
-- Como usarlo: Supabase -> SQL Editor -> New query -> pegar todo -> Run.
-- Se puede correr mas de una vez sin romper nada: no borra datos.
--
-- Al final hay una seccion "DESPUES DE CORRERLO" con lo unico que hay que hacer
-- a mano (hacerte administrador).
-- =============================================================================


-- -----------------------------------------------------------------------------
-- 1. PERFILES
-- Un perfil por usuario. Se crea solo cuando alguien se registra.
-- -----------------------------------------------------------------------------

create table if not exists public.profiles (
    id               uuid primary key references auth.users (id) on delete cascade,
    username         text not null,
    display_name     text,
    caption          text,          -- frase bajo el nombre: una de caption_presets o escrita por el usuario
    avatar_url       text,
    discord_username text,
    created_at       timestamptz not null default now(),
    constraint profiles_username_format check (username ~ '^[A-Za-z0-9_]{3,20}$'),
    constraint profiles_display_name_len check (display_name is null or char_length(display_name) between 1 and 32),
    constraint profiles_caption_len check (caption is null or char_length(caption) <= 60)
);

-- nombres reservados, para que nadie se haga pasar por el proyecto
alter table public.profiles drop constraint if exists profiles_username_reserved;
alter table public.profiles add constraint profiles_username_reserved check (lower(username) not in (
    'greeny', 'admin', 'administrador', 'administrator', 'soporte', 'support',
    'moderador', 'moderator', 'oficial', 'official', 'staff', 'nintendo'));

-- el nombre de usuario es unico sin importar mayusculas
create unique index if not exists profiles_username_lower_idx on public.profiles (lower(username));

-- Pais (codigo de dos letras, ej. DO). Opcional: sale como bandera en el perfil y en el ranking.
alter table public.profiles add column if not exists country text;
alter table public.profiles drop constraint if exists profiles_country_format;
alter table public.profiles add constraint profiles_country_format check (country is null or country ~ '^[A-Z]{2}$');

-- Donador: lo marca un administrador a mano (la cuenta de PayPal no esta unida a la de Greeny).
-- Se ve como una medalla en el perfil. Nadie puede ponerselo a si mismo.
alter table public.profiles add column if not exists donor boolean not null default false;

-- Codigo de amigo: GR-123456 (seis numeros). Sirve para seguir a
-- alguien sin tener que buscar su nombre. Se genera solo al crear el perfil.
-- Foto de perfil: una de las que trae Greeny (web/img/avatars/NN.png), por numero.
-- Al crear la cuenta toca una al azar. Ya no se aceptan fotos propias ni enlaces.
alter table public.profiles add column if not exists avatar smallint;
update public.profiles set avatar = 1 + floor(random() * 36)::int where avatar is null;
update public.profiles set avatar_url = null where avatar_url is not null;
alter table public.profiles alter column avatar set default (1 + floor(random() * 36)::int);
alter table public.profiles alter column avatar set not null;
alter table public.profiles drop constraint if exists profiles_avatar_range;
alter table public.profiles add constraint profiles_avatar_range check (avatar between 1 and 64);

alter table public.profiles add column if not exists friend_code text;

create or replace function public.new_friend_code()
returns text language plpgsql volatile set search_path = ''
as $$
declare
    bytes bytea := uuid_send(gen_random_uuid());
    digits text := '';
begin
    for i in 0..5 loop
        digits := digits || (get_byte(bytes, i) % 10)::text;
    end loop;
    return 'GR-' || digits;
end;
$$;

create or replace function public.set_friend_code()
returns trigger language plpgsql set search_path = ''
as $$
begin
    -- el codigo lo pone siempre el servidor; si choca con uno existente se prueba otro
    for attempt in 1..20 loop
        new.friend_code := public.new_friend_code();
        exit when not exists (select 1 from public.profiles p where p.friend_code = new.friend_code);
    end loop;
    return new;
end;
$$;

drop trigger if exists profiles_friend_code on public.profiles;
create trigger profiles_friend_code before insert on public.profiles
    for each row execute function public.set_friend_code();

-- perfiles sin codigo, o con el formato largo de una version anterior
do $$
declare r record; code text;
begin
    for r in select id from public.profiles where friend_code is null or friend_code !~ '^GR-[0-9]{6}$' loop
        loop
            code := public.new_friend_code();
            exit when not exists (select 1 from public.profiles p where p.friend_code = code);
        end loop;
        update public.profiles set friend_code = code where id = r.id;
    end loop;
end $$;

create unique index if not exists profiles_friend_code_idx on public.profiles (friend_code);

-- Frases sugeridas para el caption (el desplegable de la web).
create table if not exists public.caption_presets (
    id      smallint primary key,
    text_es text not null,
    text_en text not null,
    sort    smallint not null default 0
);

insert into public.caption_presets (id, text_es, text_en, sort) values
    ( 1, 'Cazatrofeos',                 'Trophy hunter',            10),
    ( 2, 'Completista sin remedio',     'Hopeless completionist',   20),
    ( 3, 'Jugador casual',              'Casual player',            30),
    ( 4, 'Al 100% o nada',              '100% or nothing',          40),
    ( 5, 'Coleccionista de logros',     'Achievement collector',    50),
    ( 6, 'Speedrunner en proceso',      'Speedrunner in training',  60),
    ( 7, 'Explorador de cada rincón',   'Explorer of every corner', 70),
    ( 8, 'Una partida más y ya',        'Just one more game',       80),
    ( 9, 'Modo difícil siempre',        'Always on hard mode',      90),
    (10, 'Cazador de secretos',         'Secret hunter',           100),
    (11, 'Jugando desde chiquito',      'Gaming since day one',    110),
    (12, 'Nuevo por aquí',              'New around here',         120)
on conflict (id) do update
    set text_es = excluded.text_es, text_en = excluded.text_en, sort = excluded.sort;


-- -----------------------------------------------------------------------------
-- 2. ADMINISTRADORES
-- Quien puede publicar juegos, logros e imagenes. Nadie mas puede escribir ahi.
-- -----------------------------------------------------------------------------

create table if not exists public.admins (
    user_id uuid primary key references auth.users (id) on delete cascade
);

create or replace function public.is_admin()
returns boolean
language sql stable security definer set search_path = ''
as $$
    select exists (select 1 from public.admins a where a.user_id = (select auth.uid()));
$$;


-- -----------------------------------------------------------------------------
-- 3. CATALOGO: JUEGOS Y LOGROS
-- La app baja de aqui el archivo de logros y los iconos de los juegos instalados.
-- -----------------------------------------------------------------------------

create table if not exists public.games (
    title_id    text primary key,          -- 16 digitos hex en mayusculas
    name        text not null,
    version     text,                      -- version del juego para la que sirven las direcciones
    build_id    text,                      -- prefijo del build id (para validar en la consola)
    definition  text not null,             -- el archivo .txt de logros completo, tal cual va en la SD
    cover_path  text,                      -- ruta en el bucket "covers" (solo la usa la web)
    published   boolean not null default false,
    updated_at  timestamptz not null default now(),
    constraint games_title_id_format check (title_id ~ '^[0-9A-F]{16}$'),
    constraint games_definition_size check (octet_length(definition) <= 65536)
);

-- En que punto va el trabajo de logros de cada juego. Lo ven los usuarios.
--   sin_empezar: el juego esta en la lista pero aun no tiene logros utiles
--   prueba:      hay logros, pero pueden fallar o cambiar
--   jugable:     funciona bien; faltan logros por añadir
--   perfecto:    lista de logros completa y probada
alter table public.games add column if not exists status text not null default 'jugable';
alter table public.games drop constraint if exists games_status_valid;
alter table public.games add constraint games_status_valid check (status in ('sin_empezar', 'prueba', 'jugable', 'perfecto'));

create table if not exists public.achievements (
    title_id   text not null references public.games (title_id) on delete cascade,
    ach_id     text not null,              -- el [id] del archivo de logros
    name_es    text not null,
    desc_es    text not null default '',
    name_en    text,
    desc_en    text,
    points     smallint not null default 10,
    icon_path  text,                       -- ruta en el bucket "icons"
    sort       smallint not null default 0,
    primary key (title_id, ach_id),
    constraint achievements_id_format check (ach_id ~ '^[A-Za-z0-9_-]{1,31}$'),
    constraint achievements_points_range check (points between 0 and 1000)
);

-- bronce, plata u oro: que tan dificil es el logro
alter table public.achievements add column if not exists tier text;
update public.achievements
   set tier = case when points <= 10 then 'bronce' when points <= 25 then 'plata' else 'oro' end
 where tier is null;
alter table public.achievements alter column tier set default 'bronce';
alter table public.achievements alter column tier set not null;
alter table public.achievements drop constraint if exists achievements_tier_valid;
alter table public.achievements add constraint achievements_tier_valid check (tier in ('bronce', 'plata', 'oro'));

-- cada cambio a un juego actualiza su fecha, para que la app sepa si tiene que volver a bajarlo
create or replace function public.touch_updated_at()
returns trigger language plpgsql set search_path = ''
as $$
begin
    new.updated_at := now();
    return new;
end;
$$;

drop trigger if exists games_touch on public.games;
create trigger games_touch before update on public.games
    for each row execute function public.touch_updated_at();


-- -----------------------------------------------------------------------------
-- 4. PROGRESO
-- Un registro por logro desbloqueado por usuario.
-- -----------------------------------------------------------------------------

create table if not exists public.unlocks (
    user_id     uuid not null references public.profiles (id) on delete cascade,
    title_id    text not null,
    ach_id      text not null,
    unlocked_at timestamptz not null,           -- cuando salio en la consola
    synced_at   timestamptz not null default now(),
    primary key (user_id, title_id, ach_id),
    foreign key (title_id, ach_id) references public.achievements (title_id, ach_id) on delete cascade
);

create index if not exists unlocks_game_idx on public.unlocks (title_id, ach_id);
create index if not exists unlocks_recent_idx on public.unlocks (unlocked_at desc);


-- -----------------------------------------------------------------------------
-- 5. CONSOLAS VINCULADAS
-- La consola no guarda tu contraseña: guarda una llave propia (token) que solo
-- sirve para subir logros a tu cuenta, y que puedes revocar desde la web.
-- En la base de datos solo se guarda el hash de esa llave.
-- -----------------------------------------------------------------------------

create table if not exists public.devices (
    id          uuid primary key default gen_random_uuid(),
    token_hash  text not null unique,
    user_id     uuid references public.profiles (id) on delete cascade,  -- null hasta que se vincula
    name        text not null default 'Switch',
    link_code   text unique,                -- codigo corto que se escribe en la web; null cuando ya se uso
    link_expires_at timestamptz,
    created_at  timestamptz not null default now(),
    last_seen_at timestamptz
);

create index if not exists devices_user_idx on public.devices (user_id);

create or replace function public.hash_token(token text)
returns text language sql immutable set search_path = ''
as $$
    select encode(sha256(convert_to(token, 'UTF8')), 'hex');
$$;

-- Codigo de 6 caracteres sin letras que se confunden (0/O, 1/I/L).
create or replace function public.new_link_code()
returns text language plpgsql volatile set search_path = ''
as $$
declare
    alphabet constant text := 'ABCDEFGHJKMNPQRSTUVWXYZ23456789';
    bytes bytea := uuid_send(gen_random_uuid());
    code text := '';
begin
    for i in 0..5 loop
        code := code || substr(alphabet, 1 + get_byte(bytes, i) % length(alphabet), 1);
    end loop;
    return code;
end;
$$;

-- (consola, sin sesion) Pide un codigo de vinculacion. Devuelve el codigo para
-- mostrar en pantalla y la llave que la consola guarda en la SD.
create or replace function public.device_link_start(device_name text default 'Switch')
returns table (link_code text, device_token text, expires_at timestamptz)
language plpgsql security definer set search_path = ''
as $$
declare
    v_token text := replace(gen_random_uuid()::text || gen_random_uuid()::text, '-', '');
    v_code text;
    v_expires timestamptz := now() + interval '10 minutes';
begin
    -- limpieza: codigos vencidos que nadie reclamo
    delete from public.devices d where d.user_id is null and d.link_expires_at < now();

    for attempt in 1..5 loop
        v_code := public.new_link_code();
        begin
            insert into public.devices (token_hash, name, link_code, link_expires_at)
            values (public.hash_token(v_token), left(coalesce(nullif(trim(device_name), ''), 'Switch'), 32), v_code, v_expires);
            return query select v_code, v_token, v_expires;
            return;
        exception when unique_violation then
            null; -- codigo repetido: probar otro
        end;
    end loop;
    raise exception 'no se pudo generar un codigo, intenta de nuevo';
end;
$$;

-- (web, con sesion) El usuario escribe el codigo que ve en la consola.
create or replace function public.device_link_claim(code text)
returns text   -- nombre de la consola vinculada
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid := (select auth.uid());
    v_name text;
begin
    if v_uid is null then
        raise exception 'hay que iniciar sesion';
    end if;

    update public.devices d
       set user_id = v_uid, link_code = null, link_expires_at = null
     where d.link_code = upper(trim(code)) and d.user_id is null and d.link_expires_at > now()
    returning d.name into v_name;

    if v_name is null then
        raise exception 'codigo incorrecto o vencido';
    end if;
    return v_name;
end;
$$;

-- (consola, con sesion por correo y contraseña) Registra la consola directamente,
-- sin codigo. Devuelve la llave que la consola guarda en la SD.
create or replace function public.device_register(device_name text default 'Switch')
returns text
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid := (select auth.uid());
    v_token text := replace(gen_random_uuid()::text || gen_random_uuid()::text, '-', '');
begin
    if v_uid is null then
        raise exception 'hay que iniciar sesion';
    end if;
    insert into public.devices (token_hash, user_id, name)
    values (public.hash_token(v_token), v_uid, left(coalesce(nullif(trim(device_name), ''), 'Switch'), 32));
    return v_token;
end;
$$;

-- (consola) Pregunta a quien pertenece. Sin filas = la llave no existe o fue revocada.
-- linked = false mientras el usuario no haya escrito el codigo en la web.
create or replace function public.device_whoami(device_token text)
returns table (linked boolean, username text, display_name text, caption text)
language plpgsql security definer set search_path = ''
as $$
begin
    update public.devices d set last_seen_at = now() where d.token_hash = public.hash_token(device_token);
    return query
        select d.user_id is not null, p.username, p.display_name, p.caption
          from public.devices d
          left join public.profiles p on p.id = d.user_id
         where d.token_hash = public.hash_token(device_token);
end;
$$;

-- (consola) Sube logros desbloqueados. `items` es una lista JSON:
--   [{"title_id":"010028600EBDA000","ach_id":"monedas_5","unlocked_at":1794264260}, ...]
-- unlocked_at va en segundos unix (0 = fecha desconocida). Los que no existan en
-- el catalogo se ignoran. Devuelve cuantos eran nuevos.
create or replace function public.device_push_unlocks(device_token text, items jsonb)
returns integer
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
    v_new integer;
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then
        raise exception 'consola no vinculada';
    end if;
    if jsonb_typeof(items) <> 'array' or jsonb_array_length(items) > 2000 then
        raise exception 'lista de logros invalida';
    end if;

    update public.devices d set last_seen_at = now() where d.token_hash = public.hash_token(device_token);

    with incoming as (
        select i.title_id, i.ach_id,
               case when i.unlocked_at is null or i.unlocked_at <= 0 then now()
                    -- una fecha en el futuro (reloj mal puesto) se guarda como "ahora"
                    else least(to_timestamp(i.unlocked_at), now()) end as unlocked_at
          from jsonb_to_recordset(items) as i (title_id text, ach_id text, unlocked_at double precision)
    ), inserted as (
        insert into public.unlocks (user_id, title_id, ach_id, unlocked_at)
        select distinct on (i.title_id, i.ach_id) v_uid, i.title_id, i.ach_id, i.unlocked_at
          from incoming i
          join public.achievements a on a.title_id = i.title_id and a.ach_id = i.ach_id
         order by i.title_id, i.ach_id, i.unlocked_at
        on conflict (user_id, title_id, ach_id) do nothing
        returning 1
    )
    select count(*) into v_new from inserted;
    return v_new;
end;
$$;


-- -----------------------------------------------------------------------------
-- 6. CREAR EL PERFIL AL REGISTRARSE
-- -----------------------------------------------------------------------------

create or replace function public.handle_new_user()
returns trigger
language plpgsql security definer set search_path = ''
as $$
declare
    meta jsonb := coalesce(new.raw_user_meta_data, '{}'::jsonb);
    -- nombre pedido al registrarse, o el de Discord, o la parte del correo antes de la @
    wanted text := coalesce(meta ->> 'username', meta ->> 'user_name', meta ->> 'preferred_username',
                            meta ->> 'name', split_part(coalesce(new.email, ''), '@', 1));
    base text := left(regexp_replace(coalesce(wanted, ''), '[^A-Za-z0-9_]', '', 'g'), 20);
    candidate text;
begin
    if char_length(base) < 3 or lower(base) in ('greeny', 'admin', 'administrador', 'administrator', 'soporte', 'support',
                                                'moderador', 'moderator', 'oficial', 'official', 'staff', 'nintendo') then
        base := 'jugador';
    end if;
    candidate := base;

    for attempt in 1..8 loop
        begin
            insert into public.profiles (id, username, display_name, discord_username)
            values (new.id, candidate,
                    nullif(left(coalesce(meta ->> 'full_name', meta ->> 'name', ''), 32), ''),
                    case when new.raw_app_meta_data ->> 'provider' = 'discord'
                         then coalesce(meta ->> 'user_name', meta ->> 'name') end);
            return new;
        exception when unique_violation then
            -- nombre ocupado: añadir un numero
            candidate := left(base, 15) || '_' || (1000 + floor(random() * 9000))::int;
        end;
    end loop;

    -- ultimo recurso: un nombre que no puede chocar
    insert into public.profiles (id, username) values (new.id, 'j_' || left(replace(new.id::text, '-', ''), 18));
    return new;
end;
$$;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created after insert on auth.users
    for each row execute function public.handle_new_user();


-- -----------------------------------------------------------------------------
-- 7. ESTADISTICAS (vistas de solo lectura para la web)
-- -----------------------------------------------------------------------------

-- cuanta gente tiene cada logro, y cuanta juega cada juego (para el % de rareza)
create or replace view public.achievement_stats with (security_invoker = true) as
select a.title_id, a.ach_id,
       count(u.user_id) as unlocked_by,
       (select count(distinct u2.user_id) from public.unlocks u2 where u2.title_id = a.title_id) as players
  from public.achievements a
  left join public.unlocks u on u.title_id = a.title_id and u.ach_id = a.ach_id
 group by a.title_id, a.ach_id;

-- Seguir a otros jugadores.
create table if not exists public.follows (
    follower_id uuid not null references public.profiles (id) on delete cascade,
    followed_id uuid not null references public.profiles (id) on delete cascade,
    created_at  timestamptz not null default now(),
    primary key (follower_id, followed_id),
    constraint follows_not_self check (follower_id <> followed_id)
);
create index if not exists follows_followed_idx on public.follows (followed_id);

-- Totales de cada usuario: logros, puntos, juegos, platinos (juegos con todos sus
-- logros), seguidores y siguiendo. El nivel y las medallas se calculan a partir
-- de estos numeros con las mismas reglas en la app y en la web.
-- COMENTARIOS DE LOS JUEGOS ----------------------------------------------------
-- Uno por persona y juego (se puede editar o borrar). Los "me gusta" dan puntos al autor:
-- 2 por cada uno, con un tope de 10 por comentario para que no se pueda inflar.
create table if not exists public.reviews (
    id         bigint generated always as identity primary key,
    title_id   text not null references public.games (title_id) on delete cascade,
    user_id    uuid not null references public.profiles (id) on delete cascade,
    body       text not null,
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now(),
    unique (title_id, user_id),
    constraint reviews_body_len check (char_length(body) between 2 and 500)
);
create table if not exists public.review_likes (
    review_id  bigint not null references public.reviews (id) on delete cascade,
    user_id    uuid not null references public.profiles (id) on delete cascade,
    created_at timestamptz not null default now(),
    primary key (review_id, user_id)
);
create index if not exists reviews_title_idx on public.reviews (title_id);

drop view if exists public.profile_stats;
create view public.profile_stats with (security_invoker = true) as
select p.id, p.username,
       coalesce(s.achievements, 0)::int as achievements,
       (coalesce(s.points, 0) + coalesce(r.review_points, 0))::int as points,
       coalesce(s.points, 0)::int       as achievement_points,
       coalesce(r.review_points, 0)::int as review_points,
       coalesce(s.games, 0)::int        as games,
       coalesce(s.platinums, 0)::int    as platinums,
       (select count(*) from public.follows f where f.followed_id = p.id)::int as followers,
       (select count(*) from public.follows f where f.follower_id = p.id)::int as following
  from public.profiles p
  left join lateral (
        select sum(g.n) as achievements, sum(g.pts) as points, count(*) as games,
               count(*) filter (where g.n = t.total) as platinums
          from (select u.title_id, count(*) as n, sum(a.points) as pts
                  from public.unlocks u
                  join public.achievements a on a.title_id = u.title_id and a.ach_id = u.ach_id
                 where u.user_id = p.id
                 group by u.title_id) g
          join (select title_id, count(*) as total from public.achievements group by title_id) t
            on t.title_id = g.title_id
       ) s on true
  left join lateral (
        select sum(least(l.n, 10) * 2) as review_points
          from (select (select count(*) from public.review_likes k where k.review_id = v.id) as n
                  from public.reviews v where v.user_id = p.id) l
       ) r on true;

-- Seguir a alguien por su codigo de amigo. Acepta el codigo con o sin "GR", guiones o espacios.
-- Devuelve el nombre de usuario de la persona seguida.
create or replace function public.follow_code(who uuid, code text)
returns text
language plpgsql security definer set search_path = ''
as $$
declare
    digits text := regexp_replace(coalesce(code, ''), '[^0-9]', '', 'g');
    target record;
begin
    if char_length(digits) <> 6 then
        raise exception 'el codigo de amigo tiene 6 numeros';
    end if;
    select p.id, p.username into target from public.profiles p where p.friend_code = 'GR-' || digits;
    if target.id is null then
        raise exception 'no hay nadie con ese codigo de amigo';
    end if;
    if target.id = who then
        raise exception 'ese es tu propio codigo';
    end if;
    insert into public.follows (follower_id, followed_id) values (who, target.id) on conflict do nothing;
    return target.username;
end;
$$;

-- (web, con sesion)
create or replace function public.follow_by_code(code text)
returns text
language plpgsql security definer set search_path = ''
as $$
begin
    if (select auth.uid()) is null then
        raise exception 'hay que iniciar sesion';
    end if;
    return public.follow_code((select auth.uid()), code);
end;
$$;

-- (consola)
create or replace function public.device_follow_by_code(device_token text, code text)
returns text
language plpgsql security definer set search_path = ''
as $$
declare v_uid uuid;
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then
        raise exception 'consola no vinculada';
    end if;
    return public.follow_code(v_uid, code);
end;
$$;

-- (consola) Datos del perfil para mostrarlos en la app. null si la consola no esta vinculada.
create or replace function public.device_profile(device_token text)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select jsonb_build_object(
               'username', p.username, 'display_name', p.display_name, 'caption', p.caption, 'avatar', p.avatar, 'country', p.country, 'donor', p.donor,
               'caption_en', (select c.text_en from public.caption_presets c where c.text_es = p.caption limit 1),
               'friend_code', p.friend_code, 'review_points', s.review_points,
               'admin', exists (select 1 from public.admins a where a.user_id = p.id),
               'achievements', s.achievements, 'points', s.points, 'games', s.games, 'platinums', s.platinums,
               'followers', s.followers, 'following', s.following)
      from public.devices d
      join public.profiles p on p.id = d.user_id
      join public.profile_stats s on s.id = p.id
     where d.token_hash = public.hash_token(device_token);
$$;

-- (consola) Cambia la foto de perfil por una de las que trae Greeny.
create or replace function public.device_set_avatar(device_token text, picture int)
returns boolean
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    if picture is null or picture < 1 or picture > 64 then raise exception 'foto invalida'; end if;
    update public.profiles set avatar = picture where id = v_uid;
    return true;
end;
$$;

-- (consola) Cambia el pais del perfil. '' o null lo quita.
create or replace function public.device_set_country(device_token text, code text)
returns boolean
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
    v_code text := nullif(upper(trim(coalesce(code, ''))), '');
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    if v_code is not null and v_code !~ '^[A-Z]{2}$' then raise exception 'pais invalido'; end if;
    update public.profiles set country = v_code where id = v_uid;
    return true;
end;
$$;

-- (consola) Cambia la frase del perfil. '' o null la quita.
create or replace function public.device_set_caption(device_token text, caption text)
returns boolean
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
    v_text text := nullif(trim(coalesce(caption, '')), '');
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    if char_length(v_text) > 60 then raise exception 'la frase es muy larga'; end if;
    update public.profiles set caption = v_text where id = v_uid;
    return true;
end;
$$;

-- (consola) Sigue a alguien por su nombre de usuario (desde el ranking). Devuelve el usuario seguido.
create or replace function public.device_follow_user(device_token text, who text)
returns text
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
    target record;
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    select p.id, p.username into target from public.profiles p where lower(p.username) = lower(who);
    if target.id is null then raise exception 'ese usuario no existe'; end if;
    if target.id = v_uid then raise exception 'ese eres tu'; end if;
    insert into public.follows (follower_id, followed_id) values (v_uid, target.id) on conflict do nothing;
    return target.username;
end;
$$;

-- RANKING ---------------------------------------------------------------------
-- Puntos de cada usuario en cada juego.
create or replace view public.game_points
with (security_invoker = true) as
select u.user_id, u.title_id, sum(a.points)::int as points, count(*)::int as achievements
  from public.unlocks u
  join public.achievements a on a.title_id = u.title_id and a.ach_id = u.ach_id
 group by u.user_id, u.title_id;

-- Los 10 primeros y la posicion de quien mira.
--   scope: 'global', 'country' (usa p_country) o 'friends' (a quienes sigue `viewer`, y el mismo)
--   p_title: null = por puntos totales; un title id = por puntos en ese juego
create or replace function public.ranking_for(viewer uuid, scope text, p_country text, p_title text)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    with base as (
        select p.id, p.username, p.display_name, p.avatar, p.country,
               s.points as total_points, s.achievements,
               case when p_title is null then s.points else coalesce(g.points, 0) end as points
          from public.profiles p
          join public.profile_stats s on s.id = p.id
          left join public.game_points g on g.user_id = p.id and g.title_id = p_title
         where (scope is distinct from 'country' or p.country = upper(p_country))
           and (scope is distinct from 'friends' or p.id = viewer
                or exists (select 1 from public.follows f where f.follower_id = viewer and f.followed_id = p.id))
    ), ranked as (
        select b.*, row_number() over (order by b.points desc, lower(b.username)) as position
          from base b where b.points > 0
    )
    select jsonb_build_object(
        'top', coalesce((select jsonb_agg(to_jsonb(r) - 'id' order by r.position)
                           from (select * from ranked order by position limit 10) r), '[]'::jsonb),
        'me', (select to_jsonb(r) - 'id' from ranked r where r.id = viewer),
        'total', (select count(*) from ranked));
$$;

-- (web) con o sin sesion
create or replace function public.ranking(scope text default 'global', country text default null, title text default null)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select public.ranking_for((select auth.uid()), scope, country, title);
$$;

-- (consola)
create or replace function public.device_ranking(device_token text, scope text default 'global', country text default null,
                                                 title text default null)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select public.ranking_for((select d.user_id from public.devices d where d.token_hash = public.hash_token(device_token)),
                              scope, country, title);
$$;

-- (administradores) Marca o desmarca a alguien como donador. Devuelve su nombre de usuario.
create or replace function public.set_donor(who text, is_donor boolean)
returns text
language plpgsql security definer set search_path = ''
as $$
declare
    target record;
begin
    if not public.is_admin() then raise exception 'solo administradores'; end if;
    select p.id, p.username into target from public.profiles p where lower(p.username) = lower(trim(who));
    if target.id is null then raise exception 'ese usuario no existe'; end if;
    update public.profiles set donor = coalesce(is_donor, false) where id = target.id;
    return target.username;
end;
$$;

-- (administradores) Lo mismo, con el correo que da PayPal. El correo no sale de la base de datos:
-- solo se devuelve el nombre de usuario de esa cuenta.
create or replace function public.set_donor_email(donor_email text, is_donor boolean)
returns text
language plpgsql security definer set search_path = ''
as $$
declare
    target record;
begin
    if not public.is_admin() then raise exception 'solo administradores'; end if;
    select p.id, p.username into target
      from auth.users u join public.profiles p on p.id = u.id
     where lower(u.email) = lower(trim(donor_email));
    if target.id is null then raise exception 'no hay ninguna cuenta con ese correo'; end if;
    update public.profiles set donor = coalesce(is_donor, false) where id = target.id;
    return target.username;
end;
$$;


-- -----------------------------------------------------------------------------
-- 8. SEGURIDAD POR FILA
-- Regla general: todo el mundo puede leer lo publico; cada quien escribe solo lo suyo.
-- -----------------------------------------------------------------------------

alter table public.profiles        enable row level security;
alter table public.caption_presets enable row level security;
alter table public.admins          enable row level security;
alter table public.games           enable row level security;
alter table public.achievements    enable row level security;
alter table public.unlocks         enable row level security;
alter table public.devices         enable row level security;
alter table public.follows         enable row level security;

-- perfiles: publicos; cada quien edita el suyo (y solo ciertas columnas, ver permisos abajo)
drop policy if exists profiles_read on public.profiles;
create policy profiles_read on public.profiles for select using (true);
drop policy if exists profiles_update_own on public.profiles;
create policy profiles_update_own on public.profiles for update
    using ((select auth.uid()) = id) with check ((select auth.uid()) = id);

drop policy if exists caption_presets_read on public.caption_presets;
create policy caption_presets_read on public.caption_presets for select using (true);
drop policy if exists caption_presets_admin_write on public.caption_presets;
create policy caption_presets_admin_write on public.caption_presets for all
    using (public.is_admin()) with check (public.is_admin());

-- admins: cada quien puede ver si el mismo lo es; nadie escribe desde la web
drop policy if exists admins_read_self on public.admins;
create policy admins_read_self on public.admins for select using ((select auth.uid()) = user_id);

-- catalogo: se leen los juegos publicados; los administradores ven y escriben todo
drop policy if exists games_read on public.games;
create policy games_read on public.games for select using (published or public.is_admin());
drop policy if exists games_admin_write on public.games;
create policy games_admin_write on public.games for all
    using (public.is_admin()) with check (public.is_admin());

drop policy if exists achievements_read on public.achievements;
create policy achievements_read on public.achievements for select
    using (public.is_admin() or exists (select 1 from public.games g where g.title_id = achievements.title_id and g.published));
drop policy if exists achievements_admin_write on public.achievements;
create policy achievements_admin_write on public.achievements for all
    using (public.is_admin()) with check (public.is_admin());

-- progreso: publico para leer. No hay regla de escritura a proposito: los logros
-- solo entran por device_push_unlocks(), desde una consola vinculada.
drop policy if exists unlocks_read on public.unlocks;
create policy unlocks_read on public.unlocks for select using (true);

-- seguir: cualquiera ve quien sigue a quien; cada uno sigue y deja de seguir por si mismo
drop policy if exists follows_read on public.follows;
create policy follows_read on public.follows for select using (true);
drop policy if exists follows_insert_own on public.follows;
create policy follows_insert_own on public.follows for insert with check ((select auth.uid()) = follower_id);
drop policy if exists follows_delete_own on public.follows;
create policy follows_delete_own on public.follows for delete using ((select auth.uid()) = follower_id);

-- consolas: cada quien ve y puede desvincular las suyas
drop policy if exists devices_read_own on public.devices;
create policy devices_read_own on public.devices for select using ((select auth.uid()) = user_id);
drop policy if exists devices_delete_own on public.devices;
create policy devices_delete_own on public.devices for delete using ((select auth.uid()) = user_id);

-- permisos de tabla (la seguridad por fila se aplica encima de esto)
grant usage on schema public to anon, authenticated;
grant select on public.profiles, public.caption_presets, public.games, public.achievements, public.unlocks,
                public.achievement_stats, public.profile_stats to anon, authenticated;
grant select on public.admins to authenticated;
grant select on public.follows to anon, authenticated;
grant insert, delete on public.follows to authenticated;
grant select, delete on public.devices to authenticated;
grant insert, update, delete on public.games, public.achievements, public.caption_presets to authenticated;

-- del perfil solo se pueden cambiar estas columnas (no el id ni la fecha de alta)
revoke update on public.profiles from anon, authenticated;
revoke update (avatar_url) on public.profiles from authenticated;
grant update (username, display_name, caption, avatar, country) on public.profiles to authenticated;

-- la llave de las consolas nunca sale por la API, ni siquiera su hash
revoke select on public.devices from authenticated;
grant select (id, user_id, name, created_at, last_seen_at) on public.devices to authenticated;

-- funciones: quitar el permiso por defecto y dar solo lo necesario
revoke execute on all functions in schema public from public, anon, authenticated;
grant execute on function public.is_admin() to anon, authenticated;
grant execute on function public.device_link_start(text) to anon, authenticated;
grant execute on function public.device_whoami(text) to anon, authenticated;
grant execute on function public.device_profile(text) to anon, authenticated;
grant execute on function public.device_set_avatar(text, int) to anon, authenticated;
grant execute on function public.device_set_country(text, text) to anon, authenticated;
grant execute on function public.device_set_caption(text, text) to anon, authenticated;
grant execute on function public.set_donor(text, boolean) to authenticated;
grant execute on function public.set_donor_email(text, boolean) to authenticated;
grant execute on function public.device_follow_user(text, text) to anon, authenticated;
revoke all on function public.ranking_for(uuid, text, text, text) from public, anon, authenticated;
grant execute on function public.ranking(text, text, text) to anon, authenticated;
grant execute on function public.device_ranking(text, text, text, text) to anon, authenticated;
grant select on public.game_points to anon, authenticated;
grant execute on function public.device_follow_by_code(text, text) to anon, authenticated;
grant execute on function public.follow_by_code(text) to authenticated;
grant execute on function public.device_push_unlocks(text, jsonb) to anon, authenticated;
grant execute on function public.device_link_claim(text) to authenticated;
grant execute on function public.device_register(text) to authenticated;


-- -----------------------------------------------------------------------------
-- 9. IMAGENES (Supabase Storage)
-- Dos carpetas publicas: iconos de logros y caratulas. Solo administradores suben.
--   icons/<TITLE_ID>/<ach_id>.png      covers/<TITLE_ID>.jpg
-- -----------------------------------------------------------------------------

insert into storage.buckets (id, name, public) values ('icons', 'icons', true), ('covers', 'covers', true), ('avatars', 'avatars', true)
on conflict (id) do update set public = excluded.public;

-- Fotos de perfil: cada usuario sube y borra solo dentro de su propia carpeta,
--   avatars/<id del usuario>/<archivo>.jpg
drop policy if exists greeny_avatars_read on storage.objects;
create policy greeny_avatars_read on storage.objects for select using (bucket_id = 'avatars');

-- Ya no se suben fotos propias: se elige una de las que trae Greeny (profiles.avatar).
drop policy if exists greeny_avatars_insert_own on storage.objects;
drop policy if exists greeny_avatars_update_own on storage.objects;

drop policy if exists greeny_avatars_delete_own on storage.objects;
create policy greeny_avatars_delete_own on storage.objects for delete
    using (bucket_id = 'avatars' and split_part(name, '/', 1) = (select auth.uid())::text);

drop policy if exists greeny_images_read on storage.objects;
create policy greeny_images_read on storage.objects for select
    using (bucket_id in ('icons', 'covers'));

drop policy if exists greeny_images_admin_insert on storage.objects;
create policy greeny_images_admin_insert on storage.objects for insert
    with check (bucket_id in ('icons', 'covers') and public.is_admin());

drop policy if exists greeny_images_admin_update on storage.objects;
create policy greeny_images_admin_update on storage.objects for update
    using (bucket_id in ('icons', 'covers') and public.is_admin())
    with check (bucket_id in ('icons', 'covers') and public.is_admin());

drop policy if exists greeny_images_admin_delete on storage.objects;
create policy greeny_images_admin_delete on storage.objects for delete
    using (bucket_id in ('icons', 'covers') and public.is_admin());


-- =============================================================================
-- DESPUES DE CORRERLO
--
-- 1. Registrate en la web de Greeny (o crea tu usuario en Authentication -> Users).
-- 2. Hazte administrador corriendo esto, con TU correo:
--
--      insert into public.admins (user_id)
--      select id from auth.users where email = 'tu-correo@ejemplo.com';
--
-- Sin ese paso nadie puede publicar juegos ni subir imagenes.
-- =============================================================================


-- -----------------------------------------------------------------------------
-- 10. POSTULACIONES AL EQUIPO
-- Cualquiera con cuenta puede postularse a un rol (una vez por rol). Cada quien ve
-- las suyas; los administradores las ven todas y las aceptan o rechazan.
-- -----------------------------------------------------------------------------
create table if not exists public.applications (
    id         bigint generated always as identity primary key,
    user_id    uuid not null references public.profiles (id) on delete cascade,
    role       text not null,
    contact    text not null,
    message    text not null default '',
    status     text not null default 'nueva',
    created_at timestamptz not null default now(),
    unique (user_id, role),
    constraint applications_role_valid check (role in ('beta_tester', 'moderador', 'arte', 'musica')),
    constraint applications_contact_len check (char_length(contact) between 2 and 80),
    constraint applications_message_len check (char_length(message) <= 1000),
    constraint applications_status_valid check (status in ('nueva', 'aceptada', 'rechazada'))
);
alter table public.applications enable row level security;

drop policy if exists applications_insert_own on public.applications;
create policy applications_insert_own on public.applications for insert
    with check ((select auth.uid()) = user_id and status = 'nueva');
drop policy if exists applications_read on public.applications;
create policy applications_read on public.applications for select
    using ((select auth.uid()) = user_id or public.is_admin());
drop policy if exists applications_admin_update on public.applications;
create policy applications_admin_update on public.applications for update
    using (public.is_admin()) with check (public.is_admin());
drop policy if exists applications_delete on public.applications;
create policy applications_delete on public.applications for delete
    using ((select auth.uid()) = user_id or public.is_admin());

grant select, insert, delete on public.applications to authenticated;
grant update (status) on public.applications to authenticated;



-- -----------------------------------------------------------------------------
-- 11. COMENTARIOS: reglas y funciones para la consola
-- -----------------------------------------------------------------------------
alter table public.reviews enable row level security;
alter table public.review_likes enable row level security;

drop policy if exists reviews_read on public.reviews;
create policy reviews_read on public.reviews for select using (true);
drop policy if exists reviews_insert_own on public.reviews;
create policy reviews_insert_own on public.reviews for insert with check ((select auth.uid()) = user_id);
drop policy if exists reviews_update_own on public.reviews;
create policy reviews_update_own on public.reviews for update
    using ((select auth.uid()) = user_id) with check ((select auth.uid()) = user_id);
drop policy if exists reviews_delete on public.reviews;
create policy reviews_delete on public.reviews for delete using ((select auth.uid()) = user_id or public.is_admin());

drop policy if exists review_likes_read on public.review_likes;
create policy review_likes_read on public.review_likes for select using (true);
-- un "me gusta" es tuyo y nunca a tu propio comentario
drop policy if exists review_likes_insert_own on public.review_likes;
create policy review_likes_insert_own on public.review_likes for insert
    with check ((select auth.uid()) = user_id
                and not exists (select 1 from public.reviews v where v.id = review_id and v.user_id = (select auth.uid())));
drop policy if exists review_likes_delete_own on public.review_likes;
create policy review_likes_delete_own on public.review_likes for delete using ((select auth.uid()) = user_id);

grant select on public.reviews, public.review_likes to anon, authenticated;
grant insert, delete on public.reviews, public.review_likes to authenticated;
grant update (body, updated_at) on public.reviews to authenticated;

-- Los comentarios de un juego, los mas queridos primero. `viewer` puede ser null.
create or replace function public.reviews_for(viewer uuid, title text)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select coalesce(jsonb_agg(x order by x.likes desc, x.created_at desc), '[]'::jsonb)
      from (select v.id, v.body, v.created_at, v.updated_at,
                   p.username, p.display_name, p.avatar, p.country, p.donor,
                   s.points as total_points, s.achievements,
                   (select count(*) from public.review_likes k where k.review_id = v.id)::int as likes,
                   exists (select 1 from public.review_likes k where k.review_id = v.id and k.user_id = viewer) as liked,
                   v.user_id = viewer as mine
              from public.reviews v
              join public.profiles p on p.id = v.user_id
              join public.profile_stats s on s.id = p.id
             where v.title_id = title
             order by 12 desc, v.created_at desc
             limit 40) x;
$$;

create or replace function public.device_reviews(device_token text, title text)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select public.reviews_for((select d.user_id from public.devices d where d.token_hash = public.hash_token(device_token)), title);
$$;

-- Escribe o cambia tu comentario de un juego; vacio lo borra.
create or replace function public.device_review_save(device_token text, title text, body text)
returns boolean
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
    v_body text := nullif(trim(coalesce(body, '')), '');
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    if v_body is null then
        delete from public.reviews where title_id = title and user_id = v_uid;
        return true;
    end if;
    if char_length(v_body) > 500 then raise exception 'el comentario es muy largo'; end if;
    insert into public.reviews (title_id, user_id, body) values (title, v_uid, v_body)
        on conflict (title_id, user_id) do update set body = excluded.body, updated_at = now();
    return true;
end;
$$;

-- Da o quita un "me gusta". Devuelve true si quedo dado.
create or replace function public.device_review_like(device_token text, review bigint, liked boolean)
returns boolean
language plpgsql security definer set search_path = ''
as $$
declare
    v_uid uuid;
begin
    select d.user_id into v_uid from public.devices d where d.token_hash = public.hash_token(device_token);
    if v_uid is null then raise exception 'consola sin vincular'; end if;
    if not liked then
        delete from public.review_likes where review_id = review and user_id = v_uid;
        return false;
    end if;
    if exists (select 1 from public.reviews v where v.id = review and v.user_id = v_uid) then
        raise exception 'no puedes darle me gusta a tu propio comentario';
    end if;
    insert into public.review_likes (review_id, user_id) values (review, v_uid) on conflict do nothing;
    return true;
end;
$$;

-- La misma lista para la web: quien mira sale de la sesion.
create or replace function public.game_reviews(title text)
returns jsonb
language sql stable security definer set search_path = ''
as $$
    select public.reviews_for((select auth.uid()), title);
$$;

revoke all on function public.reviews_for(uuid, text) from public, anon, authenticated;
grant execute on function public.game_reviews(text) to anon, authenticated;
grant execute on function public.device_reviews(text, text) to anon, authenticated;
grant execute on function public.device_review_save(text, text, text) to anon, authenticated;
grant execute on function public.device_review_like(text, bigint, boolean) to anon, authenticated;



-- -----------------------------------------------------------------------------
-- 12. ENTRAR DESDE LA CONSOLA CON CORREO Y CONTRASEÑA
-- -----------------------------------------------------------------------------
-- La web pide el captcha (Turnstile) al entrar; la consola no puede resolverlo, asi que
-- no entra por Supabase Auth sino por esta funcion, que comprueba la contraseña igual que
-- Auth (bcrypt) y frena a quien prueba muchas: 5 fallos por correo cada 15 minutos.
drop function if exists public.device_login_password(text, text, text);
create table if not exists public.login_attempts (
    email_hash text not null,
    at         timestamptz not null default now()
);
create index if not exists login_attempts_idx on public.login_attempts (email_hash, at);
alter table public.login_attempts enable row level security; -- sin reglas: nadie la lee desde fuera

create or replace function public.device_login_password(email text, password text, device_name text default 'Switch')
returns jsonb  -- {"token": llave para la SD} o {"error": texto}. Sin excepcion: si no, el fallo no quedaria anotado
language plpgsql security definer set search_path = ''
as $$
declare
    v_mail text := lower(trim(coalesce(email, '')));
    v_hash text := public.hash_token('login:' || lower(trim(coalesce(email, ''))));
    v_uid uuid;
    v_pass text;
    v_ok boolean;
    v_token text := replace(gen_random_uuid()::text || gen_random_uuid()::text, '-', '');
begin
    delete from public.login_attempts a where a.at < now() - interval '1 day';
    if (select count(*) from public.login_attempts a where a.email_hash = v_hash and a.at > now() - interval '15 minutes') >= 5 then
        return jsonb_build_object('error', 'Demasiados intentos. Espera 15 minutos.');
    end if;

    select u.id, u.encrypted_password into v_uid, v_pass
      from auth.users u
     where lower(u.email) = v_mail and u.deleted_at is null
       and u.email_confirmed_at is not null
       and (u.banned_until is null or u.banned_until < now())
     limit 1;
    -- con o sin cuenta se hace el mismo trabajo, para no delatar que correos existen
    v_ok := extensions.crypt(coalesce(password, ''), coalesce(v_pass, '$2a$10$abcdefghijklmnopqrstuuDdyOV5O1bNpqchvQ8zJfz9BH2yMGcH.')) = v_pass;
    if v_uid is null or not coalesce(v_ok, false) then
        insert into public.login_attempts (email_hash) values (v_hash);
        return jsonb_build_object('error', 'Correo o contraseña incorrectos.');
    end if;
    if not exists (select 1 from public.profiles p where p.id = v_uid) then
        return jsonb_build_object('error', 'Esa cuenta todavía no tiene perfil: entra una vez en la web.');
    end if;

    delete from public.login_attempts a where a.email_hash = v_hash;
    insert into public.devices (token_hash, user_id, name)
    values (public.hash_token(v_token), v_uid, left(coalesce(nullif(trim(device_name), ''), 'Switch'), 32));
    return jsonb_build_object('token', v_token);
end;
$$;

revoke all on function public.device_login_password(text, text, text) from public;
grant execute on function public.device_login_password(text, text, text) to anon, authenticated;
