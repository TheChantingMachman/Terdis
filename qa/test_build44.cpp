// Build 44 — test_build44.cpp
// New tests for build 44 scope:
//   1. Solar panel cap (SOLAR_PANEL_MAX=20): buy_solar_panel blocked at cap
//   2. Solar panel degradation (solar_decay_accumulator, SOLAR_DECAY_RATE=0.002)
//   3. Progressive human power cost (HUMAN_POWER_BASE=0.5, HUMAN_POWER_SCALE=0.3)
//   4. Phase 2 colony cap (COLONY_PHASE2_MAX_UNITS=5)
//   5. Tech scrap vault (buy_scrap_vault, 2 tiers)
//   6. Robot capacity expansion (buy_robot_capacity)
//   7. Phase 2→3 gate (colony_capacity VALUE >= 10.0, robots VALUE >= 10.0)
//   8. Phase 3 stubs (build_fabricator, build_reactor)
//   9. New GameState field initialization
//   10. Save/load round-trip for new fields
//   11. New events (panel_degraded, vault_expanded, robot_cap_expanded,
//                   fabricator_online, reactor_online)
//
// All values derived from spec/core-spec.md.
//
// Constants:
//   SOLAR_PANEL_MAX        = 20
//   SOLAR_DECAY_RATE       = 0.002
//   HUMAN_POWER_BASE       = 0.5
//   HUMAN_POWER_SCALE      = 0.3
//   COLONY_PHASE2_MAX_UNITS= 5
//   ROBOT_CAP_EXPAND_COST  = 500.0
//   SOLAR_COST_SCALE       = 1.05   (changed from 1.10)
//   STASIS_DRAIN_PER_POD   = 2.0    (changed from 0.4)

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int count_event(const GameState& s, const std::string& id) {
    int n = 0;
    for (const auto& ev : s.pending_events) {
        if (ev == id) n++;
    }
    return n;
}

static const GateCondition* find_gate_condition(const PhaseGate& gate, const std::string& rid) {
    for (const auto& c : gate.conditions) {
        if (c.resource_id == rid) return &c;
    }
    return nullptr;
}

// ===========================================================================
// Section 1 — Solar Panel Cap (SOLAR_PANEL_MAX = 20)
// ===========================================================================

TEST_CASE("buy_solar_panel: returns false when solar_panel_count >= 20 (at cap)", "[solar][cap][build44]") {
    GameState s = game_init();
    s.solar_panel_count = 20; // already at cap
    s.resources.at("tech_scrap").value = 500.0; // more than enough scrap
    REQUIRE(buy_solar_panel(s) == false);
}

TEST_CASE("buy_solar_panel: does not change state when at panel cap", "[solar][cap][build44]") {
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("tech_scrap").value = 500.0;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_solar_panel(s);
    REQUIRE(s.solar_panel_count == 20); // count unchanged
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before)); // scrap unchanged
}

TEST_CASE("buy_solar_panel: succeeds at panel_count=19 when affordable", "[solar][cap][build44]") {
    // cost = 5.0 * 1.05^19 ≈ 12.68; give plenty of scrap
    GameState s = game_init();
    s.solar_panel_count = 19;
    s.resources.at("tech_scrap").value = 500.0;
    REQUIRE(buy_solar_panel(s) == true);
    REQUIRE(s.solar_panel_count == 20);
}

TEST_CASE("buy_solar_panel: cap check takes priority over affordability check", "[solar][cap][build44]") {
    // At cap=20: returns false even with 10000 scrap
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("tech_scrap").value = 10000.0;
    REQUIRE(buy_solar_panel(s) == false);
}

// ===========================================================================
// Section 2 — New GameState Field Initialization
// ===========================================================================

TEST_CASE("game_init: solar_decay_accumulator = 0.0", "[init][build44]") {
    GameState s = game_init();
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("game_init: tech_scrap_vault_tier = 0", "[init][build44]") {
    GameState s = game_init();
    REQUIRE(s.tech_scrap_vault_tier == 0);
}

TEST_CASE("game_init: robots_cap_expanded = false", "[init][build44]") {
    GameState s = game_init();
    REQUIRE(s.robots_cap_expanded == false);
}

TEST_CASE("game_init: fabricator_built = false", "[init][build44]") {
    GameState s = game_init();
    REQUIRE(s.fabricator_built == false);
}

TEST_CASE("game_init: reactor_built = false", "[init][build44]") {
    GameState s = game_init();
    REQUIRE(s.reactor_built == false);
}

// ===========================================================================
// Section 3 — Solar Panel Degradation
//
// Formula: solar_decay_accumulator += SOLAR_DECAY_RATE * (solar_panel_count / SOLAR_PANEL_MAX) * delta
// Rate at max density (20 panels): 0.002 * (20/20) = 0.002/s → fires every 500 seconds
// Rate at half density (10 panels): 0.002 * (10/20) = 0.001/s → fires every 1000 seconds
// ===========================================================================

TEST_CASE("game_tick: solar_decay_accumulator advances at 20 panels (max density)", "[solar][decay][build44]") {
    // At 20 panels (normalized): rate = 0.002 * (20/20) = 0.002/s
    // After 100 seconds (no panel loss): accumulator = 0.002 * (20/20) * 100 = 0.2
    GameState s = game_init();
    s.solar_panel_count = 20;
    // Give power so nothing else interferes with panel count
    s.resources.at("power").value = 100.0;
    game_tick(s, 100.0);
    // 100s at 20 panels: accumulator = 0.002 * (20/20) * 100 = 0.2 < 1.0 → no degradation
    REQUIRE(s.solar_panel_count == 20);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.002 * (20.0 / 20.0) * 100.0));
}

TEST_CASE("game_tick: solar_decay_accumulator does not advance when panels=0", "[solar][decay][build44]") {
    // Rate = 0.005 * (0/20) * delta = 0 → accumulator stays 0
    GameState s = game_init();
    s.solar_panel_count = 0;
    s.solar_decay_accumulator = 0.0;
    game_tick(s, 100.0);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("game_tick: panel degrades when accumulator reaches 1.0 at max density (delta=500)", "[solar][decay][build44]") {
    // At 20 panels (normalized): accumulator += 0.002 * (20/20) * 500 = 1.0 → fires once
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 500.0);
    REQUIRE(s.solar_panel_count == 19); // one panel lost
}

TEST_CASE("game_tick: panel degradation pushes 'panel_degraded' event", "[solar][decay][build44]") {
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    s.pending_events.clear();
    game_tick(s, 500.0);
    REQUIRE(count_event(s, "panel_degraded") == 1);
}

TEST_CASE("game_tick: no degradation just below threshold (delta=499 at 20 panels)", "[solar][decay][build44]") {
    // accumulator = 0.002 * (20/20) * 499 = 0.998 < 1.0
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 499.0);
    REQUIRE(s.solar_panel_count == 20); // no degradation
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.002 * (20.0 / 20.0) * 499.0));
}

TEST_CASE("game_tick: two panels degrade in a single large delta (delta=1000 at 20 panels)", "[solar][decay][build44]") {
    // accumulator = 0.002 * (20/20) * 1000 = 2.0 → fires twice → 2 panels lost
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    s.pending_events.clear();
    game_tick(s, 1000.0);
    REQUIRE(s.solar_panel_count == 18);
    REQUIRE(count_event(s, "panel_degraded") == 2);
}

TEST_CASE("game_tick: degradation triggers recompute_rates (power.rate drops after panel loss)", "[solar][decay][build44]") {
    // SOLAR_POWER_RATE = 0.75; stasis at t=0: 0.05
    // Before: 0.1 + 20*0.75 - 0.05 = 15.05
    // After 1 panel lost (19 panels, t≈0): 0.1 + 19*0.75 - 0.05 = 14.3
    // Pre-load accumulator=1.0 + delta=0 to trigger degradation without advancing time/stasis.
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    s.solar_decay_accumulator = 1.0;
    recompute_rates(s); // 20 panels → power.rate = 0.1 + 15.0 - 0.05 = 15.05
    REQUIRE(s.resources.at("power").rate == Catch::Approx(15.05));
    game_tick(s, 0.0); // accumulator already ≥ 1.0 → loses 1 panel at t=0
    // After recompute with 19 panels: 0.1 + 14.25 - 0.05 = 14.3
    REQUIRE(s.resources.at("power").rate == Catch::Approx(14.3));
}

TEST_CASE("game_tick: accumulator resets to 0.0 when solar_panel_count hits 0", "[solar][decay][build44]") {
    // 1 panel, accumulator=1.0, delta=0 → fires, panels→0, accumulator→0.0 (reset)
    GameState s = game_init();
    s.solar_panel_count = 1;
    s.solar_decay_accumulator = 1.0;
    s.resources.at("power").value = 100.0;
    game_tick(s, 0.0);
    REQUIRE(s.solar_panel_count == 0);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("game_tick: accumulator at half density advances at half rate", "[solar][decay][build44]") {
    // 10 panels (normalized): rate = 0.002 * (10/20) = 0.001/s
    // After 100 seconds: accumulator = 0.002 * (10/20) * 100 = 0.1 < 1.0 → no degradation
    GameState s = game_init();
    s.solar_panel_count = 10;
    s.resources.at("power").value = 100.0;
    game_tick(s, 100.0);
    REQUIRE(s.solar_panel_count == 10); // no degradation
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.002 * (10.0 / 20.0) * 100.0));
}

// ===========================================================================
// Section 4 — Progressive Human Power Cost
//
// Formula: human_power_cost = n * BASE + SCALE * n * (n-1) / 2
//          BASE=0.5, SCALE=0.3
// n=1: 0.5; n=2: 1.3; n=3: 2.4; n=5: 5.5; n=10: 18.5
// ===========================================================================

TEST_CASE("recompute_rates: 1 awoken human → human_power_cost = 0.5", "[rates][humans][build44]") {
    // 1*0.5 + 0.3*1*0/2.0 = 0.5
    // power.rate = 0.1 + 0 - 0 - 0.5 - 0.05 = -0.45
    GameState s = game_init();
    s.awoken_humans = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("recompute_rates: 2 awoken humans → human_power_cost = 1.3", "[rates][humans][build44]") {
    // 2*0.5 + 0.3*2*1/2.0 = 1.0 + 0.3 = 1.3
    // power.rate = 0.1 + 0 - 0 - 1.3 - 0.05 = -1.25
    GameState s = game_init();
    s.awoken_humans = 2;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-1.25));
}

TEST_CASE("recompute_rates: 3 awoken humans → human_power_cost = 2.4", "[rates][humans][build44]") {
    // 3*0.5 + 0.3*3*2/2.0 = 1.5 + 0.9 = 2.4
    // power.rate = 0.1 + 0 - 0 - 2.4 - 0.05 = -2.35
    GameState s = game_init();
    s.awoken_humans = 3;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-2.35));
}

TEST_CASE("recompute_rates: 5 awoken humans → human_power_cost = 5.5", "[rates][humans][build44]") {
    // 5*0.5 + 0.3*5*4/2.0 = 2.5 + 3.0 = 5.5
    // power.rate = 0.1 + 0 - 0 - 5.5 - 0.05 = -5.45
    GameState s = game_init();
    s.awoken_humans = 5;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-5.45));
}

TEST_CASE("recompute_rates: 10 awoken humans → human_power_cost = 18.5", "[rates][humans][build44]") {
    // 10*0.5 + 0.3*10*9/2.0 = 5.0 + 13.5 = 18.5
    // power.rate = 0.1 + 0 - 0 - 18.5 - 0.05 = -18.45
    GameState s = game_init();
    s.awoken_humans = 10;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-18.45));
}

TEST_CASE("recompute_rates: progressive cost with robots and panels (n=5)", "[rates][humans][build44]") {
    // 5 humans: cost=5.5; 3 robots, 4 panels
    // power.rate = 0.1 + 4*0.75 - 3*0.2 - 5.5 - 0.05 = 0.1 + 3.0 - 0.6 - 5.5 - 0.05 = -3.05
    GameState s = game_init();
    s.awoken_humans = 5;
    s.solar_panel_count = 4;
    s.resources.at("robots").value = 3.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-3.05));
}

// ===========================================================================
// Section 5 — Phase 2 Colony Cap (COLONY_PHASE2_MAX_UNITS = 5)
// ===========================================================================

TEST_CASE("buy_colony_unit: returns false in Phase 2 when colony_unit_count >= 5", "[colony][cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.colony_unit_count = 5; // at cap
    s.resources.at("tech_scrap").value = 5000.0; // affordable
    REQUIRE(buy_colony_unit(s) == false);
}

TEST_CASE("buy_colony_unit: does not change state when at Phase 2 cap", "[colony][cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.colony_unit_count = 5;
    s.resources.at("tech_scrap").value = 5000.0;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_colony_unit(s);
    REQUIRE(s.colony_unit_count == 5);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_colony_unit: succeeds in Phase 2 when colony_unit_count < 5 and affordable", "[colony][cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.colony_unit_count = 4; // one below cap
    // cost = 200 * 1.20^4 = 200 * 2.0736 = 414.72
    s.resources.at("tech_scrap").value = 500.0;
    REQUIRE(buy_colony_unit(s) == true);
    REQUIRE(s.colony_unit_count == 5);
}

TEST_CASE("buy_colony_unit: Phase 3 removes colony cap — succeeds at unit_count=5 if affordable", "[colony][cap][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.colony_unit_count = 5; // would fail in Phase 2
    // cost = 200 * 1.20^5 = 200 * 2.48832 ≈ 497.664
    s.resources.at("tech_scrap").value = 1000.0;
    REQUIRE(buy_colony_unit(s) == true);
    REQUIRE(s.colony_unit_count == 6);
}

TEST_CASE("buy_colony_unit: Phase 1 has no colony cap", "[colony][cap][build44]") {
    // In Phase 1, the cap should not apply
    GameState s = game_init();
    s.current_phase = 1;
    s.colony_unit_count = 5; // would fail in Phase 2
    s.resources.at("tech_scrap").value = 5000.0;
    REQUIRE(buy_colony_unit(s) == true);
    REQUIRE(s.colony_unit_count == 6);
}

// ===========================================================================
// Section 6 — Tech Scrap Vault (buy_scrap_vault)
// ===========================================================================

TEST_CASE("buy_scrap_vault: returns false when vault_tier >= 2 (both tiers purchased)", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 2;
    s.resources.at("tech_scrap").value = 5000.0;
    REQUIRE(buy_scrap_vault(s) == false);
}

TEST_CASE("buy_scrap_vault: returns false when cannot afford tier 1 (need 300 scrap)", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 299.9;
    REQUIRE(buy_scrap_vault(s) == false);
}

TEST_CASE("buy_scrap_vault: tier 1 succeeds at exactly 300 scrap", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 300.0;
    REQUIRE(buy_scrap_vault(s) == true);
}

TEST_CASE("buy_scrap_vault: tier 1 deducts 300 scrap", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
}

TEST_CASE("buy_scrap_vault: tier 1 sets tech_scrap.cap = 1000.0", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(1000.0));
}

TEST_CASE("buy_scrap_vault: tier 1 increments tech_scrap_vault_tier to 1", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    buy_scrap_vault(s);
    REQUIRE(s.tech_scrap_vault_tier == 1);
}

TEST_CASE("buy_scrap_vault: tier 1 pushes 'vault_expanded' event", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    buy_scrap_vault(s);
    REQUIRE(count_event(s, "vault_expanded") == 1);
}

TEST_CASE("buy_scrap_vault: returns false when cannot afford tier 2 (need 800 scrap)", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").value = 799.9;
    REQUIRE(buy_scrap_vault(s) == false);
}

TEST_CASE("buy_scrap_vault: tier 2 succeeds at exactly 800 scrap", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").cap = 1000.0; // already upgraded once
    s.resources.at("tech_scrap").value = 800.0;
    REQUIRE(buy_scrap_vault(s) == true);
}

TEST_CASE("buy_scrap_vault: tier 2 deducts 800 scrap", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").cap = 1000.0;
    s.resources.at("tech_scrap").value = 900.0;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
}

TEST_CASE("buy_scrap_vault: tier 2 sets tech_scrap.cap = 2000.0", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").cap = 1000.0;
    s.resources.at("tech_scrap").value = 900.0;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(2000.0));
}

TEST_CASE("buy_scrap_vault: tier 2 increments tech_scrap_vault_tier to 2", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").cap = 1000.0;
    s.resources.at("tech_scrap").value = 900.0;
    buy_scrap_vault(s);
    REQUIRE(s.tech_scrap_vault_tier == 2);
}

TEST_CASE("buy_scrap_vault: tier 2 pushes 'vault_expanded' event", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    s.resources.at("tech_scrap").cap = 1000.0;
    s.resources.at("tech_scrap").value = 900.0;
    buy_scrap_vault(s);
    REQUIRE(count_event(s, "vault_expanded") == 1);
}

TEST_CASE("buy_scrap_vault: does not change state on failure (insufficient scrap)", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 100.0; // below 300 cost
    double cap_before = s.resources.at("tech_scrap").cap;
    buy_scrap_vault(s);
    REQUIRE(s.tech_scrap_vault_tier == 0);
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(cap_before));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
}

TEST_CASE("buy_scrap_vault: does not change state on failure (already at tier 2)", "[vault][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 2;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 5000.0;
    buy_scrap_vault(s);
    REQUIRE(s.tech_scrap_vault_tier == 2);
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(2000.0));
}

// ===========================================================================
// Section 7 — Robot Capacity Expansion (buy_robot_capacity)
// ===========================================================================

TEST_CASE("buy_robot_capacity: returns false when current_phase < 2", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 1;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    REQUIRE(buy_robot_capacity(s) == false);
}

TEST_CASE("buy_robot_capacity: returns false when robots_cap_expanded = true", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.robots_cap_expanded = true;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    REQUIRE(buy_robot_capacity(s) == false);
}

TEST_CASE("buy_robot_capacity: returns false when robots.value < robots.cap (not at full capacity)", "[robot_cap][build44]") {
    // Must have all 5 robots active before expanding cap
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 4.0; // not at cap of 5
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    REQUIRE(buy_robot_capacity(s) == false);
}

TEST_CASE("buy_robot_capacity: returns false when tech_scrap < 500.0", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 499.9;
    REQUIRE(buy_robot_capacity(s) == false);
}

TEST_CASE("buy_robot_capacity: returns true when all conditions met", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    REQUIRE(buy_robot_capacity(s) == true);
}

TEST_CASE("buy_robot_capacity: deducts 500 tech_scrap on success", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    buy_robot_capacity(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
}

TEST_CASE("buy_robot_capacity: sets robots.cap = 10.0 on success", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    buy_robot_capacity(s);
    REQUIRE(s.resources.at("robots").cap == Catch::Approx(10.0));
}

TEST_CASE("buy_robot_capacity: sets robots_cap_expanded = true on success", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    buy_robot_capacity(s);
    REQUIRE(s.robots_cap_expanded == true);
}

TEST_CASE("buy_robot_capacity: pushes 'robot_cap_expanded' event on success", "[robot_cap][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    buy_robot_capacity(s);
    REQUIRE(count_event(s, "robot_cap_expanded") == 1);
}

TEST_CASE("buy_robot_capacity: calls recompute_rates on success", "[robot_cap][build44]") {
    // Sentinel rate that recompute_rates must overwrite
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.resources.at("power").rate = 999.0; // sentinel
    buy_robot_capacity(s);
    // power.rate = 0.1 + 0*0.5 - 5*0.2 - 0 - 0.05 = 0.1 - 1.0 - 0.05 = -0.95
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.95));
}

TEST_CASE("buy_robot_capacity: does not change state on failure", "[robot_cap][build44]") {
    // Phase 1 → returns false, no state change
    GameState s = game_init();
    s.current_phase = 1;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_robot_capacity(s);
    REQUIRE(s.resources.at("robots").cap == Catch::Approx(5.0));
    REQUIRE(s.robots_cap_expanded == false);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

// ===========================================================================
// Section 8 — Phase 2→3 Gate
// ===========================================================================

TEST_CASE("game_init: phase_gates includes Phase 2→3 gate (phase=3)", "[phase_gates][build44]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& g : s.phase_gates) {
        if (g.phase == 3) { found = true; break; }
    }
    REQUIRE(found);
}

TEST_CASE("game_init: Phase 2→3 gate has exactly 2 conditions", "[phase_gates][build44]") {
    GameState s = game_init();
    const PhaseGate* gate3 = nullptr;
    for (const auto& g : s.phase_gates) {
        if (g.phase == 3) { gate3 = &g; break; }
    }
    REQUIRE(gate3 != nullptr);
    REQUIRE(gate3->conditions.size() == 2);
}

TEST_CASE("game_init: Phase 2→3 gate has colony_capacity VALUE >= 10.0 condition", "[phase_gates][build44]") {
    GameState s = game_init();
    const PhaseGate* gate3 = nullptr;
    for (const auto& g : s.phase_gates) {
        if (g.phase == 3) { gate3 = &g; break; }
    }
    REQUIRE(gate3 != nullptr);
    const GateCondition* c = find_gate_condition(*gate3, "colony_capacity");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(10.0));
    REQUIRE(c->condition_field == ConditionField::VALUE);
}

TEST_CASE("game_init: Phase 2→3 gate has robots VALUE >= 10.0 condition", "[phase_gates][build44]") {
    GameState s = game_init();
    const PhaseGate* gate3 = nullptr;
    for (const auto& g : s.phase_gates) {
        if (g.phase == 3) { gate3 = &g; break; }
    }
    REQUIRE(gate3 != nullptr);
    const GateCondition* c = find_gate_condition(*gate3, "robots");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(10.0));
    REQUIRE(c->condition_field == ConditionField::VALUE);
}

TEST_CASE("check_phase_gates: does not advance from Phase 2 when colony_capacity < 10.0", "[phase_gates][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("colony_capacity").value = 8.0; // below 10.0
    s.resources.at("robots").value = 10.0; // robots met
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates: does not advance from Phase 2 when robots.value < 10.0", "[phase_gates][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("colony_capacity").value = 10.0; // colony met
    s.resources.at("robots").value = 5.0; // below 10.0
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates: advances to Phase 3 when both conditions met", "[phase_gates][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("colony_capacity").value = 10.0;
    s.resources.at("robots").value = 10.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 3);
}

TEST_CASE("check_phase_gates: Phase 2→3 gate requires both colony_capacity AND robots conditions", "[phase_gates][build44]") {
    // Neither condition alone is sufficient
    GameState s1 = game_init();
    s1.current_phase = 2;
    s1.resources.at("colony_capacity").value = 10.0;
    s1.resources.at("robots").value = 0.0;
    check_phase_gates(s1);
    REQUIRE(s1.current_phase == 2);

    GameState s2 = game_init();
    s2.current_phase = 2;
    s2.resources.at("colony_capacity").value = 0.0;
    s2.resources.at("robots").value = 10.0;
    check_phase_gates(s2);
    REQUIRE(s2.current_phase == 2);
}

// ===========================================================================
// Section 9 — Phase 3 Stubs (build_fabricator, build_reactor)
// ===========================================================================

TEST_CASE("build_fabricator: returns false when current_phase < 3", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    REQUIRE(build_fabricator(s) == false);
}

TEST_CASE("build_fabricator: returns false when fabricator_built = true", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.fabricator_built = true;
    REQUIRE(build_fabricator(s) == false);
}

TEST_CASE("build_fabricator: returns true in Phase 3 when not yet built", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").value = 5000.0;
    REQUIRE(build_fabricator(s) == true);
}

TEST_CASE("build_fabricator: sets fabricator_built = true on success", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").value = 5000.0;
    build_fabricator(s);
    REQUIRE(s.fabricator_built == true);
}

TEST_CASE("build_fabricator: pushes 'fabricator_online' event on success", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").value = 5000.0;
    build_fabricator(s);
    REQUIRE(count_event(s, "fabricator_online") == 1);
}

TEST_CASE("build_fabricator: does not set fabricator_built on phase < 3 failure", "[fabricator][build44]") {
    GameState s = game_init();
    s.current_phase = 1;
    build_fabricator(s);
    REQUIRE(s.fabricator_built == false);
}

TEST_CASE("build_reactor: returns false when current_phase < 3", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    REQUIRE(build_reactor(s) == false);
}

TEST_CASE("build_reactor: returns false when reactor_built = true", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.reactor_built = true;
    REQUIRE(build_reactor(s) == false);
}

TEST_CASE("build_reactor: returns true in Phase 3 when not yet built", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    REQUIRE(build_reactor(s) == true);
}

TEST_CASE("build_reactor: sets reactor_built = true on success", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    build_reactor(s);
    REQUIRE(s.reactor_built == true);
}

TEST_CASE("build_reactor: pushes 'reactor_online' event on success", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    build_reactor(s);
    REQUIRE(count_event(s, "reactor_online") == 1);
}

TEST_CASE("build_reactor: does not set reactor_built on phase < 3 failure", "[reactor][build44]") {
    GameState s = game_init();
    s.current_phase = 1;
    build_reactor(s);
    REQUIRE(s.reactor_built == false);
}

// ===========================================================================
// Section 10 — Save/Load Round-trip for New Fields
// ===========================================================================

TEST_CASE("round-trip: robots_cap_expanded default (false) preserved", "[save_load][build44]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.robots_cap_expanded == false);
}

TEST_CASE("round-trip: robots_cap_expanded non-default (true) preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.robots_cap_expanded = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.robots_cap_expanded == true);
}

TEST_CASE("round-trip: fabricator_built default (false) preserved", "[save_load][build44]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.fabricator_built == false);
}

TEST_CASE("round-trip: fabricator_built non-default (true) preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.fabricator_built = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.fabricator_built == true);
}

TEST_CASE("round-trip: reactor_built default (false) preserved", "[save_load][build44]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.reactor_built == false);
}

TEST_CASE("round-trip: reactor_built non-default (true) preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.reactor_built = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.reactor_built == true);
}

TEST_CASE("round-trip: solar_decay_accumulator default (0.0) preserved", "[save_load][build44]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("round-trip: solar_decay_accumulator non-default preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.solar_decay_accumulator = 0.73;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.solar_decay_accumulator == Catch::Approx(0.73));
}

TEST_CASE("round-trip: tech_scrap_vault_tier default (0) preserved", "[save_load][build44]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.tech_scrap_vault_tier == 0);
}

TEST_CASE("round-trip: tech_scrap_vault_tier = 1 preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 1;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.tech_scrap_vault_tier == 1);
}

TEST_CASE("round-trip: tech_scrap_vault_tier = 2 preserved", "[save_load][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 2;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.tech_scrap_vault_tier == 2);
}

TEST_CASE("round-trip: Phase 2→3 gate is preserved by save/load", "[save_load][phase_gates][build44]") {
    // After load, the Phase 2→3 gate must still exist and function correctly
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));

    // Verify the phase-3 gate exists after load
    bool found = false;
    for (const auto& g : loaded.phase_gates) {
        if (g.phase == 3) { found = true; break; }
    }
    REQUIRE(found);

    // Verify it functions: set phase=2, satisfy conditions, expect phase→3
    loaded.current_phase = 2;
    loaded.resources.at("colony_capacity").value = 10.0;
    loaded.resources.at("robots").value = 10.0;
    check_phase_gates(loaded);
    REQUIRE(loaded.current_phase == 3);
}

// ===========================================================================
// Section 11 — New Events (integration spot-checks)
// ===========================================================================

TEST_CASE("panel_degraded event pushed by solar degradation tick", "[events][build44]") {
    // 20 panels, accumulator pre-loaded to 1.0, delta=0 → 1 degradation → 1 panel_degraded event
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.solar_decay_accumulator = 1.0;
    s.resources.at("power").value = 100.0;
    s.pending_events.clear();
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "panel_degraded") >= 1);
}

TEST_CASE("vault_expanded event pushed by buy_scrap_vault success", "[events][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    s.pending_events.clear();
    buy_scrap_vault(s);
    REQUIRE(count_event(s, "vault_expanded") == 1);
}

TEST_CASE("robot_cap_expanded event pushed by buy_robot_capacity success", "[events][build44]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.pending_events.clear();
    buy_robot_capacity(s);
    REQUIRE(count_event(s, "robot_cap_expanded") == 1);
}

TEST_CASE("fabricator_online event pushed by build_fabricator success", "[events][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").value = 5000.0;
    s.pending_events.clear();
    build_fabricator(s);
    REQUIRE(count_event(s, "fabricator_online") == 1);
}

TEST_CASE("reactor_online event pushed by build_reactor success", "[events][build44]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    s.pending_events.clear();
    build_reactor(s);
    REQUIRE(count_event(s, "reactor_online") == 1);
}

TEST_CASE("no events pushed by buy_scrap_vault on failure", "[events][build44]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 2; // already maxed
    s.resources.at("tech_scrap").value = 5000.0;
    s.pending_events.clear();
    buy_scrap_vault(s);
    REQUIRE(count_event(s, "vault_expanded") == 0);
}

TEST_CASE("no events pushed by buy_robot_capacity on failure", "[events][build44]") {
    GameState s = game_init();
    s.current_phase = 1; // phase guard fails
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.pending_events.clear();
    buy_robot_capacity(s);
    REQUIRE(count_event(s, "robot_cap_expanded") == 0);
}
