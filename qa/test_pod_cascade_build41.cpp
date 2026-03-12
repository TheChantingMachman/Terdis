// Build 41 — test_pod_cascade_build41.cpp
// Hardened tests for stasis pod death cascade and game_over edge cases.
//
// Scope:
//   1. Full escalation cascade: 1→5→10 across all 7 events (50 pods → 0, game_over)
//   2. game_over triggered precisely by 2nd expiry (5 pods + 0 awoken = 0 total)
//   3. game_over triggered precisely by 3rd expiry (10 pods + 0 awoken = 0 total)
//   4. game_over triggered by clamped 3rd+ expiry (fewer pods than 10)
//   5. game_over NOT triggered when awoken_humans keeps total living > 0 after pod wipe
//   6. game_tick fully inert after game_over — pod_death_count, stasis_pod_count,
//      power_deficit_timer, and pod_lost event all unchanged
//   7. Stasis drain cap adjusts with reduced stasis_pod_count after deaths
//   8. power_deficit_timer resets correctly across back-to-back expiry events
//   9. pod_lost event count matches death event count; game_over event emitted exactly once
//
// All values derived from spec/core-spec.md.
//
// Key constants (spec/core-spec.md):
//   STASIS_POD_DEATH_THRESHOLD = 30.0
//   STASIS_DRAIN_BASE          = 0.05
//   STASIS_DRAIN_GROWTH        = 0.003
//   STASIS_DRAIN_PER_POD       = 0.4
//   BASE_POWER_RATE            = 0.1
//   ROBOT_POWER_DRAIN          = 0.2  (per robot)
//   STASIS_POD_COUNT           = 50   (initial)

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

// Setup: power=0, 5 robots (heavy drain), timer ready to fire on next tick of delta>=1.0.
// Power stays at 0 (clamped) throughout because net rate is deeply negative.
static GameState make_cascade_base() {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 29.0; // one more second pushes to >=30
    return s;
}

// Fire one expiry event by setting the timer to 29.0 and ticking 1 second.
static void fire_one_expiry(GameState& s) {
    s.power_deficit_timer = 29.0;
    game_tick(s, 1.0);
}

// ===========================================================================
// Section 1 — Full 7-event cascade: 50 pods → 0 (game_over)
//
// Escalation schedule from spec:
//   Event 1 (pod_death_count=0): lose 1  → 50→49, count→1
//   Event 2 (pod_death_count=1): lose 5  → 49→44, count→2
//   Event 3 (pod_death_count=2): lose 10 → 44→34, count→3
//   Event 4 (pod_death_count=3): lose 10 → 34→24, count→4
//   Event 5 (pod_death_count=4): lose 10 → 24→14, count→5
//   Event 6 (pod_death_count=5): lose 10 → 14→4,  count→6
//   Event 7 (pod_death_count=6): would lose 10, clamped to 4 → 4→0, count→7, game_over
// ===========================================================================

TEST_CASE("pod cascade: full 7-event run from 50 pods to 0 reaches game_over",
          "[pod_death][cascade][build41]") {
    GameState s = make_cascade_base();
    // 0 awoken_humans throughout so total living = stasis_pod_count only

    fire_one_expiry(s); // event 1: lose 1
    REQUIRE(s.stasis_pod_count == 49);
    REQUIRE(s.pod_death_count == 1);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 2: lose 5
    REQUIRE(s.stasis_pod_count == 44);
    REQUIRE(s.pod_death_count == 2);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 3: lose 10
    REQUIRE(s.stasis_pod_count == 34);
    REQUIRE(s.pod_death_count == 3);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 4: lose 10
    REQUIRE(s.stasis_pod_count == 24);
    REQUIRE(s.pod_death_count == 4);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 5: lose 10
    REQUIRE(s.stasis_pod_count == 14);
    REQUIRE(s.pod_death_count == 5);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 6: lose 10
    REQUIRE(s.stasis_pod_count == 4);
    REQUIRE(s.pod_death_count == 6);
    REQUIRE(s.game_over == false);

    fire_one_expiry(s); // event 7: would lose 10, clamped to 4
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.pod_death_count == 7);
    REQUIRE(s.awoken_humans == 0);
    REQUIRE(s.game_over == true);
}

TEST_CASE("pod cascade: game_over event pushed on final 7th expiry",
          "[pod_death][cascade][build41]") {
    GameState s = make_cascade_base();
    for (int i = 0; i < 7; ++i) {
        s.pending_events.clear();
        fire_one_expiry(s);
    }
    // After the final expiry that reaches 0 pods, game_over event must be present
    REQUIRE(count_event(s, "game_over") == 1);
}

TEST_CASE("pod cascade: pod_lost pushed exactly once per expiry event (7 events = 7 pod_lost)",
          "[pod_death][cascade][build41]") {
    GameState s = make_cascade_base();
    int total_pod_lost = 0;
    for (int i = 0; i < 7; ++i) {
        s.pending_events.clear();
        fire_one_expiry(s);
        total_pod_lost += count_event(s, "pod_lost");
    }
    REQUIRE(total_pod_lost == 7);
}

TEST_CASE("pod cascade: power_deficit_timer is 0.0 after each expiry event",
          "[pod_death][cascade][build41]") {
    GameState s = make_cascade_base();
    for (int i = 0; i < 3; ++i) {
        fire_one_expiry(s);
        // Spec step 6: timer += delta → 30.0 → threshold met → death fires → timer = 0.0
        // No further accumulation in the same step, so post-tick timer = 0.0
        REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
    }
}

// ===========================================================================
// Section 2 — game_over triggered precisely: 2nd expiry empties pods
//
// Scenario: pod_death_count=1, stasis_pod_count=5, awoken_humans=0
// 2nd expiry: lose 5 → stasis_pod_count=0; 0+0=0 → game_over=true
// ===========================================================================

TEST_CASE("game_over: 2nd expiry with exactly 5 pods and 0 awoken triggers game_over",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 5;
    s.awoken_humans    = 0;
    s.pod_death_count  = 1;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == true);
}

TEST_CASE("game_over: 2nd expiry with 5 pods and 0 awoken pushes game_over event",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 5;
    s.awoken_humans    = 0;
    s.pod_death_count  = 1;
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "game_over") == 1);
}

// ===========================================================================
// Section 3 — game_over triggered precisely: 3rd expiry empties pods
//
// Scenario: pod_death_count=2, stasis_pod_count=10, awoken_humans=0
// 3rd expiry: lose 10 → stasis_pod_count=0; 0+0=0 → game_over=true
// ===========================================================================

TEST_CASE("game_over: 3rd expiry with exactly 10 pods and 0 awoken triggers game_over",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 10;
    s.awoken_humans    = 0;
    s.pod_death_count  = 2;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == true);
}

TEST_CASE("game_over: 4th expiry (pod_death_count=3) with exactly 10 pods triggers game_over",
          "[game_over][build41]") {
    // pod_death_count >= 2 → all events lose 10; 4th is same as 3rd
    GameState s = make_cascade_base();
    s.stasis_pod_count = 10;
    s.awoken_humans    = 0;
    s.pod_death_count  = 3;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == true);
}

// ===========================================================================
// Section 4 — game_over triggered by clamped 3rd+ expiry
//
// Scenario: pod_death_count=2, stasis_pod_count=4, awoken_humans=0
// 3rd expiry would lose 10 → clamped to 4 → 0 pods; 0+0=0 → game_over=true
// ===========================================================================

TEST_CASE("game_over: clamped 3rd expiry (4 pods, would lose 10) triggers game_over",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 4;
    s.awoken_humans    = 0;
    s.pod_death_count  = 2;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == true);
}

TEST_CASE("game_over: clamped 3rd expiry (4 pods) correctly decrements to 0",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 4;
    s.awoken_humans    = 0;
    s.pod_death_count  = 2;
    game_tick(s, 1.0);
    // pods_to_lose = min(10, 4) = 4; 4 - 4 = 0
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.pod_death_count == 3);
}

// ===========================================================================
// Section 5 — game_over NOT triggered when awoken_humans keep total living > 0
//
// Population invariant: game_over requires stasis_pod_count + awoken_humans == 0
// ===========================================================================

TEST_CASE("game_over NOT triggered: 2nd expiry, 5 pods → 0, but 1 awoken_human remains",
          "[game_over][build41]") {
    // 2nd expiry loses 5; 5 pods → 0; total = 0 + 1 = 1 → game_over = false
    GameState s = make_cascade_base();
    s.stasis_pod_count = 5;
    s.awoken_humans    = 1;
    s.pod_death_count  = 1;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.awoken_humans == 1);
    REQUIRE(s.game_over == false);
}

TEST_CASE("game_over NOT triggered: 2nd expiry, 5 pods → 0, but 1 awoken — no game_over event",
          "[game_over][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 5;
    s.awoken_humans    = 1;
    s.pod_death_count  = 1;
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "game_over") == 0);
}

TEST_CASE("game_over NOT triggered: 3rd expiry, 10 pods → 0, but 2 awoken_humans remain",
          "[game_over][build41]") {
    // 3rd expiry loses 10; 10 pods → 0; total = 0 + 2 = 2 → game_over = false
    GameState s = make_cascade_base();
    s.stasis_pod_count = 10;
    s.awoken_humans    = 2;
    s.pod_death_count  = 2;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.awoken_humans == 2);
    REQUIRE(s.game_over == false);
}

TEST_CASE("game_over NOT triggered: clamped event empties pods, but awoken_humans > 0",
          "[game_over][build41]") {
    // pod_death_count=4, stasis_pod_count=6, awoken_humans=3
    // 5th expiry: would lose 10, clamped to 6 → 0 pods; 0+3=3 → game_over=false
    GameState s = make_cascade_base();
    s.stasis_pod_count = 6;
    s.awoken_humans    = 3;
    s.pod_death_count  = 4;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 0);
    REQUIRE(s.game_over == false);
}

// ===========================================================================
// Section 6 — game_tick fully inert after game_over
//
// Build 38 covers: no time advance, no resource update (power), no milestone event.
// Build 41 adds: pod_death_count unchanged, stasis_pod_count unchanged,
//                power_deficit_timer unchanged, no pod_lost event pushed.
// ===========================================================================

TEST_CASE("game_tick fully inert after game_over: stasis_pod_count not decremented",
          "[game_over][inert][build41]") {
    // game_over=true; pod deficit would normally fire; pods must stay unchanged
    GameState s = game_init();
    s.game_over = true;
    s.stasis_pod_count    = 3;
    s.pod_death_count     = 0;
    s.power_deficit_timer = 29.0;
    s.resources.at("power").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.stasis_pod_count == 3); // unchanged
}

TEST_CASE("game_tick fully inert after game_over: pod_death_count not incremented",
          "[game_over][inert][build41]") {
    GameState s = game_init();
    s.game_over = true;
    s.stasis_pod_count    = 3;
    s.pod_death_count     = 1;
    s.power_deficit_timer = 29.0;
    s.resources.at("power").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.pod_death_count == 1); // unchanged
}

TEST_CASE("game_tick fully inert after game_over: power_deficit_timer not advanced",
          "[game_over][inert][build41]") {
    GameState s = game_init();
    s.game_over = true;
    s.power_deficit_timer = 5.0;
    s.resources.at("power").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.power_deficit_timer == Catch::Approx(5.0)); // unchanged
}

TEST_CASE("game_tick fully inert after game_over: no pod_lost event pushed",
          "[game_over][inert][build41]") {
    GameState s = game_init();
    s.game_over = true;
    s.stasis_pod_count    = 1;
    s.pod_death_count     = 0;
    s.power_deficit_timer = 29.0;
    s.resources.at("power").value = 0.0;
    s.pending_events.clear();
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "pod_lost") == 0);
}

TEST_CASE("game_tick fully inert after game_over: no game_over event re-pushed",
          "[game_over][inert][build41]") {
    // game_over is already true; further ticks must not push additional game_over events
    GameState s = game_init();
    s.game_over = true;
    s.pending_events.clear();
    game_tick(s, 1.0);
    REQUIRE(count_event(s, "game_over") == 0);
}

TEST_CASE("game_tick fully inert after game_over: scrap_boost not decayed",
          "[game_over][inert][build41]") {
    GameState s = game_init();
    s.game_over   = true;
    s.scrap_boost = 0.02;
    game_tick(s, 1.0);
    REQUIRE(s.scrap_boost == Catch::Approx(0.02)); // unchanged; no decay step
}

// ===========================================================================
// Section 7 — Stasis drain cap adjusts with reduced stasis_pod_count
//
// Formula: stasis_drain_value = min(STASIS_DRAIN_BASE + STASIS_DRAIN_GROWTH * total_time,
//                                   stasis_pod_count * STASIS_DRAIN_PER_POD)
// After pod deaths, stasis_pod_count < 50, so cap < 20.0.
// ===========================================================================

TEST_CASE("stasis_drain cap: 1 surviving pod has cap 2.0 at t=200 (unclamped)",
          "[stasis_drain][pod_death][build41]") {
    // stasis_pod_count=1 → pod_cap = 1 * 2.0 = 2.0; STASIS_DRAIN_CAP=4.0
    // formula at t=200: 0.05 + 0.001*200 = 0.25 → NOT clamped (0.25 < 2.0)
    GameState s = game_init();
    s.stasis_pod_count = 1;
    s.total_time       = 200.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.25));
}

TEST_CASE("stasis_drain cap: 2 surviving pods have cap 4.0 at t=300 (unclamped)",
          "[stasis_drain][pod_death][build41]") {
    // stasis_pod_count=2 → pod_cap = 2 * 2.0 = 4.0; STASIS_DRAIN_CAP=4.0
    // formula at t=300: 0.05 + 0.001*300 = 0.35 → NOT clamped (0.35 < 4.0)
    GameState s = game_init();
    s.stasis_pod_count = 2;
    s.total_time       = 300.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.35));
}

TEST_CASE("stasis_drain cap: 0 surviving pods → drain = 0.0",
          "[stasis_drain][pod_death][build41]") {
    // stasis_pod_count=0 → cap = 0 * 0.4 = 0.0; min(anything, 0) = 0
    GameState s = game_init();
    s.stasis_pod_count = 0;
    s.total_time       = 100.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.0));
}

TEST_CASE("stasis_drain cap: 49 pods (after 1st expiry) drain capped at 4.0 at t=7000",
          "[stasis_drain][pod_death][build41]") {
    // stasis_pod_count=49 → pod_cap = 49 * 2.0 = 98.0; STASIS_DRAIN_CAP=4.0
    // At t=7000: min(0.05+0.001*7000, 98.0, 4.0) = min(7.05, 98.0, 4.0) = 4.0
    GameState s = game_init();
    s.stasis_pod_count = 49;
    s.total_time       = 7000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

// ===========================================================================
// Section 8 — power_deficit_timer resets correctly across back-to-back expiry events
//
// Each expiry resets the timer to 0.0 (spec step 4). Subsequent ticks accumulate
// fresh deficit time from 0.
// ===========================================================================

TEST_CASE("timer reset: after 2nd expiry, timer is 0.0 (deficit accumulation restarts)",
          "[pod_death][timer][build41]") {
    GameState s = make_cascade_base();
    fire_one_expiry(s); // event 1 → timer=0, then tick adds delta=1.0... net=0
    // Confirm timer is exactly 0.0 after first expiry tick
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));

    fire_one_expiry(s); // event 2 → timer=0 again
    REQUIRE(s.power_deficit_timer == Catch::Approx(0.0));
}

TEST_CASE("timer reset: without reaching threshold, timer accumulates normally",
          "[pod_death][timer][build41]") {
    // Set timer=25, tick with delta=2.0; power stays 0 → timer = 25+2 = 27 (no expiry)
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.0;
    s.power_deficit_timer = 25.0;
    game_tick(s, 2.0);
    // No expiry (27 < 30); timer should be 27.0
    REQUIRE(s.power_deficit_timer == Catch::Approx(27.0));
    REQUIRE(s.stasis_pod_count == 50); // no death
}

// ===========================================================================
// Section 9 — Integration: game_over event emitted exactly once; no duplicate events
//
// When the final expiry triggers game_over, exactly 1 "game_over" event is pushed.
// Subsequent game_tick calls (game_over=true) must NOT push additional events.
// ===========================================================================

TEST_CASE("game_over event pushed exactly once when the killing expiry fires",
          "[game_over][events][build41]") {
    // Trigger the single expiry that kills the last pod
    GameState s = make_cascade_base();
    s.stasis_pod_count = 1;
    s.awoken_humans    = 0;
    s.pod_death_count  = 0;
    game_tick(s, 1.0); // 1st expiry: lose 1 → 0 pods → game_over
    REQUIRE(count_event(s, "game_over") == 1);
}

TEST_CASE("no duplicate game_over event after additional game_tick calls",
          "[game_over][events][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 1;
    s.awoken_humans    = 0;
    s.pod_death_count  = 0;
    game_tick(s, 1.0); // sets game_over = true
    s.pending_events.clear(); // drain events
    game_tick(s, 1.0); // must be inert
    game_tick(s, 1.0); // must be inert
    REQUIRE(count_event(s, "game_over") == 0);
}

TEST_CASE("no duplicate pod_lost event after game_over is set",
          "[game_over][events][build41]") {
    GameState s = make_cascade_base();
    s.stasis_pod_count = 1;
    s.awoken_humans    = 0;
    s.pod_death_count  = 0;
    game_tick(s, 1.0); // sets game_over = true; pod_lost pushed
    s.pending_events.clear();
    game_tick(s, 1.0); // must be inert — no additional pod_lost
    REQUIRE(count_event(s, "pod_lost") == 0);
}

// ===========================================================================
// Section 10 — Consecutive expiry: pod_lost count equals number of death events
// ===========================================================================

TEST_CASE("5 consecutive expiry events push exactly 5 pod_lost events total",
          "[pod_death][events][build41]") {
    GameState s = make_cascade_base();
    int total_pod_lost = 0;
    for (int i = 0; i < 5; ++i) {
        s.pending_events.clear();
        fire_one_expiry(s);
        total_pod_lost += count_event(s, "pod_lost");
    }
    REQUIRE(total_pod_lost == 5);
    // Verify pod count after 5 events: 50→49→44→34→24→14
    REQUIRE(s.stasis_pod_count == 14);
    REQUIRE(s.pod_death_count == 5);
}
