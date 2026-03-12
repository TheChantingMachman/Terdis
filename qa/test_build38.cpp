// Build 38 — test_build38.cpp
// New tests for build 38 scope:
//   1. New GameState fields: awoken_humans (int,0), pod_death_count (int,0), game_over (bool,false)
//   2. awaken_human() function — preconditions, success path, events, population invariant
//   3. Escalating pod deaths — 1st=1 pod, 2nd=5 pods, 3rd+=10 pods; pod_death_count increments
//   4. game_over condition — triggered when stasis_pod_count+awoken_humans==0 after pod death
//   5. game_tick step 0: early return if game_over==true
//   6. HUMAN_POWER_DRAIN in recompute_rates: -awoken_humans * 1.0
//   7. Effective workers in scrap rate: robots.value + awoken_humans * HUMAN_SCRAP_FACTOR (0.5)
//   8. Save/load round-trip for new fields
//   9. New events: "pod_lost", "human_awakened", "game_over"
//
// All values derived from spec/core-spec.md.
//
// New constants:
//   HUMAN_SCRAP_FACTOR = 0.5
//   HUMAN_POWER_DRAIN  = 1.0
//
// Changed constants:
//   STASIS_DRAIN_PER_POD = 0.4 (cap = 50*0.4 = 20.0)
//   COLONY_CAPACITY_PER_UNIT = 2
//   ROBOT_COMPUTE_DRAIN = 0.5

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

// Helper: set state to phase 2 with colony_capacity > 0 for awaken_human tests
static GameState make_phase2_state() {
    GameState s = game_init();
    s.current_phase = 2;
    // Give enough colony capacity and at least 1 pod
    s.resources.at("colony_capacity").value = 10.0;
    s.stasis_pod_count = 50;
    s.awoken_humans = 0;
    return s;
}

// ===========================================================================
// Section 1 — New GameState field initialization
// ===========================================================================

TEST_CASE("game_init: awoken_humans = 0", "[init][build38]") {
    GameState s = game_init();
    REQUIRE(s.awoken_humans == 0);
}

TEST_CASE("game_init: pod_death_count = 0", "[init][build38]") {
    GameState s = game_init();
    REQUIRE(s.pod_death_count == 0);
}

TEST_CASE("game_init: game_over = false", "[init][build38]") {
    GameState s = game_init();
    REQUIRE(s.game_over == false);
}

// ===========================================================================
// Section 2 — awaken_human(): precondition guards
// ===========================================================================

TEST_CASE("awaken_human: returns false when current_phase < 2 (phase 1)", "[awaken_human][build38]") {
    GameState s = game_init();
    REQUIRE(s.current_phase == 1);
    s.resources.at("colony_capacity").value = 20.0; // capacity available
    s.stasis_pod_count = 10;
    REQUIRE(awaken_human(s) == false);
}

TEST_CASE("awaken_human: returns false when stasis_pod_count = 0", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    s.stasis_pod_count = 0;
    REQUIRE(awaken_human(s) == false);
}

TEST_CASE("awaken_human: returns false when colony_capacity.value <= awoken_humans (no free capacity)", "[awaken_human][build38]") {
    // colony_capacity.value=5.0; awoken_humans=5 → (int)5 <= 5 → no room
    GameState s = make_phase2_state();
    s.resources.at("colony_capacity").value = 5.0;
    s.awoken_humans = 5;
    REQUIRE(awaken_human(s) == false);
}

TEST_CASE("awaken_human: returns false when colony_capacity.value < awoken_humans", "[awaken_human][build38]") {
    // colony_capacity.value=3.0; awoken_humans=4 → no room (int)3 <= 4
    GameState s = make_phase2_state();
    s.resources.at("colony_capacity").value = 3.0;
    s.awoken_humans = 4;
    REQUIRE(awaken_human(s) == false);
}

TEST_CASE("awaken_human: returns false when game_over = true", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    s.game_over = true;
    s.stasis_pod_count = 0; // game_over implies no pods remain
    REQUIRE(awaken_human(s) == false);
}

// ===========================================================================
// Section 3 — awaken_human(): success path
// ===========================================================================

TEST_CASE("awaken_human: returns true when all preconditions met", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    // colony_capacity=10, awoken_humans=0, stasis_pod_count=50, phase=2
    REQUIRE(awaken_human(s) == true);
}

TEST_CASE("awaken_human: decrements stasis_pod_count by 1 on success", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    REQUIRE(s.stasis_pod_count == 50);
    awaken_human(s);
    REQUIRE(s.stasis_pod_count == 49);
}

TEST_CASE("awaken_human: increments awoken_humans by 1 on success", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    REQUIRE(s.awoken_humans == 0);
    awaken_human(s);
    REQUIRE(s.awoken_humans == 1);
}

TEST_CASE("awaken_human: pushes 'human_awakened' event on success", "[awaken_human][build38]") {
    GameState s = make_phase2_state();
    awaken_human(s);
    REQUIRE(count_event(s, "human_awakened") == 1);
}

TEST_CASE("awaken_human: calls recompute_rates — power.rate includes HUMAN_POWER_DRAIN", "[awaken_human][build38]") {
    // With 1 awoken human (no robots, no panels, stasis_drain=0.05):
    // power.rate = 0.1 + 0*0.5 - 0*0.2 - cost(1) - 0.05 = 0.1 - 0.5 - 0.05 = -0.45
    GameState s = make_phase2_state();
    s.resources.at("power").rate = 999.0; // sentinel; must be overwritten by recompute
    awaken_human(s);
    REQUIRE(s.awoken_humans == 1);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("awaken_human: population invariant — stasis_pod_count + awoken_humans stays constant",
          "[awaken_human][build38]") {
    // Start: 50 pods, 0 awoken → total living = 50
    // After awaken: 49 pods, 1 awoken → total living = 50 (unchanged)
    GameState s = make_phase2_state();
    int total_before = s.stasis_pod_count + s.awoken_humans;
    awaken_human(s);
    int total_after = s.stasis_pod_count + s.awoken_humans;
    REQUIRE(total_after == total_before);
}

TEST_CASE("awaken_human: sequential calls correctly track population", "[awaken_human][build38]") {
    // Awaken 3 humans; each moves 1 from pods to awoken
    GameState s = make_phase2_state();
    awaken_human(s);
    awaken_human(s);
    awaken_human(s);
    REQUIRE(s.stasis_pod_count == 47);
    REQUIRE(s.awoken_humans == 3);
    REQUIRE(s.stasis_pod_count + s.awoken_humans == 50);
}

// ===========================================================================
// Section 4 — HUMAN_POWER_DRAIN in recompute_rates
//
// power.rate = 0.1 + solar*0.5 - robots*0.2 - human_power_cost(n) - stasis_drain.value
// human_power_cost(n) = n*HUMAN_POWER_BASE + HUMAN_POWER_SCALE*n*(n-1)/2
//                     = n*0.5 + 0.3*n*(n-1)/2
// ===========================================================================

TEST_CASE("recompute_rates: 1 awoken human, 0 robots, 0 panels → power.rate = -0.45",
          "[rates][humans][build38]") {
    // cost(1) = 1*0.5 + 0.3*1*0/2.0 = 0.5
    // 0.1 + 0*0.5 - 0*0.2 - 0.5 - 0.05 = -0.45
    GameState s = game_init();
    s.awoken_humans = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("recompute_rates: 2 awoken humans, 1 solar panel → power.rate = -0.50",
          "[rates][humans][build38]") {
    // cost(2) = 2*0.5 + 0.3*2*1/2.0 = 1.0 + 0.3 = 1.3
    // 0.1 + 1*0.75 - 0*0.2 - 1.3 - 0.05 = 0.1 + 0.75 - 1.3 - 0.05 = -0.50
    GameState s = game_init();
    s.awoken_humans = 2;
    s.solar_panel_count = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.50));
}

TEST_CASE("recompute_rates: 0 awoken humans does not affect power.rate", "[rates][humans][build38]") {
    // Baseline: 0.1 + 0 - 0 - 0.05 = 0.05
    GameState s = game_init();
    s.awoken_humans = 0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.05));
}

TEST_CASE("recompute_rates: 3 awoken humans, 2 robots, 2 panels",
          "[rates][humans][build38]") {
    // cost(3) = 3*0.5 + 0.3*3*2/2.0 = 1.5 + 0.9 = 2.4
    // 0.1 + 2*0.75 - 2*0.2 - 2.4 - 0.05 = 0.1 + 1.5 - 0.4 - 2.4 - 0.05 = -1.25
    GameState s = game_init();
    s.awoken_humans = 3;
    s.solar_panel_count = 2;
    s.resources.at("robots").value = 2.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-1.25));
}

// ===========================================================================
// Section 5 — Effective workers in scrap rate
//
// effective_workers = robots.value + awoken_humans * HUMAN_SCRAP_FACTOR (0.5)
// tech_scrap.rate = effective_workers * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS)
//                  * (1.0 + scrap_boost)
// BASE_SCRAP_RATE=0.1, COMPUTE_SCRAP_BONUS=0.02
// ===========================================================================

TEST_CASE("game_tick: 1 robot + 2 humans → effective_workers=2.0, rate with compute=5",
          "[tick][scrap][humans][build38]") {
    // effective_workers = 1 + 2*0.5 = 2.0
    // rate = 2.0 * (0.1 + 5*0.02) * 1.0 = 2.0 * 0.2 = 0.4
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.awoken_humans = 2;
    s.resources.at("compute").value = 5.0;
    game_tick(s, 0.0); // delta=0 to avoid compute drain
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.4));
}

TEST_CASE("game_tick: 5 robots + 0 humans → effective_workers=5.0, rate with compute=20",
          "[tick][scrap][humans][build38]") {
    // effective_workers = 5 + 0*0.5 = 5.0
    // rate = 5.0 * (0.1 + 20*0.02) * 1.0 = 5.0 * 0.5 = 2.5
    GameState s = game_init();
    s.resources.at("robots").value  = 5.0;
    s.awoken_humans = 0;
    s.resources.at("compute").value = 20.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(2.5));
}

TEST_CASE("game_tick: 0 robots + 3 humans → effective_workers=1.5, rate with compute=0",
          "[tick][scrap][humans][build38]") {
    // effective_workers = 0 + 3*0.5 = 1.5
    // rate = 1.5 * (0.1 + 0*0.02) * 1.0 = 1.5 * 0.1 = 0.15
    GameState s = game_init();
    s.resources.at("robots").value  = 0.0;
    s.awoken_humans = 3;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.15));
}

TEST_CASE("game_tick: 0 robots + 0 humans → effective_workers=0, scrap rate=0",
          "[tick][scrap][humans][build38]") {
    // effective_workers = 0; rate = 0.0 regardless of compute
    GameState s = game_init();
    s.resources.at("robots").value  = 0.0;
    s.awoken_humans = 0;
    s.resources.at("compute").value = 50.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.0));
}

// ===========================================================================
// Section 6 — Escalating pod deaths
//
// When power_deficit_timer >= 30.0 and stasis_pod_count > 0:
//   1st event (pod_death_count==0): loses 1 pod, pod_death_count→1
//   2nd event (pod_death_count==1): loses 5 pods, pod_death_count→2
//   3rd+ event (pod_death_count>=2): loses 10 pods, pod_death_count→3+
//   Clamp: pods_to_lose = min(pods_to_lose, stasis_pod_count)
// ===========================================================================

static GameState make_pod_death_setup() {
    // State ready to trigger pod death on next game_tick(s, 1.0):
    // 5 robots → power drain; power=0 → timer accumulates
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0;
    return s;
}

TEST_CASE("game_tick pod death: 1st expiry (pod_death_count=0) loses 1 pod", "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    REQUIRE(s.pod_death_count == 0);
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 49); // 50 - 1
    REQUIRE(s.pod_death_count == 1);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("game_tick pod death: 2nd expiry (pod_death_count=1) loses 5 pods", "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    s.pod_death_count = 1;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 45); // 50 - 5
    REQUIRE(s.pod_death_count == 2);
}

TEST_CASE("game_tick pod death: 3rd expiry (pod_death_count=2) loses 10 pods", "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    s.pod_death_count = 2;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 40); // 50 - 10
    REQUIRE(s.pod_death_count == 3);
}

TEST_CASE("game_tick pod death: 4th expiry (pod_death_count=3) loses 10 pods", "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    s.pod_death_count = 3;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 40); // 50 - 10
    REQUIRE(s.pod_death_count == 4);
}

TEST_CASE("game_tick pod death: clamp — cannot lose more pods than remain (1st expiry, 0 pods)",
          "[pod_death][build38]") {
    // pod_death_count=0 (1st expiry → would lose 1); only 0 pods remain → clamp to 0
    // But stasis_pod_count=0 → pod death does not trigger (guard in spec)
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 0;
    s.pod_death_count = 0;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0); // unchanged: no pods to lose
}

TEST_CASE("game_tick pod death: clamp — 2nd expiry with only 3 pods → loses 3 not 5",
          "[pod_death][build38]") {
    // pod_death_count=1 (2nd expiry → would lose 5); only 3 pods → clamped to 3
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 3;
    s.pod_death_count = 1;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0); // 3 - min(5,3) = 3 - 3 = 0
    REQUIRE(s.pod_death_count == 2);
}

TEST_CASE("game_tick pod death: clamp — 3rd expiry with only 7 pods → loses 7 not 10",
          "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 7;
    s.pod_death_count = 2;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0); // 7 - min(10,7) = 7 - 7 = 0
}

TEST_CASE("game_tick pod death: pod_lost event pushed each time", "[pod_death][build38]") {
    GameState s = make_pod_death_setup();
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "pod_lost") == 1);
}

// ===========================================================================
// Section 7 — game_over condition
//
// When stasis_pod_count + awoken_humans == 0 after pod death:
//   game_over = true, "game_over" event pushed
// ===========================================================================

TEST_CASE("game_tick: game_over set when all humans die (stasis_pod_count+awoken_humans==0)",
          "[game_over][build38]") {
    // 1st expiry (pod_death_count=0) loses 1 pod; start with 1 pod, 0 awoken
    // After death: 0+0=0 → game_over=true
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 1;
    s.awoken_humans = 0;
    s.pod_death_count = 0;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == true);
}

TEST_CASE("game_tick: game_over event pushed when all humans die", "[game_over][build38]") {
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 1;
    s.awoken_humans = 0;
    s.pod_death_count = 0;
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "game_over") == 1);
}

TEST_CASE("game_tick: game_over not set when pods remain after death", "[game_over][build38]") {
    // 1st expiry (pod_death_count=0) loses 1 pod; start with 50 pods → 49 remain
    GameState s = make_pod_death_setup();
    REQUIRE(s.stasis_pod_count == 50);
    game_tick(s, 1.0);
    REQUIRE(s.game_over == false);
}

TEST_CASE("game_tick: game_over not set when awoken humans survive even if pods gone",
          "[game_over][build38]") {
    // 2nd expiry (pod_death_count=1) loses 5 pods; 5 pods → 0 pods; but 3 awoken_humans → total=3 > 0
    GameState s = make_pod_death_setup();
    s.stasis_pod_count = 5;
    s.awoken_humans = 3;
    s.pod_death_count = 1;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.awoken_humans == 3);
    REQUIRE(s.game_over == false);
}

// ===========================================================================
// Section 8 — game_tick step 0: early return when game_over == true
//
// No resource updates, no milestone checks, no time advancement
// ===========================================================================

TEST_CASE("game_tick: returns immediately when game_over = true (no time advance)",
          "[game_over][tick][build38]") {
    GameState s = game_init();
    s.game_over = true;
    s.total_time = 0.0;
    game_tick(s, 1.0);
    // total_time should not change if early return is working
    REQUIRE(s.total_time == Catch::Approx(0.0));
}

TEST_CASE("game_tick: returns immediately when game_over = true (no resource updates)",
          "[game_over][tick][build38]") {
    GameState s = game_init();
    s.game_over = true;
    // Give robots so power would normally drain
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    double power_before = s.resources.at("power").value;
    game_tick(s, 10.0);
    // Power should not have changed (early return)
    REQUIRE(s.resources.at("power").value == Catch::Approx(power_before));
}

TEST_CASE("game_tick: returns immediately when game_over = true (no milestone events)",
          "[game_over][tick][build38]") {
    GameState s = game_init();
    s.game_over = true;
    s.resources.at("compute").value = 5.0; // would trigger first_compute milestone
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "first_compute") == 0);
}

// ===========================================================================
// Section 9 — Save/load round-trip for new fields
// ===========================================================================

TEST_CASE("round-trip: awoken_humans default (0) preserved", "[save_load][build38]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.awoken_humans == 0);
}

TEST_CASE("round-trip: awoken_humans non-default (5) preserved", "[save_load][build38]") {
    GameState s = game_init();
    s.awoken_humans = 5;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.awoken_humans == 5);
}

TEST_CASE("round-trip: pod_death_count default (0) preserved", "[save_load][build38]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.pod_death_count == 0);
}

TEST_CASE("round-trip: pod_death_count non-default (3) preserved", "[save_load][build38]") {
    GameState s = game_init();
    s.pod_death_count = 3;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.pod_death_count == 3);
}

TEST_CASE("round-trip: game_over default (false) preserved", "[save_load][build38]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.game_over == false);
}

TEST_CASE("round-trip: game_over = true preserved", "[save_load][build38]") {
    GameState s = game_init();
    s.game_over = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.game_over == true);
}

TEST_CASE("round-trip: game_tick honours restored game_over=true (early return)",
          "[save_load][game_over][build38]") {
    // After load, game_over=true must be respected by game_tick
    GameState s = game_init();
    s.game_over = true;
    GameState loaded = load_game(save_game(s));
    double time_before = loaded.total_time;
    game_tick(loaded, 1.0);
    REQUIRE(loaded.total_time == Catch::Approx(time_before)); // no advance
}

// ===========================================================================
// Section 10 — awaken_human() integration with scrap rate
//
// Awakened humans contribute to effective_workers, which drives scrap production.
// ===========================================================================

TEST_CASE("awaken_human + game_tick: awakened human contributes to scrap rate",
          "[awaken_human][integration][build38]") {
    // After awakening 2 humans: effective_workers = 0 + 2*0.5 = 1.0
    // With compute=0: rate = 1.0 * 0.1 = 0.1
    GameState s = make_phase2_state();
    s.resources.at("robots").value  = 0.0;
    s.resources.at("compute").value = 0.0;
    awaken_human(s); // awoken_humans=1
    awaken_human(s); // awoken_humans=2
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.1));
}

TEST_CASE("awaken_human + game_tick: power.rate reflects HUMAN_POWER_DRAIN after tick",
          "[awaken_human][integration][build38]") {
    // After awakening 1 human (no robots, no panels, stasis_drain≈0.05 at t=0):
    // power.rate = 0.1 - cost(1) - 0.05 = 0.1 - 0.5 - 0.05 = -0.45
    GameState s = make_phase2_state();
    awaken_human(s);
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

// ===========================================================================
// Section 11 — Escalating death integration: sequential deaths with timer reset
// ===========================================================================

TEST_CASE("game_tick: three sequential pod-death events with correct escalation",
          "[pod_death][integration][build38]") {
    // Trigger 3 pod deaths in sequence:
    //   Event 1 (pod_death_count=0): loses 1; 50→49; pod_death_count=1
    //   Event 2 (pod_death_count=1): loses 5; 49→44; pod_death_count=2
    //   Event 3 (pod_death_count=2): loses 10; 44→34; pod_death_count=3
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;

    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0); // event 1: loses 1
    REQUIRE(s.stasis_pod_count == 49);
    REQUIRE(s.pod_death_count == 1);

    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0); // event 2: loses 5
    REQUIRE(s.stasis_pod_count == 44);
    REQUIRE(s.pod_death_count == 2);

    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0); // event 3: loses 10
    REQUIRE(s.stasis_pod_count == 34);
    REQUIRE(s.pod_death_count == 3);
}
