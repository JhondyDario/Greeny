// Overlay de Greeny para Tesla: muestra los logros del juego que esta abierto.
//
// No lee la memoria del juego: solo los archivos que el sysmodule ya deja en la SD
// (definiciones, logros desbloqueados y progreso). Por eso no afecta al juego.
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

extern "C" {
void __libnx_init_time(void);   // de libnx: carga la zona horaria de la consola
#include "config.h"
#include "engine.h"
}

#define DIR_ROOT "sdmc:/config/greeny"

static lg_game g_game;   // grande: mejor fuera de la pila
static bool g_en;
static u64 g_tid;

enum class State { Ok, NoGame, NoDefs, Bad };
static State g_state = State::NoGame;
static int g_got, g_pts, g_total_pts;
static bool g_refresh;
static bool g_time;       // servicio de hora abierto: localtime da la hora de la consola    // al volver a mostrar el overlay: releer progreso y desbloqueos

static const char *T(const char *es, const char *en) { return g_en ? en : es; }

static bool system_english() {
    u64 code = 0;
    if (R_FAILED(setInitialize())) return false;
    bool en = R_SUCCEEDED(setGetSystemLanguage(&code)) && strncmp(reinterpret_cast<const char *>(&code), "es", 2) != 0;
    setExit();
    return en;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (n >= 0 && n < 512 * 1024) ? static_cast<char *>(malloc(n + 1)) : nullptr;
    if (buf && fread(buf, 1, n, f) != static_cast<size_t>(n)) { free(buf); buf = nullptr; }
    if (buf) buf[n] = 0;
    fclose(f);
    return buf;
}

// Relee solo lo que cambia mientras se juega: desbloqueos y progreso.
static void reload_state() {
    char path[96];
    for (int i = 0; i < g_game.nach; i++) g_game.ach[i].unlocked = false;
    snprintf(path, sizeof(path), DIR_ROOT "/unlocked/%016lX.txt", g_tid);
    lg_store_load(&g_game, path);
    snprintf(path, sizeof(path), DIR_ROOT "/progress/%016lX.txt", g_tid);
    lg_progress_load(&g_game, path);
    g_got = g_pts = g_total_pts = 0;
    for (int i = 0; i < g_game.nach; i++) {
        g_total_pts += g_game.ach[i].points;
        if (g_game.ach[i].unlocked) { g_got++; g_pts += g_game.ach[i].points; }
    }
}

// Lee el juego abierto y sus logros. Se llama al abrir el overlay desde el menu de Tesla.
static void load() {
    gr_config cfg;
    gr_config_load(&cfg, DIR_ROOT "/config.ini");
    g_en = cfg.lang == GR_LANG_EN || (cfg.lang == GR_LANG_AUTO && system_english());

    g_state = State::NoGame;
    g_tid = 0;
    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || R_FAILED(pminfoGetProgramId(&g_tid, pid)) || !g_tid) return;

    char path[96], err[128];
    snprintf(path, sizeof(path), DIR_ROOT "/games/%016lX.txt", g_tid);
    char *text = read_file(path);
    if (!text) { g_state = State::NoDefs; return; }
    memset(&g_game, 0, sizeof(g_game));
    bool ok = lg_parse(&g_game, text, err, sizeof(err)) == 0;
    free(text);
    if (!ok) { g_state = State::Bad; return; }
    reload_state();
    g_state = State::Ok;
}

static tsl::Color tier_color(const lg_ach *a) {
    switch (lg_ach_tier(a)) {
        case LG_TIER_GOLD:   return tsl::Color(0xF, 0xC, 0x1, 0xF);
        case LG_TIER_SILVER: return tsl::Color(0xB, 0xC, 0xD, 0xF);
        default:             return tsl::Color(0xC, 0x7, 0x3, 0xF);
    }
}

static bool progress_of(const lg_ach *a, u64 *have, u64 *goal) {
    if (a->unlocked || a->prog_cond < 0 || !a->prog_known || a->prog_goal == 0) return false;
    *goal = a->prog_goal;
    *have = a->prog_value < a->prog_goal ? a->prog_value : a->prog_goal;
    return true;
}

// Detalle de un logro: nombre, descripcion completa, puntos, dificultad y progreso.
class GuiDetail : public tsl::Gui {
    int m_i;
public:
    explicit GuiDetail(int i) : m_i(i) {}

    tsl::elm::Element *createUI() override {
        const lg_ach *a = &g_game.ach[m_i];
        auto *frame = new tsl::elm::OverlayFrame("Greeny", g_game.game[0] ? g_game.game : "");
        auto *list = new tsl::elm::List();
        list->addItem(new tsl::elm::CustomDrawer([a](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
            const tsl::Color ink(0xF, 0xF, 0xF, 0xF), gray(0xA, 0xB, 0xA, 0xF), green(0x4, 0xD, 0x5, 0xF);
            r->drawRect(x + 4, y + 10, 6, 34, r->a(tier_color(a)));
            r->drawString(lg_ach_name(a, g_en), false, x + 22, y + 36, 24, r->a(ink));

            // descripcion partida en lineas que quepan
            const char *d = lg_ach_desc(a, g_en);
            char line[128];
            s32 ly = y + 78;
            while (*d && ly < y + h - 110) {
                size_t best = 0, j = 0;
                for (;;) {
                    while (d[j] && d[j] != ' ') j++;
                    if (j >= sizeof(line)) break;
                    memcpy(line, d, j); line[j] = 0;
                    if (r->drawString(line, false, 0, 0, 19, tsl::style::color::ColorTransparent).first > w - 40 && best) break;
                    best = j;
                    if (!d[j]) break;
                    j++;
                }
                if (!best) break;
                memcpy(line, d, best); line[best] = 0;
                r->drawString(line, false, x + 22, ly, 19, r->a(gray));
                ly += 28;
                d += best;
                while (*d == ' ') d++;
            }

            char buf[64];
            snprintf(buf, sizeof(buf), "%u %s", a->points, T("puntos", "points"));
            r->drawString(buf, false, x + 22, y + h - 72, 20, r->a(tsl::Color(0xF, 0xC, 0x1, 0xF)));
            u64 have, goal;
            if (a->unlocked) {
                s32 dw = r->drawString(T("Desbloqueado", "Unlocked"), false, x + 22, y + h - 34, 22, r->a(green)).first;
                // fecha y hora del desbloqueo, en la hora de la consola (0 = no se sabe)
                time_t t = static_cast<time_t>(a->unlocked_at);
                struct tm tm;
                if (a->unlocked_at && localtime_r(&t, &tm)) {
                    char when[40];
                    strftime(when, sizeof(when), g_en ? "%m/%d/%Y  %H:%M" : "%d/%m/%Y  %H:%M", &tm);
                    r->drawString(when, false, x + 22 + dw + 14, y + h - 34, 19, r->a(gray));
                }
            } else if (progress_of(a, &have, &goal)) {
                snprintf(buf, sizeof(buf), "%s: %llu / %llu", T("Tu progreso", "Your progress"), (unsigned long long)have, (unsigned long long)goal);
                r->drawString(buf, false, x + 22, y + h - 40, 19, r->a(ink));
                s32 bw = w - 44;
                r->drawRect(x + 22, y + h - 26, bw, 10, r->a(tsl::Color(0x3, 0x4, 0x3, 0xF)));
                r->drawRect(x + 22, y + h - 26, static_cast<s32>(bw * have / goal), 10, r->a(tsl::Color(0xF, 0xC, 0x1, 0xF)));
            } else {
                r->drawString(T("Bloqueado", "Locked"), false, x + 22, y + h - 34, 22, r->a(gray));
            }
        }), 330);
        frame->setContent(list);
        return frame;
    }
};

static void value_of(const lg_ach *a, char *value, size_t cap) {
    u64 have, goal;
    if (a->unlocked) snprintf(value, cap, "\uE14B %u", a->points);  // check
    else if (progress_of(a, &have, &goal)) snprintf(value, cap, "%llu/%llu", (unsigned long long)have, (unsigned long long)goal);
    else snprintf(value, cap, "%u pts", a->points);
}

// Lista: primero los que faltan (los que tienen progreso arriba), luego los conseguidos.
class GuiList : public tsl::Gui {
    tsl::elm::ListItem *m_items[LG_MAX_ACH];
    int m_idx[LG_MAX_ACH], m_n = 0, m_frames = 0;
public:
    // cada 2 segundos (y al volver a mostrarse) se ponen al dia los valores de la derecha
    void update() override {
        if (g_state != State::Ok || (!g_refresh && ++m_frames < 120)) return;
        m_frames = 0;
        g_refresh = false;
        reload_state();
        char value[32];
        for (int k = 0; k < m_n; k++) {
            value_of(&g_game.ach[m_idx[k]], value, sizeof(value));
            m_items[k]->setValue(value);
        }
    }

    tsl::elm::Element *createUI() override {
        char sub[96];
        if (g_state == State::Ok)
            snprintf(sub, sizeof(sub), T("%d de %d  ·  %d / %d pts", "%d of %d  ·  %d / %d pts"), g_got, g_game.nach, g_pts, g_total_pts);
        else snprintf(sub, sizeof(sub), "%s", T("Logros", "Achievements"));
        auto *frame = new tsl::elm::OverlayFrame(g_state == State::Ok && g_game.game[0] ? g_game.game : "Greeny", sub);

        if (g_state != State::Ok) {
            const char *msg = g_state == State::NoGame ? T("No hay ningún juego abierto.", "No game is running.")
                            : g_state == State::NoDefs ? T("Este juego no tiene logros en Greeny.", "This game has no achievements in Greeny.")
                                                       : T("El archivo de logros de este juego tiene un error.", "This game's achievement file has an error.");
            frame->setContent(new tsl::elm::CustomDrawer([msg](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
                r->drawString(msg, false, x + 20, y + 60, 20, r->a(tsl::Color(0xF, 0xF, 0xF, 0xF)));
            }));
            return frame;
        }

        auto *list = new tsl::elm::List();
        static int order[LG_MAX_ACH];
        int n = 0;
        for (int pass = 0; pass < 3; pass++)
            for (int i = 0; i < g_game.nach; i++) {
                u64 h, g;
                const lg_ach *a = &g_game.ach[i];
                bool prog = progress_of(a, &h, &g);
                if ((pass == 0 && prog) || (pass == 1 && !a->unlocked && !prog) || (pass == 2 && a->unlocked)) order[n++] = i;
            }

        bool header_done = false, header_got = false;
        for (int k = 0; k < n; k++) {
            const int i = order[k];
            const lg_ach *a = &g_game.ach[i];
            if (!a->unlocked && !header_done) { list->addItem(new tsl::elm::CategoryHeader(T("Por conseguir", "To unlock"))); header_done = true; }
            if (a->unlocked && !header_got) { list->addItem(new tsl::elm::CategoryHeader(T("Conseguidos", "Unlocked"))); header_got = true; }
            char value[32];
            value_of(a, value, sizeof(value));
            auto *item = new tsl::elm::ListItem(lg_ach_name(a, g_en), value);
            m_items[m_n] = item;
            m_idx[m_n++] = i;
            item->setClickListener([i](u64 keys) {
                if (keys & HidNpadButton_A) { tsl::changeTo<GuiDetail>(i); return true; }
                return false;
            });
            list->addItem(item);
        }
        frame->setContent(list);
        return frame;
    }
};

class GreenyOverlay : public tsl::Overlay {
public:
    // libtesla no monta la SD por su cuenta (cambia el arranque de libnx): sin esto,
    // fopen("sdmc:/...") falla y parece que el juego no tiene logros.
    // el servicio de hora tampoco viene: sin el, la fecha del logro saldria en UTC
    void initServices() override {
        pminfoInitialize();
        fsdevMountSdmc();
        if (R_SUCCEEDED(timeInitialize())) { __libnx_init_time(); g_time = true; }
    }
    void exitServices() override { if (g_time) timeExit(); fsdevUnmountAll(); pminfoExit(); }
    void onShow() override { g_refresh = true; }
    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        load();
        return initially<GuiList>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<GreenyOverlay>(argc, argv);
}
