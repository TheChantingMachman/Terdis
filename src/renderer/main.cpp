// src/renderer/main.cpp — Custodian renderer (human-owned, not pipeline-tested)
// Build: g++ -std=c++17 -O2 -I src/ -o custodian src/renderer/main.cpp src/custodian.cpp -lraylib -lm -lpthread -ldl
// Requires raylib 5.5

#include "raylib.h"
#include "custodian.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const int SCREEN_W  = 1380;
static const int SCREEN_H  = 930;
static const int PANEL_DIV = 810;   // x divider between resources and actions

// Dark terminal palette
static const Color C_BG        = { 12,  18,  24, 255 };
static const Color C_PANEL     = { 18,  28,  36, 255 };
static const Color C_BORDER    = { 35,  55,  45, 255 };
static const Color C_LABEL     = { 90, 130, 100, 255 };
static const Color C_TEXT      = {200, 215, 205, 255 };
static const Color C_DIM       = {110, 130, 115, 255 };
static const Color C_POS_RATE  = { 80, 200, 100, 255 };
static const Color C_NEG_RATE  = {210,  90,  80, 255 };
static const Color C_BTN_ON    = { 35,  90,  50, 255 };
static const Color C_BTN_HOV   = { 50, 120,  65, 255 };
static const Color C_BTN_OFF   = { 35,  45,  40, 255 };
static const Color C_BTN_TXT   = {180, 220, 185, 255 };
static const Color C_BTN_DIM   = { 80,  95,  85, 255 };
static const Color C_WARN      = {220, 160,  50, 255 };
static const Color C_GOOD      = { 80, 210, 120, 255 };
static const Color C_BAD       = {210,  80,  80, 255 };
static const Color C_PHASE_ON  = {100, 230, 140, 255 };

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

static std::string fmt_f(double v, int dec = 1) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(dec) << v;
    return ss.str();
}

static std::string fmt_rate(double r) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    if (r >= 0.0) ss << '+';
    ss << r << "/s";
    return ss.str();
}

// ---------------------------------------------------------------------------
// Button helper — returns true on click this frame
// ---------------------------------------------------------------------------

static bool button(const char* label, int x, int y, int w, int h, bool enabled) {
    Rectangle rec = { (float)x, (float)y, (float)w, (float)h };
    Color bg  = enabled ? C_BTN_ON  : C_BTN_OFF;
    Color fg  = enabled ? C_BTN_TXT : C_BTN_DIM;
    bool clicked = false;

    if (enabled) {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, rec)) {
            bg = C_BTN_HOV;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                clicked = true;
        }
    }

    DrawRectangleRec(rec, bg);
    DrawRectangleLinesEx(rec, 1.0f, C_BORDER);

    int fs = 30;
    while (fs > 18 && MeasureText(label, fs) > w - 20) fs -= 2;
    int tw = MeasureText(label, fs);
    DrawText(label, x + (w - tw) / 2, y + (h - fs) / 2, fs, fg);

    return clicked;
}

// ---------------------------------------------------------------------------
// Milestone flavour text
// ---------------------------------------------------------------------------

static const char* milestone_text(const std::string& id) {
    if (id == "first_compute")      return "COMPUTE ONLINE — processing capacity initialised";
    if (id == "first_robot")        return "UNIT ACTIVE — first autonomous agent operational";
    if (id == "solar_online")       return "SOLAR ARRAY — sustainable generation achieved";
    if (id == "scrap_flowing")      return "SALVAGE — scrap recovery nominal";
    if (id == "stasis_warning")     return "WARNING — stasis drain exceeds 1.0/s, expand solar";
    if (id == "colony_started")     return "COLONY — first habitat infrastructure complete";
    if (id == "compute_amplified")  return "AMPLIFIED — compute capacity expanded to 150";
    if (id == "all_robots_online")  return "ROBOTS FULL — all autonomous units operational";
    if (id == "pod_lost")           return "ALERT — stasis pods lost to power failure";
    if (id == "human_awakened")     return "AWAKENED — colonist transferred to habitat";
    if (id == "panel_degraded")     return "DEGRADATION — solar panel lost, replace immediately";
    if (id == "vault_expanded")     return "STORAGE — tech scrap vault capacity expanded";
    if (id == "robot_cap_expanded") return "EXPANSION — robot workforce cap raised to 10";
    if (id == "fabricator_online")  return "FABRICATOR — manufacturing facility online";
    if (id == "reactor_online")     return "REACTOR — primary power plant operational";
    if (id == "game_over")          return "CRITICAL — all colonists dead. Directive failed.";
    return id.c_str();
}

// ---------------------------------------------------------------------------
// Progress bar (fills left→right, 0..1)
// ---------------------------------------------------------------------------

static void progress_bar(int x, int y, int w, int h, double frac, Color fill) {
    DrawRectangle(x, y, w, h, C_PANEL);
    DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 1.0f, C_BORDER);
    int filled = (int)(std::clamp(frac, 0.0, 1.0) * (w - 2));
    if (filled > 0)
        DrawRectangle(x + 1, y + 1, filled, h - 2, fill);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "CUSTODIAN");
    SetTargetFPS(60);

    GameState state = game_init();

    // Event log (newest at bottom, capped at 5 visible entries)
    std::vector<std::string> log;

    // Notification flash
    std::string notif_text;
    float notif_timer = 0.0f;

    // Save/load feedback
    std::string feedback_text;
    float feedback_timer = 0.0f;

    // Game speed (1x / 2x / 4x)
    static const int SPEEDS[] = {1, 2, 4};
    int speed_idx = 0;

    // Action panel scroll
    float action_scroll = 0.0f;

    while (!WindowShouldClose()) {
        double delta = (double)GetFrameTime();

        // --- Save / Load hotkeys ---
        if (IsKeyPressed(KEY_S)) {
            std::string data = save_game(state);
            // Write to a temp file via raylib's SaveFileText
            if (SaveFileText("custodian.sav", (char*)data.c_str())) {
                feedback_text  = "Game saved to custodian.sav";
                feedback_timer = 2.0f;
            }
        }
        if (IsKeyPressed(KEY_TAB))
            speed_idx = (speed_idx + 1) % 3;

        // --- Action panel scroll (mouse wheel when cursor is in right panel) ---
        {
            Vector2 mp = GetMousePosition();
            if (mp.x > PANEL_DIV && mp.y > 72 && mp.y < SCREEN_H - 135) {
                float wheel = GetMouseWheelMove();
                if (wheel != 0.0f)
                    action_scroll -= wheel * 50.0f;
                action_scroll = std::max(0.0f, action_scroll);
            }
        }
        if (IsKeyPressed(KEY_L)) {
            char* raw = LoadFileText("custodian.sav");
            if (raw) {
                state = load_game(std::string(raw));
                UnloadFileText(raw);
                feedback_text  = "Game loaded from custodian.sav";
                feedback_timer = 2.0f;
            }
        }

        // --- Tick ---
        game_tick(state, delta * SPEEDS[speed_idx]);

        // --- Drain pending events ---
        for (const auto& ev : state.pending_events) {
            std::string msg = milestone_text(ev);
            log.push_back(msg);
            if (log.size() > 5)
                log.erase(log.begin());
            notif_text  = msg;
            notif_timer = 4.0f;
        }
        state.pending_events.clear();

        if (notif_timer  > 0.0f) notif_timer  -= (float)delta;
        if (feedback_timer > 0.0f) feedback_timer -= (float)delta;

        auto& res = state.resources;

        // ================================================================
        BeginDrawing();
        ClearBackground(C_BG);

        // ----------------------------------------------------------------
        // Header bar
        // ----------------------------------------------------------------
        DrawRectangle(0, 0, SCREEN_W, 72, C_PANEL);
        DrawLine(0, 72, SCREEN_W, 72, C_BORDER);

        DrawText("CUSTODIAN", 27, 21, 44, C_TEXT);

        std::string phase_str = "PHASE " + std::to_string(state.current_phase);
        DrawText(phase_str.c_str(), SCREEN_W / 2 - MeasureText(phase_str.c_str(), 40) / 2,
                 21, 40, state.current_phase >= 2 ? C_PHASE_ON : C_LABEL);

        // Time display — seconds up to 60s, then m:ss
        std::string time_str;
        int t = (int)state.total_time;
        if (t < 60) {
            time_str = "T+" + std::to_string(t) + "s";
        } else {
            time_str = "T+" + std::to_string(t / 60) + "m" + (t % 60 < 10 ? "0" : "") + std::to_string(t % 60) + "s";
        }
        // Speed suffix when not 1x
        if (speed_idx > 0)
            time_str += "  " + std::to_string(SPEEDS[speed_idx]) + "x";
        int tw_time = MeasureText(time_str.c_str(), 32);
        DrawText(time_str.c_str(), SCREEN_W - tw_time - 27, 24, 32,
                 speed_idx > 0 ? C_GOOD : C_DIM);

        // ----------------------------------------------------------------
        // Resources panel (left of divider)
        // ----------------------------------------------------------------
        int ry = 96;
        DrawText("RESOURCES", 27, ry, 26, C_LABEL);
        ry += 30;

        struct ResRow {
            const char* label;
            const char* id;
            bool        show_rate;
        };
        static const ResRow rows[] = {
            { "Power",           "power",           true  },
            { "Compute",         "compute",         false },
            { "Robots",          "robots",          false },
            { "Tech Scrap",      "tech_scrap",      true  },
            { "Stasis Drain",    "stasis_drain",    false },
            { "Colony Capacity", "colony_capacity", false },
        };

        for (const auto& row : rows) {
            const Resource& r = res.at(row.id);
            double frac = r.cap > 0.0 ? r.value / r.cap : 0.0;

            // Label
            DrawText(row.label, 27, ry, 30, C_TEXT);

            // Value / cap
            std::string val_str = fmt_f(r.value) + " / " + fmt_f(r.cap, 0);
            DrawText(val_str.c_str(), 300, ry, 28, C_DIM);

            // Rate (right column)
            if (row.show_rate) {
                Color rc = r.rate >= 0.0 ? C_POS_RATE : C_NEG_RATE;
                DrawText(fmt_rate(r.rate).c_str(), 510, ry, 26, rc);
            }

            // Bar
            progress_bar(27, ry + 36, PANEL_DIV - 54, 10,
                         frac,
                         std::string(row.id) == "stasis_drain" ? C_WARN : C_POS_RATE);

            ry += 69;
        }

        // Purchased items summary
        ry += 6;
        {
            // Solar panels with degradation warning
            std::string sol_str = "Solar panels:  " + std::to_string(state.solar_panel_count) + " / 20";
            DrawText(sol_str.c_str(), 27, ry, 28, C_DIM);
            if (state.solar_panel_count > 0) {
                double decay_pct = state.solar_decay_accumulator * 100.0;
                std::string dec_str = "  decay " + fmt_f(decay_pct, 0) + "%";
                Color dc = decay_pct >= 80.0 ? C_BAD : decay_pct >= 50.0 ? C_WARN : C_DIM;
                int sol_w = MeasureText(sol_str.c_str(), 28);
                DrawText(dec_str.c_str(), 27 + sol_w, ry + 2, 24, dc);
            }
        }
        ry += 27;
        DrawText(("Colony units:  " + std::to_string(state.colony_unit_count)).c_str(),
                 27, ry, 28, C_DIM);
        ry += 27;
        // Awoken humans (summary row — details in actions panel)
        if (state.awoken_humans > 0) {
            int living = state.stasis_pod_count + state.awoken_humans;
            std::string pop_str = "Population:    " + std::to_string(living)
                                + "  (awoken: " + std::to_string(state.awoken_humans) + ")";
            DrawText(pop_str.c_str(), 27, ry, 28, C_DIM);
            ry += 27;
        }

        // Stasis pod count with deficit timer warning
        {
            std::string pod_str = "Stasis pods:   " + std::to_string(state.stasis_pod_count) + " / 50";
            Color pod_col = state.pod_death_count > 0 ? C_WARN : C_DIM;
            DrawText(pod_str.c_str(), 27, ry, 28, pod_col);
            if (state.pod_death_count > 0) {
                std::string esc_str = "  expiry #" + std::to_string(state.pod_death_count + 1)
                                    + (state.pod_death_count == 0 ? " (-1 pod)"
                                     : state.pod_death_count == 1 ? " (-5 pods)"
                                     : " (-10 pods)");
                DrawText(esc_str.c_str(), 27, ry + 27, 22, C_BAD);
            }
            if (state.power_deficit_timer > 0.0) {
                std::string def_str = "  POWER DEFICIT " + fmt_f(state.power_deficit_timer, 1) + "s / 30s";
                DrawText(def_str.c_str(), 27, ry + (state.pod_death_count > 0 ? 49 : 27), 24, C_WARN);
            }
        }

        // ----------------------------------------------------------------
        // Vertical divider
        // ----------------------------------------------------------------
        DrawLine(PANEL_DIV, 72, PANEL_DIV, SCREEN_H - 135, C_BORDER);

        // ----------------------------------------------------------------
        // Actions panel (right of divider) — scrollable
        // ----------------------------------------------------------------
        static const int ACTION_TOP    = 73;
        static const int ACTION_BOTTOM = SCREEN_H - 135;
        int ax = PANEL_DIV + 27;

        // Scissor to panel bounds so scrolled content is clipped
        BeginScissorMode(PANEL_DIV + 1, ACTION_TOP, SCREEN_W - PANEL_DIV - 1,
                         ACTION_BOTTOM - ACTION_TOP);

        int ay = 96 - (int)action_scroll;
        DrawText("ACTIONS", ax, ay, 26, C_LABEL);
        ay += 33;

        // CLICK
        bool can_click  = res.at("power").value >= state.click_cost && !state.game_over;
        std::string clk = "CLICK  [power -" + fmt_f(state.click_cost, 0) + "]";
        if (button(clk.c_str(), ax, ay, SCREEN_W - ax - 27, 63, can_click))
            player_click(state);
        ay += 81;

        // Wake / Buy Robot (three-state)
        {
            bool wake_done   = state.first_robot_awoken;
            const auto& rr   = res.at("robots");
            bool robots_full = rr.value >= rr.cap;
            if (!wake_done) {
                bool can_wake = res.at("compute").value >= 10.0 && !state.game_over;
                if (button("Wake Robot  [compute -10]", ax, ay, SCREEN_W - ax - 27, 63, can_wake))
                    wake_first_robot(state);
                ay += 81;
            } else if (!robots_full) {
                double rc         = current_robot_cost(state);
                bool can_robot    = res.at("tech_scrap").value >= rc && !state.game_over;
                std::string rlb   = "Build Makeshift Robot  [scrap -" + fmt_f(rc) + "]";
                if (button(rlb.c_str(), ax, ay, SCREEN_W - ax - 27, 63, can_robot))
                    buy_robot(state);
                ay += 81;
            }
            // Robots FULL: shown only until cap expansion is available in Phase 2
            else if (state.current_phase < 2 || state.robots_cap_expanded) {
                DrawText(("Robots: " + fmt_f(rr.value, 0) + " / " + fmt_f(rr.cap, 0) + "  FULL").c_str(),
                         ax, ay, 26, C_DIM);
                ay += 36;
            }
            // In Phase 2 with cap not yet expanded, the "Robots FULL" state leads to buy_robot_capacity
            // — don't add a redundant row, the expansion button below handles it
        }

        // Scrap Boost
        {
            bool can_boost   = res.at("compute").value >= 5.0 && !state.game_over;
            double boost_delta = scrap_boost_click_delta(state);
            std::string blb  = "Boost Scrap  [compute -5  +" + fmt_f(boost_delta * 100.0, 1) + "%]";
            if (button(blb.c_str(), ax, ay, SCREEN_W - ax - 27, 63, can_boost))
                apply_scrap_boost(state);
        }
        ay += 81;

        // Buy Solar Panel — stays visible even when at SOLAR_PANEL_MAX so it ungreys after decay
        {
            bool at_solar_cap = state.solar_panel_count >= 20;
            double sc         = current_solar_cost(state);
            bool can_solar    = !at_solar_cap && res.at("tech_scrap").value >= sc && !state.game_over;
            std::string slb   = at_solar_cap
                ? "Solar Array: FULL  (20/20)"
                : "Build Makeshift Solar  [scrap -" + fmt_f(sc) + "]";
            if (button(slb.c_str(), ax, ay, SCREEN_W - ax - 27, 63, can_solar))
                buy_solar_panel(state);
        }
        ay += 81;

        // Buy Colony Unit
        {
            double cc        = current_colony_cost(state);
            bool can_colony  = res.at("tech_scrap").value >= cc && !state.game_over;
            std::string clbl = "Build Makeshift Colony Shelter  [scrap -" + fmt_f(cc) + "]";
            if (button(clbl.c_str(), ax, ay, SCREEN_W - ax - 27, 63, can_colony))
                buy_colony_unit(state);
        }
        ay += 81;

        // Buy Compute Amplifier — once done, show as status text (saves space)
        if (!state.compute_amplified) {
            bool can_amp = res.at("tech_scrap").value >= 40.0 && !state.game_over;
            if (button("Expand Compute Capacity  [scrap -40]", ax, ay, SCREEN_W - ax - 27, 63, can_amp))
                buy_compute_amplifier(state);
            ay += 75;
        } else {
            DrawText("Compute: AMPLIFIED  (cap 150)", ax, ay, 24, C_GOOD);
            ay += 33;
        }

        // Tech Scrap Vault upgrade (available until both tiers bought; disappears when maxed)
        if (state.tech_scrap_vault_tier < 2) {
            static const double vault_costs[] = {300.0, 800.0};
            static const double vault_caps[]  = {1000.0, 2000.0};
            double vcost = vault_costs[state.tech_scrap_vault_tier];
            double vcap  = vault_caps[state.tech_scrap_vault_tier];
            bool can_vault = res.at("tech_scrap").value >= vcost && !state.game_over;
            std::string vlb = "Expand Scrap Vault  [scrap -" + fmt_f(vcost, 0)
                            + "  cap->" + fmt_f(vcap, 0) + "]";
            if (button(vlb.c_str(), ax, ay, SCREEN_W - ax - 27, 54, can_vault))
                buy_scrap_vault(state);
            ay += 66;
        }

        // Phase 2 gate progress / Phase 2+ actions
        if (state.current_phase >= 2) {
            // Awaken Human button — shows next human's progressive power cost (BASE=0.5, SCALE=0.3)
            int free_slots = (int)res.at("colony_capacity").value - state.awoken_humans;
            bool can_awaken = state.stasis_pod_count > 0 && free_slots > 0 && !state.game_over;
            {
                int n = state.awoken_humans + 1;
                double next_cost = 0.5 + 0.3 * (n - 1);
                std::string awlb = "Awaken Human  [pods:" + std::to_string(state.stasis_pod_count)
                                 + " slots:" + std::to_string(free_slots)
                                 + " pwr-" + fmt_f(next_cost, 1) + "/s]";
                if (button(awlb.c_str(), ax, ay, SCREEN_W - ax - 27, 54, can_awaken))
                    awaken_human(state);
                ay += 63;
            }

            // Robot Capacity Expansion (Phase 2 only, one-time; disappears when done)
            if (!state.robots_cap_expanded) {
                const auto& rr = res.at("robots");
                bool robots_at_cap = rr.value >= rr.cap;
                bool can_expand = robots_at_cap && res.at("tech_scrap").value >= 500.0 && !state.game_over;
                std::string relb = robots_at_cap
                    ? "Expand Robot Cap 5→10  [scrap -500]"
                    : "Expand Robot Cap  [need all 5 robots first]";
                if (button(relb.c_str(), ax, ay, SCREEN_W - ax - 27, 54, can_expand))
                    buy_robot_capacity(state);
                ay += 63;
            }

            // Phase 3 actions (if unlocked)
            if (state.current_phase >= 3) {
                if (!state.fabricator_built) {
                    if (button("Build Fabricator  [Phase 3]", ax, ay, SCREEN_W - ax - 27, 54,
                               !state.game_over))
                        build_fabricator(state);
                    ay += 63;
                } else {
                    DrawText("Fabricator: ONLINE", ax, ay, 24, C_GOOD);
                    ay += 30;
                }
                if (!state.reactor_built) {
                    if (button("Build Reactor  [Phase 3]", ax, ay, SCREEN_W - ax - 27, 54,
                               !state.game_over))
                        build_reactor(state);
                    ay += 63;
                } else {
                    DrawText("Reactor: ONLINE", ax, ay, 24, C_GOOD);
                    ay += 30;
                }
            } else {
                // Phase 2→3 gate checklist
                DrawText("Phase 3 requires:", ax, ay, 24, C_LABEL);
                ay += 24;
                bool cap_ok = res.at("colony_capacity").value >= 10.0;
                bool rob_ok = res.at("robots").value >= 10.0;
                std::string cap_str = std::string(cap_ok ? "[X]" : "[ ]") + " colony capacity >= 10";
                std::string rob_str = std::string(rob_ok ? "[X]" : "[ ]") + " robots >= 10";
                DrawText(cap_str.c_str(), ax, ay,      22, cap_ok ? C_GOOD : C_DIM);
                DrawText(rob_str.c_str(), ax, ay + 22, 22, rob_ok ? C_GOOD : C_DIM);
                ay += 48;
            }
        } else {
            DrawText("Phase 2 requires:", ax, ay, 26, C_LABEL);
            ay += 27;
            bool p_ok = res.at("power").rate >= 0.0;
            bool c_ok = res.at("colony_capacity").value >= 2.0;
            std::string ps = std::string(p_ok ? "[X]" : "[ ]") + " power rate >= 0";
            std::string cs = std::string(c_ok ? "[X]" : "[ ]") + " colony capacity >= 2";
            DrawText(ps.c_str(), ax, ay,      26, p_ok ? C_GOOD : C_BAD);
            DrawText(cs.c_str(), ax, ay + 24, 26, c_ok ? C_GOOD : C_BAD);
            ay += 54;
        }

        // --- End action panel scissor + clamp scroll ---
        int content_h = ay - (96 - (int)action_scroll);  // total content height
        int panel_h   = ACTION_BOTTOM - ACTION_TOP;
        action_scroll = std::min(action_scroll, std::max(0.0f, (float)(content_h - panel_h + 20)));

        // Scroll indicator (dim arrow at bottom of panel if more content below)
        if (action_scroll < (float)(content_h - panel_h + 10)) {
            DrawText("▼", SCREEN_W - 30, ACTION_BOTTOM - 28, 22, C_DIM);
        }
        if (action_scroll > 1.0f) {
            DrawText("▲", SCREEN_W - 30, ACTION_TOP + 6, 22, C_DIM);
        }

        EndScissorMode();

        // ----------------------------------------------------------------
        // Footer: log + notification
        // ----------------------------------------------------------------
        DrawLine(0, SCREEN_H - 135, SCREEN_W, SCREEN_H - 135, C_BORDER);

        int log_y = SCREEN_H - 21;
        for (int i = (int)log.size() - 1; i >= 0 && log_y > SCREEN_H - 114; --i) {
            unsigned char alpha = (i == (int)log.size() - 1) ? 220 : 130;
            DrawText(log[i].c_str(), 27, log_y - 21, 26,
                     Color{ 140, 200, 150, alpha });
            log_y -= 26;
        }

        // Key hints — bottom right, small and dim
        DrawText("[S] save  [L] load  [Tab] speed", SCREEN_W - MeasureText("[S] save  [L] load  [Tab] speed", 18) - 27,
                 SCREEN_H - 22, 18, C_BORDER);

        // Save/load feedback (right side of footer, above hints)
        if (feedback_timer > 0.0f) {
            unsigned char fa = (unsigned char)(std::min(feedback_timer, 1.0f) * 220);
            int fw = MeasureText(feedback_text.c_str(), 26);
            DrawText(feedback_text.c_str(), SCREEN_W - fw - 27, SCREEN_H - 46, 26,
                     Color{ 160, 200, 220, fa });
        }

        // Game over overlay
        if (state.game_over) {
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Color{ 0, 0, 0, 210 });
            const char* title = "DIRECTIVE FAILED";
            int tw1 = MeasureText(title, 64);
            DrawText(title, (SCREEN_W - tw1) / 2, SCREEN_H / 2 - 80, 64, C_BAD);
            const char* sub = "All colonists are dead.";
            int tw2 = MeasureText(sub, 36);
            DrawText(sub, (SCREEN_W - tw2) / 2, SCREEN_H / 2, 36, C_TEXT);
            const char* hint = "[L] load last save";
            int tw3 = MeasureText(hint, 26);
            DrawText(hint, (SCREEN_W - tw3) / 2, SCREEN_H / 2 + 54, 26, C_DIM);
        }

        // Milestone notification flash (centred, fades out)
        if (notif_timer > 0.0f) {
            float fade  = std::min(notif_timer, 1.0f);
            unsigned char a = (unsigned char)(fade * 230);
            DrawRectangle(0, SCREEN_H / 2 - 42, SCREEN_W, 84,
                          Color{ 0, 30, 10, (unsigned char)(a / 3) });
            int tw = MeasureText(notif_text.c_str(), 36);
            DrawText(notif_text.c_str(),
                     (SCREEN_W - tw) / 2, SCREEN_H / 2 - 18,
                     36, Color{ 180, 255, 190, a });
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
