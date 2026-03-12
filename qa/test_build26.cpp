// Build 26 — test_build26.cpp
// Tests for new build 26 features:
//   1. scrap_boost_click_delta()
//   2. apply_scrap_boost()
//   3. game_tick: scrap boost decay (step 3)
//   4. game_tick: dynamic scrap rate with scrap_boost > 0 (step 4)
//   5. GameState new fields: scrap_boost, power_deficit_timer
//   6. game_tick: power deficit timer and stasis pod death (step 7)
//   7. save/load round-trip for new fields
//
// All values derived from spec/core-spec.md.
//
// Constants:
//   SCRAP_BOOST_COMPUTE_COST = 5.0
//   SCRAP_BOOST_BASE         = 0.03
//   SCRAP_BOOST_DECAY        = 0.002
//   STASIS_POD_DEATH_THRESHOLD = 30.0
//
// Formula: scrap_boost_click_delta = SCRAP_BOOST_BASE / (1.0 + scrap_boost / SCRAP_BOOST_BASE)
// Formula: scrap rate = robots * (BASE_SCRAP_RATE + compute * COMPUTE_SCRAP_BONUS) * (1 + scrap_boost)

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — scrap_boost_click_delta()
// ===========================================================================

TEST_CASE("scrap_boost_click_delta at scrap_boost=0.0 returns 0.06", "[scrap_boost][delta]") {
    // 0.06 / (1.0 + 0.0 / 0.06) = 0.06 / 1.0 = 0.06
    GameState s = game_init();
    s.scrap_boost = 0.0;
    REQUIRE(scrap_boost_click_delta(s) == Catch::Approx(0.06));
}

TEST_CASE("scrap_boost_click_delta at scrap_boost=0.03 returns 0.04", "[scrap_boost][delta]") {
    // 0.06 / (1.0 + 0.03 / 0.06) = 0.06 / 1.5 = 0.04
    GameState s = game_init();
    s.scrap_boost = 0.03;
    REQUIRE(scrap_boost_click_delta(s) == Catch::Approx(0.04));
}

TEST_CASE("scrap_boost_click_delta at scrap_boost=0.06 returns 0.03", "[scrap_boost][delta]") {
    // 0.06 / (1.0 + 0.06 / 0.06) = 0.06 / 2.0 = 0.03
    GameState s = game_init();
    s.scrap_boost = 0.06;
    REQUIRE(scrap_boost_click_delta(s) == Catch::Approx(0.03));
}

TEST_CASE("scrap_boost_click_delta does not modify state", "[scrap_boost][delta]") {
    GameState s = game_init();
    s.scrap_boost = 0.03;
    scrap_boost_click_delta(s);
    REQUIRE(s.scrap_boost == Catch::Approx(0.03));
}

// ===========================================================================
// Section 2 — apply_scrap_boost() guard conditions
// ===========================================================================

TEST_CASE("apply_scrap_boost returns false when compute.value = 0.0", "[scrap_boost][guard]") {
    GameState s = game_init();
    s.resources.at("compute").value = 0.0;
    REQUIRE(apply_scrap_boost(s) == false);
}

TEST_CASE("apply_scrap_boost returns false when compute.value = 4.9 (below cost)", "[scrap_boost][guard]") {
    // SCRAP_BOOST_COMPUTE_COST = 5.0; 4.9 < 5.0 → false
    GameState s = game_init();
    s.resources.at("compute").value = 4.9;
    REQUIRE(apply_scrap_boost(s) == false);
}

TEST_CASE("apply_scrap_boost does not change state when compute insufficient", "[scrap_boost][guard]") {
    GameState s = game_init();
    s.resources.at("compute").value = 4.9;
    double boost_before   = s.scrap_boost;
    double compute_before = s.resources.at("compute").value;
    apply_scrap_boost(s);
    REQUIRE(s.scrap_boost == Catch::Approx(boost_before));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(compute_before));
}

// ===========================================================================
// Section 3 — apply_scrap_boost() success path
// ===========================================================================

TEST_CASE("apply_scrap_boost returns true when compute.value >= 5.0", "[scrap_boost][success]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    REQUIRE(apply_scrap_boost(s) == true);
}

TEST_CASE("apply_scrap_boost deducts 5.0 from compute.value", "[scrap_boost][success]") {
    // compute was 10.0 → 10.0 - 5.0 = 5.0
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    apply_scrap_boost(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(5.0));
}

TEST_CASE("apply_scrap_boost adds scrap_boost_click_delta to scrap_boost (first call)", "[scrap_boost][success]") {
    // scrap_boost=0.0: delta = 0.06 / 1.0 = 0.06; new boost = 0.0 + 0.06 = 0.06
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    s.scrap_boost = 0.0;
    apply_scrap_boost(s);
    REQUIRE(s.scrap_boost == Catch::Approx(0.06));
}

TEST_CASE("apply_scrap_boost second call applies diminishing return", "[scrap_boost][success]") {
    // First call:  scrap_boost=0.0,  delta=0.06/(1+0/0.06)=0.06 → boost=0.06, compute=10→5
    // Second call: scrap_boost=0.06, delta=0.06/(1+0.06/0.06)=0.03 → boost=0.09, compute=5→0
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    s.scrap_boost = 0.0;
    apply_scrap_boost(s); // boost → 0.06, compute → 5.0
    apply_scrap_boost(s); // boost → 0.09, compute → 0.0
    REQUIRE(s.scrap_boost == Catch::Approx(0.09));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("apply_scrap_boost at exactly cost boundary (compute=5.0) succeeds", "[scrap_boost][success]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    bool result = apply_scrap_boost(s);
    REQUIRE(result == true);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

// ===========================================================================
// Section 4 — GameState new fields initialization
// ===========================================================================

TEST_CASE("game_init: scrap_boost starts at 0.0", "[init][scrap_boost]") {
    GameState s = game_init();
    REQUIRE(s.scrap_boost == Catch::Approx(0.0));
}

TEST_CASE("game_init: power_deficit_timer starts at 0.0", "[init][power_deficit_timer]") {
    GameState s = game_init();
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

// ===========================================================================
// Section 5 — game_tick: scrap boost decay (step 3)
// Formula: scrap_boost = max(0.0, scrap_boost - SCRAP_BOOST_DECAY * delta_seconds)
// SCRAP_BOOST_DECAY = 0.002
// ===========================================================================

TEST_CASE("game_tick: scrap_boost=0.1 decays to 0.098 after delta=1.0", "[tick][scrap_boost][decay]") {
    // max(0.0, 0.1 - 0.002*1.0) = 0.098
    GameState s = game_init();
    s.scrap_boost = 0.1;
    game_tick(s, 1.0);
    REQUIRE(s.scrap_boost == Catch::Approx(0.098));
}

TEST_CASE("game_tick: scrap_boost=0.001 clamps to 0.0 when decay exceeds value", "[tick][scrap_boost][decay]") {
    // max(0.0, 0.001 - 0.002*1.0) = max(0.0, -0.001) = 0.0
    GameState s = game_init();
    s.scrap_boost = 0.001;
    game_tick(s, 1.0);
    REQUIRE(s.scrap_boost == Catch::Approx(0.0));
}

TEST_CASE("game_tick: scrap_boost=0.0 stays 0.0 after tick", "[tick][scrap_boost][decay]") {
    GameState s = game_init();
    s.scrap_boost = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.scrap_boost == Catch::Approx(0.0));
}

// ===========================================================================
// Section 6 — game_tick: dynamic scrap rate with scrap_boost > 0 (step 4)
// Formula: tech_scrap.rate = robots * (BASE_SCRAP_RATE + compute * COMPUTE_SCRAP_BONUS) * (1 + scrap_boost)
// BASE_SCRAP_RATE=0.1, COMPUTE_SCRAP_BONUS=0.02
// Note: delta=0.0 used to avoid boost decay between steps 3 and 4.
// ===========================================================================

TEST_CASE("game_tick: scrap rate with robots=1, compute=0, scrap_boost=0.1 → 0.11/s", "[tick][scrap_boost][rate]") {
    // decay step with delta=0: boost stays 0.1
    // rate = 1 * (0.1 + 0*0.02) * (1.0 + 0.1) = 0.1 * 1.1 = 0.11
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    s.scrap_boost = 0.1;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.11));
}

TEST_CASE("game_tick: scrap rate with robots=2, compute=5, scrap_boost=0.03 → 0.412/s", "[tick][scrap_boost][rate]") {
    // decay step with delta=0: boost stays 0.03
    // rate = 2 * (0.1 + 5*0.02) * (1.0 + 0.03) = 2 * 0.2 * 1.03 = 0.412
    GameState s = game_init();
    s.resources.at("robots").value  = 2.0;
    s.resources.at("compute").value = 5.0;
    s.scrap_boost = 0.03;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.412));
}

TEST_CASE("game_tick: scrap rate with scrap_boost=0.0 matches baseline (multiplier=1.0)", "[tick][scrap_boost][rate]") {
    // scrap_boost=0.0 → multiplier = 1.0 → unaffected
    // rate = 1 * (0.1 + 0) * 1.0 = 0.1
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    s.scrap_boost = 0.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.1));
}

// ===========================================================================
// Section 7 — game_tick: power deficit timer (step 7)
// ===========================================================================

TEST_CASE("game_tick: power_deficit_timer stays 0.0 when power > 0 after advance", "[tick][power_deficit_timer]") {
    // Default state: power=15.0, positive rate → power stays > 0 after advance
    // Step 7: power > 0 → timer = 0.0
    GameState s = game_init();
    s.power_deficit_timer = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("game_tick: power_deficit_timer accumulates when power = 0 after advance", "[tick][power_deficit_timer]") {
    // 5 robots: power.rate = 0.1 - 5*0.2 - 0.05 = -0.95 (negative)
    // power.value=0.0 → after advance: clamp(0 + (-0.95)*1.0, 0, 100) = 0.0
    // Step 7: power <= 0 → timer += 1.0
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.power_deficit_timer == Catch::Approx(1.0));
}

TEST_CASE("game_tick: power_deficit_timer resets to 0.0 when power recovers above 0", "[tick][power_deficit_timer]") {
    // 1 solar panel: power.rate = 0.1 + 0.5 - 0 - 0.05 = 0.55 (positive)
    // power.value=1.0 → after advance: 1.55 > 0
    // timer was 10.0 → resets to 0.0
    GameState s = game_init();
    s.solar_panel_count = 1;
    s.resources.at("power").value = 1.0;
    s.power_deficit_timer = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

// ===========================================================================
// Section 8 — game_tick: stasis pod death (step 7)
// STASIS_POD_DEATH_THRESHOLD = 30.0
// ===========================================================================

TEST_CASE("game_tick: pod dies when power_deficit_timer reaches 30.0", "[tick][pod_death]") {
    // Setup: 5 robots (high drain), power=0, timer=29.0
    // After advance: power still 0 → timer += 1.0 = 30.0 >= 30.0 → pod dies
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 49);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("game_tick: pod_lost event pushed when pod dies", "[tick][pod_death]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "pod_lost")
                 != s.pending_events.end();
    REQUIRE(found);
}

TEST_CASE("game_tick: pod death does not trigger when timer < 30.0", "[tick][pod_death]") {
    // timer=28.9 + 1.0 = 29.9 < 30.0 → no death
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 28.9;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 50);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "pod_lost")
                 != s.pending_events.end();
    REQUIRE_FALSE(found);
}

TEST_CASE("game_tick: pod death does not trigger when stasis_pod_count = 0", "[tick][pod_death]") {
    // All pods already dead — cannot lose more
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.stasis_pod_count = 0;
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0); // unchanged
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "pod_lost")
                 != s.pending_events.end();
    REQUIRE_FALSE(found);
}

TEST_CASE("game_tick: timer resets after pod death enabling second pod to die", "[tick][pod_death]") {
    // First tick (delta=1.0): timer 29→30 → 1st expiry (pod_death_count=0 → loses 1); 50→49, timer=0, pod_death_count=1
    // Second tick (delta=30.0): power still 0 → timer 0→30 → 2nd expiry (pod_death_count=1 → loses 5); 49→44, timer=0, pod_death_count=2
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0);  // first death: loses 1 pod (pod_death_count was 0)
    REQUIRE(s.stasis_pod_count == 49);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
    game_tick(s, 30.0); // second death: loses 5 pods (pod_death_count was 1)
    REQUIRE(s.stasis_pod_count == 44);
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("game_tick: stasis_drain cap uses updated stasis_pod_count after pod death", "[tick][pod_death]") {
    // After pod death: pod_count=49. At total_time=700:
    //   pod_cap = 49 * 2.0 = 98.0; STASIS_DRAIN_CAP=4.0
    //   formula = 0.05 + 0.001*700 = 0.75 → not clamped (0.75 < 4.0)
    // Setup: cause pod death in first tick, then tick at total_time=700
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0); // kills pod (pod_death_count=0 → loses 1); pod_count=49; total_time becomes 1.0
    REQUIRE(s.stasis_pod_count == 49);
    // Now force total_time to 700
    s.total_time = 700.0;
    game_tick(s, 0.0);
    // formula at t=700 = 0.75; min(0.75, 98.0, 4.0) = 0.75 (not clamped)
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.75));
}

// ===========================================================================
// Section 9 — save/load round-trip for new fields
// ===========================================================================

TEST_CASE("round-trip: scrap_boost=0.0 (default) preserved", "[save_load][scrap_boost]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.scrap_boost == Catch::Approx(0.0));
}

TEST_CASE("round-trip: scrap_boost non-default (0.05) preserved", "[save_load][scrap_boost]") {
    GameState s = game_init();
    s.scrap_boost = 0.05;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.scrap_boost == Catch::Approx(0.05));
}

TEST_CASE("round-trip: power_deficit_timer=0.0 (default) preserved", "[save_load][power_deficit_timer]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("round-trip: power_deficit_timer non-default (15.0) preserved", "[save_load][power_deficit_timer]") {
    GameState s = game_init();
    s.power_deficit_timer = 15.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.power_deficit_timer == Catch::Approx(15.0));
}
